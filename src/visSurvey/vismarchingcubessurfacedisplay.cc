/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : May 2002
-*/

static const char* rcsID = "$Id: vismarchingcubessurfacedisplay.cc,v 1.14 2007-10-30 20:19:40 cvsyuancheng Exp $";

#include "vismarchingcubessurfacedisplay.h"

#include "arrayndimpl.h"
#include "emmanager.h"
#include "emmarchingcubessurface.h"
#include "executor.h"
#include "iopar.h"
#include "marchingcubes.h"
#include "marchingcubeseditor.h"
#include "randcolor.h"
#include "position.h"
#include "survinfo.h"
#include "visboxdragger.h"
#include "viscoord.h"
#include "visdragger.h"
#include "visellipsoid.h"
#include "visevent.h"
#include "vismarchingcubessurface.h"
#include "visinvisiblelinedragger.h"
#include "vismaterial.h"
#include "vispickstyle.h"
#include "vispolyline.h"
#include "visshape.h"

#define mKernelSize 11

mCreateFactoryEntry( visSurvey::MarchingCubesDisplay );

namespace visSurvey
{

MarchingCubesDisplay::MarchingCubesDisplay()
    : VisualObjectImpl(true)
    , emsurface_( 0 )
    , initialdragger_( visBase::BoxDragger::create() )
    , displaysurface_( 0 )
    , surfaceeditor_( 0 )
    , factordragger_( 0 )
    , eventcatcher_( 0 )
    , allowdrag_( false )
    , initialellipsoid_( visBase::Ellipsoid::create() )
    , minsampleinlsz_( 0 )						
    , minsamplecrlsz_( 0 )						
    , minsamplezsz_( 0 )
    , previoussample_( false )
    , kernelpickstyle_( 0 )			     
    , kernelellipsoid_( 0 )
    , kernelsize_( Coord3(0, 0, 0) )
    , normalline_( visBase::IndexedPolyLine::create() )			   
{
    initialdragger_->ref();
    initialdragger_->turnOn( true );
    addChild( initialdragger_->getInventorNode() );

    initialellipsoid_->ref();
    initialellipsoid_->removeSwitch();
    initialellipsoid_->setMaterial( 0 );
    addChild( initialellipsoid_->getInventorNode() );

    normalline_->ref();
    addChild( normalline_->getInventorNode() );

    getMaterial()->setAmbience( 0.3 );

    CubeSampling cs(false);
    CubeSampling sics = SI().sampling(true);
    cs.hrg.start.inl = (5*sics.hrg.start.inl+3*sics.hrg.stop.inl)/8;
    cs.hrg.start.crl = (5*sics.hrg.start.crl+3*sics.hrg.stop.crl)/8;
    cs.hrg.stop.inl = (3*sics.hrg.start.inl+5*sics.hrg.stop.inl)/8;
    cs.hrg.stop.crl = (3*sics.hrg.start.crl+5*sics.hrg.stop.crl)/8;
    cs.zrg.start = ( 5*sics.zrg.start + 3*sics.zrg.stop ) / 8;
    cs.zrg.stop = ( 3*sics.zrg.start + 5*sics.zrg.stop ) / 8;
    SI().snap( cs.hrg.start, BinID(0,0) );
    SI().snap( cs.hrg.stop, BinID(0,0) );
    float z0 = SI().zRange(true).snap( cs.zrg.start ); cs.zrg.start = z0;
    float z1 = SI().zRange(true).snap( cs.zrg.stop ); cs.zrg.stop = z1;

    Coord3 center( ((float) cs.hrg.start.inl+cs.hrg.stop.inl)/2,
	    	   ((float) cs.hrg.start.crl+cs.hrg.stop.crl)/2,
		   cs.zrg.center() );

    Coord3 width( cs.hrg.stop.inl-cs.hrg.start.inl,
	    	  cs.hrg.stop.crl-cs.hrg.start.crl,	                                          cs.zrg.width() );

    initialdragger_->setCenter( center );
    initialdragger_->setWidth( width );
    initialellipsoid_->setCenterPos( center );
    initialellipsoid_->setWidth( width );

    initialdragger_->setSpaceLimits(
	    Interval<float>( sics.hrg.start.inl, sics.hrg.stop.inl ),
	    Interval<float>( sics.hrg.start.crl, sics.hrg.stop.crl ),
	    Interval<float>( sics.zrg.start, sics.zrg.stop ) );

    minsampleinlsz_ = width.x/4;
    minsamplecrlsz_ = width.y/4;
    minsamplezsz_ = width.z/4;
    previoussample_ = cs;

    initialdragger_->finished.notify( 
	    mCB(this, MarchingCubesDisplay, initialDraggerMovedCB) );

    initialdragger_->motion.notify(
	    mCB(this, MarchingCubesDisplay, initialDraggerMovingCB) );

    setColor( getRandomColor( false ) );
}


MarchingCubesDisplay::~MarchingCubesDisplay()
{
    if ( emsurface_ ) emsurface_->unRef();
    
    if ( displaysurface_ )
    {
	removeChild( displaysurface_->getInventorNode() );
	displaysurface_->unRef();
    }

    if ( factordragger_ )
    {
	factordragger_->motion.remove(mCB(this,MarchingCubesDisplay,
		    		      factorDrag));
	factordragger_->needsDirection.remove(mCB(this,MarchingCubesDisplay,
		    		      setDragDirection));
	factordragger_->unRef();
    }

    if ( kerneldragger_ )
    {
	kerneldragger_->finished.remove( 
		mCB(this, MarchingCubesDisplay, kernelDraggerMovedCB) );
	kerneldragger_->motion.remove( 
		mCB(this, MarchingCubesDisplay, kernelDraggerMovingCB) );

	removeChild( kerneldragger_->getInventorNode() );
	kerneldragger_->unRef();
    }

    if ( kernelpickstyle_ )
	kernelpickstyle_->unRef();

    if ( kernelellipsoid_ )
	kernelellipsoid_->unRef();

    if ( normalline_ )
	normalline_->unRef();

    delete surfaceeditor_;
    setSceneEventCatcher(0);
    removeInitialDragger();
}


bool MarchingCubesDisplay::setVisSurface(
	visBase::MarchingCubesSurface* surface )
{
    if ( displaysurface_ )
    {
	removeChild( displaysurface_->getInventorNode() );
	displaysurface_->unRef();
	displaysurface_ = 0;
    }
	
    if ( emsurface_ ) emsurface_->unRef();
    emsurface_ = 0;

    if ( !surface || !surface->getSurface() )
	return false;

    emsurface_ = new EM::MarchingCubesSurface( EM::EMM() );

    if ( !emsurface_ )
	return false;

    emsurface_->ref();

    if ( !emsurface_->isOK() ||
	 !emsurface_->setSurface( surface->getSurface() ) )
    {
	emsurface_->unRef();
	emsurface_ = 0;
	return false;
    }

    SamplingData<float> sd = surface->getScale( 0 );
    emsurface_->setInlSampling(
	    SamplingData<int>( mNINT(sd.start), mNINT(sd.step) ) );

    sd = surface->getScale( 1 );
    emsurface_->setCrlSampling(
	    SamplingData<int>( mNINT(sd.start), mNINT(sd.step) ) );

    emsurface_->setZSampling( surface->getScale( 2 ) );

    EM::EMM().addObject( emsurface_ );

    displaysurface_ = surface;
    displaysurface_->ref();
    displaysurface_->setSelectable( false );
    displaysurface_->setRightHandSystem( righthandsystem_ );
    addChild( displaysurface_->getInventorNode() );

    if ( displaysurface_->getMaterial() )
	getMaterial()->setFrom( *displaysurface_->getMaterial() );

    displaysurface_->setMaterial( 0 );
    emsurface_->setPreferredColor( getMaterial()->getColor() );
    
    removeInitialDragger();
    return true;
}


EM::ObjectID MarchingCubesDisplay::getEMID() const
{ return emsurface_ ? emsurface_->id() : -1; }


#define mErrRet(s) { errmsg = s; return false; }

bool MarchingCubesDisplay::setEMID( const EM::ObjectID& emid )
{
    if ( emsurface_ )
	emsurface_->unRef();

    emsurface_ = 0;

    RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );
    mDynamicCastGet( EM::MarchingCubesSurface*, emmcsurf, emobject.ptr() );
    if ( !emmcsurf )
    {
	if ( displaysurface_ ) displaysurface_->turnOn( false );
	return false;
    }

    emsurface_ = emmcsurf;
    emsurface_->ref();

    updateVisFromEM( false );
    return true;
}


void MarchingCubesDisplay::updateVisFromEM( bool onlyshape )
{
    if ( !onlyshape || !displaysurface_ )
    {
	getMaterial()->setColor( emsurface_->preferredColor() );
	if ( !emsurface_->name().isEmpty() )
	    setName( emsurface_->name() );
	else setName( "<New body>" );

	if ( !displaysurface_ )
	{
	    displaysurface_ = visBase::MarchingCubesSurface::create();
	    displaysurface_->ref();
	    displaysurface_->removeSwitch();
	    displaysurface_->setMaterial( 0 );
	    displaysurface_->setSelectable( false );
	    displaysurface_->setRightHandSystem( righthandsystem_ );
	    addChild( displaysurface_->getInventorNode() );
	}

	displaysurface_->setScales(
		SamplingData<float>(emsurface_->inlSampling()),
		SamplingData<float>(emsurface_->crlSampling()),
				    emsurface_->zSampling() );

	displaysurface_->setSurface( emsurface_->surface() );
	displaysurface_->turnOn( true );
    }

    if ( emsurface_ && !emsurface_->surface().isEmpty() )
	removeInitialDragger();
    
    displaysurface_->touch( !onlyshape );
}


void MarchingCubesDisplay::showManipulator( bool yn )
{
    if ( initialdragger_ )
    	initialdragger_->turnOn( yn );
    else if ( displaysurface_ && yn && !factordragger_ )
    {
	factordragger_ = visBase::InvisibleLineDragger::create();
	factordragger_->ref();
	addChild( factordragger_->getInventorNode() );
	factordragger_->setShape( displaysurface_ );
	removeChild( displaysurface_->getInventorNode() );
	factordragger_->motion.notify(mCB(this,MarchingCubesDisplay,
		    		      factorDrag ));
	factordragger_->needsDirection.notify(mCB(this,MarchingCubesDisplay,
		    		      setDragDirection));
    }

    if ( kernelellipsoid_ )
    {
	kernelellipsoid_->turnOn( yn );

	if ( kerneldragger_ )
	    kerneldragger_->turnOn( yn );
    }

    if ( normalline_  )
	normalline_->turnOn( yn );

    allowdrag_ = yn;
} 


bool MarchingCubesDisplay::isManipulatorShown() const
{
    return allowdrag_;
}


bool MarchingCubesDisplay::hasInitialShape()
{ 
    return ( initialdragger_ && !displaysurface_ ) ||
       	   ( initialdragger_ && displaysurface_->getSurface()->isEmpty() ); 
}


bool MarchingCubesDisplay::createInitialBody( bool allowswap )
{
    if ( !initialdragger_ )
	return false;

    if ( !allowswap )
	return true;
    
    const Coord3 center = initialdragger_->center();
    const Coord3 width = initialdragger_->width();

    CubeSampling cs( true );
    cs.zrg = StepInterval<float>(
	    center.z-width.z/2, center.z+width.z/2, SI().zStep() );
    cs.hrg.start.inl = mNINT(center.x-width.x/2);
    cs.hrg.stop.inl = mNINT(center.x+width.x/2)+1;
    cs.hrg.start.crl = mNINT(center.y-width.y/2);
    cs.hrg.stop.crl = mNINT(center.y+width.y/2)+1;

    const int xsz = cs.nrInl();
    const int ysz = cs.nrCrl();
    const int zsz = cs.nrZ();
    const float hxsz = (float)xsz/2-0.5;
    const float hysz = (float)ysz/2-0.5;
    const float hzsz = (float)zsz/2-0.5;
    
    PtrMan<Array3DImpl<float> > array = new Array3DImpl<float> (
	    				xsz+2, ysz+2, zsz+2 );
    if ( !array || !array->isOK() )
	return false;
    
    for ( int idx=0; idx<xsz+2; idx++ )
    {
	for ( int idy=0; idy<ysz+2; idy++ )
	{
	    for ( int idz=0; idz<zsz+2; idz++ )
	    {
		const float diffx = ((float)idx-1-hxsz)/hxsz;
		const float diffy = ((float)idy-1-hysz)/hysz;
		const float diffz = ((float)idz-1-hzsz)/hzsz;
		const float diff = sqrt( diffx*diffx+diffy*diffy+diffz*diffz );
		array->set( idx, idy, idz, diff );
	    }
	}
    } 

    emsurface_->setInlSampling(
	    SamplingData<int>(cs.hrg.start.inl,cs.hrg.step.inl) );
    emsurface_->setCrlSampling(
	    SamplingData<int>(cs.hrg.start.crl,cs.hrg.step.crl) );

    emsurface_->setZSampling( SamplingData<float>( cs.zrg.start, cs.zrg.step ));
    
    ::MarchingCubesSurface& mcs = emsurface_->surface();
    mcs.removeAll();
    mcs.setVolumeData( -1, -1, -1, *array, 1 );

    array = 0;

    updateVisFromEM( false );

    return true;
}


void MarchingCubesDisplay::removeInitialDragger()
{
    if ( initialellipsoid_ )
    {
	removeChild( initialellipsoid_->getInventorNode() );
	initialellipsoid_->unRef();
	initialellipsoid_ = 0;
    }
    
    if ( !initialdragger_ )
	return;
    
    initialdragger_->finished.remove(
	    mCB(this, MarchingCubesDisplay, initialDraggerMovedCB) );

    initialdragger_->motion.remove(
	    mCB(this, MarchingCubesDisplay, initialDraggerMovingCB) );

    removeChild( initialdragger_->getInventorNode() );
    initialdragger_->unRef();
    initialdragger_ = 0;
}


void MarchingCubesDisplay::initialDraggerMovedCB( CallBacker* )
{
    if ( !initialdragger_ )
	return;

    const Coord3 center = initialdragger_->center();
    const Coord3 width = initialdragger_->width();

    CubeSampling cs( true );
    cs.zrg = StepInterval<float>(
	    center.z-width.z/2, center.z+width.z/2, SI().zStep() );
    cs.hrg.start.inl = mNINT(center.x-width.x/2);
    cs.hrg.stop.inl = mNINT(center.x+width.x/2)+1;
    cs.hrg.start.crl = mNINT(center.y-width.y/2);
    cs.hrg.stop.crl = mNINT(center.y+width.y/2)+1;
    cs.snapToSurvey();

    previoussample_ = cs;

    Coord3 newcenterpos( cs.hrg.inlRange().center(), 
	    		 cs.hrg.crlRange().center(), cs.zrg.center() );
    Coord3 newwidth( cs.hrg.inlRange().width(false), 
	    	     cs.hrg.crlRange().width(false), cs.zrg.width(false) );
    initialdragger_->setCenter( newcenterpos );
    initialdragger_->setWidth( newwidth );
    initialellipsoid_->setCenterPos( newcenterpos );
    initialellipsoid_->setWidth( newwidth );
}


void MarchingCubesDisplay::initialDraggerMovingCB( CallBacker* )
{
    if ( !initialdragger_ )
	return;
    
    if ( !initialellipsoid_ )
    {
	initialellipsoid_ = visBase::Ellipsoid::create();
	initialellipsoid_->ref();
	addChild( initialellipsoid_->getInventorNode() );
    }

    Coord3 center = initialdragger_->center();
    Coord3 width = initialdragger_->width();
    
    if ( width.x<minsampleinlsz_ || width.y<minsamplecrlsz_ ||
	 width.z<minsamplezsz_ )
    {
	Coord3 centerpos( previoussample_.hrg.inlRange().center(), 
			  previoussample_.hrg.crlRange().center(),
	    		  previoussample_.zrg.center() );
    	Coord3 prevwidth( previoussample_.hrg.inlRange().width(false), 
			  previoussample_.hrg.crlRange().width(false),
    			  previoussample_.zrg.width(false) );
	
	initialdragger_->setCenter( centerpos );    
    	initialdragger_->setWidth( prevwidth );
	
	initialellipsoid_->setCenterPos( centerpos );
    	initialellipsoid_->setWidth( prevwidth );
    }
    else
    {
	previoussample_.zrg = StepInterval<float>(
		center.z-width.z/2, center.z+width.z/2, SI().zStep() );
	previoussample_.hrg.start.inl = mNINT(center.x-width.x/2);
	previoussample_.hrg.stop.inl = mNINT(center.x+width.x/2)+1;
	previoussample_.hrg.start.crl = mNINT(center.y-width.y/2);
	previoussample_.hrg.stop.crl = mNINT(center.y+width.y/2)+1;
	previoussample_.snapToSurvey();
	
	initialellipsoid_->setCenterPos( center );
    	initialellipsoid_->setWidth( width );
    }
}

   
void MarchingCubesDisplay::kernelDraggerMovedCB( CallBacker* )
{
    if ( !kerneldragger_ || !kernelellipsoid_ )
	return;

    kernelsize_ = kerneldragger_->getSize();
    Coord3 center = kerneldragger_->getPos();

    kernelellipsoid_->setCenterPos( center );
    kernelellipsoid_->setWidth( kernelsize_ );
    setNormalLine( center, kernelsize_ );
}


void MarchingCubesDisplay::kernelDraggerMovingCB( CallBacker* )
{
    if ( !kerneldragger_ || !kernelellipsoid_ )
	return;

    Coord3 center = kerneldragger_->getPos();
    Coord3 width = kerneldragger_->getSize();

    kernelellipsoid_->setCenterPos( center );
    kernelellipsoid_->setWidth( width );
    setNormalLine( center, width );
}


MultiID MarchingCubesDisplay::getMultiID() const
{
    return emsurface_ ? emsurface_->multiID() : MultiID();
}


void MarchingCubesDisplay::setColor( Color nc )
{
    if ( emsurface_ ) emsurface_->setPreferredColor(nc);
    getMaterial()->setColor( nc );
}


NotifierAccess* MarchingCubesDisplay::materialChange()
{ return &getMaterial()->change; }


Color MarchingCubesDisplay::getColor() const
{ return getMaterial()->getColor(); }


void MarchingCubesDisplay::fillPar( IOPar& par, TypeSet<int>& saveids ) const
{
    visBase::VisualObjectImpl::fillPar( par, saveids );
    par.set( sKeyEarthModelID(), getMultiID() );
}


int MarchingCubesDisplay::usePar( const IOPar& par )
{
    int res = visBase::VisualObjectImpl::usePar( par );
    if ( res!=1 ) return res;

    MultiID newmid;
    if ( par.get(sKeyEarthModelID(),newmid) )
    {
	EM::ObjectID emid = EM::EMM().getObjectID( newmid );
	RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );
	if ( !emobject )
	{
	    PtrMan<Executor> loader = EM::EMM().objectLoader( newmid );
	    if ( loader ) loader->execute();
	    emid = EM::EMM().getObjectID( newmid );
	    emobject = EM::EMM().getObject( emid );
	}

	if ( emobject ) setEMID( emobject->id() );
    }

    return 1;
}


void MarchingCubesDisplay::setDisplayTransformation(visBase::Transformation* nt)
{
    if ( displaysurface_ ) displaysurface_->setDisplayTransformation( nt );
    if ( initialellipsoid_ ) initialellipsoid_->setDisplayTransformation( nt );
    if ( kernelellipsoid_ ) kernelellipsoid_->setDisplayTransformation( nt );
    if ( normalline_ ) normalline_->setDisplayTransformation( nt );
    if ( kerneldragger_ ) kerneldragger_->setDisplayTransformation( nt );
}


void MarchingCubesDisplay::setRightHandSystem( bool yn )
{
    visBase::VisualObjectImpl::setRightHandSystem( yn );
    if ( displaysurface_ ) displaysurface_->setRightHandSystem( yn );
}


visBase::Transformation* MarchingCubesDisplay::getDisplayTransformation()
{ return displaysurface_ ? displaysurface_->getDisplayTransformation() : 0; }


void MarchingCubesDisplay::setDragDirection( CallBacker* cb )
{
    if ( !allowdrag_ )
	return;

    const Coord3& wp = factordragger_->getStartPos();
    int surfacepos[] = { emsurface_->inlSampling().nearestIndex( wp.x ),
			 emsurface_->crlSampling().nearestIndex( wp.y ),
		 	 emsurface_->zSampling().nearestIndex( wp.z ) };
    const BinID bid( emsurface_->inlSampling().atIndex( surfacepos[0]) ,
	             emsurface_->crlSampling().atIndex( surfacepos[1]) );
    
    Coord3 center( bid.inl, bid.crl, wp.z );
    Coord3 width( emsurface_->inlSampling().step*mKernelSize,
	          emsurface_->crlSampling().step*mKernelSize,
     		  emsurface_->zSampling().step*mKernelSize );
    
    if ( kernelsize_.x!=0 && kernelsize_.y!=0 && kernelsize_.z!=0 )
	width = kernelsize_;

    int size[] = { mNINT(width.x/emsurface_->inlSampling().step),
		   mNINT(width.y/emsurface_->crlSampling().step),
	 	   mNINT(width.z/emsurface_->zSampling().step) };
    
    if ( !surfaceeditor_ )
	surfaceeditor_ = new MarchingCubesSurfaceEditor(emsurface_->surface());
    
    PtrMan<Array3D<unsigned char> > kernel =
	createKernel( size[0], size[1], size[2] );
    surfaceeditor_->setKernel( *kernel, surfacepos[0]-size[0]/2, 
	    surfacepos[1]-size[1]/2, surfacepos[2]-size[1]/2 );

    if ( !kernelpickstyle_ )
    {
	kerneldragger_ = visBase::Dragger::create();
	kerneldragger_->ref();
	kerneldragger_->setDraggerType( visBase::Dragger::Scale3D );
	addChild( kerneldragger_->getInventorNode() );
	kerneldragger_->setRotation( Coord3(1,0,0), 0 );
	kerneldragger_->setSize( width );

     	kernelpickstyle_  = visBase::PickStyle::create();
     	kernelpickstyle_->ref();
     	addChild( kernelpickstyle_->getInventorNode() );
	kernelpickstyle_->setStyle( visBase::PickStyle::Unpickable );
    
	kernelellipsoid_ = visBase::Ellipsoid::create();
	kernelellipsoid_->ref();
	addChild( kernelellipsoid_->getInventorNode() );
    }

    kernelellipsoid_->setCenterPos( center );
    kernelellipsoid_->setWidth( width );

    if ( kerneldragger_ )
    {
	kerneldragger_->setPos( center );
	kerneldragger_->finished.notify( 
		mCB(this, MarchingCubesDisplay, kernelDraggerMovedCB) );
	kerneldragger_->motion.notify( 
		mCB(this, MarchingCubesDisplay, kernelDraggerMovingCB) );
    }
   
    if ( !kernelellipsoid_->getMaterial() )
	kernelellipsoid_->setMaterial( visBase::Material::create() );

    kernelellipsoid_->getMaterial()->setTransparency( 0.8 );
    kernelellipsoid_->getMaterial()->setColor( 
	    getMaterial()->getColor().complementaryColor() );
    
    setNormalLine( center, width );
}


void MarchingCubesDisplay::setNormalLine( Coord3& center, Coord3& width )
{
    if ( !normalline_ || !emsurface_ )
	return;

    float r = width.x<width.y? width.x:width.y;
    r = r<width.z? r:width.z;
    Coord3 nm = surfaceeditor_->getCenterNormal();
    nm = Coord3(nm.x*emsurface_->inlSampling().step, 
	        nm.y*emsurface_->crlSampling().step,
	       	nm.z*emsurface_->zSampling().step);
    
    normalline_->getCoordinates()->setPos( 0, center-r*nm );
    normalline_->getCoordinates()->setPos( 1, center+r*nm );
    normalline_->setCoordIndex(0, 0);
    normalline_->setCoordIndex(1, 1);
    normalline_->setCoordIndex(2, -1);
    
    if ( !normalline_->getMaterial() )
	normalline_->setMaterial( visBase::Material::create() );

    normalline_->getMaterial()->setTransparency( 0 );
    normalline_->getMaterial()->setColor( 
	    getMaterial()->getColor().complementaryColor() );

    if ( factordragger_ )
    	factordragger_->setDirection( nm );
}


void MarchingCubesDisplay::factorDrag( CallBacker* )
{
    if ( !factordragger_ )
	return;

    const float drag = factordragger_->getTranslation().dot(
	    surfaceeditor_->getCenterNormal() );
    surfaceeditor_->setFactor( -mNINT(drag*255) );
    updateVisFromEM( true );
}


Array3D<unsigned char>*
MarchingCubesDisplay::createKernel( int xsz, int ysz, int zsz ) const
{
    Array3D<unsigned char>* res = new Array3DImpl<unsigned char>(xsz,ysz,zsz);
    if ( !res || !res->isOK() )
    {
	delete res;
	return 0;
    }

    const int hxsz = xsz/2; const int hysz = ysz/2; const int hzsz = zsz/2;

    for ( int idx=0; idx<xsz; idx++ )
    {
	float xval = idx-hxsz; xval /= hxsz; xval *= xval;
	for ( int idy=0; idy<ysz; idy++ )
	{
	    float yval = idy-hysz; yval /= hysz; yval *= yval;
	    for ( int idz=0; idz<zsz; idz++ )
	    {
		float zval = idz-hzsz; zval /= hzsz; zval *= zval;

		int invdist = mNINT((1-sqrt( xval+yval+zval ))*255);
		if ( invdist<0 ) invdist = 0;
		res->set( idx, idy, idz, invdist );
	    }
	}
    }

    return res;
}


}; // namespace visSurvey
