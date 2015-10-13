/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        K. Tingdahl
 Date:          Dec 2005
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id: uihorizontracksetup.cc 38749 2015-04-02 19:49:51Z nanne.hemstra@dgbes.com $";

#include "uihorizontracksetup.h"

#include "draw.h"
#include "emhorizon2d.h"
#include "emhorizon3d.h"
#include "emmanager.h"
#include "emsurfaceauxdata.h"
#include "emundo.h"
#include "executor.h"
#include "horizonadjuster.h"
#include "horizon2dseedpicker.h"
#include "horizon3dseedpicker.h"
#include "horizon2dtracker.h"
#include "horizon3dtracker.h"
#include "mpeengine.h"
#include "ptrman.h"
#include "randcolor.h"
#include "sectiontracker.h"
#include "seisdatapack.h"
#include "seispreload.h"
#include "survinfo.h"

#include "uibutton.h"
#include "uibuttongroup.h"
#include "uicolor.h"
#include "uiflatviewer.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uimpecorrelationgrp.h"
#include "uimpeeventgrp.h"
#include "uimpepartserv.h"
#include "uimsg.h"
#include "uiseissel.h"
#include "uiseparator.h"
#include "uislider.h"
#include "uitabstack.h"
#include "uitaskrunner.h"
#include "uitoolbar.h"
#include "uitoolbutton.h"
#include "od_helpids.h"


#define mErrRet(s) { uiMSG().error( s ); return false; }

namespace MPE
{

void uiBaseHorizonSetupGroup::initClass()
{
    uiMPE().setupgrpfact.addFactory( uiBaseHorizonSetupGroup::create,
				     Horizon2DTracker::keyword() );
    uiMPE().setupgrpfact.addFactory( uiBaseHorizonSetupGroup::create,
				     Horizon3DTracker::keyword() );
}


uiSetupGroup* uiBaseHorizonSetupGroup::create( uiParent* p, const char* typestr)
{
    const FixedString type( typestr );
    if ( type != EM::Horizon3D::typeStr() && type != EM::Horizon2D::typeStr() )
	return 0;

    return new uiBaseHorizonSetupGroup( p, typestr );
}


uiBaseHorizonSetupGroup::uiBaseHorizonSetupGroup( uiParent* p,
						  const char* typestr )
    : uiHorizonSetupGroup( p, typestr )
{}


uiHorizonSetupGroup::uiHorizonSetupGroup( uiParent* p, const char* typestr )
    : uiSetupGroup(p,"")
    , trackmgr_(0)
    , sectiontracker_(0)
    , horadj_(0)
    , is2d_(FixedString(typestr)==EM::Horizon2D::typeStr())
    , modeChanged_(this)
    , varianceChanged_(this)
    , propertyChanged_(this)
    , mps_(0)
{
    tabgrp_ = new uiTabStack( this, "TabStack" );
    uiGroup* modegrp = createModeGroup();
    tabgrp_->addTab( modegrp, tr("Mode") );

    eventgrp_ = new uiEventGroup( tabgrp_->tabGroup(), is2d_ );
    tabgrp_->addTab( eventgrp_, tr("Event") );

    correlationgrp_ = new uiCorrelationGroup( tabgrp_->tabGroup(), is2d_ );
    tabgrp_->addTab( correlationgrp_, tr("Correlation") );

//    uiGroup* vargrp = createVarianceGroup();
//    tabgrp_->addTab( vargrp, tr("Variance") );

    uiGroup* propertiesgrp = createPropertyGroup();
    tabgrp_->addTab( propertiesgrp, uiStrings::sProperties() );

    mDynamicCastGet(uiDialog*,dlg,p)
    toolbar_ = new uiToolBar( dlg, tr("Tracking tools"), uiToolBar::Left );
    initToolBar();

    engine().actionCalled.notify( mCB(this,uiHorizonSetupGroup,mpeActionCB) );
}


uiHorizonSetupGroup::~uiHorizonSetupGroup()
{
    engine().actionCalled.remove( mCB(this,uiHorizonSetupGroup,mpeActionCB) );
}


void uiHorizonSetupGroup::initToolBar()
{
    trackbutid_ = -1;

    startbutid_ = toolbar_->addButton("autotrack",tr("Start Auto Tracking [K]"),
				mCB(this,uiHorizonSetupGroup,startCB) );
    toolbar_->setShortcut( startbutid_, "k" );

    stopbutid_ = toolbar_->addButton( "stop", tr("Stop Auto Tracking [S]"),
				mCB(this,uiHorizonSetupGroup,stopCB) );
    toolbar_->setShortcut( stopbutid_, "s" );

    savebutid_ = toolbar_->addButton( "save", uiStrings::phrSave(
			  toUiString("%1 [Ctrl+S]").arg(uiStrings::sHorizon())),
			  mCB(this,uiHorizonSetupGroup,saveCB) );
    toolbar_->setShortcut( savebutid_, "ctrl+s" );

    retrackbutid_ = toolbar_->addButton( "retrackhorizon", tr("Retrack All"),
				mCB(this,uiHorizonSetupGroup,retrackCB) );

    undobutid_ = toolbar_->addButton( "undo", toUiString("%1 [Ctrl+Z]")
					.arg(tr("Undo")),
					mCB(this,uiHorizonSetupGroup,undoCB) );
    toolbar_->setShortcut( undobutid_, "ctrl+z" );

    redobutid_ = toolbar_->addButton( "redo", toUiString("%1 [Ctrl+Y]")
					.arg(tr("Redo")),
					mCB(this,uiHorizonSetupGroup,redoCB) );
    toolbar_->setShortcut( redobutid_, "ctrl+y" );

    updateButtonSensitivity();
}


void uiHorizonSetupGroup::setMPEPartServer( uiMPEPartServer* mps )
{
    mps_ = mps;
}


void uiHorizonSetupGroup::mpeActionCB( CallBacker* )
{
    updateButtonSensitivity();
}


void uiHorizonSetupGroup::updateButtonSensitivity()
{
    const bool enable = engine().getState() == MPE::Engine::Stopped;
    toolbar_->setSensitive( startbutid_, enable && !is2d_ );
    toolbar_->setSensitive( stopbutid_, !enable && !is2d_ );
    toolbar_->setSensitive( savebutid_, enable );
    toolbar_->setSensitive( retrackbutid_, enable );

    toolbar_->setSensitive( undobutid_, EM::EMM().undo().canUnDo() );
    toolbar_->setSensitive( redobutid_, EM::EMM().undo().canReDo() );

    if ( trackbutid_ != -1 )
    {
	EMTracker* tracker = engine().getActiveTracker();
	toolbar_->turnOn( trackbutid_, tracker ? tracker->isEnabled() : false );
    }
}


void uiHorizonSetupGroup::enabTrackCB( CallBacker* )
{
    engine().enableTracking( toolbar_->isOn(trackbutid_) );
}


void uiHorizonSetupGroup::startCB( CallBacker* )
{
    uiString errmsg;
    if ( !engine().startTracking(errmsg) && !errmsg.isEmpty() )
	uiMSG().error( errmsg );
}


void uiHorizonSetupGroup::stopCB( CallBacker* )
{
    engine().stopTracking();
}


void uiHorizonSetupGroup::saveCB( CallBacker* )
{
    if ( !mps_ ) return;

    mps_->sendMPEEvent( uiMPEPartServer::evStoreEMObject() );
}


void uiHorizonSetupGroup::retrackCB( CallBacker* )
{
    uiString errmsg;
    if ( !engine().startRetrack(errmsg) && !errmsg.isEmpty() )
	uiMSG().error( errmsg );
}


void uiHorizonSetupGroup::undoCB( CallBacker* )
{
    MouseCursorChanger mcc( MouseCursor::Wait );
    uiString errmsg;
    engine().undo( errmsg );
    if ( !errmsg.isEmpty() )
	uiMSG().message( errmsg );

    updateButtonSensitivity();
}


void uiHorizonSetupGroup::redoCB( CallBacker* )
{
    MouseCursorChanger mcc( MouseCursor::Wait );
    uiString errmsg;
    engine().redo( errmsg );
    if ( !errmsg.isEmpty() )
	uiMSG().message( errmsg );

    updateButtonSensitivity();
}


uiGroup* uiHorizonSetupGroup::createModeGroup()
{
    uiGroup* grp = new uiGroup( tabgrp_->tabGroup(), "Mode" );

    modeselgrp_ = new uiButtonGroup( grp, "ModeSel", OD::Vertical );
    modeselgrp_->setExclusive( true );
    grp->setHAlignObj( modeselgrp_ );

    const int nrmodes = EMSeedPicker::nrTrackModes( is2d_ );
    for ( int idx=0; idx<nrmodes; idx++ )
    {
	EMSeedPicker::TrackMode md = (EMSeedPicker::TrackMode)idx;
	uiRadioButton* butptr = new uiRadioButton( modeselgrp_,
		    EMSeedPicker::getTrackModeText(md,is2d_) );
	butptr->activated.notify(
		    mCB(this,uiHorizonSetupGroup,seedModeChange) );
	mode_ =  EMSeedPicker::TrackBetweenSeeds;
    }

    uiSeparator* sep = new uiSeparator( grp );
    sep->attach( stretchedBelow, modeselgrp_ );
    BufferStringSet strs; strs.add( "Seed Trace" ).add( "Adjacent Parent" );
    methodfld_ = new uiGenInput( grp, tr("Method"), StringListInpSpec(strs) );
    methodfld_->attach( alignedBelow, modeselgrp_ );
    methodfld_->attach( ensureBelow, sep );

    return grp;
}


uiGroup* uiHorizonSetupGroup::createVarianceGroup()
{
    uiGroup* grp = new uiGroup( tabgrp_->tabGroup(), "Variance" );

    usevarfld_ = new uiGenInput( grp, tr("Use Variance"), BoolInpSpec(false) );
    usevarfld_->valuechanged.notify(
	    mCB(this,uiHorizonSetupGroup,selUseVariance) );
    usevarfld_->valuechanged.notify(
	    mCB(this,uiHorizonSetupGroup,varianceChangeCB) );

    const IOObjContext ctxt =
	uiSeisSel::ioContext( is2d_ ? Seis::Line : Seis::Vol, true );
    uiSeisSel::Setup ss( is2d_, false );
    variancefld_ = new uiSeisSel( grp, ctxt, ss );
    variancefld_->attach( alignedBelow, usevarfld_ );

    varthresholdfld_ =
	new uiGenInput( grp, tr("Variance threshold"), FloatInpSpec() );
    varthresholdfld_->attach( alignedBelow, variancefld_ );
    varthresholdfld_->valuechanged.notify(
	    mCB(this,uiHorizonSetupGroup,varianceChangeCB) );

    grp->setHAlignObj( usevarfld_ );
    return grp;
}


uiGroup* uiHorizonSetupGroup::createPropertyGroup()
{
    uiGroup* grp = new uiGroup( tabgrp_->tabGroup(), "Properties" );
    colorfld_ = new uiColorInput( grp,
				uiColorInput::Setup(Color::Green())
				.withdesc(false).lbltxt(tr("Horizon Color")) );
    colorfld_->colorChanged.notify(
			mCB(this,uiHorizonSetupGroup,colorChangeCB) );
    grp->setHAlignObj( colorfld_ );

    linewidthfld_ = new uiSlider( grp, uiSlider::Setup(tr("Line Width"))
					.withedit(true),
				  "Line Width" );
    linewidthfld_->setInterval( 1, 15, 1 );
    linewidthfld_->valueChanged.notify(
			mCB(this,uiHorizonSetupGroup,colorChangeCB) );
    linewidthfld_->attach( alignedBelow, colorfld_ );

    seedtypefld_ = new uiGenInput( grp, tr("Seed Shape/Color"),
			StringListInpSpec(MarkerStyle3D::TypeDef()) );
    seedtypefld_->valuechanged.notify(
			mCB(this,uiHorizonSetupGroup,seedTypeSel) );
    seedtypefld_->attach( alignedBelow, linewidthfld_ );

    seedcolselfld_ = new uiColorInput( grp,
				uiColorInput::Setup(Color::DgbColor())
				.withdesc(false) );
    seedcolselfld_->attach( rightTo, seedtypefld_ );
    seedcolselfld_->colorChanged.notify(
				mCB(this,uiHorizonSetupGroup,seedColSel) );

    seedsliderfld_ = new uiSlider( grp,
				uiSlider::Setup(tr("Seed Size")).
				withedit(true),	"Seed Size" );
    seedsliderfld_->setInterval( 1, 15 );
    seedsliderfld_->valueChanged.notify(
			mCB(this,uiHorizonSetupGroup,seedSliderMove) );
    seedsliderfld_->attach( alignedBelow, seedtypefld_ );

    parentcolfld_ = new uiColorInput( grp,
				uiColorInput::Setup(Color::Yellow())
				.withdesc(false).lbltxt(tr("Parents")) );
    parentcolfld_->colorChanged.notify(
			mCB(this,uiHorizonSetupGroup,specColorChangeCB) );
    parentcolfld_->attach( alignedBelow, seedsliderfld_ );

    selectioncolfld_ = new uiColorInput( grp,
				uiColorInput::Setup(Color::Yellow())
				.withdesc(false).lbltxt(tr("Selections")) );
    selectioncolfld_->colorChanged.notify(
			mCB(this,uiHorizonSetupGroup,specColorChangeCB) );
    selectioncolfld_->attach( rightTo, parentcolfld_ );

    lockcolfld_ = new uiColorInput( grp,
				uiColorInput::Setup(Color::Orange())
				.withdesc(false).lbltxt(tr("Locked")) );
    lockcolfld_->colorChanged.notify(
			mCB(this,uiHorizonSetupGroup,specColorChangeCB) );
    lockcolfld_->attach( alignedBelow, parentcolfld_ );

    return grp;
}


NotifierAccess* uiHorizonSetupGroup::eventChangeNotifier()
{ return eventgrp_->changeNotifier(); }


NotifierAccess*	uiHorizonSetupGroup::correlationChangeNotifier()
{ return correlationgrp_->changeNotifier(); }


void uiHorizonSetupGroup::selUseVariance( CallBacker* )
{
    const bool usevar = usevarfld_->getBoolValue();
    variancefld_->setSensitive( usevar );
    varthresholdfld_->setSensitive( usevar );
}


void uiHorizonSetupGroup::seedModeChange( CallBacker* )
{
    mode_ = (EMSeedPicker::TrackMode)modeselgrp_->selectedId();
    modeChanged_.trigger();

    const bool usedata = mode_ != EMSeedPicker::DrawBetweenSeeds;
    const bool invol = mode_ == EMSeedPicker::TrackFromSeeds;
    tabgrp_->setTabEnabled( eventgrp_, usedata );
    tabgrp_->setTabEnabled( correlationgrp_, usedata );

    toolbar_->setSensitive( startbutid_, invol );
    toolbar_->setSensitive( stopbutid_, invol );
    toolbar_->setSensitive( retrackbutid_, usedata );
}


void uiHorizonSetupGroup::varianceChangeCB( CallBacker* )
{ varianceChanged_.trigger(); }


void uiHorizonSetupGroup::specColorChangeCB( CallBacker* cb )
{
    if ( !sectiontracker_ ) return;

    mDynamicCastGet(EM::Horizon3D*,hor3d,&sectiontracker_->emObject())
    if ( !hor3d ) return;

    if ( cb == parentcolfld_ )
	hor3d->setParentColor( parentcolfld_->color() );
    else if ( cb==selectioncolfld_ )
	hor3d->setSelectionColor( selectioncolfld_->color() );
    else if ( cb==lockcolfld_ )
	hor3d->setLockColor( lockcolfld_->color() );
}


void uiHorizonSetupGroup::colorChangeCB( CallBacker* )
{
    propertyChanged_.trigger();
}


void uiHorizonSetupGroup::seedTypeSel( CallBacker* )
{
    const MarkerStyle3D::Type newtype =
	(MarkerStyle3D::Type)(MarkerStyle3D::None+seedtypefld_->getIntValue());
    if ( markerstyle_.type_ == newtype )
	return;
    markerstyle_.type_ = newtype;
    propertyChanged_.trigger();
}


void uiHorizonSetupGroup::seedSliderMove( CallBacker* )
{
    const float sldrval = seedsliderfld_->getFValue();
    const int newsize = mNINT32(sldrval);
    if ( markerstyle_.size_ == newsize )
	return;
    markerstyle_.size_ = newsize;
    propertyChanged_.trigger();
}


void uiHorizonSetupGroup::seedColSel( CallBacker* )
{
    const Color newcolor = seedcolselfld_->color();
    if ( markerstyle_.color_ == newcolor )
	return;
    markerstyle_.color_ = newcolor;
    propertyChanged_.trigger();
}


void uiHorizonSetupGroup::setSectionTracker( SectionTracker* st )
{
    sectiontracker_ = st;
    mDynamicCastGet(HorizonAdjuster*,horadj,sectiontracker_->adjuster())
    horadj_ = horadj;
    if ( !horadj_ ) return;

    initStuff();
    correlationgrp_->setSectionTracker( st );
    eventgrp_->setSectionTracker( st );
}


void uiHorizonSetupGroup::initModeGroup()
{
    if ( EMSeedPicker::nrTrackModes(is2d_) > 0 )
	modeselgrp_->selectButton( mode_ );

    methodfld_->setValue(
	horadj_->getCompareMethod()==EventTracker::SeedTrace ? 0 : 1 );
}


void uiHorizonSetupGroup::initStuff()
{
    initModeGroup();
//    initVarianceGroup();
//    selUseVariance(0);
    initPropertyGroup();
}


void uiHorizonSetupGroup::initVarianceGroup()
{
}


void uiHorizonSetupGroup::initPropertyGroup()
{
    seedsliderfld_->setValue( markerstyle_.size_ );
    seedcolselfld_->setColor( markerstyle_.color_ );
    seedtypefld_->setValue( markerstyle_.type_ - MarkerStyle3D::None );
}


void uiHorizonSetupGroup::setMode( EMSeedPicker::TrackMode mode )
{
    mode_ = mode;
    modeselgrp_->selectButton( (int)mode_ );
}


EMSeedPicker::TrackMode uiHorizonSetupGroup::getMode() const
{
    return (EMSeedPicker::TrackMode)
	(modeselgrp_ ? modeselgrp_->selectedId() : 0);
}


void uiHorizonSetupGroup::setSeedPos( const TrcKeyValue& tkv )
{
    eventgrp_->setSeedPos( tkv );
    correlationgrp_->setSeedPos( tkv );
    updateButtonSensitivity();
}


void uiHorizonSetupGroup::setColor( const Color& col )
{ colorfld_->setColor( col ); }

const Color& uiHorizonSetupGroup::getColor()
{ return colorfld_->color(); }

void uiHorizonSetupGroup::setLineWidth( int w )
{ linewidthfld_->setValue( w ); }

int uiHorizonSetupGroup::getLineWidth() const
{ return linewidthfld_->getIntValue(); }


void uiHorizonSetupGroup::setMarkerStyle( const MarkerStyle3D& markerstyle )
{
    markerstyle_ = markerstyle;
    initPropertyGroup();
}


const MarkerStyle3D& uiHorizonSetupGroup::getMarkerStyle()
{
    return markerstyle_;
}


bool uiHorizonSetupGroup::commitToTracker( bool& fieldchange ) const
{
    if ( !sectiontracker_ || !horadj_ )
	return false;

    fieldchange = false;
    correlationgrp_->commitToTracker( fieldchange );
    eventgrp_->commitToTracker( fieldchange );

    if ( !horadj_ || horadj_->getNrAttributes()<1 )
    {
	uiMSG().warning( tr("Unable to apply tracking setup") );
	return true;
    }

    horadj_->setCompareMethod( methodfld_->getIntValue()==0 ?
		EventTracker::SeedTrace : EventTracker::AdjacentParent );

    return true;
}


void uiHorizonSetupGroup::showGroupOnTop( const char* grpnm )
{
    tabgrp_->setCurrentPage( grpnm );
    mDynamicCastGet(uiDialog*,dlg,parent())
    if ( dlg && !dlg->isHidden() )
    {
	 dlg->showNormal();
	 dlg->raise();
    }
}


} //namespace MPE
