/*
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert Bril
 Date:          July 2015
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id: $";

#include "uisegyreadfinisher.h"
#include "uiseissel.h"
#include "uiwellsel.h"
#include "uiseissubsel.h"
#include "uiseistransf.h"
#include "uiseisioobjinfo.h"
#include "uibatchjobdispatchersel.h"
#include "uicombobox.h"
#include "uifileinput.h"
#include "uichecklist.h"
#include "uibutton.h"
#include "uitaskrunner.h"
#include "uimsg.h"
#include "ui2dgeomman.h"
#include "segybatchio.h"
#include "segydirecttr.h"
#include "segydirectdef.h"
#include "segyscanner.h"
#include "seistrc.h"
#include "seisread.h"
#include "seiswrite.h"
#include "seisimporter.h"
#include "seisioobjinfo.h"
#include "segytr.h"
#include "welldata.h"
#include "wellmanager.h"
#include "welllog.h"
#include "welltrack.h"
#include "welllogset.h"
#include "welld2tmodel.h"
#include "iostrm.h"
#include "dbman.h"
#include "survgeom2d.h"
#include "posinfo2dsurv.h"
#include "file.h"
#include "filepath.h"


uiString uiSEGYReadFinisher::getWinTile( const FullSpec& fs )
{
    const Seis::GeomType gt = fs.geomType();
    const bool isvsp = fs.isVSP();

    uiString ret;
    if ( fs.spec_.nrFiles() > 1 && !isvsp && Seis::is2D(gt) )
	ret = tr("Import %1s");
    else
	ret = tr("Import %1");

    if ( isvsp )
	ret.arg( tr("Zero-offset VSP") );
    else
	ret.arg( Seis::nameOf(gt) );
    return ret;
}


uiString uiSEGYReadFinisher::getDlgTitle( const char* usrspec )
{
    uiString ret( tr("Importing %1") );
    ret.arg( usrspec );
    return ret;
}


uiSEGYReadFinisher::uiSEGYReadFinisher( uiParent* p, const FullSpec& fs,
					const char* usrspec, bool istime )
    : uiDialog(p,uiDialog::Setup(getWinTile(fs),getDlgTitle(usrspec),
    mODHelpKey(mSEGYReadFinisherHelpID)))
    , fs_(fs)
    , outwllfld_(0)
    , lognmfld_(0)
    , inpdomfld_(0)
    , isfeetfld_(0)
    , outimpfld_(0)
    , outscanfld_(0)
    , transffld_(0)
    , batchfld_(0)
    , docopyfld_(0)
    , coordsfromfld_(0)
    , coordfilefld_(0)
    , coordfileextfld_(0)
{
    objname_ = File::Path( usrspec ).baseName();
    const bool is2d = Seis::is2D( fs_.geomType() );
    if ( !is2d )
	objname_.replace( '*', 'x' );

    if ( fs_.isVSP() )
	crVSPFields( istime );
    else
	crSeisFields( istime );

    postFinalise().notify( mCB(this,uiSEGYReadFinisher,initWin) );
}


void uiSEGYReadFinisher::crSeisFields( bool istime )
{
    const Seis::GeomType gt = fs_.geomType();
    const bool is2d = Seis::is2D( gt );
    const bool ismulti = fs_.spec_.nrFiles() > 1;

    uiGroup* attgrp = 0;
    if ( gt != Seis::Line )
    {
	docopyfld_ = new uiGenInput( this, uiStrings::phrCopy(
				     uiStrings::sData().toLower()),
		BoolInpSpec(true,tr("Yes (import)"),tr("No (scan&&link)")) );
	docopyfld_->valuechanged.notify(mCB(this,uiSEGYReadFinisher,doScanChg));
	attgrp = docopyfld_;
    }

    uiSeisTransfer::Setup trsu( gt );
    trsu.withnullfill( false ).fornewentry( true );
    transffld_ = new uiSeisTransfer( this, trsu );
    if ( attgrp )
	transffld_->attach( alignedBelow, attgrp );
    if ( is2d )
    {
	uiSeis2DSubSel& selfld = *transffld_->selFld2D();
	if ( !ismulti )
	    selfld.setSelectedLine( objname_ );
	else
	{
	    selfld.setSelectedLine( "*" );
	    selfld.setSensitive( false );
	}
    }
    attgrp = transffld_;

    if ( is2d )
	cr2DCoordSrcFields( attgrp, ismulti );

    uiSeisSel::Setup copysu( gt );
    copysu.enabotherdomain( true )
	  .isotherdomain( istime != SI().zIsTime() );
    IOObjContext ctxt( uiSeisSel::ioContext( gt, false ) );
    outimpfld_ = new uiSeisSel( this, ctxt, copysu );
    outimpfld_->attach( alignedBelow, attgrp );
    if ( !is2d )
	outimpfld_->setInputText( objname_ );

    if ( gt != Seis::Line )
    {
	uiSeisSel::Setup scansu( copysu );
	scansu.withwriteopts( false );
	ctxt.toselect_.allownonuserselectable_ = true;
	ctxt.fixTranslator( SEGYDirectSeisTrcTranslator::translKey() );
	outscanfld_ = new uiSeisSel( this, ctxt, scansu );
	outscanfld_->attach( alignedBelow, attgrp );
	if ( !is2d )
	    outscanfld_->setInputText( objname_ );

	if ( gt != Seis::LinePS || !ismulti )
	{
	    batchfld_ = new uiBatchJobDispatcherSel( this, true,
						     Batch::JobSpec::SEGY );
	    batchfld_->setJobName( "Read SEG-Y" );
	    batchfld_->jobSpec().pars_.setYN( SEGY::IO::sKeyIs2D(), is2d );
	    batchfld_->attach( alignedBelow, outimpfld_ );
	}
    }
}


void uiSEGYReadFinisher::cr2DCoordSrcFields( uiGroup*& attgrp, bool ismulti )
{
    uiLabeledComboBox* lcb = new uiLabeledComboBox( this,
					    tr("Coordinate source") );
    lcb->attach( alignedBelow, attgrp );
    attgrp = lcb;
    coordsfromfld_ = lcb->box();
    coordsfromfld_->selectionChanged.notify(
			mCB(this,uiSEGYReadFinisher,coordsFromChg) );
    coordsfromfld_->addItem( tr("The trace headers") );

    coordsfromfld_->addItem( ismulti
	    ? tr("'Nr X Y' files (bend-points needed)")
	    : tr("A 'Nr X Y' file (bend-points needed)") );
    if ( ismulti )
    {
	coordfileextfld_ = new uiGenInput( this, tr("File extension"),
					   StringInpSpec("crd") );
	coordfileextfld_->attach( rightOf, lcb );
    }
    else
    {
	coordfilefld_ = new uiFileInput( this, uiStrings::sFileName() );
	coordfilefld_->attach( alignedBelow, lcb );

	const Coord mincoord( SI().minCoord(true) );
	coordsfromfld_->addItem( tr("Generate straight line") );
	coordsstartfld_ = new uiGenInput( this, tr("Start coordinate"),
			DoubleInpSpec((double)mNINT64(mincoord.x_)),
			DoubleInpSpec((double)mNINT64(mincoord.y_)) );
	coordsstartfld_->attach( alignedBelow, lcb );
	coordsstartfld_->setElemSzPol( uiObject::Small );
	coordsstepfld_ = new uiGenInput( this, uiStrings::sStep(),
			DoubleInpSpec(mNINT32(SI().crlDistance())),
			DoubleInpSpec(0) );
	coordsstepfld_->attach( rightOf, coordsstartfld_ );
	coordsstepfld_->setElemSzPol( uiObject::Small );

	attgrp = coordfilefld_;
    }
}


void uiSEGYReadFinisher::crVSPFields( bool istime )
{
    uiString inptxt( tr("Input Z (%1-%2) is") );
    const float startz = fs_.readopts_.timeshift_;
    const float endz = startz + fs_.readopts_.sampleintv_ * (fs_.pars_.ns_-1);
    inptxt.arg( startz ).arg( endz );
    const char* doms[] = { "TWT", "TVDSS", "MD", 0 };
    inpdomfld_ = new uiGenInput( this, inptxt, StringListInpSpec(doms) );
    if ( !istime )
	inpdomfld_->setValue( 1 );
    inpdomfld_->valuechanged.notify( mCB(this,uiSEGYReadFinisher,inpDomChg) );
    isfeetfld_ = new uiCheckBox( this, tr("in Feet") );
    isfeetfld_->attach( rightOf, inpdomfld_ );
    isfeetfld_->setChecked( fs_.zinfeet_ );

    outwllfld_ = new uiWellSel( this, true, tr("Add to Well"), false );
    outwllfld_->selectionDone.notify( mCB(this,uiSEGYReadFinisher,wllSel) );
    outwllfld_->attach( alignedBelow, inpdomfld_ );

    uiLabeledComboBox* lcb = new uiLabeledComboBox( this, tr("New log name") );
    lcb->attach( alignedBelow, outwllfld_ );
    lognmfld_ = lcb->box();
    lognmfld_->setReadOnly( false );
    lognmfld_->setText( objname_ );
}


uiSEGYReadFinisher::~uiSEGYReadFinisher()
{
}


void uiSEGYReadFinisher::initWin( CallBacker* )
{
    inpDomChg( 0 );
    coordsFromChg( 0 );
    doScanChg( 0 );
}


void uiSEGYReadFinisher::wllSel( CallBacker* )
{
    if ( !lognmfld_ )
	return;

    BufferStringSet nms; Well::MGR().getLogNames( outwllfld_->key(), nms );
    BufferString curlognm = lognmfld_->text();
    lognmfld_->setEmpty();
    lognmfld_->addItems( nms );
    if ( curlognm.isEmpty() )
	curlognm = "VSP";
    lognmfld_->setText( curlognm );
}


void uiSEGYReadFinisher::inpDomChg( CallBacker* )
{
    if ( !isfeetfld_ )
	return;

    isfeetfld_->display( inpdomfld_->getIntValue() != 0 );
}


void uiSEGYReadFinisher::coordsFromChg( CallBacker* )
{
    if ( !coordsfromfld_ )
	return;

    const int selidx = coordsfromfld_->currentItem();
    if ( coordfileextfld_ )
	coordfileextfld_->display( selidx == 1 );
    else
    {
	coordfilefld_->display( selidx == 1 );
	coordsstartfld_->display( selidx == 2 );
	coordsstepfld_->display( selidx == 2 );
    }
}


void uiSEGYReadFinisher::doScanChg( CallBacker* )
{
    if ( !docopyfld_ )
	return;

    const bool copy = docopyfld_->getBoolValue();
    outimpfld_->display( copy );
    transffld_->display( copy );
    if ( outscanfld_ )
	outscanfld_->display( !copy );
}


#define mErrRet(s) { uiMSG().error(s); return false; }

bool uiSEGYReadFinisher::doVSP()
{
    const IOObj* wllioobj = outwllfld_->ioobj();
    if ( !wllioobj )
	return false;
    const BufferString lognm( lognmfld_->text() );
    if ( lognm.isEmpty() )
	mErrRet(uiStrings::phrEnter(tr("a valid name for the new log")))

    PtrMan<SEGYSeisTrcTranslator> trl = SEGYSeisTrcTranslator::getInstance();
    trl->filePars() = fs_.pars_; trl->fileReadOpts() = fs_.readopts_;
    PtrMan<IOObj> seisioobj = fs_.spec_.getIOObj( true );
    SeisTrc trc;
    if (   !trl->initRead( seisioobj->getConn(Conn::Read), Seis::Scan )
	|| !trl->read(trc) )
	mErrRet(trl->errMsg())

    const int idom = inpdomfld_->getIntValue();
    const bool isdpth = idom > 0;
    const bool ismd = idom == 2;
    const bool inft = isdpth && isfeetfld_->isChecked();

    const DBKey wllkey( wllioobj->key() );
    uiRetVal uirv;
    RefMan<Well::Data> wd = Well::MGR().fetchForEdit( wllkey, Well::LoadReqs(),
						      uirv );
    if ( !wd )
	mErrRet( uirv )

    const Well::Track& track = wd->track();
    Well::Log* wl = new Well::Log( lognm );
    wl->pars().set( sKey::FileName(), fs_.spec_.fileName() );
    wd->logs().add( wl );

    float prevdah = mUdf(float);
    for ( int isamp=0; isamp<trc.size(); isamp++ )
    {
	float z = trc.samplePos( isamp );
	if ( !isdpth )
	    z = wd->d2TModel().getDah( z, track );
	else
	{
	    if ( inft )
		z *= mFromFeetFactorF;
	    if ( !ismd )
		prevdah = z = track.getDahForTVD( z, prevdah );
	}
	wl->addValue( z, trc.get(isamp,0) );
    }

    uirv = Well::MGR().save( wllkey );
    if ( uirv.isError() )
	mErrRet( uirv )

    return true;
}


void uiSEGYReadFinisher::updateInIOObjPars( IOObj& inioobj,
					    const IOObj& outioobj )
{
    fs_.fillPar( inioobj.pars() );
    const bool outissidom = ZDomain::isSI( outioobj.pars() );
    if ( !outissidom )
	ZDomain::Def::get(outioobj.pars()).set( inioobj.pars() );
    DBM().setEntry( inioobj );
}


SeisStdImporterReader* uiSEGYReadFinisher::getImpReader( const IOObj& ioobj,
			    SeisTrcWriter& wrr, Pos::GeomID geomid )
{
    SeisStdImporterReader* rdr = new SeisStdImporterReader( ioobj, "SEG-Y" );
    rdr->removeNull( transffld_->removeNull() );
    rdr->setResampler( transffld_->getResampler() );
    rdr->setScaler( transffld_->getScaler() );
    Seis::SelData* sd = transffld_->getSelData();
    if ( sd )
    {
	sd->setGeomID( geomid );
	rdr->setSelData( sd );
	wrr.setSelData( sd->clone() );
    }
    return rdr;
}


bool uiSEGYReadFinisher::do3D( const IOObj& inioobj, const IOObj& outioobj,
				bool doimp )
{
    Executor* exec;
    const Seis::GeomType gt = fs_.geomType();
    PtrMan<SeisTrcWriter> wrr; PtrMan<SeisImporter> imp;
    PtrMan<SEGY::FileIndexer> indexer;
    if ( doimp )
    {
	wrr = new SeisTrcWriter( &outioobj );
	imp = new SeisImporter( getImpReader(inioobj,*wrr,mUdfGeomID),
				*wrr, gt );
	exec = imp.ptr();
    }
    else
    {
	indexer = new SEGY::FileIndexer( outioobj.key(), !Seis::isPS(gt),
			fs_.spec_, false, inioobj.pars() );
	exec = indexer.ptr();
    }

    uiTaskRunner dlg( this );
    if ( !dlg.execute( *exec ) )
	return false;

    wrr.erase(); // closes output
    if ( !handleWarnings(!doimp,indexer,imp) )
	{ DBM().removeEntry( outioobj.key() ); return false; }

    if ( indexer )
    {
	IOPar inpiop; fs_.fillPar( inpiop );
	IOPar rep( "SEG-Y scan report" );
	indexer->scanner()->getReport( rep, &inpiop );
	uiSEGY::displayReport( parent(), rep );
    }

    uiSeisIOObjInfo oinf( outioobj, true );
    return oinf.provideUserInfo();
}


bool uiSEGYReadFinisher::do2D( const IOObj& inioobj, const IOObj& outioobj,
				bool doimp, const char* inplnm )
{
    const int nrlines = fs_.spec_.nrFiles();
    bool overwr_warn = true; bool overwr = true;
    TypeSet<Pos::GeomID> geomids;
    for ( int iln=0; iln<nrlines; iln++ )
    {
	const BufferString fnm( fs_.spec_.fileName(iln) );
	BufferString lnm( inplnm );
	if ( nrlines > 1 )
	    lnm = getWildcardSubstLineName( iln );

	const bool morelines = iln < nrlines - 1;
	bool isnew = true;
	if ( !handleExistingGeometry(lnm,morelines,overwr_warn,overwr,isnew) )
	    return false;

	Pos::GeomID geomid = isnew ? Geom2DImpHandler::getGeomID(lnm)
				   : Survey::GM().getGeomID(lnm);
	if ( mIsUdfGeomID(geomid) )
	    mErrRet( tr("Internal: Cannot create line geometry in database") )

	if ( !exec2Dimp(inioobj,outioobj,doimp,fnm,lnm,geomid) )
	    return false;

	geomids += geomid;
    }

    uiSeisIOObjInfo oinf( outioobj, true );
    return oinf.provideUserInfo2D( &geomids );
}


bool uiSEGYReadFinisher::doBatch( bool doimp )
{
    const BufferString jobname( doimp ? "Import SEG-Y (" : "Scan SEG-Y (",
				objname_, ")" );
    batchfld_->setJobName( jobname );
    IOPar& jobpars = batchfld_->jobSpec().pars_;
    const bool isps = Seis::isPS( fs_.geomType() );
    jobpars.set( SEGY::IO::sKeyTask(), doimp ? SEGY::IO::sKeyImport()
	    : (isps ? SEGY::IO::sKeyIndexPS() : SEGY::IO::sKeyIndex3DVol()) );
    fs_.fillPar( jobpars );

    IOPar outpars;
    transffld_->fillPar( outpars );
    outFld(doimp)->fillPar( outpars );
    jobpars.mergeComp( outpars, sKey::Output() );

    return batchfld_->start();
}


bool uiSEGYReadFinisher::exec2Dimp( const IOObj& inioobj, const IOObj& outioobj,
				bool doimp, const char* fnm, const char* lnm,
				Pos::GeomID geomid )
{
    Executor* exec;
    const Seis::GeomType gt = fs_.geomType();
    PtrMan<SeisTrcWriter> wrr; PtrMan<SeisImporter> imp;
    PtrMan<SEGY::FileIndexer> indexer; PtrMan<IOStream> iniostrm;
    SEGY::FileSpec fspec( fs_.spec_ );
    fspec.setFileName( fnm );
    if ( doimp )
    {
	iniostrm = static_cast<IOStream*>( fspec.getIOObj( true ) );
	updateInIOObjPars( *iniostrm, outioobj );
	wrr = new SeisTrcWriter( &outioobj );
	imp = new SeisImporter( getImpReader(*iniostrm,*wrr,geomid), *wrr, gt );
	BufferString nm( imp->name() ); nm.add( " (" ).add( lnm ).add( ")" );
	imp->setName( nm );
	exec = imp.ptr();
    }
    else
    {
	indexer = new SEGY::FileIndexer( outioobj.key(), !Seis::isPS(gt),
					 fspec, false, inioobj.pars() );
	exec = indexer.ptr();
    }


    uiTaskRunner dlg( this );
    if ( !dlg.execute( *exec ) )
	return false;

    wrr.erase(); // closes output
    handleWarnings( false, indexer, imp );

    return true;
}


bool uiSEGYReadFinisher::handleExistingGeometry( const char* lnm, bool morelns,
					     bool& overwr_warn, bool& overwr,
					     bool& isnewline )
{
    Pos::GeomID geomid = Survey::GM().getGeomID( lnm );
    if ( mIsUdfGeomID(geomid) )
	return true;

    isnewline = false;
    int choice = overwr ? 1 : 2;
    if ( overwr_warn )
    {
	uiDialog::Setup dsu( tr("Overwrite geometry?"),
		tr("Geometry of Line '%1' is already present."
                "\n\nDo you want to overwrite it?").arg(lnm),
                mODHelpKey(mhandleExistingGeometryHelpID));
	uiDialog dlg( this, dsu );
	uiCheckList* optfld = new uiCheckList( &dlg, uiCheckList::OneOnly );
	optfld->addItem( tr("Cancel: stop the import"), "cancel" );
	optfld->addItem( tr("Yes: set the geometry from the SEG-Y file"),
							"checkgreen" );
	optfld->addItem( tr("No: keep the existing geometry"), "handstop" );
	if ( morelns )
	{
	    optfld->addItem( tr("All: overwrite all existing geometries"),
				"doall" );
	    optfld->addItem( tr("None: keep all existing geometries"),
				"donone" );
	    choice = 3;
	}
	optfld->setChecked( choice, true );
	if ( !dlg.go() || optfld->firstChecked() == 0 )
	    return false;

	choice = optfld->firstChecked();
	if ( choice > 2 )
	    overwr_warn = false;
    }

    overwr = choice == 1 || choice == 3;
    if ( overwr )
    {
	Survey::Geometry* geom = Survey::GMAdmin().getGeometry(geomid );
	mDynamicCastGet(Survey::Geometry2D*,geom2d,geom);
	if ( geom2d )
	    geom2d->dataAdmin().setEmpty();
    }

    return true;
}


BufferString uiSEGYReadFinisher::getWildcardSubstLineName( int iln ) const
{
    BufferString fnm( fs_.spec_.fileName( iln ) );
    BufferString wildcardexpr = fs_.spec_.usrStr();

    char* pwc = wildcardexpr.getCStr();
    char* pfnm = fnm.getCStr();
    while ( *pwc && *pfnm && *pwc != '*' )
	{ pwc++; pfnm++; }
    if ( !*pwc || !*pfnm )
	return BufferString( "_" );

    BufferString ret;
    while ( pwc )
    {
	char* nonwcstart = pwc + 1;
	if ( !*nonwcstart )
	    break;

	pwc = firstOcc( nonwcstart, '*' ); // move to next wildcard, if there
	if ( pwc )
	    *pwc = '\0';

	char* subststart = pfnm;
	pfnm = firstOcc( pfnm, nonwcstart );
	if ( !pfnm )
	    { pErrMsg("Huh"); if ( ret.isEmpty() ) ret.set( "_" ); return ret; }
	*pfnm = '\0';
	pfnm += FixedString(nonwcstart).size(); // to next wildcard section

	if ( !ret.isEmpty() )
	    ret.add( '_' );
	ret.add( subststart );
    }
    return ret;
}


bool uiSEGYReadFinisher::putCoordChoiceInSpec()
{
    const int selidx = coordsfromfld_->currentItem();
    SEGY::FileReadOpts& opts = fs_.readopts_;
    opts.coorddef_ = (SEGY::FileReadOpts::CoordDefType)selidx;
    if ( selidx < 1 )
	return true;

    if ( selidx == 1 )
    {
	if ( coordfileextfld_ )
	    opts.coordfnm_.set( "ext=" ).add( coordfileextfld_->text() );
	else
	{
	    const BufferString fnm( coordfilefld_->fileName() );
	    if ( fnm.isEmpty() || !File::exists(fnm) )
	    {
		mErrRet(
		    tr("Please choose an existing file for the coordinates") )
		return false;
	    }
	    opts.coordfnm_.set( fnm );
	}
    }
    else
    {
	const Coord startcrd( coordsstartfld_->getCoord() );
	if ( mIsUdf(startcrd.x_) || mIsUdf(startcrd.y_) )
	    mErrRet(uiStrings::phrEnter(tr("the start coordinate")))
	else if ( !SI().isReasonable(startcrd) )
	    mErrRet(tr("The start coordinate is too far from the survey"))

	Coord stepcrd( coordsstepfld_->getCoord() );
	if ( mIsUdf(stepcrd.x_) ) stepcrd.x_ = 0;
	if ( mIsUdf(stepcrd.y_) ) stepcrd.y_ = 0;
	if ( mIsZero(stepcrd.x_,0.001) && mIsZero(stepcrd.y_,0.001) )
	    mErrRet(tr("The steps cannot both be zero"))

	opts.startcoord_ = startcrd;
	opts.stepcoord_ = stepcrd;
    }

    return true;
}


bool uiSEGYReadFinisher::handleWarnings( bool withstop,
				SEGY::FileIndexer* indexer, SeisImporter* imp )
{
    BufferStringSet warns;
    if ( indexer )
	warns.add( indexer->scanner()->warnings(), false );
    else
    {
	if ( imp->nrSkipped() > 0 )
	    warns += new BufferString("During import, ", imp->nrSkipped(),
				      " traces were rejected" );
	SeisStdImporterReader& stdrdr
		= static_cast<SeisStdImporterReader&>( imp->reader() );
	SeisTrcTranslator* transl = stdrdr.reader().seisTranslator();
	if ( transl && transl->haveWarnings() )
	    warns.add( transl->warnings(), false );
    }

    return uiSEGY::displayWarnings( warns, withstop );
}


void uiSEGYReadFinisher::setAsDefaultObj()
{
    if ( fs_.isVSP() || outputid_.isInvalid() )
	return;

    PtrMan<IOObj> ioobj = DBM().get( outputid_ );
    if ( ioobj )
	ioobj->setSurveyDefault();
}


bool uiSEGYReadFinisher::acceptOK()
{
    outputid_.setInvalid();
    if ( fs_.isVSP() )
	return doVSP();

    const bool doimp = docopyfld_ ? docopyfld_->getBoolValue() : true;
    const IOObj* outioobj = outFld(doimp)->ioobj();
    if ( !outioobj )
	return false;
    outputid_ = outioobj->key();

    const bool is2d = Seis::is2D( fs_.geomType() );
    BufferString lnm;

    if ( is2d )
    {
	lnm = transffld_->selFld2D()->selectedLine();
	if ( lnm.isEmpty() )
	    mErrRet( uiStrings::phrEnter(tr("a line name")) )
	if ( !putCoordChoiceInSpec() )
	    return false;
    }

    if ( doimp && !Seis::isPS(fs_.geomType()) )
    {
	uiSeisIOObjInfo oinf( *outioobj, true );
	if ( !oinf.checkSpaceLeft(transffld_->spaceInfo()) )
	    return false;
    }

    if ( batchfld_ && batchfld_->wantBatch() )
	return doBatch( doimp );

    PtrMan<IOObj> inioobj = fs_.spec_.getIOObj( true );
    updateInIOObjPars( *inioobj, *outioobj );

    return is2d ? do2D( *inioobj, *outioobj, doimp, lnm )
		: do3D( *inioobj, *outioobj, doimp );
}
