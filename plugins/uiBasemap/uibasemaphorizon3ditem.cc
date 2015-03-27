/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Henrique Mageste
 Date:		December 2014
________________________________________________________________________

-*/

static const char* rcsID mUsedVar = "$Id$";

#include "uibasemaphorizon3ditem.h"

#include "basemaphorizon3d.h"

#include "uibitmapdisplay.h"
#include "uigraphicsscene.h"
#include "uigraphicsview.h"
#include "uiioobjselgrp.h"
#include "uimenu.h"
#include "uistrings.h"
#include "uitaskrunner.h"
#include "uiworld2ui.h"

#include "emhorizon3d.h"
#include "emmanager.h"
#include "emsurfacetr.h"
#include "executor.h"
#include "flatposdata.h"
#include "flatview.h"

#include "coltabindex.h"
#include "coltabmapper.h"
#include "coltabsequence.h"
#include "ioman.h"
#include "survinfo.h"

// uiBasemapHorizon3DGroup
uiBasemapHorizon3DGroup::uiBasemapHorizon3DGroup( uiParent* p, bool isadd )
    : uiBasemapGroup(p)
{
    ioobjfld_ = new uiIOObjSelGrp( this, mIOObjContext(EMHorizon3D),
				   uiIOObjSelGrp::Setup(OD::ChooseAtLeastOne) );
    ioobjfld_->selectionChanged.notify(
				   mCB(this,uiBasemapHorizon3DGroup,selChg));

    addNameField();
}


uiBasemapHorizon3DGroup::~uiBasemapHorizon3DGroup()
{
}


bool uiBasemapHorizon3DGroup::acceptOK()
{
    const bool res = uiBasemapGroup::acceptOK();
    if ( !res ) return false;

    PtrMan<Executor> exec = EM::EMM().objectLoader( mids_ );
    if ( !exec ) return true;

    uiTaskRunner uitr( &BMM().getBasemap() );
    return uitr.execute( *exec );
}


bool uiBasemapHorizon3DGroup::fillPar( IOPar& par ) const
{
    const bool res = uiBasemapGroup::fillPar( par );

    const int nritems = mids_.size();
    par.set( sKey::NrItems(), nritems );
    for ( int idx=0; idx<nritems; idx++ )
    {
	IOPar ipar;
	ipar.set( sKey::Name(), IOM().nameOf(mids_[idx]) );
	ipar.set( IOPar::compKey(sKey::ID(),0), mids_[idx] );
	const BufferString key = IOPar::compKey( sKeyItem(), idx );
	par.mergeComp( ipar, key );
    }

    return res;
}


bool uiBasemapHorizon3DGroup::usePar( const IOPar& par )
{
    const bool res = uiBasemapGroup::usePar( par );
    int nritems = 0;
    par.get( sKey::NrItems(), nritems );

    TypeSet<MultiID> mids( nritems, MultiID::udf() );
    for ( int idx=0; idx<nritems; idx++ )
	par.get( IOPar::compKey(sKey::ID(),idx), mids[idx] );

    ioobjfld_->setChosen( mids );

    return res;
}


uiObject* uiBasemapHorizon3DGroup::lastObject()
{ return ioobjfld_->attachObj(); }


void uiBasemapHorizon3DGroup::selChg(CallBacker *)
{
    ioobjfld_->getChosen( mids_ );
    const int nrsel = ioobjfld_->nrChosen();
    if ( nrsel==1 )
	setItemName( IOM().nameOf(ioobjfld_->currentID()) );
    else
    {
	BufferString typestr = ioobjfld_->getContext().trgroup->userName();
	typestr.add( "s" );
	setItemName( typestr );
    }
}



// uiBasemapHorizonObject
uiBasemapHorizon3DObject::uiBasemapHorizon3DObject( BaseMapObject* bmobj )
    : uiBaseMapObject(bmobj)
    , appearance_(*new FlatView::Appearance)
    , bitmapdisp_(*new uiBitMapDisplay(appearance_))
    , hor3d_(0)
    , dp_(0)
{
    appearance_.ddpars_.show( false, true );
    bitmapdisp_.getDisplay()->setZValue( 0 );
    itemgrp_.add( bitmapdisp_.getDisplay() );
}


uiBasemapHorizon3DObject::~uiBasemapHorizon3DObject()
{
    itemgrp_.remove( bitmapdisp_.getDisplay(), false );

    delete &appearance_;
    delete &bitmapdisp_;

    if ( hor3d_ ) hor3d_->unRef();
    DPM(DataPackMgr::FlatID()).release( dp_ );
}


void uiBasemapHorizon3DObject::setHorizon( const EM::Horizon3D* hor3d )
{
    if ( hor3d_ ) hor3d_->unRef();
    hor3d_ = hor3d;
    if ( hor3d_ ) hor3d_->ref();

    Array2D<float>* arr = hor3d_->createArray2D( hor3d_->sectionID(0) );
    if ( !arr ) return;

    BufferStringSet dimnames;
    dimnames.add("X").add("Y").add(sKey::Inline()).add(sKey::Crossline());

    StepInterval<double> inlrg, crlrg;
    inlrg.setFrom( hor3d_->range().inlRange() );
    crlrg.setFrom( hor3d_->range().crlRange() );
    dp_ = new MapDataPack( "Horizon3D", arr );
    dp_->setProps( inlrg, crlrg, true, &dimnames );
    DPM(DataPackMgr::FlatID()).addAndObtain( dp_ );
    bitmapdisp_.setDataPack( dp_, false );
    bitmapdisp_.getDisplay()->show();
    update();
}


void uiBasemapHorizon3DObject::update()
{
    if ( !dp_ ) return;

    const FlatPosData& pd = dp_->posData();
    StepInterval<double> rg0( pd.range(true) );
    StepInterval<double> rg1( pd.range(false) );
    rg0.sort( true );
    rg1.sort( true );
    bitmapdisp_.setBoundingBox(
	uiWorldRect(rg0.start,rg1.stop,rg0.stop,rg1.start) );
    bitmapdisp_.update();
}



// uiBasemapHorizon3DTreeItem
uiBasemapHorizon3DTreeItem::uiBasemapHorizon3DTreeItem( const char* nm )
    : uiBasemapTreeItem(nm)
    , uibmobj_(new uiBasemapHorizon3DObject(0))
{
    BMM().getBasemap().worldItemGroup().add( &uibmobj_->itemGrp() );
}


uiBasemapHorizon3DTreeItem::~uiBasemapHorizon3DTreeItem()
{
    BMM().getBasemap().worldItemGroup().remove( &uibmobj_->itemGrp(), true );
    delete uibmobj_;
}


bool uiBasemapHorizon3DTreeItem::usePar( const IOPar& par )
{
    uiBasemapTreeItem::usePar( par );

    MultiID mid;
    if ( !par.get(IOPar::compKey(sKey::ID(),0),mid) ) return false;

    RefMan<EM::EMObject> emobj = EM::EMM().loadIfNotFullyLoaded( mid, 0 );
    mDynamicCastGet(EM::Horizon3D*,hor,emobj.ptr());
    if ( !hor ) return false;

    uibmobj_->setHorizon( hor );
    return true;
}


void uiBasemapHorizon3DTreeItem::checkCB(CallBacker *)
{
    uibmobj_->show( isChecked() );
}


bool uiBasemapHorizon3DTreeItem::showSubMenu()
{
    uiMenu mnu( getUiParent(), "Action" );
    mnu.insertItem( new uiAction(uiStrings::sEdit(false)), sEditID() );
    mnu.insertItem( new uiAction(uiStrings::sRemove(true)), sRemoveID() );
    const int mnuid = mnu.exec();
    return handleSubMenu( mnuid );
}


bool uiBasemapHorizon3DTreeItem::handleSubMenu( int mnuid )
{
    return uiBasemapTreeItem::handleSubMenu(mnuid);
}


// uiBasemapHorizon3DItem
int uiBasemapHorizon3DItem::defaultZValue() const
{ return 0; }

const char* uiBasemapHorizon3DItem::iconName() const
{ return "basemap-horizon3d"; }

uiBasemapGroup* uiBasemapHorizon3DItem::createGroup( uiParent* p, bool isadd )
{
    return new uiBasemapHorizon3DGroup( p, isadd );
}

uiBasemapTreeItem* uiBasemapHorizon3DItem::createTreeItem( const char* nm )
{ return new uiBasemapHorizon3DTreeItem( nm ); }