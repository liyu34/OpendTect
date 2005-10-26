/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : Oct 2005
-*/

static const char* rcsID = "$Id: zaxistransform.cc,v 1.4 2005-10-26 22:07:16 cvskris Exp $";

#include "zaxistransform.h"

#include "survinfo.h"

DefineEnumNames(ZAxisTransform,ZType,1,"Z Type")
{ "Time", "Depth", "StratDepth", 0 };


ZAxisTransform::ZAxisTransform()
{ mRefCountConstructor; }


ZAxisTransform::~ZAxisTransform()
{ }


int ZAxisTransform::addVolumeOfInterest( const CubeSampling& ) { return -1; }


void ZAxisTransform::setVolumeOfInterest( int, const CubeSampling& ) {}


void ZAxisTransform::removeVolumeOfInterest( int ) {}


float ZAxisTransform::transform( const Coord3& pos ) const
{ return transform( BinIDValue( SI().transform(pos), pos.z ) ); }



float ZAxisTransform::transformBack( const Coord3& pos ) const
{ return transformBack( BinIDValue( SI().transform(pos), pos.z ) ); }


ZAxisTransformSampler::ZAxisTransformSampler( const ZAxisTransform& trans,
					      bool b, const BinID& nbid,
					      const SamplingData<double>& nsd )
    : transform( trans )
    , back( b )
    , bid( nbid )
    , sd( nsd )
{ transform.ref(); }


ZAxisTransformSampler::~ZAxisTransformSampler()
{ transform.unRef(); }


float ZAxisTransformSampler::operator[](int idx) const
{
    const BinIDValue bidval( BinIDValue(bid,sd.atIndex(idx)) );
    return back ? transform.transformBack( bidval )
	        : transform.transform( bidval );
}


ZAxisTransformFactorySet::~ZAxisTransformFactorySet()
{}


ZAxisTransform* ZAxisTransformFactorySet::create(
	const ZAxisTransform::ZType& t0, const ZAxisTransform::ZType& t1) const
{
    for ( int idx=0; idx<factories.size(); idx++ )
    {
	ZAxisTransform* res = factories[idx]( t0, t1 );
	if ( res ) return res;
	res = factories[idx]( t1, t0 );
	if ( res ) return res;
    }

    return 0;
}


void ZAxisTransformFactorySet::addFactory(
	ZAxisTransformFactory factory )
{
    factories += factory;
}


ZAxisTransformFactorySet& ZATF()
{
    static ZAxisTransformFactorySet factoryset;
    return factoryset;
}
