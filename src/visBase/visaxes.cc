/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Ranojay Sen
 Date:		14-01-2013
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";


#include "visaxes.h"

#include "color.h"
#include "viscamera.h"
#include "vistext.h"

#include <osg/Camera>
#include <osgGeo/AxesNode>

mCreateFactoryEntry( visBase::Axes );


namespace visBase
{

Axes::Axes()
    : axesnode_(new osgGeo::AxesNode)
    , mastercamera_(0)
    , pixeldensity_( getDefaultPixelDensity() )
    , annottextsize_(18)
{
    setOsgNode( axesnode_ );
}


Axes::~Axes()
{
   unRefPtr( mastercamera_ );
}


float Axes::getRadius() const
{
    return axesnode_->getRadius();
}


void Axes::setRadius( float rad )
{
    axesnode_->setRadius( rad );
}


float Axes::getLength() const
{
    return axesnode_->getLength();
}


void Axes::setLength( float len )
{
    axesnode_->setLength( len );
}


void Axes::setPosition( float x, float y )
{
    axesnode_->setPosition( osg::Vec2(x,y) );
}


void Axes::setSize( float rad, float len )
{
    axesnode_->setSize( osg::Vec2(rad,len) );
}


void Axes::setAnnotationColor( const Color& annotcolor )
{
#define mColTof(c) ((float) ( c / 255.0f )) \

    osg::Vec4 anncol( mColTof(annotcolor.r()),
		      mColTof(annotcolor.g()),
		      mColTof(annotcolor.b()), 1.0f );
    axesnode_->setAnnotationColor( anncol );
}


void Axes::setAnnotationTextSize( int size )
{
    const float sizefactor = pixeldensity_/getDefaultPixelDensity();
    axesnode_->setAnnotationTextSize(size*sizefactor);
    annottextsize_ = size;
}


void Axes::setAnnotationFont( const FontData& fd )
{
    osgText::Font* newfont = OsgFontCreator::create( fd );
    axesnode_->setAnnotationFont( newfont );
    setAnnotationTextSize( fd.pointSize() );
    requestSingleRedraw();
}


void Axes::setPixelDensity(float dpi)
{
    if ( dpi==pixeldensity_ )
	return;

    DataObject::setPixelDensity( dpi );

    pixeldensity_ = dpi;
    setAnnotationTextSize( annottextsize_ );
}


void Axes::setMasterCamera( visBase::Camera* camera )
{
    mastercamera_ = camera;
    mastercamera_->ref();
    axesnode_->setMasterCamera( mastercamera_->osgCamera() );
}

} //namespace visBase
