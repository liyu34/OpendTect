/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A.H. Bril
 * DATE     : Oct 2003
-*/


#include "ui2dgeomman.h"
#include "uiodmain.h"
#include "uiodmenumgr.h"
#include "uidialog.h"
#include "uifileinput.h"
#include "uilabel.h"
#include "uimenu.h"
#include "uimsg.h"
#include "uiseissel.h"
#include "uitaskrunner.h"

#include "dztimporter.h"
#include "posinfo2d.h"
#include "survinfo.h"
#include "survgeom2d.h"
#include "od_istream.h"
#include "filepath.h"
#include "odplugin.h"

#include "uiimpgprmod.h"
#include "od_helpids.h"


static const char* menunm = "GPR: DZT ...";


mDefODPluginInfo(uiImpGPR)
{
    mDefineStaticLocalObject( PluginInfo, retpi,(
	"GPR: .DZT import",
	"OpendTect",
	"Bert",
	"0.0.2",
	"Imports GPR data in DZT format."
	"\nThanks to Matthias Schuh (m.schuh@neckargeo.net) for information,"
	"\ntest data and comments.") );
    return &retpi;
}


class uiImpGPRMgr :  public CallBacker
{
public:

			uiImpGPRMgr(uiODMain&);
			~uiImpGPRMgr();

    uiODMain&		appl_;
    void		updMnu(CallBacker*);
    void		doWork(CallBacker*);
};


uiImpGPRMgr::uiImpGPRMgr( uiODMain& a )
	: appl_(a)
{
    mAttachCB( appl_.menuMgr().dTectMnuChanged, uiImpGPRMgr::updMnu );
    updMnu(0);
}


uiImpGPRMgr::~uiImpGPRMgr()
{
    detachAllNotifiers();
}


void uiImpGPRMgr::updMnu( CallBacker* )
{
    appl_.menuMgr().getMnu( true, uiODApplMgr::Seis )->insertItem(
		new uiAction(toUiString(menunm),mCB(this,uiImpGPRMgr,doWork),
									"gpr"));
}


class uiDZTImporter : public uiDialog
{ mODTextTranslationClass(uiDZTImporter);
public:

uiDZTImporter( uiParent* p )
    : uiDialog(p,Setup(uiStrings::phrImport(tr("GPR-DZT Seismics")),mNoDlgTitle,
                        mODHelpKey(mDZTImporterHelpID) ))
    , inpfld_(0)
{
    setOkText( uiStrings::sImport() );

    if ( !SI().has2D() )
	{ new uiLabel(this,tr("TODO: implement 3D loading")); return; }

    uiFileInput::Setup fisu( uiFileDialog::Gen );
    fisu.filter( "*.dzt" ).forread( true );
    inpfld_ = new uiFileInput(this, uiStrings::phrInput(tr("DZT file")), fisu);
    inpfld_->valuechanged.notify( mCB(this,uiDZTImporter,inpSel) );

    nrdeffld_ = new uiGenInput( this, tr("%1 definition: start, step")
					.arg( uiStrings::sTraceNumber() ),
				IntInpSpec(1), IntInpSpec(1) );
    nrdeffld_->attach( alignedBelow, inpfld_ );
    startposfld_ = new uiGenInput( this, tr("Start position (X,Y)"),
				PositionInpSpec(SI().minCoord(true)) );
    startposfld_->attach( alignedBelow, nrdeffld_ );
    stepposfld_ = new uiGenInput( this, tr("Step in X/Y"), FloatInpSpec(),
					FloatInpSpec(0) );
    stepposfld_->attach( alignedBelow, startposfld_ );

    zfacfld_ = new uiGenInput( this, tr("Z Factor"), FloatInpSpec(1) );
    zfacfld_->attach( alignedBelow, stepposfld_ );

    lnmfld_ = new uiGenInput( this, uiStrings::phrOutput(mJoinUiStrs(sLine(),
							sName().toLower())) );
    lnmfld_->attach( alignedBelow, zfacfld_ );

    outfld_ = new uiSeisSel( this, uiSeisSel::ioContext(Seis::Line,false),
			     uiSeisSel::Setup(Seis::Line) );
    outfld_->attach( alignedBelow, lnmfld_ );
}

void inpSel( CallBacker* )
{
    const BufferString fnm( inpfld_->fileName() );
    if ( fnm.isEmpty() ) return;

    od_istream stream( fnm );

    if ( !stream.isOK() ) return;

    DZT::FileHeader fh; uiString emsg;
    if ( !fh.getFrom(stream,emsg) ) return;

    File::Path fp( fnm ); fp.setExtension( "", true );
    lnmfld_->setText( fp.fileName() );

    const float tdist = fh.spm ? 1.f / ((float)fh.spm) : SI().inlDistance();
    stepposfld_->setValue( tdist, 0 );

}

#define mErrRet(s) { uiMSG().error(s); return false; }

bool acceptOK()
{
    if ( !inpfld_ ) return true;

    const BufferString fnm( inpfld_->fileName() );
    if ( fnm.isEmpty() ) mErrRet(tr("Please enter the input file name"))
    const BufferString lnm( lnmfld_->text() );
    if ( lnm.isEmpty() ) mErrRet(tr("Please enter the output line name"))

    Pos::GeomID geomid = Geom2DImpHandler::getGeomID( lnm );
    if ( mIsUdfGeomID(geomid) )
	return false;

    const IOObj* ioobj = outfld_->ioobj();
    if ( !ioobj ) return false;

    DZT::Importer importer( fnm, *ioobj, geomid );
    importer.fh_.nrdef_.start = nrdeffld_->getIntValue(0);
    importer.fh_.nrdef_.step = nrdeffld_->getIntValue(0);
    importer.fh_.cstart_ = startposfld_->getCoord();
    importer.fh_.cstep_.x_ = stepposfld_->getFValue(0);
    importer.fh_.cstep_.y_ = stepposfld_->getFValue(1);
    importer.zfac_ = zfacfld_->getFValue();

    uiTaskRunner taskrunner( this );
    return TaskRunner::execute( &taskrunner, importer );
}

    uiFileInput*	inpfld_;
    uiGenInput*		lnmfld_;
    uiGenInput*		nrdeffld_;
    uiGenInput*		startposfld_;
    uiGenInput*		stepposfld_;
    uiGenInput*		zfacfld_;
    uiSeisSel*		outfld_;

};


void uiImpGPRMgr::doWork( CallBacker* )
{
    if ( !uiSurvey::userIsOKWithPossibleTypeChange(true) ) return;

    uiDZTImporter dlg( &appl_ );
    dlg.go();
}


mDefODInitPlugin(uiImpGPR)
{
    mDefineStaticLocalObject( PtrMan<uiImpGPRMgr>, theinst_, = 0 );
    if ( theinst_ ) return 0;

    theinst_ = new uiImpGPRMgr( *ODMainWin() );
    if ( !theinst_ )
	return "Cannot instantiate ImpGPR plugin";

    return 0; // All OK - no error messages
}
