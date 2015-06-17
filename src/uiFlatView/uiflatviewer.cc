/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Feb 2007
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "uiflatviewer.h"

#include "uiflatauxdatadisplay.h"
#include "uibitmapdisplay.h"
#include "uigraphicsscene.h"
#include "uigraphicsview.h"

#include "bufstringset.h"
#include "uigraphicssceneaxismgr.h"
#include "flatposdata.h"
#include "flatviewaxesdrawer.h"
#include "threadwork.h"
#include "mousecursor.h"


uiFlatViewer::uiFlatViewer( uiParent* p )
    : uiGroup(p,"Flat viewer")
    , view_( new uiGraphicsView( this, "Flatview" ) )
    , axesdrawer_(*new AxesDrawer(*this))
    , extfac_(0.5f)
    , worldgroup_( new uiGraphicsItemGroup( true ) )
    , annotwork_( mCB(this,uiFlatViewer,updateAnnotCB) )
    , auxdatawork_( mCB(this,uiFlatViewer,updateAuxDataCB) )
    , bitmapwork_( mCB(this,uiFlatViewer,updateBitmapCB) )
    , control_(0)
    , xseldatarange_(mUdf(float),mUdf(float))
    , yseldatarange_(mUdf(float),mUdf(float))
    , useseldataranges_(false)
    , viewChanged(this)
    , dataChanged(this)
    , dispParsChanged(this)
    , annotChanged(this)
    , dispPropChanged(this)
    , updatebitmapsonresize_(true)
    , wr_(0,0,1,1)
{
    updatequeueid_ =
	Threads::WorkManager::twm().addQueue( Threads::WorkManager::Manual,
		 			      "FlatViewer");

    view_->preDraw.notify( mCB(this,uiFlatViewer,updateCB ) );
    view_->disableScrollZoom();
    view_->scene().addItem( worldgroup_ );
    view_->setDragMode( uiGraphicsView::ScrollHandDrag );
    view_->setScrollBarPolicy( false, uiGraphicsViewBase::ScrollBarAlwaysOff );
    view_->setScrollBarPolicy( true, uiGraphicsViewBase::ScrollBarAlwaysOff );
    view_->setSceneBorder( 2 );
    view_->reSize.notify( mCB(this,uiFlatViewer,reSizeCB) );
    setStretch( 2, 2 ); view_->setStretch( 2, 2 );

    bitmapdisp_ = new uiBitMapDisplay( appearance(), false );
    bitmapdisp_->getDisplay()->setZValue( bitMapZVal() );
    worldgroup_->add( bitmapdisp_->getDisplay() );

    axesdrawer_.setZValue( annotZVal() );
    axesdrawer_.setWorldCoords( wr_ );
    mAttachCB( axesdrawer_.layoutChanged(), uiFlatViewer::reSizeCB );
}


uiFlatViewer::~uiFlatViewer()
{
    detachAllNotifiers();
    Threads::WorkManager::twm().removeQueue( updatequeueid_, false );

    delete &axesdrawer_;

    bitmapdisp_->removeDisplay();
    delete bitmapdisp_;

    delete view_->scene().removeItem( worldgroup_ );

    deepErase( auxdata_ );
}


MouseEventHandler& uiFlatViewer::getMouseEventHandler()
{ return view_->getMouseEventHandler(); }


void uiFlatViewer::reSizeCB( CallBacker* )
{
    if ( !updatebitmapsonresize_ )
	return;

    axesdrawer_.updateScene();
    updateTransforms();
    w2ui_.set( getViewRect(), wr_ );
}


uiBorder uiFlatViewer::getAnnotBorder() const
{
    return axesdrawer_.getAnnotBorder();
}


uiRect uiFlatViewer::getViewRect( bool withextraborders ) const
{
    return axesdrawer_.getViewRect( withextraborders );
}


void uiFlatViewer::updateAuxDataCB( CallBacker* )
{
    for ( int idx=0; idx<auxdata_.size(); idx++ )
    {
	auxdata_[idx]->touch();
    }
}

#define mAHLineType (uiAxisHandler::AuxPosData::LineType)

void uiFlatViewer::updateAnnotCB( CallBacker* cb )
{
    const FlatView::Annotation& annot = appearance().annot_;
    axesdrawer_.setAuxLineStyle( annot.x1_.auxlinestyle_, true );
    axesdrawer_.setAuxLineStyle( annot.x1_.auxhllinestyle_, true, true );
    axesdrawer_.showAuxPositions( true, annot.x1_.showauxpos_,
	    			  annot.x1_.showauxlines_ );
    TypeSet<uiAxisHandler::AuxPosData> x1poss;
    if ( !annot.x1_.auxposs_.isEmpty() )
    {
	x1poss.setSize( annot.x1_.auxposs_.size(), uiAxisHandler::AuxPosData());
	for ( int idx=0; idx<annot.x1_.auxposs_.size(); idx++ )
	{
	    x1poss[idx].pos_ = annot.x1_.auxposs_[idx].pos_;
	    x1poss[idx].linetype_ =
		mAHLineType int (annot.x1_.auxposs_[idx].linetype_);
	    x1poss[idx].name_ = annot.x1_.auxposs_[idx].name_;
	}
    }

    axesdrawer_.setAuxAnnotPositions( x1poss, true ); 

    axesdrawer_.setAuxLineStyle( annot.x2_.auxlinestyle_, false );
    axesdrawer_.setAuxLineStyle( annot.x2_.auxhllinestyle_, false, true );
    axesdrawer_.showAuxPositions( false, annot.x2_.showauxpos_,
	    			  annot.x2_.showauxlines_ );
    TypeSet<uiAxisHandler::AuxPosData> x2poss;
    if ( !annot.x2_.auxposs_.isEmpty() )
    {
	x2poss.setSize( annot.x2_.auxposs_.size(), uiAxisHandler::AuxPosData());
	for ( int idx=0; idx<annot.x2_.auxposs_.size(); idx++ )
	{
	    x2poss[idx].pos_ = annot.x2_.auxposs_[idx].pos_;
	    x2poss[idx].linetype_ =
		mAHLineType int (annot.x2_.auxposs_[idx].linetype_);
	    x2poss[idx].name_ = annot.x2_.auxposs_[idx].name_;
	}
    }

    axesdrawer_.setAuxAnnotPositions( x2poss, false );

    axesdrawer_.setWorldCoords( wr_ );
    if ( !wr_.checkCorners(!annot.x1_.reversed_,annot.x2_.reversed_) )
    {
	if ( isVisible(true) && wr_.revX()!=annot.x1_.reversed_ )
	    updateBitmapCB( cb ); //<<-- To redraw bitmaps with wiggles flipped.

	setView( wr_ ); //<<-- To flip the resultant image.
    }

    reSizeCB(0); // Needed as annotation changes may make view-area
    		 // larger or smaller.
    annotChanged.trigger();
}


void uiFlatViewer::updateTransforms()
{
    if ( mIsZero(wr_.width(),mDefEps) || mIsZero(wr_.height(),mDefEps) )
        return;

    const uiRect viewrect = getViewRect();
    const double xscale = viewrect.width()/(wr_.right()-wr_.left());
    const double yscale = viewrect.height()/(wr_.bottom()-wr_.top());
    const double xpos = viewrect.left()-xscale*wr_.left();
    const double ypos = viewrect.top()-yscale*wr_.top();

    worldgroup_->setPos( uiWorldPoint( xpos, ypos ) );
    worldgroup_->setScale( (float) xscale, (float) yscale );
}


void uiFlatViewer::setBoundingRect( const uiRect& boundingrect )
{
    const uiRect viewrect = getViewRect( false );
    int extrawidth = viewrect.width() - boundingrect.width();
    int extraheight = viewrect.height() - boundingrect.height();
    if ( extrawidth < 0 ) extrawidth = 0;
    if ( extraheight < 0 ) extraheight = 0;
    const uiBorder border( 0, 0, extrawidth, extraheight );
    axesdrawer_.setExtraBorder( border );
}


void uiFlatViewer::setExtraBorders( const uiSize& lfttp, const uiSize& rghtbt )
{
    uiBorder border( lfttp.width(), lfttp.height(), rghtbt.width(),
		     rghtbt.height() );
    axesdrawer_.setExtraBorder( border );
}


void uiFlatViewer::setInitialSize( const uiSize& sz )
{
    setPrefWidth( sz.width() ); setPrefHeight( sz.height() );
    view_->setPrefWidth( sz.width() ); view_->setPrefHeight( sz.height() );
}


uiWorldRect uiFlatViewer::getBoundingBox( bool wva ) const
{
    ConstDataPackRef<FlatDataPack> dp = obtainPack( wva, true );
    if ( !dp ) return uiWorldRect(0,0,1,1);

    const FlatPosData& pd = dp->posData();
    StepInterval<double> rg0( pd.range(true) );
    StepInterval<double> rg1( pd.range(false) );
    if (useseldataranges_ && !xseldatarange_.isUdf() && !yseldatarange_.isUdf())
    {
	rg0.limitTo( xseldatarange_ );
	rg1.limitTo( yseldatarange_ );
    }
    rg0.sort( true );
    rg1.sort( true );

    rg0.widen( extfac_ * rg0.step, true );
    if ( mIsZero(rg1.width(),mDefEps) )
	rg1.widen( extfac_ * rg1.step, true );
    //<-- rg1 is not widened as wiggles are not drawn in the extended area.
    return uiWorldRect(rg0.start,rg1.stop,rg0.stop,rg1.start);
}


uiWorldRect uiFlatViewer::boundingBox() const
{
    const bool wvavisible = isVisible( true );
    uiWorldRect wr1 = getBoundingBox( wvavisible );
    if ( wvavisible && isVisible(false) )
    {
	uiWorldRect wr2 = getBoundingBox( false );
	if ( wr1.left() > wr2.left() )
	    wr1.setLeft( wr2.left() );
	if ( wr1.right() < wr2.right() )
	    wr1.setRight( wr2.right() );
	if ( wr1.top() < wr2.top() )
	    wr1.setTop( wr2.top() );
	if ( wr1.bottom() > wr2.bottom() )
	    wr1.setBottom( wr2.bottom() );
    }

    return wr1;
}


StepInterval<double> uiFlatViewer::posRange( bool forx1 ) const
{
    ConstDataPackRef<FlatDataPack> dp = obtainPack( false, true );
    return dp ? dp->posData().range(forx1) : StepInterval<double>();
}


void uiFlatViewer::setView( const uiWorldRect& wr )
{
    if ( wr.topLeft() == wr.bottomRight() )
	return;

    wr_ = wr;

    wr_.sortCorners( !appearance().annot_.x1_.reversed_,
		     appearance().annot_.x2_.reversed_ );

    axesdrawer_.setWorldCoords( wr_ );
    updateTransforms();

    w2ui_.set( getViewRect(), wr_ );

    viewChanged.trigger();
}


void uiFlatViewer::setViewToBoundingBox()
{
    setView( hasPack(true) ? getBoundingBox(true) : getBoundingBox(false) );
}


#define mAddToQueue( work ) \
    Threads::WorkManager::twm().addWork( work, 0, updatequeueid_, false, true )

void uiFlatViewer::handleChange( unsigned int dct )
{
    if ( Math::AreBitsSet( dct, Auxdata ) )
	mAddToQueue( auxdatawork_ );

    if ( Math::AreBitsSet( dct, Annot ) )
	mAddToQueue( annotwork_ );

    if ( Math::AreBitsSet( dct, BitmapData | DisplayPars, false ) )
	mAddToQueue( bitmapwork_ );

    view_->rePaint();
}


void uiFlatViewer::updateCB( CallBacker* )
{
    Threads::WorkManager::twm().executeQueue( updatequeueid_ );
}


void uiFlatViewer::updateBitmapCB( CallBacker* )
{
    MouseCursorChanger cursorchgr( MouseCursor::Wait );

    ConstDataPackRef<FlatDataPack> wvapack = obtainPack( true );
    ConstDataPackRef<FlatDataPack> vdpack = obtainPack( false );
    bitmapdisp_->setDataPack( wvapack.ptr(), true );
    bitmapdisp_->setDataPack( vdpack.ptr(), false );

    bitmapdisp_->setBoundingBox( boundingBox() );
    bitmapdisp_->update();
    dataChanged.trigger();
    dispParsChanged.trigger();
}


int uiFlatViewer::getAnnotChoices( BufferStringSet& bss ) const
{
    ConstDataPackRef<FlatDataPack> fdp = obtainPack( false, true );
    if ( fdp )
	fdp->getAltDim0Keys( bss );
    if ( !bss.isEmpty() )
	bss.addIfNew( fdp->dimName(true) );

    const int res = bss.indexOf( appearance().annot_.x1_.name_ );
    return res<0 ? mUdf(int) : res;
}


void uiFlatViewer::setAnnotChoice( int sel )
{
    ConstDataPackRef<FlatDataPack> fdp = obtainPack( false, true );
    if ( !fdp ) return;

    FlatView::Annotation::AxisData& x1axisdata = appearance().annot_.x1_;
    BufferStringSet altdim0keys; fdp->getAltDim0Keys( altdim0keys );
    x1axisdata.name_ = altdim0keys.validIdx(sel) ? altdim0keys.get(sel).buf()
						 : fdp->dimName(true);
    x1axisdata.annotinint_ = fdp->dimValuesInInt( x1axisdata.name_ );
    axesdrawer_.altdim0_ = sel;
}


void uiFlatViewer::reGenerate( FlatView::AuxData& ad )
{
    mDynamicCastGet( FlatView::uiAuxDataDisplay*, uiad, &ad );
    if ( !uiad )
    {
	pErrMsg("Invalid auxdata added");
	return;
    }

    uiad->updateCB( 0 );
}


FlatView::AuxData* uiFlatViewer::createAuxData(const char* nm) const
{ return new FlatView::uiAuxDataDisplay(nm); }


FlatView::AuxData* uiFlatViewer::getAuxData(int idx)
{ return auxdata_[idx]; }


const FlatView::AuxData* uiFlatViewer::getAuxData(int idx) const
{ return auxdata_[idx]; }


int uiFlatViewer::nrAuxData() const
{ return auxdata_.size(); }


void uiFlatViewer::addAuxData( FlatView::AuxData* a )
{
    mDynamicCastGet( FlatView::uiAuxDataDisplay*, uiad, a );
    if ( !uiad )
    {
	pErrMsg("Invalid auxdata added");
	return;
    }

    uiGraphicsItemGroup* graphicsitemgrp = uiad->getDisplay();
    if ( !graphicsitemgrp ) return;
    graphicsitemgrp->setZValue( auxDataZVal() );
    worldgroup_->add( graphicsitemgrp );
    uiad->setViewer( this );
    auxdata_ += uiad;
}


FlatView::AuxData* uiFlatViewer::removeAuxData( FlatView::AuxData* a )
{
    return removeAuxData( auxdata_.indexOf( (FlatView::uiAuxDataDisplay*) a ) );
}


FlatView::AuxData* uiFlatViewer::removeAuxData( int idx )
{
    if ( idx==-1 )
	return 0;

    worldgroup_->remove( auxdata_[idx]->getDisplay(), true );
    auxdata_[idx]->removeDisplay();
    return auxdata_.removeSingle(idx);
}


void uiFlatViewer::setSelDataRanges( Interval<double> xrg,Interval<double> yrg)
{
    useseldataranges_ = true;
    xseldatarange_ = xrg;
    yseldatarange_ = yrg;
    viewChanged.trigger();
}

