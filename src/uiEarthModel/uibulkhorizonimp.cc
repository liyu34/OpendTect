/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Nanne Hemstra
 Date:          May 2012
________________________________________________________________________

-*/


#include "uibulkhorizonimp.h"

#include "binidvalset.h"
#include "emhorizon3d.h"
#include "emmanager.h"
#include "executor.h"
#include "posinfodetector.h"
#include "od_istream.h"
#include "survinfo.h"
#include "tableascio.h"
#include "tabledef.h"

#include "uibutton.h"
#include "uifiledlg.h"
#include "uifileinput.h"
#include "uimsg.h"
#include "uitaskrunner.h"
#include "uitblimpexpdatasel.h"
#include "od_helpids.h"


class BulkHorizonAscIO : public Table::AscIO
{
public:
BulkHorizonAscIO( const Table::FormatDesc& fd, od_istream& strm )
    : Table::AscIO(fd)
    , strm_(strm)
    , finishedreadingheader_(false)
{}


static Table::FormatDesc* getDesc()
{
    Table::FormatDesc* fd = new Table::FormatDesc( "BulkHorizon" );

    fd->headerinfos_ += new Table::TargetInfo( "Undefined Value",
			StringInpSpec(sKey::FloatUdf()), Table::Required );
    fd->bodyinfos_ += new Table::TargetInfo( "Horizon name", Table::Required );
    fd->bodyinfos_ += Table::TargetInfo::mkHorPosition( true );
    fd->bodyinfos_ += Table::TargetInfo::mkZPosition( true );
    return fd;
}


bool isXY() const
{ return formOf( false, 1 ) == 0; }


bool getData( BufferString& hornm, Coord3& crd )
{
    if ( !finishedreadingheader_ )
    {
	if ( !getHdrVals(strm_) )
	    return false;

	udfval_ = getFValue( 0 );
	finishedreadingheader_ = true;
    }


    const int ret = getNextBodyVals( strm_ );
    if ( ret <= 0 ) return false;

    hornm = text( 0 );
    crd.x_ = getDValue( 1, udfval_ );
    crd.y_ = getDValue( 2, udfval_ );
    crd.z_ = getFValue( 3, udfval_ );
    return true;
}

    od_istream&		strm_;
    float		udfval_;
    bool		finishedreadingheader_;
};


uiBulkHorizonImport::uiBulkHorizonImport( uiParent* p )
    : uiDialog(p,uiDialog::Setup(uiStrings::phrImport(tr("Multiple Horizons")),
				 mNoDlgTitle,
				 mODHelpKey(mBulkHorizonImportHelpID) )
			    .modal(false))
    , fd_(BulkHorizonAscIO::getDesc())
{
    setOkText( uiStrings::sImport() );

    inpfld_ = new uiFileInput( this, 
		      uiStrings::sInputASCIIFile(),
		      uiFileInput::Setup().withexamine(true)
		      .examstyle(File::Table) );

    dataselfld_ = new uiTableImpDataSel( this, *fd_,
                                    mODHelpKey(mTableImpDataSelwellsHelpID) );
    dataselfld_->attach( alignedBelow, inpfld_ );
}


uiBulkHorizonImport::~uiBulkHorizonImport()
{
    delete fd_;
}


#define mErrRet(s) { if ( !s.isEmpty() ) uiMSG().error(s); return false; }

bool uiBulkHorizonImport::acceptOK()
{
    const BufferString fnm( inpfld_->fileName() );
    if ( fnm.isEmpty() )
	mErrRet( uiStrings::phrEnter(tr("the input file name")) )
    od_istream strm( fnm );
    if ( !strm.isOK() )
	mErrRet(uiStrings::phrCannotOpen(uiStrings::sInputFile().toLower()))

    if ( !dataselfld_->commit() )
	return false;

    ObjectSet<BinIDValueSet> data;
    BufferStringSet hornms;
    BulkHorizonAscIO aio( *fd_, strm );
    BufferString hornm; Coord3 crd;
    while ( aio.getData(hornm,crd) )
    {
	if ( hornm.isEmpty() )
	    continue;

	BinIDValueSet* bidvs = 0;
	const int hidx = hornms.indexOf( hornm );
	if ( data.validIdx(hidx) )
	    bidvs = data[hidx];
	else
	{
	    bidvs = new BinIDValueSet( 1, false );
	    data += bidvs;
	    hornms.add( hornm );
	}

	if ( !crd.isDefined() )
	    continue;

	if ( aio.isXY() )
	    bidvs->add( SI().transform(crd.getXY()), crd.z_ );
	else
	{
	    BinID bid( mNINT32(crd.x_), mNINT32(crd.y_) );
	    bidvs->add( bid, crd.z_ );
	}
    }

    // TODO: Check if name exists, ask user to overwrite or give new name
    BufferStringSet errors;
    uiTaskRunner dlg( this );
    for ( int idx=0; idx<hornms.size(); idx++ )
    {
	RefMan<EM::Horizon3D> hor3d = EM::Horizon3D::create( hornms.get(idx) );
	if ( !hor3d )
	{
	    pErrMsg( "Huh?" );
	    continue;
	}

	BinIDValueSet* bidvs = data[idx];
	BinID bid;
	BinIDValueSet::SPos pos;
	PosInfo::Detector detector( PosInfo::Detector::Setup(false) );
	while ( bidvs->next(pos) )
	{
	    bid = bidvs->getBinID( pos );
	    detector.add( SI().transform(bid), bid );
	}
	detector.finish();

	TrcKeySampling hs;
	detector.getTrcKeySampling( hs );
	ObjectSet<BinIDValueSet> curdata; curdata += bidvs;
	PtrMan<Executor> importer = hor3d->importer( curdata, hs );
	if ( !importer || !TaskRunner::execute( &dlg, *importer ) )
	    continue;

	PtrMan<Executor> saver = hor3d->saver();
	if ( !saver || !TaskRunner::execute( &dlg, *saver ) )
	    continue;
    }

    return true;
}
