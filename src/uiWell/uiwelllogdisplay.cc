/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Mar 2008
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uiwelllogdisplay.cc,v 1.38 2010-04-13 12:55:16 cvsbruno Exp $";

#include "uiwelllogdisplay.h"
#include "uiwelldisplaycontrol.h"

#include "uicolor.h"
#include "uigraphicsscene.h"
#include "uigraphicsitemimpl.h"
#include "uiwellstratdisplay.h"
#include "uiwellinfopanels.h"

#include "coltabsequence.h"
#include "dataclipper.h"
#include "randcolor.h"
#include "survinfo.h"
#include "unitofmeasure.h"
#include "welldata.h"
#include "welld2tmodel.h"
#include "welldisp.h"
#include "welllog.h"
#include "welllogset.h"
#include "wellmarker.h"
#include "welltrack.h"

#include <iostream>


#define mDefZPosInLoop(val)\
    float zpos = val;\
    if ( data_.zistime_ && data_.d2tm_ )\
	zpos = data_.d2tm_->getTime( zpos )*1000;\
    if ( zpos < data_.zrg_.start )\
	continue;\
    else if ( zpos > data_.zrg_.stop )\
	break;\
    if ( data_.dispzinft_ && !data_.zistime_)\
	zpos *= mToFeetFactor;
uiWellLogDisplay::LineData::LineData( uiGraphicsScene& scn, Setup su )
    : zrg_(mUdf(float),0)
    , setup_(su)  
    , xax_(&scn,uiAxisHandler::Setup(uiRect::Top)
					    .nogridline(su.noxgridline_)
					    .border(su.border_)
					    .noborderspace(su.noborderspace_)
					    .noaxisline(su.noxaxisline_)
					    .ticsz(su.xaxisticsz_))
    , yax_(&scn,uiAxisHandler::Setup(uiRect::Left)
					    .nogridline(su.noygridline_)
					    .noaxisline(su.noyaxisline_)
					    .border(su.border_)
					    .noborderspace(su.noborderspace_))
    , valrg_(mUdf(float),0)
    , curvenmitm_(0)
{
}


uiWellLogDisplay::LogData::LogData( uiGraphicsScene& scn, bool first, Setup s )
    : LineData(scn,s)
    , wl_(0)
    , wld_(Well::DisplayProperties::Log())		   
    , unitmeas_(0)
    , xrev_(false)
    , isyaxisleft_(true)	     
{
    if ( !first )
    {
	xax_.setup().side_ = uiRect::Bottom;
	yax_.setup().nogridline(true);
    }
}


uiWellLogDisplay::MarkerItem::MarkerItem( Well::Marker& mrk )
    : mrk_(mrk) 
{
}


uiWellLogDisplay::MarkerItem::~MarkerItem()
{
    delete itm_; delete txtitm_;
}



uiWellLogDisplay::uiWellLogDisplay( uiParent* p, const Setup& su )
    : uiGraphicsView(p,"Well Log display viewer")
    , setup_(su)
    , highlightedmrk_(0)	     
    , highlightedMarkerItemChged(this)			     
    , ld1_(scene(),true,LineData::Setup()
	    				.noxaxisline(su.noxaxisline_)
	    				.noyaxisline(su.noyaxisline_)
					.xaxisticsz(su.axisticsz_)
	    				.noygridline(su.noygridline_)
	    				.noxgridline(su.noxgridline_)
					.noborderspace(su.noborderspace_))
    , ld2_(scene(),false,LineData::Setup()
	    				.noxaxisline(su.noxaxisline_)
					.noyaxisline(su.noyaxisline_)
					.xaxisticsz(su.axisticsz_)
					.noborderspace(su.noborderspace_)
	    				.noygridline(su.noygridline_)
	    				.noxgridline(su.noxgridline_))
{
    if ( su.nobackground_ )
    {
	setNoSytemBackGroundAttribute();
	uisetBackgroundColor( Color( 255, 255, 255, 0 )  );
	scene().setBackGroundColor( Color( 255, 255, 255, 0 )  );
    }

    data_.dispzinft_ = SI().depthsInFeetByDefault();
    reSize.notify( mCB(this,uiWellLogDisplay,reSized) );
    setScrollBarPolicy( true, uiGraphicsView::ScrollBarAlwaysOff );
    setScrollBarPolicy( false, uiGraphicsView::ScrollBarAlwaysOff );

    finaliseDone.notify( mCB(this,uiWellLogDisplay,init) );
}


void uiWellLogDisplay::reSized( CallBacker* )
{
    draw();
}


void uiWellLogDisplay::init( CallBacker* )
{
    dataChanged();
    show();
}


void uiWellLogDisplay::dataChanged()
{
    gatherInfo(); draw();
}


void uiWellLogDisplay::gatherInfo()
{
    setAxisRelations();
    gatherInfo( true );
    gatherInfo( false );

    if ( mIsUdf(data_.zrg_.start)  && ld1_.wl_ )
    {
	data_.zrg_ = ld1_.zrg_;
	if ( ld2_.wl_ )
	    data_.zrg_.include( ld2_.zrg_ );
    }
    setAxisRanges( true );
    setAxisRanges( false );
	
    BufferString znm;
    if ( data_.zistime_ )
	znm += "TWT", "(ms)";
    else
	znm += "MD ", data_.dispzinft_ ? "(ft)" : "(m)";

    ld1_.yax_.setup().side_ = ld1_.isyaxisleft_ ? uiRect::Left : uiRect::Right;
    ld2_.yax_.setup().side_ = ld2_.isyaxisleft_ ? uiRect::Left : uiRect::Right;
    ld1_.xax_.setup().maxnumberdigitsprecision_ = 3;
    ld1_.xax_.setup().epsaroundzero_ = 1e-5;
    ld1_.yax_.setup().islog( ld1_.wld_.islogarithmic_ );
    ld1_.yax_.setup().name( znm );
    ld2_.xax_.setup().maxnumberdigitsprecision_ = 3;
    ld2_.xax_.setup().epsaroundzero_ = 1e-5;
    ld2_.yax_.setup().islog( ld2_.wld_.islogarithmic_ );
    ld2_.yax_.setup().name( znm );
    if ( ld1_.wl_ ) ld1_.xax_.setup().name( ld1_.wl_->name() );
    if ( ld2_.wl_ ) ld2_.xax_.setup().name( ld2_.wl_->name() );
}


void uiWellLogDisplay::setAxisRelations()
{
    ld1_.xax_.setBegin( setup_.noxpixbefore_ ? 0 : &ld1_.yax_ );
    ld1_.yax_.setBegin( setup_.noypixbefore_ ? 0 : &ld1_.xax_ );
    ld1_.xax_.setEnd(  setup_.noxpixafter_ ? 0 : &ld1_.yax_ );
    ld1_.yax_.setEnd( setup_.noypixafter_ ? 0 : &ld1_.xax_ );
    ld2_.xax_.setBegin( setup_.noxpixbefore_ ? 0 : &ld2_.yax_ );
    ld2_.yax_.setBegin( setup_.noypixbefore_ ? 0 : &ld2_.xax_ );
    ld2_.xax_.setEnd(  setup_.noxpixafter_ ? 0 : &ld2_.yax_ );
    ld2_.yax_.setEnd( setup_.noypixafter_ ? 0 : &ld2_.xax_ );
    
    ld1_.xax_.setNewDevSize( width(), height() );
    ld1_.yax_.setNewDevSize( height(), width() );
    ld2_.xax_.setNewDevSize( width(), height() );
    ld2_.yax_.setNewDevSize( height(), width() );
}


void uiWellLogDisplay::gatherInfo( bool first )
{
    uiWellLogDisplay::LogData& ld = first ? ld1_ : ld2_;
    const int sz = ld.wl_ ? ld.wl_->size() : 0;
    if ( sz < 2 )
    {
	if ( !first )
	{
	    ld2_.copySetupFrom( ld1_ );
	    ld2_.zrg_ = ld1_.zrg_;
	    ld2_.valrg_ = ld1_.valrg_;
	}
	return;
    }

    if ( ld.wld_.cliprate_ || mIsUdf( ld.wld_.range_.start ) )
    {
	DataClipSampler dcs( sz );
	dcs.add( ld.wl_->valArr(), sz );
	ld.valrg_ = dcs.getRange( ld.wld_.cliprate_ );
    }
    else
	ld.valrg_ = ld.wld_.range_;

    float startpos = ld.wl_->dah( 0 );
    float stoppos = ld.wl_->dah( sz-1 );
    if ( data_.zistime_ && data_.d2tm_ )
    {
	startpos = data_.d2tm_->getTime( startpos )*1000;
	stoppos = data_.d2tm_->getTime( stoppos )*1000;
    }

    ld.zrg_.start = startpos;
    ld.zrg_.stop = stoppos;
}


void uiWellLogDisplay::setAxisRanges( bool first )
{
    uiWellLogDisplay::LogData& ld = first ? ld1_ : ld2_;
    const int sz = ld.wl_ ? ld.wl_->size() : 0;
    if ( sz < 2 ) return;

    Interval<float> dispvalrg( ld.valrg_ );
    if ( ld.xrev_ ) Swap( dispvalrg.start, dispvalrg.stop );
    ld.xax_.setBounds( dispvalrg );

    Interval<float> dispzrg( data_.zrg_.stop, data_.zrg_.start );
    if ( data_.dispzinft_ && !data_.zistime_ )
	dispzrg.scale( mToFeetFactor );
    ld.yax_.setBounds( dispzrg );

    if ( first )
    {
	// Set default for 2nd
	ld2_.xax_.setBounds( dispvalrg );
	ld2_.yax_.setBounds( dispzrg );
    }
}


#define mRemoveSet( itms ) \
    for ( int idx=0; idx<itms.size(); idx++ ) \
    scene().removeItem( itms[idx] ); \
    deepErase( itms );
void uiWellLogDisplay::draw()
{
    setAxisRelations();
    if ( mIsUdf(data_.zrg_.start) ) return;

    drawLog( true );
    drawLog( false );
    
    mRemoveSet( ld1_.curvepolyitms_ );
    if ( ld1_.wld_.islogfill_ )
	drawFilling( true );
    mRemoveSet( ld2_.curvepolyitms_ );
    if ( ld2_.wld_.islogfill_ )
	drawFilling( false );

    ld1_.xax_.plotAxis(); ld1_.yax_.plotAxis();
    ld2_.xax_.plotAxis(); ld2_.yax_.plotAxis();

    drawMarkers();
    drawZPicks();
}


void uiWellLogDisplay::drawLog( bool first )
{
    uiWellLogDisplay::LogData& ld = first ? ld1_ : ld2_;
    ld.al_ = Alignment(Alignment::HCenter, first ? 
	    			Alignment::Top : Alignment::Bottom );
    drawLine( ld, ld.wl_ );
    
    if ( ld.unitmeas_ )
	ld.xax_.annotAtEnd( BufferString("(",ld.unitmeas_->symbol(),")") );
}


void uiWellLogDisplay::drawLine( LogData& ld, const Well::DahObj* wd )
{
    mRemoveSet( ld.curveitms_ );
    scene().removeItem( ld.curvenmitm_ );
    delete ld.curvenmitm_; ld.curvenmitm_ = 0;

    if ( !wd ) return;
    const int sz = wd ? wd->size() : 0;
    if ( sz < 2 ) return;

    ObjectSet< TypeSet<uiPoint> > pts;
    TypeSet<uiPoint>* curpts = new TypeSet<uiPoint>;

    for ( int idx=0; idx<sz; idx++ )
    {
	mDefZPosInLoop( wd->dah( idx ) )
	float val = wd->value( idx );

	if ( mIsUdf(val) )
	{
	    if ( !curpts->isEmpty() )
	    {
		pts += curpts;
		curpts = new TypeSet<uiPoint>;
	    }
	    continue;
	}
	*curpts += uiPoint( ld.xax_.getPix(val), ld.yax_.getPix(zpos) );
    }
    if ( curpts->isEmpty() )
	delete curpts;
    else
	pts += curpts;
    if ( pts.isEmpty() ) return;

    LineStyle ls(LineStyle::Solid);
    ls.width_ = ld.wld_.size_;
    ls.color_ = ld.wld_.color_;
    for ( int idx=0; idx<pts.size(); idx++ )
    {
	uiPolyLineItem* pli = scene().addItem( new uiPolyLineItem(*pts[idx]) );
	pli->setPenStyle( ls );
	ld.curveitms_ += pli;
    }
    ld.curvenmitm_ = scene().addItem( new uiTextItem(wd->name(),ld.al_) );
    ld.curvenmitm_->setTextColor( ls.color_ );
    uiPoint txtpt;
    if ( ld.al_.vPos() == Alignment::Top )
	txtpt = uiPoint( (*pts[0])[0] );
    else
    {
	TypeSet<uiPoint>& lastpts( *pts[pts.size()-1] );
	txtpt = lastpts[lastpts.size()-1];
    }
    ld.curvenmitm_->setPos( txtpt );
    
    deepErase( pts );
}


void uiWellLogDisplay::drawFilling( bool first )
{
    uiWellLogDisplay::LogData& ld = first ? ld1_ : ld2_;

    mRemoveSet( ld.curvepolyitms_ );
    float colstep = ( ld.xax_.range().stop - ld.xax_.range().start ) / 255;

    const int sz = ld.wl_ ? ld.wl_->size() : 0;
    if ( sz < 2 ) return;

    ObjectSet< TypeSet<uiPoint> > pts;
    TypeSet<int> colorintset;
    uiPoint closept;
    closept.x = ( first || ld.xrev_ ) ? ld.xax_.getPix(ld.xax_.range().stop) 
				      : ld.xax_.getPix(ld.xax_.range().start);
    closept.y = ( first || ld.xrev_ ) ? ld.yax_.getPix(ld.yax_.range().stop) 
				      : ld.yax_.getPix(ld.yax_.range().stop);
    int prevcolidx = 0;
    TypeSet<uiPoint>* curpts = new TypeSet<uiPoint>;
    *curpts += closept; 

    uiPoint pt;
    for ( int idx=0; idx<sz; idx++ )
    {
	mDefZPosInLoop( ld.wl_->dah( idx ) )
	float val = ld.wl_->value( idx );

	if ( mIsUdf(val) )
	{
	    if ( !curpts->isEmpty() )
	    {
		pts += curpts;
		curpts = new TypeSet<uiPoint>;
		colorintset += 0;
	    }
	    continue;
	}
	
	pt.x = ld.xax_.getPix(val); 
	pt.y = ld.yax_.getPix(zpos);
	*curpts += pt;

	int colindex = ld.xrev_ ? (int)(ld.xax_.range().stop-val)
				: (int)(val-ld.xax_.range().start);
	colindex = (int)( colindex/colstep );
	if ( colindex != prevcolidx )
	{
	    *curpts += uiPoint( closept.x, pt.y ); 
	    if ( !curpts->isEmpty() )
	    {
		colorintset += colindex;
		prevcolidx = colindex;
		pts += curpts;
	    }
	    curpts = new TypeSet<uiPoint>;
	    *curpts += uiPoint( closept.x, pt.y ); 
	} 
    }
    if ( pts.isEmpty() ) return;
    *pts[pts.size()-1] += uiPoint( closept.x, pt.y ); 

    const int tabidx = ColTab::SM().indexOf( ld.wld_.seqname_ );
    const ColTab::Sequence* seq = ColTab::SM().get( tabidx<0 ? 0 : tabidx );
    for ( int idx=0; idx<pts.size(); idx++ )
    {
	uiPolygonItem* pli = scene().addPolygon( *pts[idx], true );
	ld.curvepolyitms_ += pli;
	Color color = 
	    ld.wld_.issinglecol_ ? ld.wld_.seiscolor_
				 : seq->color( (float)colorintset[idx]/255 );
	pli->setFillColor( color );
	LineStyle ls;
	ls.width_ = 1;
	ls.color_ = color;
	pli->setPenStyle( ls );
    }

    deepErase( pts );
}


#define mDefHorLineX1X2Y() \
    const int x1 = ld1_.xax_.getRelPosPix( 0 ); \
    const int x2 = ld1_.xax_.getRelPosPix( 1 ); \
    const int y = ld1_.yax_.getPix( zpos )
void uiWellLogDisplay::drawMarkers()
{
    for ( int idx=0; idx<markeritms_.size(); idx++ ) \
    {
	scene().removeItem( markeritms_[idx]->itm_ ); 
	scene().removeItem( markeritms_[idx]->txtitm_ ); 
    }
    deepErase( markeritms_ );
    if ( !data_.markers_ ) return;

    for ( int idx=0; idx<data_.markers_->size(); idx++ )
    {
	Well::Marker& mrkr = *((*data_.markers_)[idx]);
	if ( mrkr.color() == Color::NoColor() ) continue;

	mDefZPosInLoop( mrkr.dah() )
	mDefHorLineX1X2Y();

	MarkerItem* markeritm = new MarkerItem( mrkr );
	uiLineItem* li = scene().addItem( new uiLineItem(x1,y,x2,y,true) );
	li->setPenStyle( LineStyle(setup_.markerls_.type_,
		    		   setup_.markerls_.width_,mrkr.color()) );
	markeritm->itm_ = li;
	BufferString mtxt( mrkr.name() );
	if ( setup_.nrmarkerchars_ < mtxt.size() )
	    mtxt[setup_.nrmarkerchars_] = '\0';
	uiTextItem* ti = scene().addItem(
			 new uiTextItem(mtxt,mAlignment(Right,VCenter)) );
	ti->setPos( uiPoint(x1-1,y) );
	ti->setTextColor( mrkr.color() );
	markeritm->color_ = mrkr.color();
	markeritm->txtitm_ = ti;
	markeritms_ += markeritm;
    }
}


void uiWellLogDisplay::reDrawMarkers( CallBacker* )
{
    drawMarkers();
}



uiWellLogDisplay::MarkerItem* uiWellLogDisplay::getMarkerItem( 
						const Well::Marker* mrk )
{
    for ( int idx=0; idx<markeritms_.size(); idx++ )
    {
	if ( markeritms_[idx]->mrk_ == mrk )
	    return markeritms_[idx];
    }
    return 0;
}


void uiWellLogDisplay::highlightMarkerItem( const Well::Marker* mrk  )
{
    if ( highlightedmrk_ )
    {
	uiWellLogDisplay::MarkerItem* mitm = getMarkerItem( highlightedmrk_ );
	if ( mitm ) 
	    mitm->itm_->setPenStyle( LineStyle(setup_.markerls_.type_,
				     setup_.markerls_.width_, 
				     mitm->color_) );
    }
    MarkerItem* mrkitm = mrk ? getMarkerItem( mrk ) : 0;
    if ( mrkitm )
    {
	mrkitm->itm_->setPenStyle( LineStyle(setup_.markerls_.type_,
				    setup_.markerls_.width_+1, 
				    mrkitm->color_) );
	mrkitm->itm_->setCursor( MouseCursor::SizeVer );
    }
    highlightedmrk_ = mrk;
    highlightedMarkerItemChged.trigger();
}


void uiWellLogDisplay::drawZPicks()
{
    mRemoveSet( zpickitms_ );

    for ( int idx=0; idx<zpicks_.size(); idx++ )
    {
	const PickData& pd = zpicks_[idx];
	mDefZPosInLoop( pd.dah_ )
	mDefHorLineX1X2Y();

	uiLineItem* li = scene().addItem( new uiLineItem(x1,y,x2,y,true) );
	Color lcol( setup_.pickls_.color_ );
	if ( pd.color_ != Color::NoColor() )
	    lcol = pd.color_;
	li->setPenStyle( LineStyle(setup_.pickls_.type_,setup_.pickls_.width_,
		    		   lcol) );
	zpickitms_ += li;
    }
}



uiWellDisplay::Params::Params( Well::Data& wd, int lw, int lh )
    : logwidth_(lw)
    , logheight_(lh)  
    , wd_(wd)
    , zrg_(mUdf(float),0)
    , d2tm_(wd.d2TModel())
    , zistime_(wd.haveD2TModel())		    
{
}


uiWellDisplay::uiWellDisplay( uiParent* p, const Setup& s, Well::Data& wd)
    	: uiGroup(p,wd.name())
	, stratdisp_(0)	
	, pms_(Params(wd,s.logwidth_,s.logheight_))		
{
    if ( s.nobackground_ )
	setNoBackGround();
    
    for ( int idx=0; idx<s.nrpanels_; idx++ )
	addLogPanel( s.noborderspace_, idx==0 );

    pms_.wd_.dispparschanged.notify(mCB(this,uiWellDisplay,updateProperties));
    mrkedit_ = new uiWellDisplayMarkerEdit( *logDisplay(0), wd );

    setHSpacing( 0 );
    setStretch( 2, 2 );
    setPrefWidth( getDispWidth() );
    setPrefHeight( mLogHeight );
    updateProperties(0);
    
    //setInitialZRange();
}


uiWellDisplay::uiWellDisplay( uiWellDisplay& orgdisp, const ShapeSetup& su )
    : uiGroup(su.parent_,"")
    , stratdisp_(orgdisp.stratDisp())
    , pms_(orgdisp.params())				     
    , mrkedit_(orgdisp.markerEdit())
{
    setNoBackGround();

    for ( int idx=0; idx<orgdisp.nrLogDisp(); idx++ )
    {
	if ( su.nrlogpanels_ == idx ) break; //TODO delete non used ones
	uiWellLogDisplay* log = orgdisp.logDisplay(idx);
	log->reParent( this );
	if ( idx ) log->attach( rightOf, logdisps_[idx-1] );
	log->setPrefWidth( mLogWidth );
	log->setPrefHeight( mLogHeight );
	log->setStretch( 2, 2 );
	logdisps_ += log;
    }
    
    for ( int idx=logdisps_.size(); idx<su.nrlogpanels_; idx++ )
    {
	addLogPanel( true, false );
	logdisps_[idx]->setPrefWidth( mLogWidth );
	if ( mrkedit_ ) mrkedit_->addLogDisplay( *logdisps_[idx] );
    }
   
    if ( su.withstrat_ ) 
	setStratDisp();
    else 
    { delete stratdisp_; stratdisp_ = 0; }

    setPrefWidth( getDispWidth() );
    setPrefHeight( mLogHeight );
    setHSpacing( 0 );
    setStretch( 2, 2 );
    updateProperties(0);
}


uiWellDisplay::~uiWellDisplay()
{
}


int uiWellDisplay::getDispWidth()
{
    return nrLogDisp()*mLogWidth + (hasStratDisp() ? mStratWidth : 0);
}


void uiWellDisplay::setInitialZRange()
{
    Interval<float> zrg(0,0);
    const Well::Log* wl = 0;
    for ( int dispidx=0; dispidx<logdisps_.size();  dispidx++ )
    {
	if ( !logdisps_[dispidx] ) continue;
	Interval<float> dahrg;
	uiWellLogDisplay::LogData ld = logdisps_[dispidx]->logData( true );
	wl = ld.wl_;
	if ( wl ) dahrg = wl->dahRange();
	if ( !dahrg.overlaps( zrg, false ) )
	    zrg = dahrg;

	ld = logdisps_[dispidx]->logData( false );
	wl = ld.wl_;
	if ( wl ) dahrg = wl->dahRange();
	if ( !dahrg.overlaps( zrg, false ) )
	    zrg = dahrg;
    }
    //Swap( zrg.start, zrg.stop );
    if ( pms_.zistime_ && pms_.d2tm_ ) 
	zrg.set( pms_.d2tm_->getTime(zrg.start)*1000, 
		 pms_.d2tm_->getTime(zrg.stop)*1000 );
  
    pms_.zrg_ = zrg; 
}


void uiWellDisplay::setStratDisp()
{
    if ( stratdisp_ )
	stratdisp_->reParent(this);
    else
    {
	stratdisp_ = new uiWellStratDisplay(this,true,pms_.wd_.markers());
	pms_.wd_.markerschanged.notify( 
		mCB(stratdisp_,uiWellStratDisplay,doDataChange) );
    }

    stratdisp_->setPrefWidth( mLogWidth );
    stratdisp_->setPrefHeight( mLogHeight );
    stratdisp_->setD2TModel( pms_.d2tm_ );
    stratdisp_->setZIsTime( pms_.zistime_ );
    stratdisp_->doDataChange(0);
    if ( nrLogDisp() )
	stratdisp_->attach( ensureRightOf, logdisps_[nrLogDisp()-1] );
}


void uiWellDisplay::addLogPanel( bool noborderspace, bool isleft )
{
    uiWellLogDisplay::Setup wldsu; wldsu.nrmarkerchars(3);
    wldsu.noxpixafter_ = isleft; wldsu.noxpixbefore_ = !isleft;
    if ( noborderspace ) 
    { 
	wldsu.nobackground_ = true;
	wldsu.axisticsz_ = -15; 
	wldsu.noborderspace_ = true; 
	wldsu.noygridline_ = true;
	wldsu.noxgridline_ = false;
	wldsu.noyaxisline_ = true; 
    }
    wldsu.border_ = uiBorder(0);
    wldsu.border_.setLeft(0); wldsu.border_.setRight(0);
    uiWellLogDisplay* logdisp = new uiWellLogDisplay( this, wldsu );
    if ( logdisps_.size() )
	logdisp->attach( rightOf, logdisps_[logdisps_.size()-1] );
    logdisp->setPrefWidth( pms_.logwidth_ );
    logdisp->setPrefHeight( mLogHeight );
    logdisps_ += logdisp;
    pms_.wd_.markerschanged.notify(mCB(logdisp,uiWellLogDisplay,reDrawMarkers));
}


void uiWellDisplay::setLog( const char* logname, int logidx, bool left )
{
    const Well::Log* l = pms_.wd_.logs().getLog( logname );
    if ( !logdisps_[logidx] ) return;

    uiWellLogDisplay::LogData& ld = logdisps_[logidx]->logData(left);
    ld.wl_ 	    = l;
    ld.xrev_        = false;
    ld.isyaxisleft_ = left;
}


void uiWellDisplay::dataChanged( CallBacker* cb )
{
    for ( int idx=0; idx<logdisps_.size(); idx++ )
    {
	uiWellLogDisplay* logdisp = logdisps_[idx];
	if ( !logdisp ) continue;
	logdisps_[idx]->data().zrg_ = pms_.zrg_;
	logdisp->data().zistime_ = pms_.zistime_ && pms_.d2tm_;
	logdisp->data().d2tm_ = pms_.d2tm_;
	logdisp->data().markers_ = &pms_.wd_.markers();
	logdisp->dataChanged();
    }
}


void uiWellDisplay::updateProperties( CallBacker* cb )
{
    for ( int idx=0; idx<logdisps_.size(); idx++ )
    {
	uiWellLogDisplay* ld = logdisps_[idx];
	if ( !ld ) continue;
	setLog( pms_.wd_.displayProperties().left_.name_, idx, true );
	setLog( pms_.wd_.displayProperties().right_.name_, idx, false );
	logdisps_[idx]->logData(true).wld_= pms_.wd_.displayProperties().left_;
	logdisps_[idx]->logData(false).wld_ = pms_.wd_.displayProperties().right_;
    }
    dataChanged( 0 );
}



uiWellDisplayWin::uiWellDisplayWin( uiParent* p, Well::Data& wd )
    : uiMainWin(p,"")
    , welldisp_(*new uiWellDisplay(this,uiWellDisplay::Setup()
							.withstratdisp(true)
							,wd))
    , wd_(wd)						    
{
    BufferString msg( "2D Viewer ");
    msg += wd.name();
    setCaption( msg );
    wd_.tobedeleted.notify( mCB(this,uiWellDisplayWin,closeWin) );
}


void uiWellDisplayWin::closeWin( CallBacker* )
{ close(); }

