/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          Jan 2005
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "visarrowdisplay.h"

#include "pickset.h"
#include "survinfo.h"
#include "trigonometry.h"
#include "viscoord.h"
#include "visdrawstyle.h"
#include "visevent.h"
#include "vislines.h"

mCreateFactoryEntry( visSurvey::ArrowDisplay );

namespace visSurvey 
{
ArrowDisplay::ArrowDisplay()
    : arrowtype_(Double)
    , linestyle_(new visBase::DrawStyle)
    , group_(new visBase::DataObjectGroup)
{
    addChild( group_->osgNode() );
    linestyle_->ref();
    setLineWidth( 2 );
}


ArrowDisplay::~ArrowDisplay()
{
    linestyle_->unRef();
}


void ArrowDisplay::setDisplayTransformation( const mVisTrans* tf )
{
    displaytransform_ = tf;
}


const mVisTrans* ArrowDisplay::getDisplayTransformation() const
{
    return displaytransform_;
}


void ArrowDisplay::setType( Type typ )
{
    arrowtype_ = typ;

    for ( int idx=group_->size()-1; idx>=0; idx-- )
    {
	mDynamicCastGet( visBase::Lines*, lines, group_->getObject(idx));
	if ( !lines ) continue;
	updateLineShape( lines );
    }
}


void ArrowDisplay::setScene( visSurvey::Scene* ns )
{
    visSurvey::SurveyObject::setScene( ns );
}


ArrowDisplay::Type ArrowDisplay::getType() const
{ return arrowtype_; }


void ArrowDisplay::setLineWidth( int nw )
{
    linestyle_->setLineStyle( LineStyle(LineStyle::Solid,nw) );
}


int ArrowDisplay::getLineWidth() const
{
    return linestyle_->lineStyle().width_;
}


void ArrowDisplay::zScaleCB( CallBacker* )
{
    fullRedraw();
}


void ArrowDisplay::dispChg( CallBacker* cb )
{
    fullRedraw();
    LocationDisplay::dispChg( cb );
}


visBase::VisualObject* ArrowDisplay::createLocation() const
{
    visBase::Lines* lines = visBase::Lines::create();
    lines->setMaterial( 0 );
    lines->ref();
    lines->setSelectable( true );
    lines->setDisplayTransformation( displaytransform_ );
    lines->addNodeState( linestyle_ );
    Geometry::IndexedPrimitiveSet* indices =
				Geometry::IndexedPrimitiveSet::create( false );
    indices->ref();
    lines->addPrimitiveSet( indices );
    updateLineShape( lines );
    lines->unRefNoDelete();
    return lines;
}
	

void ArrowDisplay::setPosition( int idx, const Pick::Location& loc )
{
    mDynamicCastGet( visBase::Lines*, lines, group_->getObject(idx) );

    if ( lines )
	updateLineShape( lines );
    else
    {
	visBase::Lines* newline =
	    static_cast<visBase::Lines*>( createLocation() );
	group_->addObject( newline );
    
	newline->getCoordinates()->setPos( 0, loc.pos_ );
	if ( mIsUdf(loc.dir_.radius) || mIsUdf(loc.dir_.theta) ||
	     mIsUdf(loc.dir_.phi) )
	    return;

	const Coord3 d0 = world2Display( loc.pos_ );
	Coord3 vector = spherical2Cartesian( loc.dir_, true );

	if ( scene_ )
	    vector.z /= -scene_->getZScale();
	const Coord3 c1 = loc.pos_+vector;
	Coord3 d1 = world2Display( c1 );
	Coord3 displayvector = d1-d0;
	const double len = displayvector.abs();
	if ( mIsZero(len,1e-3) )
	    return;

	displayvector /= len;
	displayvector *= set_->disp_.pixsize_;
	//Note: pos.vec points in the direction of the tail, not the arrow.
	d1 = d0+displayvector;
	newline->getCoordinates()->setPos( 1, display2World(d1) );

	const Coord3 planenormal( sin(loc.dir_.phi), -cos(loc.dir_.phi), 0 );
	const Quaternion plus45rot(planenormal, M_PI/4);
	const Quaternion minus45rot(planenormal, -M_PI/4 );
	displayvector.z /= -scene_->getZScale(); 
	Coord3 arrowheadvec = minus45rot.rotate( displayvector*.3 );
	arrowheadvec.z /= scene_->getZScale(); 
	newline->getCoordinates()->setPos( 2, display2World(d0+arrowheadvec) );
    
	arrowheadvec = plus45rot.rotate( displayvector*.3 );
	arrowheadvec /= scene_->getZScale(); 
	newline->getCoordinates()->setPos( 3, display2World(d0+arrowheadvec) );
    }
}


void ArrowDisplay::updateLineShape( visBase::Lines* lines ) const
{
    if ( !lines || lines->nrPrimitiveSets()<1 )
	return;

    lines->ref();
    mDynamicCastGet(Geometry::IndexedPrimitiveSet*,
		    indices,lines->getPrimitiveSet(0));
    if ( !indices )
	return;

    TypeSet<int> indexarray;

    indexarray += 0; //0
    indexarray += 1; //1
   
    if ( arrowtype_==Bottom || arrowtype_==Double )
    {
	 indexarray += 0; //2
	 indexarray += 3; //3
    }
    else
    {
	indexarray += 0; //4
	indexarray += 2; //5
    }
    if ( arrowtype_ == Double )
    {
	indexarray += 0; //4
	indexarray += 2; //5
    }

    indices->setEmpty();
    indices->set( indexarray.arr(), indexarray.size() );
    lines->dirtyCoordinates();
    requestSingleRedraw();
    lines->unRef();
}


void ArrowDisplay::removePosition( int idx )
{
    if ( idx >= group_->size() )
	return;

    group_->removeObject( idx );
}


int ArrowDisplay::clickedMarkerIndex(const visBase::EventInfo& evi)const
{
    for ( int idx=0; idx<group_->size(); idx++ )
    {
	mDynamicCastGet(visBase::Lines*,lines,group_->getObject(idx));
	if ( lines && evi.pickedobjids.isPresent(lines->id()) )
	    return idx;
    }

    return -1;
}


}; // namespace visSurvey
