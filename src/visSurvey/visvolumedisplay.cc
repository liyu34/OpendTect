/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra
 Date:          August 2002
 RCS:           $Id: visvolumedisplay.cc,v 1.45 2004-01-09 16:27:08 nanne Exp $
________________________________________________________________________

-*/


#include "visvolumedisplay.h"

#include "viscubeview.h"
#include "vistexture3viewer.h"
#include "cubesampling.h"
#include "attribsel.h"
#include "attribslice.h"
#include "arrayndimpl.h"
#include "survinfo.h"
#include "visselman.h"
#include "visdataman.h"
#include "sorting.h"
#include "iopar.h"
#include "vismaterial.h"

mCreateFactoryEntry( visSurvey::VolumeDisplay );

namespace visSurvey {

const char* VolumeDisplay::volumestr = "Cube ID";
const char* VolumeDisplay::volrenstr = "Volren";
const char* VolumeDisplay::inlinestr = "Inline";
const char* VolumeDisplay::crosslinestr = "Crossline";
const char* VolumeDisplay::timestr = "Time";
const char* VolumeDisplay::inlineposstr = "Inline position";
const char* VolumeDisplay::crosslineposstr = "Crossline position";
const char* VolumeDisplay::timeposstr = "Time position";
const char* VolumeDisplay::inlineshowstr = "Inline shown";
const char* VolumeDisplay::crosslineshowstr = "Crossline shown";
const char* VolumeDisplay::timeshowstr = "Time shown";


VolumeDisplay::VolumeDisplay()
    : VisualObject(true)
    , cube(0)
    , as(*new AttribSelSpec)
    , colas(*new ColorAttribSel)
    , cache(0)
    , colcache(0)
    , slicemoving(this)
{
    setCube( visBase::CubeView::create() );

    cube->getMaterial()->setAmbience( 0.8 );
    cube->getMaterial()->setDiffIntensity( 0.8 );

    CubeSampling cs;
    cs.hrg.start.inl = (5*SI().range().start.inl+3*SI().range().stop.inl)/8;
    cs.hrg.start.crl = (5*SI().range().start.crl+3*SI().range().stop.crl)/8;
    cs.hrg.stop.inl = (3*SI().range().start.inl+5*SI().range().stop.inl)/8;
    cs.hrg.stop.crl = (3*SI().range().start.crl+5*SI().range().stop.crl)/8;
    cs.zrg.start = ( 5*SI().zRange().start + 3*SI().zRange().stop ) / 8;
    cs.zrg.stop = ( 3*SI().zRange().start + 5*SI().zRange().stop ) / 8;
    SI().snap( cs.hrg.start, BinID(0,0) );
    SI().snap( cs.hrg.stop, BinID(0,0) );
    float z0 = SI().zRange().snap( cs.zrg.start ); cs.zrg.start = z0;
    float z1 = SI().zRange().snap( cs.zrg.stop ); cs.zrg.stop = z1;
    
    setCubeSampling( cs );

    inlid = cube->addSlice( 0, (cs.hrg.start.inl+cs.hrg.stop.inl)/2 );
    initSlice( inlid );
    crlid = cube->addSlice( 1, (cs.hrg.start.crl+cs.hrg.stop.crl)/2 );
    initSlice( crlid );
    tslid = cube->addSlice( 2, cs.zrg.center()  );
    initSlice( tslid );

    visBase::DM().getObj( inlid )->setName(inlinestr);
    visBase::DM().getObj( crlid )->setName(crosslinestr);
    visBase::DM().getObj( tslid )->setName(timestr);

    const int volrenid = cube->getVolRenId();
    visBase::DM().getObj( volrenid )->setName(volrenstr);

    setColorTab( getColorTab() );
}


VolumeDisplay::~VolumeDisplay()
{
    cube->getBoxManipEnd()->remove(mCB(this,VolumeDisplay,manipMotionFinishCB));
    cube->unRef();

    delete &as;
    delete &colas;

    delete cache;
    delete colcache;
}


void VolumeDisplay::setCube( visBase::CubeView* cv )
{
    if ( cube )
    {
	cube->getBoxManipEnd()->remove(
				mCB(this,VolumeDisplay,manipMotionFinishCB) );
	cube->unRef();
    }
    
    cube = cv;
    cube->ref();
    cube->getBoxManipEnd()->notify(mCB(this,VolumeDisplay,manipMotionFinishCB));
}


void VolumeDisplay::setCenter( const Coord3& pos )
{ cube->setCenter( pos ); }


Coord3 VolumeDisplay::center() const
{ return cube->center(); }


Coord3 VolumeDisplay::manipCenter() const
{ return cube->draggerCenter(); }


void VolumeDisplay::setWidth( const Coord3& pos )
{ cube->setWidth( pos ); }


Coord3 VolumeDisplay::width() const
{ return cube->width(); }


Coord3 VolumeDisplay::manipWidth() const
{ return cube->draggerWidth(); }


void VolumeDisplay::showBox( bool yn )
{ cube->showBox( yn ); }


void VolumeDisplay::resetManip()
{
    cube->resetDragger();
}


void VolumeDisplay::getPlaneIds( int& id0, int& id1, int& id2 )
{
    id0 = inlid;
    id1 = crlid;
    id2 = tslid;
}


float VolumeDisplay::getPlanePos( int id__ )
{
    return cube->slicePosition( id__ );
}


AttribSelSpec& VolumeDisplay::getAttribSelSpec()
{ return as; }


const AttribSelSpec& VolumeDisplay::getAttribSelSpec() const
{ return as; }


void VolumeDisplay::setAttribSelSpec( const AttribSelSpec& as_ )
{
    if ( as==as_ ) return;
    as = as_;
    delete cache;
    cache = 0;
    cube->useTexture( false );
}


ColorAttribSel& VolumeDisplay::getColorSelSpec()
{ return colas; }


const ColorAttribSel& VolumeDisplay::getColorSelSpec() const
{ return colas; }


void VolumeDisplay::setColorSelSpec( const ColorAttribSel& as_ )
{ colas = as_; }


CubeSampling& VolumeDisplay::getCubeSampling(bool manippos)
{
    Coord3 center_ = manippos ? cube->draggerCenter() : cube->center();
    Coord3 width_ = manippos ? cube->draggerWidth() : cube->width();

    CubeSampling* cs = new CubeSampling;
    cs->hrg.start = BinID( mNINT( center_.x - width_.x / 2 ),
		  	  mNINT( center_.y - width_.y / 2 ) );

    cs->hrg.stop = BinID( mNINT( center_.x + width_.x / 2 ),
		  	 mNINT( center_.y + width_.y / 2 ) );

    cs->hrg.step = BinID( SI().inlStep(), SI().crlStep() );

    cs->zrg.start = center_.z - width_.z / 2;
    cs->zrg.stop = center_.z + width_.z / 2;

    return *cs;
}


void VolumeDisplay::setCubeSampling( const CubeSampling& cs_ )
{
    Coord3 newwidth( cs_.hrg.stop.inl - cs_.hrg.start.inl,
			 cs_.hrg.stop.crl - cs_.hrg.start.crl, 
			 cs_.zrg.stop - cs_.zrg.start );
    setWidth( newwidth );
    Coord3 newcenter( (cs_.hrg.stop.inl + cs_.hrg.start.inl) / 2,
			  (cs_.hrg.stop.crl + cs_.hrg.start.crl) / 2,
			  (cs_.zrg.stop + cs_.zrg.start) / 2 );
    setCenter( newcenter );
    resetManip();
}


bool VolumeDisplay::putNewData( AttribSliceSet* sliceset, 
					   bool colordata )
{
    if ( !sliceset )
    {
	delete sliceset;
	return false;
    }

    if ( colordata )
    {
	Interval<float> cliprate( colas.cliprate0, colas.cliprate1 );
	cube->setColorPars( colas.reverse, colas.useclip,
			    colas.useclip ? cliprate : colas.range );
    }

    setData( sliceset, colordata ? colas.datatype : 0 );
    if ( colordata )
    {
	delete colcache;
	colcache = sliceset;
	return true;
    }

    delete cache;
    cache = sliceset;
    return true;
}


void VolumeDisplay::setData( const AttribSliceSet* sliceset,
					int datatype )
{
    PtrMan<Array3D<float> > datacube = sliceset->createArray( 0, 1, 2 );
    cube->setData( datacube, datatype );
    cube->useTexture( true );
}


const AttribSliceSet* VolumeDisplay::getCachedData( bool colordata ) const
{
    return colordata ? colcache : cache;
}


float VolumeDisplay::getValue( const Coord3& pos ) const
{
    if ( !cache ) return mUndefValue;

    const CubeSampling& cs = getCubeSampling(false);
    Coord3 origo( cs.hrg.start.inl, cs.hrg.start.crl, cs.zrg.start );

    Coord3 localpos;
    localpos.x = pos.x - origo.x;
    localpos.y = pos.y - origo.y;
    localpos.z = pos.z - origo.z;

    Coord3 cubewidth = width();

    const int setsz = cache->size();
    int sz0, sz1, idx;
    sz0 = sz1 = idx = 0;
    while ( true )
    {
	AttribSlice* slice = (*cache)[idx];
	if ( !slice ) { idx++; continue; }

	sz0 = slice->info().getSize(0);
	sz1 = slice->info().getSize(1);
	break;
    }

    if ( !sz0 || !sz1 ) return mUndefValue;

    double setidx, idx0, idx1;
    if ( cache->direction == AttribSlice::Inl )
    {
	setidx = localpos.x * (setsz-1) / cubewidth.x;
	idx0 = localpos.y * (sz0-1) / cubewidth.y;
	idx1 = localpos.z * (sz1-1) / cubewidth.z;
    }
    else if ( cache->direction == AttribSlice::Crl )
    {
	setidx = localpos.y * (setsz-1) / cubewidth.y;
	idx0 = localpos.x * (sz0-1) / cubewidth.x;
	idx1 = localpos.z * (sz1-1) / cubewidth.z;
    }
    else if ( cache->direction == AttribSlice::Hor )
    {
	setidx = localpos.z * (setsz-1) / cubewidth.z;
	idx0 = localpos.x * (sz0-1) / cubewidth.x;
	idx1 = localpos.y * (sz1-1) / cubewidth.y;
    }

    if ( setidx < 0 || setidx > setsz-1 || 
	 idx0 < 0 || idx0 > sz0-1 || idx1 < 0 || idx1 > sz1-1 )
	return mUndefValue;

    AttribSlice* curslice = (*cache)[(int)setidx];
    return curslice ? curslice->get( (int)idx0, (int)idx1 ) : mUndefValue;
}


void VolumeDisplay::turnOn( bool yn ) 
{ cube->turnOn( yn ); }


bool VolumeDisplay::isOn() const 
{ return cube->isOn(); }


void VolumeDisplay::manipMotionFinishCB( CallBacker* )
{
    CubeSampling cs = getCubeSampling(true);
    SI().snap( cs.hrg.start, BinID(0,0) );
    SI().snap( cs.hrg.stop, BinID(0,0) );
    float z0 = SI().zRange().snap( cs.zrg.start ); cs.zrg.start = z0;
    float z1 = SI().zRange().snap( cs.zrg.stop ); cs.zrg.stop = z1;

    const Interval<int> inlrange( SI().range(true).start.inl,
				  SI().range(true).stop.inl );
    const Interval<int> crlrange( SI().range(true).start.crl,
				  SI().range(true).stop.crl );

    BinIDRange bidr( cs.hrg.start, cs.hrg.stop );
    SI().checkRange( bidr );
    Interval<double> zrg( cs.zrg.start, cs.zrg.stop );
    SI().checkZRange( zrg );
    if ( bidr.start.inl == bidr.stop.inl ||
	 bidr.start.crl == bidr.stop.crl ||
	 zrg.start == zrg.stop )
    {
	resetManip();
	return;
    }
    else
    {
	cs.hrg.start = bidr.start;
	cs.hrg.stop = bidr.stop;
	cs.zrg.start = (float)zrg.start;
	cs.zrg.stop = (float)zrg.stop;
    }


    const Coord3 newwidth( cs.hrg.stop.inl - cs.hrg.start.inl,
			 cs.hrg.stop.crl - cs.hrg.start.crl, 
			 cs.zrg.stop - cs.zrg.start );
    cube->setDraggerWidth( newwidth );
    const Coord3 newcenter( (cs.hrg.stop.inl + cs.hrg.start.inl) / 2,
			  (cs.hrg.stop.crl + cs.hrg.start.crl) / 2,
			  (cs.zrg.stop + cs.zrg.start) / 2 );
    cube->setDraggerCenter( newcenter );
}


void VolumeDisplay::initSlice( int sliceid )
{
    DataObject* dobj = visBase::DM().getObj( sliceid );
    mDynamicCastGet(visBase::MovableTextureSlice*,ts,dobj);
    if ( ts )
    {
	ts->setMaterial( cube->getMaterial() );
	ts->motion.notify( mCB(this,VolumeDisplay,sliceMoving) );
    }
}


void VolumeDisplay::sliceMoving( CallBacker* )
{
    slicemoving.trigger();
}


void VolumeDisplay::setColorTab( visBase::VisColorTab& ctab )
{
    cube->setVolRenColorTab( ctab );
    cube->setViewerColorTab( ctab );
}


const visBase::VisColorTab& VolumeDisplay::getColorTab() const
{ return cube->getViewerColorTab(); }


visBase::VisColorTab& VolumeDisplay::getColorTab()
{ return cube->getViewerColorTab(); }


const TypeSet<float>& VolumeDisplay::getHistogram() const
{ return cube->getHistogram(); }


void VolumeDisplay::setMaterial( visBase::Material* nm)
{ cube->setMaterial(nm); }


visBase::Material* VolumeDisplay::getMaterial()
{ return cube->getMaterial(); }


const visBase::Material* VolumeDisplay::getMaterial() const
{ return cube->getMaterial(); }


int VolumeDisplay::getVolRenId() const
{ return cube->getVolRenId(); }


SoNode* VolumeDisplay::getInventorNode() 
{ return cube->getInventorNode(); }


bool VolumeDisplay::isVolRenShown() const
{
    return cube->isVolRenShown();
}


void VolumeDisplay::fillPar( IOPar& par, TypeSet<int>& saveids) const
{
    visBase::VisualObject::fillPar( par, saveids );

    int volid = cube->id();
    par.set( volumestr, volid );
    if ( saveids.indexOf( volid )==-1 ) saveids += volid;

    par.set( inlineposstr, cube->slicePosition(inlid) );
    par.set( crosslineposstr, cube->slicePosition(crlid) );
    par.set( timeposstr, cube->slicePosition(tslid) );
    par.setYN( inlineshowstr, cube->isSliceShown(inlid) );
    par.setYN( crosslineshowstr, cube->isSliceShown(crlid) );
    par.setYN( timeshowstr, cube->isSliceShown(tslid) );

    as.fillPar(par);
    colas.fillPar(par);
}


int VolumeDisplay::usePar( const IOPar& par )
{
    int res =  visBase::VisualObject::usePar( par );
    if ( res!=1 ) return res;

    int volid;
    if ( !par.get( volumestr, volid ))
	return -1;

    visBase::DataObject* dataobj = visBase::DM().getObj( volid );
    if ( !dataobj ) return 0;
    mDynamicCastGet(visBase::CubeView*,cv,dataobj);
    if ( !cv ) return -1;
    setCube( cv );

    const CubeSampling& cs = getCubeSampling(false);
    Coord3 origo( cs.hrg.start.inl, cs.hrg.start.crl, cs.zrg.start );

    float pos = origo.x;
    par.get( inlineposstr, pos );
    inlid = cube->addSlice( 0, pos );
    initSlice( inlid );
    bool show = true;
    par.getYN( inlineshowstr, show );
    cube->showSlice( inlid, show );
    visBase::DM().getObj( inlid )->setName(inlinestr);

    pos = origo.y;
    par.get( crosslineposstr, pos );
    crlid = cube->addSlice( 1, pos );
    initSlice( crlid );
    show = true;
    par.getYN( crosslineshowstr, show );
    cube->showSlice( crlid, show );
    visBase::DM().getObj( crlid )->setName(crosslinestr);

    pos = origo.z;
    par.get( timeposstr, pos );
    tslid = cube->addSlice( 2, pos );
    initSlice( tslid );
    show = true;
    par.getYN( timeshowstr, show );
    cube->showSlice( tslid, show );
    visBase::DM().getObj( tslid )->setName(timestr);

    const int volrenid = cube->getVolRenId();
    visBase::DM().getObj( volrenid )->setName(volrenstr);

    if ( !as.usePar(par) ) return -1;
    colas.usePar( par );

    setColorTab( getColorTab() );

    return 1;
}

}; // namespace visSurvey
