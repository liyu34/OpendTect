/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Mahant Mothey
 Date:		February 2015
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id: seisdatapack.cc 38551 2015-03-18 05:38:02Z mahant.mothey@dgbes.com $";


#include "seisdatapack.h"

#include "atomic.h"
#include "arrayndimpl.h"
#include "arrayndslice.h"
#include "binidvalset.h"
#include "convmemvalseries.h"
#include "flatposdata.h"
#include "paralleltask.h"
#include "randomlinegeom.h"
#include "seistrc.h"
#include "survinfo.h"
#include "keystrs.h"

#include <limits.h>


class Regular2RandomDataCopier : public ParallelTask
{
public:
		Regular2RandomDataCopier( RandomSeisDataPack& ransdp,
					  const RegularSeisDataPack& regsdp,
					  int rancompidx )
		    : ransdp_( ransdp )
		    , regsdp_( regsdp )
		    , path_( ransdp.getPath() )
		    , regidx_( -1 )
		    , ranidx_( rancompidx )
		    , domemcopy_( false )
		    , samplebytes_( sizeof(float) )
		    , srcptr_( 0 )
		    , dstptr_( 0 )
		    , srctrcbytes_( 0 )
		    , dsttrcbytes_( 0 )
		    , srclnbytes_( 0 )
		    , bytestocopy_( 0 )
		    , idzoffset_( 0 )
		{}

    od_int64	nrIterations() const			{ return path_.size(); }

    bool	doPrepare(int nrthreads);
    bool	doWork(od_int64 start,od_int64 stop,int thread);


protected:

    RandomSeisDataPack&		ransdp_;
    const RegularSeisDataPack&	regsdp_;
    const TrcKeyPath&		path_;
    int				regidx_;
    int				ranidx_;
    bool			domemcopy_;
    int				samplebytes_;
    const unsigned char*	srcptr_;
    unsigned char*		dstptr_;
    od_int64			srctrcbytes_;
    od_int64			dsttrcbytes_;
    od_int64			srclnbytes_;
    od_int64			bytestocopy_;
    int				idzoffset_;
};


bool Regular2RandomDataCopier::doPrepare( int nrthreads )
{
    regidx_ = regsdp_.getComponentIdx( ransdp_.getComponentName(ranidx_),
				       ranidx_ );

    if ( !regsdp_.validComp(regidx_) || !ransdp_.validComp(ranidx_) )
	return false;

    if ( !regsdp_.getZRange().overlaps(ransdp_.getZRange()) )
	return false;

    idzoffset_ = regsdp_.getZRange().nearestIndex( ransdp_.getZRange().start );

    if ( !regsdp_.getZRange().isCompatible(ransdp_.getZRange(),1e-3) )
    {
	pErrMsg( "Unexpected incompatibility of datapack Z-ranges" );
	return false;
    }

    if ( regsdp_.getDataDesc() != ransdp_.getDataDesc() )
	return true;

    srcptr_ = mCast( const unsigned char*, regsdp_.data(regidx_).getData() );
    mDynamicCastGet( const ConvMemValueSeries<float>*, regstorage,
		     regsdp_.data(regidx_).getStorage() );
    if ( regstorage )
    {
	srcptr_ = mCast( const unsigned char*, regstorage->storArr() );
	samplebytes_ = regsdp_.getDataDesc().nrBytes();
    }

    dstptr_ = mCast( unsigned char*, ransdp_.data(ranidx_).getData() );
    mDynamicCastGet( const ConvMemValueSeries<float>*, ranstorage,
		     ransdp_.data(ranidx_).getStorage() );
    if ( ranstorage )
	dstptr_ = mCast( unsigned char*, ranstorage->storArr() );

    if ( !srcptr_ || !dstptr_ )
	return true;

    srctrcbytes_ = samplebytes_ * regsdp_.sampling().size(TrcKeyZSampling::Z);
    srclnbytes_ = srctrcbytes_ * regsdp_.sampling().size(TrcKeyZSampling::Crl);
    dsttrcbytes_ = samplebytes_ * (ransdp_.getZRange().nrSteps()+1);

    bytestocopy_ = dsttrcbytes_;

    if ( idzoffset_ < 0 )
    {
	dstptr_ -= samplebytes_ * idzoffset_;
	bytestocopy_ += samplebytes_ * idzoffset_;
    }
    else
	srcptr_ += samplebytes_ * idzoffset_;

    const int stopoffset = regsdp_.getZRange().nrSteps() -
		regsdp_.getZRange().nearestIndex( ransdp_.getZRange().stop );

    if ( stopoffset < 0 )
	bytestocopy_ += samplebytes_ * stopoffset;

    domemcopy_ = true;
    return true;
}


bool Regular2RandomDataCopier::doWork( od_int64 start, od_int64 stop,
				       int thread )
{
    for ( int idx=mCast(int,start); idx<=mCast(int,stop); idx++ )
    {
	const TrcKeySampling& hsamp = regsdp_.sampling().hsamp_;
	if ( !hsamp.lineRange().includes(path_[idx].lineNr(),true) ||
	     !hsamp.trcRange().includes(path_[idx].trcNr(),true) )
	    continue;

	const int shiftedtogetnearestinl = path_[idx].lineNr() +
					   hsamp.step_.lineNr()/2;
	const int inlidx = hsamp.inlIdx( shiftedtogetnearestinl );
	const int shiftedtogetnearestcrl = path_[idx].trcNr() +
					   hsamp.step_.trcNr()/2;
	const int crlidx = hsamp.crlIdx( shiftedtogetnearestcrl );

	if ( domemcopy_ )
	{
	    const unsigned char* srcptr = srcptr_ + inlidx*srclnbytes_
						  + crlidx*srctrcbytes_;
	    unsigned char* dstptr = dstptr_ + idx*dsttrcbytes_;
	    OD::sysMemCopy( dstptr, srcptr, bytestocopy_ );
	    continue;
	}

	for ( int newidz=0; newidz<=ransdp_.getZRange().nrfSteps(); newidz++ )
	{
	    const int oldidz = newidz + idzoffset_;
	    const float val =
		regsdp_.data(regidx_).info().validPos(inlidx,crlidx,oldidz) ?
		regsdp_.data(regidx_).get(inlidx,crlidx,oldidz) : mUdf(float);

	    ransdp_.data(ranidx_).set( 0, idx, newidz, val );
	}
    }

    return true;
}


//=============================================================================
// SeisVolumeDataPack

void SeisVolumeDataPack::fillTrace( const TrcKey& trcky, SeisTrc& trc ) const
{
    const int nrcomps = nrComponents();
    DataCharacteristics dc;
    if ( !scaler_ )
	dc = DataCharacteristics( getDataDesc() );
    for ( int icomp=0; icomp<nrcomps; icomp++ )
	trc.data().setComponent( dc, icomp );

    const int trcsz = getZRange().nrSteps() + 1;
    trc.reSize( trcsz, false );

    const int globidx = getGlobalIdx( trcky );
    if ( globidx < 0 )
	{ trc.zero(); return; }

    Array1DImpl<float> copiedtrc( trcsz );
    for ( int icomp=0; icomp<nrcomps; icomp++ )
    {
	const float* vals = getTrcData( icomp, globidx );
	if ( !vals && !getCopiedTrcData(icomp,globidx,copiedtrc) )
	{
	    trc.zero();
	    return;
	}

	float* copiedtrcptr = copiedtrc.getData();
	for ( int isamp=0; isamp<trcsz; isamp++ )
	{
	    const float val = vals ? vals[isamp] : copiedtrcptr
						  ? copiedtrcptr[isamp]
						  : copiedtrc.get( isamp );
	    trc.set( isamp, val, icomp );
	}
    }

    SeisTrcInfo& inf = trc.info();
    const StepInterval<float> zrg = getZRange();
    inf.sampling_.start = zrg.start;
    inf.sampling_.step = zrg.step;
    inf.trckey_ = trcky;
    inf.coord_ = trcky.getCoord();
    inf.offset_ = 0.f;
}


//=============================================================================

// RegularSeisDataPack

RegularSeisDataPack::RegularSeisDataPack( const char* cat,
					  const BinDataDesc* bdd )
    : SeisVolumeDataPack(cat,bdd)
    , sampling_(false) //MUST be set to false in the constructor
    , trcssampling_(0)
{}


RegularSeisDataPack::~RegularSeisDataPack()
{
}


RegularSeisDataPack* RegularSeisDataPack::clone() const
{
    RegularSeisDataPack* ret = getSimilar();
    if ( trcsSampling() )
	ret->setTrcsSampling( new PosInfo::SortedCubeData(*trcsSampling()) );

    ret->setZDomain( zDomain() );
    ret->setRefNrs( refnrs_ );
    ret->setDataDesc( getDataDesc() );
    if ( getScaler() )
	ret->setScaler( *getScaler() );

    if ( !ret->copyFrom(*this) )
	{ delete ret; return 0; }

    return ret;
}


RegularSeisDataPack* RegularSeisDataPack::getSimilar() const
{
    RegularSeisDataPack* ret = new RegularSeisDataPack( category(), &desc_ );
    ret->setSampling( sampling() );
    return ret;
}


bool RegularSeisDataPack::copyFrom( const RegularSeisDataPack& oth )
{
    componentnames_.setEmpty();
    deepErase( arrays_ );
    for ( int icomp=0; icomp<oth.nrComponents(); icomp++ )
    {
	if ( !addComponent(oth.getComponentName(icomp)) )
	    return false;

	*arrays_[icomp] = oth.data( icomp );
    }

    return true;
}


void RegularSeisDataPack::setTrcsSampling( PosInfo::CubeData* newposdata )
{
    if ( is2D() )
	{ pErrMsg("Will not use CubeData for 2D"); return; }

    trcssampling_ = newposdata;
}


const PosInfo::CubeData* RegularSeisDataPack::trcsSampling() const
{
    return is2D() ? 0 : trcssampling_.ptr();
}


void RegularSeisDataPack::getTrcPositions( PosInfo::CubeData& cd ) const
{
    cd.setEmpty();
    if ( !is2D() )
    {
	if ( trcssampling_ )
	    cd = *trcssampling_;
	else if ( !sampling_.isDefined() )
	    cd.setEmpty();
	else
	{
	    const TrcKeySampling& hs = sampling_.hsamp_;
	    cd.generate( hs.start_, hs.stop_, hs.step_ );
	}
    }
}


TrcKey RegularSeisDataPack::getTrcKey( int globaltrcidx ) const
{ return sampling_.hsamp_.trcKeyAt( globaltrcidx ); }

bool RegularSeisDataPack::is2D() const
{ return sampling_.hsamp_.survid_ == Survey::GM().get2DSurvID(); }

int RegularSeisDataPack::getGlobalIdx( const TrcKey& tk ) const
{
    const int ret = mCast(int,sampling_.hsamp_.globalIdx(tk));
    return ret < nrTrcs() ? ret : -1;
}


bool RegularSeisDataPack::addComponent( const char* nm )
{
    if ( !sampling_.isDefined() || sampling_.hsamp_.totalNr()>INT_MAX )
	return false;

    if ( !addArray(sampling_.nrLines(),sampling_.nrTrcs(),sampling_.nrZ()) )
	return false;

    componentnames_.add( nm );
    return true;
}


void RegularSeisDataPack::dumpInfo( IOPar& par ) const
{
    VolumeDataPack::dumpInfo( par );

    const TrcKeySampling& tks = sampling_.hsamp_;
    if ( is2D() )
    {
	par.set( sKey::TrcRange(), tks.start_.trcNr(), tks.stop_.trcNr(),
				   tks.step_.trcNr() );
    }
    else
    {
	par.set( sKey::InlRange(), tks.start_.lineNr(), tks.stop_.lineNr(),
				   tks.step_.lineNr() );
	par.set( sKey::CrlRange(), tks.start_.trcNr(), tks.stop_.trcNr(),
				   tks.step_.trcNr() );
    }

    par.set( sKey::ZRange(), sampling_.zsamp_.start, sampling_.zsamp_.stop,
			     sampling_.zsamp_.step );
}


DataPack::ID RegularSeisDataPack::createDataPackForZSlice(
						const BinIDValueSet* bivset,
						const TrcKeyZSampling& tkzs,
						const ZDomain::Info& zinfo,
						const BufferStringSet& names )
{
    if ( !bivset || !tkzs.isDefined() || tkzs.nrZ()!=1 )
	return DataPack::cNoID();

    RegularSeisDataPack* regsdp = new RegularSeisDataPack(
				    VolumeDataPack::categoryStr(false,true) );
    regsdp->setSampling( tkzs );
    for ( int idx=1; idx<bivset->nrVals(); idx++ )
    {
	const char* name = names.validIdx(idx-1) ? names[idx-1]->buf()
						 : sKey::EmptyString().buf();
	regsdp->addComponent( name );
	BinIDValueSet::SPos pos;
	BinID bid;
	while ( bivset->next(pos,true) )
	{
	    bivset->get( pos, bid );
	    regsdp->data(idx-1).set( tkzs.hsamp_.inlIdx(bid.inl()),
				     tkzs.hsamp_.crlIdx(bid.crl()), 0,
				     bivset->getVals(pos)[idx] );
	}
    }

    regsdp->setZDomain( zinfo );
    DPM(DataPackMgr::SeisID()).add( regsdp );
    return regsdp->id();
}


// RandomSeisDataPack
RandomSeisDataPack::RandomSeisDataPack( const char* cat,
					const BinDataDesc* bdd )
    : SeisVolumeDataPack(cat,bdd)
    , rdlid_(-1)
{
}


RandomSeisDataPack* RandomSeisDataPack::getSimilar() const
{
    RandomSeisDataPack* ret = new RandomSeisDataPack( category(), &desc_ );
    ret->setRandomLineID( rdlid_ );
    ret->setPath( path_ );
    ret->setZRange( zsamp_ );
    return ret;
}


TrcKey RandomSeisDataPack::getTrcKey( int trcidx ) const
{
    return path_.validIdx(trcidx) ? path_[trcidx] : TrcKey::udf();
}


bool RandomSeisDataPack::addComponent( const char* nm )
{
    if ( path_.isEmpty() || zsamp_.isUdf() )
	return false;

    if ( !addArray(1,nrTrcs(),zsamp_.nrSteps()+1) )
	return false;

    componentnames_.add( nm );
    return true;
}


int RandomSeisDataPack::getGlobalIdx(const TrcKey& tk) const
{
    return path_.indexOf(tk);
}


void RandomSeisDataPack::setRandomLineID( int rdlid )
{
    ConstRefMan<Geometry::RandomLine> rdmline = Geometry::RLM().get( rdlid );
    if ( !rdmline )
	return;

    TypeSet<BinID> knots, rdlpath;
    rdmline->allNodePositions( knots );
    Geometry::RandomLine::getPathBids( knots, rdmline->getSurvID(), rdlpath );
    path_.setSize( rdlpath.size(), TrcKey::udf() );
    for ( int idx=0; idx<rdlpath.size(); idx++ )
	path_[idx] = TrcKey( rdlpath[idx] );
    rdlid_ = rdlid;
}


DataPack::ID RandomSeisDataPack::createDataPackFrom(
					const RegularSeisDataPack& regsdp,
					int rdmlineid,
					const Interval<float>& zrange,
					const BufferStringSet* compnames )
{
    ConstRefMan<Geometry::RandomLine> rdmline = Geometry::RLM().get(rdmlineid);
    if ( !rdmline || regsdp.isEmpty() )
	return DataPack::cNoID();

    RandomSeisDataPack* randsdp = new RandomSeisDataPack(
	    VolumeDataPack::categoryStr(true,false), &regsdp.getDataDesc() );
    randsdp->setRandomLineID( rdmlineid );
    if ( regsdp.getScaler() )
	randsdp->setScaler( *regsdp.getScaler() );

    TrcKeyPath& path = randsdp->getPath();
    const TrcKeySampling& tks = regsdp.sampling().hsamp_;

    // Remove outer undefined traces at both sides
    int pathidx = path.size()-1;
    while ( pathidx>0 && !tks.includes(path[pathidx]) )
	path.removeSingle( pathidx-- );

    while ( path.size()>1 && !tks.includes(path[0]) )
	path.removeSingle( 0 );

    // Auxiliary TrcKeyZSampling to limit z-range and if no overlap at all,
    // preserve one dummy voxel for displaying the proper undefined color.
    TrcKeyZSampling auxtkzs;
    auxtkzs.hsamp_.start_ = path.first().binID();
    auxtkzs.hsamp_.stop_ = path.last().binID();
    auxtkzs.zsamp_ = zrange;
    if ( !auxtkzs.adjustTo(regsdp.sampling(),true) && path.size()>1 )
	 path.removeRange( 1, path.size()-1 );

    randsdp->setZRange( auxtkzs.zsamp_ );

    const int nrcomps = compnames ? compnames->size() : regsdp.nrComponents();
    for ( int idx=0; idx<nrcomps; idx++ )
    {
	const char* compnm = compnames ? compnames->get(idx).buf()
				       : regsdp.getComponentName(idx);

	if ( regsdp.getComponentIdx(compnm,idx) >= 0 )
	{
	    randsdp->addComponent( compnm );
	    Regular2RandomDataCopier copier( *randsdp, regsdp,
					     randsdp->nrComponents()-1 );
	    copier.execute();
	}
    }

    randsdp->setZDomain( regsdp.zDomain() );
    randsdp->setName( regsdp.name() );
    DPM(DataPackMgr::SeisID()).add( randsdp );
    return randsdp->id();
}


#define mKeyInl		SeisTrcInfo::toString(SeisTrcInfo::BinIDInl)
#define mKeyCrl		SeisTrcInfo::toString(SeisTrcInfo::BinIDCrl)
#define mKeyCoordX	SeisTrcInfo::toString(SeisTrcInfo::CoordX)
#define mKeyCoordY	SeisTrcInfo::toString(SeisTrcInfo::CoordY)
#define mKeyTrcNr	SeisTrcInfo::toString(SeisTrcInfo::TrcNr)
#define mKeyRefNr	SeisTrcInfo::toString(SeisTrcInfo::RefNr)

// SeisFlatDataPack
SeisFlatDataPack::SeisFlatDataPack( const SeisVolumeDataPack& source, int comp )
    : FlatDataPack(source.category())
    , source_(source)
    , comp_(comp)
    , zsamp_(source.getZRange())
    , rdlid_(source.getRandomLineID())
{
    source_.ref();
    DPM(DataPackMgr::SeisID()).add(const_cast<SeisVolumeDataPack*>(&source));
    setName( source_.getComponentName(comp_) );
}


SeisFlatDataPack::~SeisFlatDataPack()
{
    source_.unRef();
}


bool SeisFlatDataPack::dimValuesInInt( const char* keystr ) const
{
    const FixedString key( keystr );
    return key==mKeyInl || key==mKeyCrl || key==mKeyTrcNr ||
	   key==sKey::Series();
}


void SeisFlatDataPack::getAltDim0Keys( BufferStringSet& keys ) const
{
    if ( !isVertical() )
	return;

    for ( int idx=0; idx<tiflds_.size(); idx++ )
	keys.add( SeisTrcInfo::toString(tiflds_[idx]) );
}


double SeisFlatDataPack::getAltDim0Value( int ikey, int i0 ) const
{
    if ( !tiflds_.validIdx(ikey) )
	return posdata_.position( true, i0 );

    switch ( tiflds_[ikey] )
    {
	case SeisTrcInfo::BinIDInl:	return SI().transform(
						getCoord(i0,0).getXY()).inl();
	case SeisTrcInfo::BinIDCrl:	return SI().transform(
						getCoord(i0,0).getXY()).crl();
	case SeisTrcInfo::CoordX:	return getCoord(i0,0).x_;
	case SeisTrcInfo::CoordY:	return getCoord(i0,0).y_;
	case SeisTrcInfo::TrcNr:	return getPath()[i0].trcNr();
	case SeisTrcInfo::RefNr:	return source_.getRefNr(i0);
	default:			return posdata_.position(true,i0);
    }
}


void SeisFlatDataPack::getAuxInfo( int i0, int i1, IOPar& iop ) const
{
    const Coord3 crd = getCoord( i0, i1 );
    iop.set( mKeyCoordX, crd.x_ );
    iop.set( mKeyCoordY, crd.y_ );
    iop.set( sKey::ZCoord(), crd.z_ * zDomain().userFactor() );

    if ( is2D() )
    {
	const int trcidx = nrTrcs()==1 ? 0 : i0;
	const TrcKey& tk = getTrcKey( trcidx );
	iop.set( mKeyTrcNr, tk.trcNr() );
	iop.set( mKeyRefNr, source_.getRefNr(trcidx) );
    }
    else
    {
	const BinID bid = SI().transform( crd.getXY() );
	iop.set( mKeyInl, bid.inl() );
	iop.set( mKeyCrl, bid.crl() );
    }
}


float SeisFlatDataPack::nrKBytes() const
{
    return source_.nrKBytes() / source_.nrComponents();
}


#define mStepIntvD( rg ) \
    StepInterval<double>( rg.start, rg.stop, rg.step )

void SeisFlatDataPack::setPosData()
{
    const TrcKeyPath& path = getPath();
    const int nrtrcs = path.size();
    float* pos = new float[nrtrcs];
    pos[0] = 0;

    TrcKey prevtk = path[0];
    for ( int idx=1; idx<nrtrcs; idx++ )
    {
	const TrcKey& trckey = path[idx];
	if ( trckey.isUdf() )
	    pos[idx] = mCast(float,(pos[idx-1]));
	else
	{
	    const float dist = prevtk.distTo( trckey );
	    if ( mIsUdf(dist) )
		pos[idx] = pos[idx-1];
	    else
	    {
		pos[idx] = pos[idx-1] + dist;
		prevtk = trckey;
	    }
	}
    }

    posData().setX1Pos( pos, nrtrcs, 0 );
    posData().setRange( false, mStepIntvD(zsamp_) );
}



#define mIsStraight ((getTrcKey(0).distTo(getTrcKey(nrTrcs()-1))/ \
	posdata_.position(true,nrTrcs()-1))>0.99)

RegularFlatDataPack::RegularFlatDataPack(
		const RegularSeisDataPack& source, int comp )
    : SeisFlatDataPack(source,comp)
    , sampling_(source.sampling())
    , dir_(sampling_.defaultDir())
    , usemulticomps_(comp_==-1)
    , hassingletrace_(nrTrcs()==1)
{
    if ( usemulticomps_ )
	setSourceDataFromMultiCubes();
    else
	setSourceData();
}


Coord3 RegularFlatDataPack::getCoord( int i0, int i1 ) const
{
    const bool isvertical = dir_ != TrcKeyZSampling::Z;
    const int trcidx = isvertical ? (hassingletrace_ ? 0 : i0)
				  : i0*sampling_.nrTrcs()+i1;
    const Coord c = Survey::GM().toCoord( getTrcKey(trcidx) );
    return Coord3( c.x_, c.y_, sampling_.zsamp_.atIndex(isvertical ? i1 : 0) );
}


void RegularFlatDataPack::setTrcInfoFlds()
{
    if ( hassingletrace_ )
	{ pErrMsg( "Trace info fields set for single trace display." ); return;}

    if ( is2D() )
    {
	tiflds_ += SeisTrcInfo::TrcNr;
	tiflds_ += SeisTrcInfo::RefNr;
    }
    else
    {
	if ( dir_ == TrcKeyZSampling::Crl )
	    tiflds_ += SeisTrcInfo::BinIDInl;
	else if ( dir_ == TrcKeyZSampling::Inl )
	    tiflds_ += SeisTrcInfo::BinIDCrl;
    }

    if ( is2D() && !mIsStraight )
	return;

    tiflds_ += SeisTrcInfo::CoordX;
    tiflds_ += SeisTrcInfo::CoordY;
}


const char* RegularFlatDataPack::dimName( bool dim0 ) const
{
    if ( dim0 && hassingletrace_ ) return sKey::Series();
    if ( is2D() ) return dim0 ? "Distance"
			      : zDomain().userName().getFullString();
    return dim0 ? (dir_==TrcKeyZSampling::Inl ? mKeyCrl : mKeyInl)
		: (dir_==TrcKeyZSampling::Z
			? mKeyCrl : zDomain().userName().getFullString());
}


void RegularFlatDataPack::setSourceDataFromMultiCubes()
{
    const int nrcomps = source_.nrComponents();
    const int nrz = sampling_.zsamp_.nrSteps() + 1;
    posdata_.setRange( true, StepInterval<double>(0,nrcomps-1,1) );
    posdata_.setRange( false, mStepIntvD(sampling_.zsamp_) );

    arr2d_ = new Array2DImpl<float>( nrcomps, nrz );
    for ( int idx=0; idx<nrcomps; idx++ )
	for ( int idy=0; idy<nrz; idy++ )
	    arr2d_->set( idx, idy, source_.data(idx).get(0,0,idy) );
}


void RegularFlatDataPack::setSourceData()
{
    const bool isz = dir_==TrcKeyZSampling::Z;
    if ( !isz )
    {
	path_.setCapacity( source_.nrTrcs(), false );
	for ( int idx=0; idx<source_.nrTrcs(); idx++ )
	    path_ += source_.getTrcKey( idx );
    }

    if ( !is2D() )
    {
	const bool isinl = dir_==TrcKeyZSampling::Inl;
	posdata_.setRange( true, isinl ? mStepIntvD(sampling_.hsamp_.crlRange())
				: mStepIntvD(sampling_.hsamp_.inlRange()) );
	posdata_.setRange( false, isz ? mStepIntvD(sampling_.hsamp_.crlRange())
				      : mStepIntvD(sampling_.zsamp_) );
    }
    else
	setPosData();

    const int dim0 = dir_==TrcKeyZSampling::Inl ? 1 : 0;
    const int dim1 = dir_==TrcKeyZSampling::Z ? 1 : 2;
    Array2DSlice<float>* slice2d = new Array2DSlice<float>(source_.data(comp_));
    slice2d->setDimMap( 0, dim0 );
    slice2d->setDimMap( 1, dim1 );
    slice2d->setPos( dir_, 0 );
    slice2d->init();
    arr2d_ = slice2d;
    setTrcInfoFlds();
}



RandomFlatDataPack::RandomFlatDataPack(
		const RandomSeisDataPack& source, int comp )
    : SeisFlatDataPack(source,comp)
    , path_(source.getPath())
{
    setSourceData();
}


Coord3 RandomFlatDataPack::getCoord( int i0, int i1 ) const
{
    const Coord coord = path_.validIdx(i0) ? Survey::GM().toCoord(path_[i0])
					   : Coord::udf();
    return Coord3( coord, zsamp_.atIndex(i1) );
}


void RandomFlatDataPack::setTrcInfoFlds()
{
    if ( !mIsStraight )
	return;

    tiflds_ +=	SeisTrcInfo::BinIDInl;
    tiflds_ +=	SeisTrcInfo::BinIDCrl;
    tiflds_ +=	SeisTrcInfo::CoordX;
    tiflds_ +=	SeisTrcInfo::CoordY;
}


void RandomFlatDataPack::setSourceData()
{
    setPosData();
    Array2DSlice<float>* slice2d = new Array2DSlice<float>(source_.data(comp_));
    slice2d->setDimMap( 0, 1 );
    slice2d->setDimMap( 1, 2 );
    slice2d->setPos( 0, 0 );
    slice2d->init();
    arr2d_ = slice2d;
    setTrcInfoFlds();
}
