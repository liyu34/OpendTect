/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bruno
 Date:          July 2011
________________________________________________________________________

-*/


#include "stratsynth.h"

#include "attribengman.h"
#include "attribdesc.h"
#include "attribdescset.h"
#include "attribparam.h"
#include "attribprocessor.h"
#include "attribfactory.h"
#include "attribstorprovider.h"
#include "binidvalset.h"
#include "envvars.h"
#include "fftfilter.h"
#include "prestackattrib.h"
#include "prestackanglecomputer.h"
#include "prestacksyntheticdata.h"
#include "raytracerrunner.h"
#include "seisbufadapters.h"
#include "seistrc.h"
#include "seistrcprop.h"
#include "statruncalc.h"
#include "stratlayermodel.h"
#include "stratlayersequence.h"
#include "stratsynthlevel.h"
#include "syntheticdataimpl.h"
#include "unitofmeasure.h"
#include "timeser.h"
#include "waveletmanager.h"

StratSynth::StratSynth( const Strat::LayerModelProvider& lmp, bool useed )
    : lmp_(lmp)
    , useed_(useed)
    , level_(0)
    , taskr_(0)
    , wvlt_(0)
    , lastsyntheticid_(0)
    , swaveinfomsgshown_(0)
{
}


StratSynth::~StratSynth()
{
    deepErase( synthetics_ );
    setLevel( 0 );
    if ( wvlt_ )
	wvlt_->unRef();
}


const Strat::LayerModel& StratSynth::layMod() const
{
    return lmp_.getEdited( useed_ );
}


void StratSynth::setWavelet( const Wavelet* wvlt )
{
    if ( !wvlt )
	return;

    if ( wvlt_ )
	wvlt_->unRef();
    wvlt_ = wvlt;
    wvlt_->ref();
    genparams_.wvltnm_ = wvlt_->name();
}


void StratSynth::clearSynthetics()
{
    deepErase( synthetics_ );
}


#define mErrRet( msg, act )\
{\
    errmsg_ = sErrRetMsg().arg( synthgenpar.name_ ).arg( msg ); \
    act;\
}


bool StratSynth::canRayModelsBeRemoved( const IOPar& sdraypar ) const
{
    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	SynthGenParams sdsgp;
	synthetics_[idx]->fillGenParams( sdsgp );
	const bool ispsbased = sdsgp.synthtype_==SynthGenParams::AngleStack ||
			       sdsgp.synthtype_==SynthGenParams::AVOGradient;
	if ( !ispsbased &&
	     synthetics_[idx]->haveSameRM(sdraypar,sdsgp.raypars_) )
	    return false;
    }

    return true;
}


const PreStack::GatherSetDataPack* StratSynth::getRelevantAngleData(
					const IOPar& sdraypar ) const
{
    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	const SyntheticData* sd = synthetics_[idx];
	mDynamicCastGet(const PreStack::PreStackSyntheticData*,presd,sd);
	if ( !presd ) continue;
	SynthGenParams sgp;
	sd->fillGenParams( sgp );
	if ( sd->haveSameRM(sgp.raypars_,sdraypar) )
	    return &presd->angleData();
    }
    return 0;
}


bool StratSynth::disableSynthetic( const char* nm )
{
    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	SyntheticData* sd = synthetics_[idx];
	if ( sd->name() != nm )
	    continue;

	SynthGenParams sgp;
	sd->fillGenParams( sgp );
	if ( sgp.isPSBased() )
	{
	    sgp.inpsynthnm_ = SynthGenParams::sKeyInvalidInputPS();
	    sd->useGenParams( sgp );
	    return true;
	}
    }

    return false;
}


bool StratSynth::removeSynthetic( const char* nm )
{
    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	if ( synthetics_[idx]->name() == nm )
	{
	    SyntheticData* sd = synthetics_.removeSingle( idx );
	    SynthGenParams sgp;
	    sd->fillGenParams( sgp );
	    if ( canRayModelsBeRemoved(sgp.raypars_) )
	    {
		RayModelSet* rms = sd->raymodels_;
		deepErase( *rms );
	    }

	    return true;
	}
    }

    return false;
}


SyntheticData* StratSynth::addSynthetic()
{
    SyntheticData* sd = generateSD();
    if ( sd )
    {
	int propidx = 0;
	while ( propidx<synthetics_.size() )
	{
	    if ( synthetics_[propidx]->synthType() ==
		 SynthGenParams::StratProp )
		break;
	    propidx++;
	}
	synthetics_.insertAt( sd, propidx );
    }

    return sd;
}


SyntheticData* StratSynth::addSynthetic( const SynthGenParams& synthgen )
{
    SyntheticData* sd = generateSD( synthgen );
    if ( sd )
	synthetics_ += sd;
    return sd;
}



SyntheticData* StratSynth::replaceSynthetic( int id )
{
    SyntheticData* sd = getSynthetic( id );
    if ( !sd ) return 0;

    const int sdidx = synthetics_.indexOf( sd );
    sd = generateSD();
    if ( sd )
    {
	sd->setName( synthetics_[sdidx]->name() );
	delete synthetics_.replace( sdidx, sd );
    }

    return sd;
}


SyntheticData* StratSynth::addDefaultSynthetic()
{
    genparams_.synthtype_ = SynthGenParams::ZeroOffset;
    genparams_.createName( genparams_.name_ );
    SyntheticData* sd = addSynthetic();
    return sd;
}


int StratSynth::syntheticIdx( const char* nm ) const
{
    for ( int idx=0; idx<synthetics().size(); idx ++ )
    {
	if( synthetics_[idx]->name() == nm )
	    return idx;
    }
    return 0;
}


SyntheticData* StratSynth::getSynthetic( const char* nm )
{
    for ( int idx=0; idx<synthetics().size(); idx ++ )
    {
	if ( synthetics_[idx]->name() == nm )
	    return synthetics_[idx];
    }
    return 0;
}


SyntheticData* StratSynth::getSynthetic( int id )
{
    for ( int idx=0; idx<synthetics().size(); idx ++ )
    {
	if ( synthetics_[idx]->id_ == id )
	    return synthetics_[ idx ];
    }
    return 0;
}


SyntheticData* StratSynth::getSyntheticByIdx( int idx )
{
    return synthetics_.validIdx( idx ) ?  synthetics_[idx] : 0;
}


const SyntheticData* StratSynth::getSyntheticByIdx( int idx ) const
{
    return synthetics_.validIdx( idx ) ?  synthetics_[idx] : 0;
}


int StratSynth::syntheticIdx( const PropertyRef& pr ) const
{
    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	mDynamicCastGet(const StratPropSyntheticData*,pssd,synthetics_[idx]);
	if ( !pssd ) continue;
	if ( pr == pssd->propRef() )
	    return idx;
    }
    return 0;
}


void StratSynth::getSyntheticNames( BufferStringSet& nms, bool wantprest ) const
{
    nms.erase();
    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	if ( synthetics_[idx]->isPS()==wantprest )
	    nms.add( synthetics_[idx]->name() );
    }
}


void StratSynth::getSyntheticNames( BufferStringSet& nms,
				    SynthGenParams::SynthType synthtype ) const
{
    nms.erase();
    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	const SyntheticData* sd = synthetics_[idx];
	if ( sd->synthType()==synthtype )
	    nms.add( sd->name() );
    }
}


SyntheticData* StratSynth::getSynthetic( const	PropertyRef& pr )
{
    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	mDynamicCastGet(StratPropSyntheticData*,pssd,synthetics_[idx]);
	if ( !pssd ) continue;
	if ( pr == pssd->propRef() )
	    return pssd;
    }
    return 0;
}


int StratSynth::nrSynthetics() const
{
    return synthetics_.size();
}


SyntheticData* StratSynth::generateSD()
{ return generateSD( genparams_ ); }


#define mSetBool( str, newval ) \
{ \
    mDynamicCastGet(Attrib::BoolParam*,param,psdesc->getValParam(str)) \
    param->setValue( newval ); \
}


#define mSetEnum( str, newval ) \
{ \
    mDynamicCastGet(Attrib::EnumParam*,param,psdesc->getValParam(str)) \
    param->setValue( newval ); \
}

#define mSetFloat( str, newval ) \
{ \
    Attrib::ValParam* param  = psdesc->getValParam( str ); \
    param->setValue( newval ); \
}


#define mSetString( str, newval ) \
{ \
    Attrib::ValParam* param = psdesc->getValParam( str ); \
    param->setValue( newval ); \
}


SyntheticData* StratSynth::createSynthData( const SyntheticData& sd,
					    const TrcKeyZSampling& cs,
					    const SynthGenParams& synthgenpar,
					    bool isanglestack )
{
    if ( !sd.isPS() )
	return 0;

    mDynamicCastGet(const PreStack::PreStackSyntheticData&,presd,sd);
    const PreStack::GatherSetDataPack& gdp = presd.preStackPack();
    DataPack::FullID dpfid( DataPackMgr::SeisID(), gdp.id() );
    const BufferString dpidstring( "#", dpfid.toString() );
    Attrib::Desc* psdesc =
	Attrib::PF().createDescCopy(Attrib::PSAttrib::attribName());
    mSetString( Attrib::StorageProvider::keyStr(), dpidstring.buf() );

    if ( isanglestack )
    {
	mSetEnum(Attrib::PSAttrib::calctypeStr(),PreStack::PropCalc::Stats);
	mSetEnum(Attrib::PSAttrib::stattypeStr(), Stats::Average );
    }
    else
    {
	mSetEnum(Attrib::PSAttrib::calctypeStr(),PreStack::PropCalc::LLSQ);
	mSetEnum(Attrib::PSAttrib::offsaxisStr(),PreStack::PropCalc::Sinsq);
	mSetEnum(Attrib::PSAttrib::lsqtypeStr(), PreStack::PropCalc::Coeff );
    }

    mSetBool(Attrib::PSAttrib::useangleStr(), true );
    mSetFloat(Attrib::PSAttrib::offStartStr(), synthgenpar.anglerg_.start );
    mSetFloat(Attrib::PSAttrib::offStopStr(), synthgenpar.anglerg_.stop );
    mSetFloat(Attrib::PSAttrib::gathertypeStr(), Attrib::PSAttrib::Ang );
    mSetFloat(Attrib::PSAttrib::xaxisunitStr(), Attrib::PSAttrib::Deg );
    mSetFloat(Attrib::PSAttrib::angleDPIDStr(), presd.angleData().id().getI() );
    psdesc->setUserRef( synthgenpar.name_ );
    psdesc->updateParams();
    PtrMan<Attrib::DescSet> descset = new Attrib::DescSet( false );
    if ( !descset ) return 0;
    Attrib::DescID attribid = descset->addDesc( psdesc );
    PtrMan<Attrib::EngineMan> aem = new Attrib::EngineMan;
    TypeSet<Attrib::SelSpec> attribspecs;
    Attrib::SelSpec sp( 0, attribid );
    sp.set( *psdesc );
    attribspecs += sp;
    aem->setAttribSet( descset );
    aem->setAttribSpecs( attribspecs );
    aem->setTrcKeyZSampling( cs );
    BinIDValueSet bidvals( 0, false );
    const ObjectSet<PreStack::Gather>& gathers = gdp.getGathers();
    for ( int idx=0; idx<gathers.size(); idx++ )
	bidvals.add( gathers[idx]->getBinID() );
    SeisTrcBuf* dptrcbufs = new SeisTrcBuf( true );
    Interval<float> zrg( cs.zsamp_ );
    uiString errmsg;
    PtrMan<Attrib::Processor> proc =
	aem->createTrcSelOutput( errmsg, bidvals, *dptrcbufs, 0, &zrg);
    if ( !proc || !proc->getProvider() )
	mErrRet( errmsg, return 0 ) ;
    proc->getProvider()->setDesiredVolume( cs );
    proc->getProvider()->setPossibleVolume( cs );
    mDynamicCastGet(Attrib::PSAttrib*,psattr,proc->getProvider());
    if ( !psattr )
	mErrRet( proc->message(), return 0 );

    if ( !TaskRunner::execute(taskr_,*proc) )
	mErrRet( proc->message(), return 0 ) ;
    SeisTrcBufDataPack* angledp =
	new SeisTrcBufDataPack( dptrcbufs, Seis::Line,
				SeisTrcInfo::TrcNr, synthgenpar.name_ );

    if ( isanglestack )
	return new AngleStackSyntheticData( synthgenpar, *angledp );
    else
	return new AVOGradSyntheticData( synthgenpar, *angledp );
}


SyntheticData* StratSynth::createAVOGradient( const SyntheticData& sd,
					     const TrcKeyZSampling& cs,
					     const SynthGenParams& synthgenpar )
{
    return createSynthData( sd, cs, synthgenpar, false );
}


SyntheticData* StratSynth::createAngleStack( const SyntheticData& sd,
					     const TrcKeyZSampling& cs,
					     const SynthGenParams& synthgenpar )
{
    return createSynthData( sd, cs, synthgenpar, true );
}


class ElasticModelCreator : public ParallelTask
{ mODTextTranslationClass(ElasticModelCreator);
public:
ElasticModelCreator( const Strat::LayerModel& lm, TypeSet<ElasticModel>& ems )
    : ParallelTask( "Elastic Model Generator" )
    , lm_(lm)
    , aimodels_(ems)
{
    aimodels_.setSize( lm_.size(), ElasticModel() );
}


uiString message() const
{
    if ( errmsg_.isEmpty() )
	return tr("Generating elastic model");
    else
	return errmsg_;
}

uiString nrDoneText() const	{ return tr("Models done"); }

protected :

bool doWork( od_int64 start, od_int64 stop, int threadid )
{
    for ( int idm=(int) start; idm<=stop; idm++ )
    {
	addToNrDone(1);
	ElasticModel& curem = aimodels_[idm];
	const Strat::LayerSequence& seq = lm_.sequence( idm );
	if ( seq.isEmpty() )
	    continue;

	if ( !fillElasticModel(seq,curem) )
	    return false;
    }

    return true;
}

bool fillElasticModel( const Strat::LayerSequence& seq, ElasticModel& aimodel )
{
    const ElasticPropSelection& eps = lm_.elasticPropSel();
    const PropertyRefSelection& props = lm_.propertyRefs();

    aimodel.erase();
    uiString errmsg;
    if ( !eps.isValidInput(&errmsg) )
    {
	mutex_.lock();
	errmsg_ = tr("Cannot create elastic model as %1").arg(errmsg);
	mutex_.unLock();
	return false;
    }

    ElasticPropGen elpgen( eps, props );
    const float srddepth = -1.f*mCast(float,SI().seismicReferenceDatum() );
    int firstidx = 0;
    if ( seq.startDepth() < srddepth )
	firstidx = seq.nearestLayerIdxAtZ( srddepth );

    if ( seq.isEmpty() )
    {
	mutex_.lock();
	errmsg_ = tr("Elastic model is not proper to generate synthetics as a "
		  "layer sequence has no layers");
	mutex_.unLock();
	return false;
    }

    for ( int idx=firstidx; idx<seq.size(); idx++ )
    {
	const Strat::Layer* lay = seq.layers()[idx];
	float thickness = lay->thickness();
	if ( idx == firstidx )
	    thickness -= srddepth - lay->zTop();
	if ( thickness < 1e-4 )
	    continue;

	float dval =mUdf(float), pval = mUdf(float), sval = mUdf(float);
	TypeSet<float> layervals; lay->getValues( layervals );
	elpgen.getVals( dval, pval, sval, layervals.arr(), layervals.size() );

	// Detect water - reset Vs
	/* TODO disabled for now
	if ( pval < cMaximumVpWaterVel() )
	    sval = 0;*/

	aimodel += ElasticLayer( thickness, pval, sval, dval );
    }

    if ( aimodel.isEmpty() )
    {
	mutex_.lock();
	errmsg_ = tr("After discarding layers with no thickness "
		     "no layers remained");
	mutex_.unLock();
	return false;
    }

    return true;
}

static float cMaximumVpWaterVel()
{ return 1510.f; }

od_int64 nrIterations() const
{ return lm_.size(); }

const Strat::LayerModel&	lm_;
TypeSet<ElasticModel>&		aimodels_;
Threads::Mutex			mutex_;
uiString			errmsg_;

};


bool StratSynth::createElasticModels()
{
    clearElasticModels();

    if ( layMod().isEmpty() )
	return false;

    ElasticModelCreator emcr( layMod(), aimodels_ );
    if ( !TaskRunner::execute(taskr_,emcr) )
	return false;
    bool modelsvalid = false;
    for ( int idx=0; idx<aimodels_.size(); idx++ )
    {
	if ( !aimodels_[idx].isEmpty() )
	{
	    modelsvalid = true;
	    break;
	}
    }

    errmsg_.setEmpty();
    if ( !modelsvalid )
	return false;

    return adjustElasticModel( layMod(), aimodels_, useed_ );
}


class PSAngleDataCreator : public Executor
{ mODTextTranslationClass(PSAngleDataCreator)
public:

PSAngleDataCreator( const PreStack::PreStackSyntheticData& pssd,
		    const ObjectSet<RayTracer1D>& rts )
    : Executor("Creating Angle Gather" )
    , gathers_(pssd.preStackPack().getGathers())
    , rts_(rts)
    , nrdone_(0)
    , pssd_(pssd)
    , anglecomputer_(new PreStack::ModelBasedAngleComputer)
{
    anglecomputer_->setFFTSmoother( 10.f, 15.f );
}

~PSAngleDataCreator()
{
    delete anglecomputer_;
}

od_int64 totalNr() const
{ return rts_.size(); }

od_int64 nrDone() const
{ return nrdone_; }

uiString message() const
{
    return tr("Calculating Angle Gathers");
}

uiString nrDoneText() const
{
    return tr( "Models done" );
}

ObjectSet<PreStack::Gather>& angleGathers()	{ return anglegathers_; }

protected:

void convertAngleDataToDegrees( PreStack::Gather* ag ) const
{
    Array2D<float>& agdata = ag->data();
    const int dim0sz = agdata.info().getSize(0);
    const int dim1sz = agdata.info().getSize(1);
    for ( int idx=0; idx<dim0sz; idx++ )
    {
	for ( int idy=0; idy<dim1sz; idy++ )
	{
	    const float radval = agdata.get( idx, idy );
	    if ( mIsUdf(radval) ) continue;
	    const float dval =	Math::toDegrees( radval );
	    agdata.set( idx, idy, dval );
	}
    }
}


int nextStep()
{
    if ( !gathers_.validIdx(nrdone_) )
	return Finished();
    const PreStack::Gather* gather = gathers_[(int)nrdone_];
    anglecomputer_->setOutputSampling( gather->posData() );
    const TrcKey trckey( gather->getBinID() );
    anglecomputer_->setRayTracer( rts_[(int)nrdone_], trckey );
    anglecomputer_->setTrcKey( trckey );
    RefMan<PreStack::Gather> anglegather = anglecomputer_->computeAngles();
    convertAngleDataToDegrees( anglegather );
    TypeSet<float> azimuths;
    gather->getAzimuths( azimuths );
    anglegather->setAzimuths( azimuths );
    const BufferString angledpnm( pssd_.name(), "(Angle Gather)" );
    anglegather->setName( angledpnm );
    anglegather->setBinID( gather->getBinID() );
    anglegathers_ += anglegather;
    DPM(DataPackMgr::FlatID()).add( anglegather );
    nrdone_++;
    return MoreToDo();
}

    const ObjectSet<RayTracer1D>&		rts_;
    const RefObjectSet<PreStack::Gather>	gathers_;
    const PreStack::PreStackSyntheticData&		pssd_;
    RefObjectSet<PreStack::Gather>		anglegathers_;
    PreStack::ModelBasedAngleComputer*		anglecomputer_;
    od_int64					nrdone_;

};


void StratSynth::createAngleData( PreStack::PreStackSyntheticData& pssd,
				  const ObjectSet<RayTracer1D>& rts )
{
    PSAngleDataCreator angledatacr( pssd, rts );
    TaskRunner::execute( taskr_, angledatacr );
    pssd.setAngleData( angledatacr.angleGathers() );
}



bool StratSynth::runSynthGen( RaySynthGenerator& synthgen,
			      const SynthGenParams& synthgenpar )
{
    BufferString capt( "Generating ", synthgenpar.name_ );
    synthgen.setName( capt.buf() );
    const IOPar& raypars = synthgenpar.raypars_;
    synthgen.usePar( raypars );
    bool needsetwvlt = synthgenpar.wvltnm_.isEmpty();
    if ( !needsetwvlt )
    {
	const DBKey ky = WaveletMGR().getIDByName( synthgenpar.wvltnm_ );
	synthgen.setWavelet( WaveletMGR().fetch(ky) );
    }

    synthgen.enableFourierDomain( !GetEnvVarYN("DTECT_CONVOLVE_USETIME") );
    return TaskRunner::execute( taskr_, synthgen );
}


SyntheticData* StratSynth::generateSD( const SynthGenParams& synthgenpar )
{
    errmsg_.setEmpty();

    if ( layMod().isEmpty() )
    {
	errmsg_ = tr("Empty layer model.");
	return 0;
    }

    const bool ispsbased =
	synthgenpar.synthtype_ == SynthGenParams::AngleStack ||
	synthgenpar.synthtype_ == SynthGenParams::AVOGradient;

    if ( synthgenpar.synthtype_ == SynthGenParams::PreStack &&
	 !swaveinfomsgshown_ )
    {
	if ( !adjustElasticModel(layMod(),aimodels_,true) )
	    return 0;
    }

    SyntheticData* sd = 0;
    for ( int sdidx=0; sdidx<synthetics_.size(); sdidx++ )
    {
	bool samerm = synthetics_[sdidx]->haveSameRM(
						synthetics_[sdidx]->getRayPar(),
						synthgenpar.raypars_ );
	if ( samerm )
	{
	    sd = synthetics_[sdidx];
	    break;
	}
    }

    PtrMan<RaySynthGenerator> synthgen = 0;
    bool hasrms = sd && sd->raymodels_->size();
    if ( hasrms )
	synthgen = new RaySynthGenerator( sd, false ); //New constructor
    else
	synthgen = new RaySynthGenerator( &aimodels_, synthgenpar );
    if ( !ispsbased )
    {
	if ( !runSynthGen(*synthgen,synthgenpar) )
	    return 0;
    }

    sd = synthgen->getSyntheticData();
    if ( synthgenpar.synthtype_ == SynthGenParams::PreStack || ispsbased )
    {
	if ( !ispsbased )
	{
	    mDynamicCastGet(PreStack::PreStackSyntheticData*,presd,sd);
	    if ( hasrms )
	    {
		const PreStack::GatherSetDataPack* anglegather =
		    getRelevantAngleData( synthgenpar.raypars_ );
		if ( anglegather )
		    presd->setAngleData( anglegather->getGathers() );
	    }
	    else
		createAngleData( *presd, synthgen->rayTracers() );
	}
	else
	{
	    BufferString inputsdnm( synthgenpar.inpsynthnm_ );
	    if ( useed_ )
		inputsdnm += sKeyFRNameSuffix();
	    sd = getSynthetic( inputsdnm );
	    if ( !sd )
		mErrRet( tr(" input prestack synthetic data not found."),
			 return 0 )
	    CubeSampling cs( false );
	    cs.hsamp_.survid_ = TrcKey::stdSynthSurvID();
	    for ( int idx=0; idx<sd->zerooffsd2tmodels_.size(); idx++ )
	    {
		const SeisTrc* trc = sd->getTrace( idx );
		const SamplingData<float>& trcsd = trc->info().sampling_;
		if ( !idx )
		{
		    cs.zsamp_.start = trcsd.start;
		    cs.zsamp_.stop = trcsd.atIndex( trc->size()-1 );
		    cs.zsamp_.step = trcsd.step;
		    continue;
		}

		cs.zsamp_.include( trcsd.start, false );
		cs.zsamp_.include( trcsd.atIndex(trc->size()-1), false );
	    }

	    if ( synthgenpar.synthtype_ == SynthGenParams::AngleStack )
		sd = createAngleStack( *sd, cs, synthgenpar );
	    else if ( synthgenpar.synthtype_ == SynthGenParams::AVOGradient )
		sd = createAVOGradient( *sd, cs, synthgenpar );
	}
    }

    if ( !sd )
	return 0;

    if ( useed_ )
    {
	BufferString sdnm = sd->name();
	sdnm += sKeyFRNameSuffix();
	sd->setName( sdnm );
    }

    sd->id_ = ++lastsyntheticid_;

    return sd;
}


void StratSynth::generateOtherQuantities()
{
    if ( synthetics_.isEmpty() ) return;

    for ( int idx=0; idx<synthetics_.size(); idx++ )
    {
	const SyntheticData* sd = synthetics_[idx];
	mDynamicCastGet(const PostStackSyntheticData*,pssd,sd);
	mDynamicCastGet(const StratPropSyntheticData*,prsd,sd);
	if ( !pssd || prsd ) continue;
	return generateOtherQuantities( *pssd, layMod() );
    }
}


mClass(WellAttrib) StratPropSyntheticDataCreator : public ParallelTask
{ mODTextTranslationClass(StratPropSyntheticDataCreator);

public:
StratPropSyntheticDataCreator( ObjectSet<SyntheticData>& synths,
		    const PostStackSyntheticData& sd,
		    const Strat::LayerModel& lm,
		    int& lastsynthid, bool useed )
    : ParallelTask( "Creating Synthetics for Properties" )
    , synthetics_(synths)
    , sd_(sd)
    , lm_(lm)
    , lastsyntheticid_(lastsynthid)
    , isprepared_(false)
    , useed_(useed)
{
}


od_int64 nrIterations() const
{ return lm_.size(); }


uiString message() const
{
    return !isprepared_ ? tr("Preparing Models") : tr("Calculating");
}

uiString nrDoneText() const
{
    return tr("Models done");
}


protected:

bool doPrepare( int nrthreads )
{
    const StepInterval<double>& zrg =
	sd_.postStackPack().posData().range( false );

    layermodels_.setEmpty();
    const int sz = zrg.nrSteps() + 1;
    for ( int idz=0; idz<sz; idz++ )
	layermodels_ += new Strat::LayerModel();
    const PropertyRefSelection& props = lm_.propertyRefs();
    for ( int iprop=1; iprop<props.size(); iprop++ )
    {
	SeisTrcBuf* trcbuf = new SeisTrcBuf( sd_.postStackPack().trcBuf() );
	SeisTrcBufDataPack* seisbuf =
	    new SeisTrcBufDataPack( trcbuf, Seis::Line, SeisTrcInfo::TrcNr,
				    props[iprop]->name() );
	seisbufdps_ += seisbuf;
    }

    for ( int iseq=0; iseq<lm_.size(); iseq++ )
    {
	addToNrDone( 1 );
	const Strat::LayerSequence& seq = lm_.sequence( iseq );
	const TimeDepthModel& t2d = *sd_.zerooffsd2tmodels_[iseq];
	const Interval<float> seqdepthrg = seq.zRange();
	const float seqstarttime = t2d.getTime( seqdepthrg.start );
	const float seqstoptime = t2d.getTime( seqdepthrg.stop );
	Interval<float> seqtimerg( seqstarttime, seqstoptime );
	for ( int idz=0; idz<sz; idz++ )
	{
	    Strat::LayerModel* lmsamp = layermodels_[idz];
	    if ( !lmsamp )
		continue;

	    lmsamp->addSequence();
	    Strat::LayerSequence& curseq = lmsamp->sequence( iseq );
	    const float time = mCast( float, zrg.atIndex(idz) );
	    if ( !seqtimerg.includes(time,false) )
		continue;

	    const float dptstart = t2d.getDepth( time - (float)zrg.step );
	    const float dptstop = t2d.getDepth( time + (float)zrg.step );
	    Interval<float> depthrg( dptstart, dptstop );
	    seq.getSequencePart( depthrg, true, curseq );
	}
    }

    isprepared_ = true;
    resetNrDone();
    return true;
}


bool doFinish( bool success )
{
    const PropertyRefSelection& props = lm_.propertyRefs();
    SynthGenParams sgp;
    sd_.fillGenParams( sgp );
    for ( int idx=0; idx<seisbufdps_.size(); idx++ )
    {
	SeisTrcBufDataPack* dp = seisbufdps_[idx];
	BufferString propnm = props[idx+1]->name();
	if ( useed_ )
	    propnm += StratSynth::sKeyFRNameSuffix();
	BufferString nm( "[", propnm, "]" );
	dp->setName( nm );
	StratPropSyntheticData* prsd =
		 new StratPropSyntheticData( sgp, *dp, *props[idx+1] );
	prsd->id_ = ++lastsyntheticid_;
	prsd->setName( nm );

	deepCopy( prsd->zerooffsd2tmodels_, sd_.zerooffsd2tmodels_ );
	synthetics_ += prsd;
    }

    return true;
}


bool doWork( od_int64 start, od_int64 stop, int threadid )
{
    const StepInterval<double>& zrg =
	sd_.postStackPack().posData().range( false );
    const int sz = layermodels_.size();
    const PropertyRefSelection& props = lm_.propertyRefs();
    for ( int iseq=mCast(int,start); iseq<=mCast(int,stop); iseq++ )
    {
	addToNrDone( 1 );
	const Strat::LayerSequence& seq = lm_.sequence( iseq );
	const TimeDepthModel& t2d = *sd_.zerooffsd2tmodels_[iseq];
	Interval<float> seqtimerg(  t2d.getTime(seq.zRange().start),
				    t2d.getTime(seq.zRange().stop) );

	for ( int iprop=1; iprop<props.size(); iprop++ )
	{
	    const bool propisvel = props[iprop]->stdType() == PropertyRef::Vel;
	    SeisTrcBufDataPack* dp = seisbufdps_[iprop-1];
	    SeisTrcBuf& trcbuf = dp->trcBuf();
	    const int bufsz = trcbuf.size();
	    SeisTrc* rawtrc = iseq < bufsz ? trcbuf.get( iseq ) : 0;
	    if ( !rawtrc )
		continue;

	    PointBasedMathFunction propvals( PointBasedMathFunction::Linear,
					     PointBasedMathFunction::EndVal );
	    for ( int idz=0; idz<sz; idz++ )
	    {
		const float time = mCast( float, zrg.atIndex(idz) );
		if ( !seqtimerg.includes(time,false) )
		    continue;

		if ( !layermodels_.validIdx(idz) )
		    continue;

		const Strat::LayerSequence& curseq =
		    layermodels_[idz]->sequence(iseq);
		if ( curseq.isEmpty() )
		    continue;

		Stats::CalcSetup laypropcalc( true );
		laypropcalc.require( Stats::Average );
		Stats::RunCalc<double> propval( laypropcalc );
		for ( int ilay=0; ilay<curseq.size(); ilay++ )
		{
		    const Strat::Layer* lay = curseq.layers()[ilay];
		    if ( !lay ) continue;
		    const float val = lay->value(iprop);
		    if ( mIsUdf(val) || ( propisvel && val < 1e-5f ) )
			continue;

		    const PropertyRef* pr = props[iprop];
		    const UnitOfMeasure* uom =
			UoMR().getDefault( pr->name(), pr->stdType() );
		    const float userval =
			!uom ? mUdf(float) : uom->getUserValueFromSI( val );
		    propval.addValue( propisvel ? 1.f / userval : userval,
				      lay->thickness() );
		}
		const float val = mCast( float, propval.average() );
		if ( mIsUdf(val) || ( propisvel && val < 1e-5f ) )
		    continue;

		propvals.add( time, propisvel ? 1.f / val : val );
	    }

	    Array1DImpl<float> proptr( sz );
	    for ( int idz=0; idz<sz; idz++ )
	    {
		const float time = mCast( float, zrg.atIndex(idz) );
		proptr.set( idz, propvals.getValue( time ) );
	    }

	    const float step = mCast( float, zrg.step );
	    ::FFTFilter filter( sz, step );
	    const float f4 = 1.f / (2.f * step );
	    filter.setLowPass( f4 );
	    if ( !filter.apply(proptr) )
		continue;

	    for ( int idz=0; idz<sz; idz++ )
		rawtrc->set( idz, proptr.get( idz ), 0 );
	}
    }

    return true;
}


    const PostStackSyntheticData&	sd_;
    const Strat::LayerModel&		lm_;
    ManagedObjectSet<Strat::LayerModel> layermodels_;
    ObjectSet<SyntheticData>&		synthetics_;
    ObjectSet<SeisTrcBufDataPack>	seisbufdps_;
    int&				lastsyntheticid_;
    bool				isprepared_;
    bool				useed_;

};


void StratSynth::generateOtherQuantities( const PostStackSyntheticData& sd,
					  const Strat::LayerModel& lm )
{
    StratPropSyntheticDataCreator propcreator( synthetics_, sd, lm,
					       lastsyntheticid_, useed_ );
    TaskRunner::execute( taskr_, propcreator );
}


uiString StratSynth::errMsg() const
{ return errmsg_; }


uiString StratSynth::infoMsg() const
{ return infomsg_; }



class ElasticModelAdjuster : public ParallelTask
{ mODTextTranslationClass(ElasticModelAdjuster)
public:

ElasticModelAdjuster( const Strat::LayerModel& lm,
		      TypeSet<ElasticModel>& aimodels, bool checksvel )
    : ParallelTask("Checking & adjusting elastic models")
    , lm_(lm)
    , aimodels_(aimodels)
    , checksvel_(checksvel)
{
}

od_int64 nrIterations() const
{
    return aimodels_.size();
}

uiString message() const
{
    return !errmsg_.isEmpty() ? errmsg_ : tr( "Checking Models" );
}

uiString nrDoneText() const
{
    return tr( "Models done" );
}


uiString infoMsg() const			{ return infomsg_; }
uiString errMsg() const				{ return errmsg_; }

protected:

bool doWork( od_int64 start , od_int64 stop , int )
{
    for ( int midx=mCast(int,start); midx<=mCast(int,stop); midx++ )
    {
	addToNrDone( 1 );
	const Strat::LayerSequence& seq = lm_.sequence( midx );
	ElasticModel& aimodel = aimodels_[midx];
	if ( aimodel.isEmpty() ) continue;

	ElasticModel tmpmodel( aimodel );
	int erroridx = -1;
	tmpmodel.checkAndClean( erroridx, !checksvel_, checksvel_, false );
	if ( tmpmodel.isEmpty() )
	{
	    uiString startstr(
		checksvel_ ? uiStrings::phrCannotCreate(
			   tr("prestack synthetics as all")) : tr("All") );
	    uiString propstr( checksvel_ ? tr("Swave velocity")
					 : tr("Pwave velocity/Density") );
	    errmsg_ = tr( "%1 the values of %2 in elastic model are invalid. "
			  "Probably units are not set correctly." )
				.arg(startstr).arg(propstr);
	    return false;
	}
	else if ( erroridx != -1 )
	{
	    bool needinterpolatedvel = false;
	    bool needinterpoltedden = false;
	    bool needinterpolatedsvel = false;
	    uiString msg;
	    for ( int idx=erroridx; idx<aimodel.size(); idx++ )
	    {
		const ElasticLayer& layer = aimodel[idx];
		const bool needinfo = msg.isEmpty();
		const bool incorrectpvel = !layer.isValidVel();
		const bool incorrectden = !layer.isValidDen();
		const bool incorrectsvel = !layer.isValidVs();
		if ( !incorrectpvel && !incorrectden && !incorrectsvel )
		    continue;

		if ( incorrectpvel )
		{
		    needinterpolatedvel = true;
		    if ( needinfo && infomsg_.isEmpty() )
		    {
			const UnitOfMeasure* uom = UoMR().get( "Meter/second" );
			msg.append( tr("'Pwave' ( sample value: %1 %2 )")
				.arg(toString(layer.vel_))
				.arg(uom ? uom->symbol() : "") );
		    }
		}

		if ( incorrectden )
		{
		    needinterpoltedden = true;
		    if ( needinfo && infomsg_.isEmpty() )
		    {
			const UnitOfMeasure* uom = UoMR().get( "Kg/m3" );
			msg.append( tr("'Density' ( sample value: %1 %2 )")
				.arg(toString(layer.vel_))
				.arg(uom ? uom->symbol() : "") );
		    }
		}

		if ( incorrectsvel )
		{
		    needinterpolatedsvel = true;
		    if ( needinfo && infomsg_.isEmpty() )
		    {
			const UnitOfMeasure* uom = UoMR().get( "Meter/second" );
			msg.append( tr("'Swave' ( sample value: %1 %2 )")
				.arg(toString(layer.vel_))
				.arg(uom ? uom->symbol() : "") );
		    }
		}
	    }

	    if ( infomsg_.isEmpty() )
	    {
		infomsg_ = tr( "Layer model contains invalid values of the "
			       "following properties: %1. First occurence "
			       "found in layer '%2' of pseudo well number '%3'."
			       "Invalid values will be interpolated. "
			       "The resulting synthetics may be incorrect" )
		    .arg( msg ).arg(seq.layers()[erroridx]->name()).arg(midx+1);
	    }

	    aimodel.interpolate( needinterpolatedvel, needinterpoltedden,
				 needinterpolatedsvel );
	}

	aimodel.mergeSameLayers();
	aimodel.upscale( 5.0f );
    }

    return true;
}


    const Strat::LayerModel&	lm_;
    TypeSet<ElasticModel>&	aimodels_;
    uiString			infomsg_;
    uiString			errmsg_;
    bool			checksvel_;
};


bool StratSynth::adjustElasticModel( const Strat::LayerModel& lm,
				     TypeSet<ElasticModel>& aimodels,
				     bool checksvel )
{
    ElasticModelAdjuster emadjuster( lm, aimodels, checksvel );
    const bool res = TaskRunner::execute( taskr_, emadjuster );
    infomsg_ = emadjuster.infoMsg();
    swaveinfomsgshown_ = checksvel;
    return res;
}


void StratSynth::getLevelDepths( const Strat::Level& lvl,
				 TypeSet<float>& zvals ) const
{
    zvals.setEmpty();
    for ( int iseq=0; iseq<layMod().size(); iseq++ )
	zvals += layMod().sequence(iseq).depthPositionOf( lvl );
}


static void convD2T( TypeSet<float>& zvals,
		     const ObjectSet<const TimeDepthModel>& d2ts )
{
    for ( int imdl=0; imdl<zvals.size(); imdl++ )
	zvals[imdl] = d2ts.validIdx(imdl) && !mIsUdf(zvals[imdl]) ?
		d2ts[imdl]->getTime( zvals[imdl] ) : mUdf(float);
}


bool StratSynth::setLevelTimes( const char* sdnm )
{
    SyntheticData* sd = getSynthetic( sdnm );
    if ( !sd ) return false;

    mDynamicCastGet(PostStackSyntheticData*,postsd,sd);
    if ( !postsd ) return false;
    SeisTrcBuf& tb = postsd->postStackPack().trcBuf();
    getLevelTimes( tb, sd->zerooffsd2tmodels_ );
    return true;
}


void StratSynth::getLevelTimes( const Strat::Level& lvl,
				const ObjectSet<const TimeDepthModel>& d2ts,
				TypeSet<float>& zvals ) const
{
    getLevelDepths( lvl, zvals );
    convD2T( zvals, d2ts );
}


void StratSynth::getLevelTimes( SeisTrcBuf& trcs,
			const ObjectSet<const TimeDepthModel>& d2ts,
			int dispeach ) const
{
    if ( !level_ ) return;

    TypeSet<float> times = level_->zvals_;
    convD2T( times, d2ts );

    for ( int idx=0; idx<trcs.size(); idx++ )
    {
	const SeisTrc& trc = *trcs.get( idx );
	SeisTrcPropCalc stp( trc );
	const int d2tidx = dispeach==-1 ? idx : idx*dispeach;
	float z = times.validIdx( d2tidx ) ? times[d2tidx] : mUdf( float );
	trcs.get( idx )->info().zref_ = z;
	if ( !mIsUdf( z ) && level_->snapev_ != VSEvent::None )
	{
	    Interval<float> tg( z, trc.startPos() );
	    mFlValSerEv ev1 = stp.find( level_->snapev_, tg, 1 );
	    tg.start = z; tg.stop = trc.endPos();
	    mFlValSerEv ev2 = stp.find( level_->snapev_, tg, 1 );
	    float tmpz = ev2.pos;
	    const bool ev1invalid = mIsUdf(ev1.pos) || ev1.pos < 0;
	    const bool ev2invalid = mIsUdf(ev2.pos) || ev2.pos < 0;
	    if ( ev1invalid && ev2invalid )
		continue;
	    else if ( ev2invalid )
		tmpz = ev1.pos;
	    else if ( fabs(z-ev1.pos) < fabs(z-ev2.pos) )
		tmpz = ev1.pos;

	    z = tmpz;
	}
	trcs.get( idx )->info().pick_ = z;
    }
}


void StratSynth::setLevel( const StratSynthLevel* lvl )
{ delete level_; level_ = lvl; }



void StratSynth::trimTraces( SeisTrcBuf& tbuf,
			     const ObjectSet<const TimeDepthModel>& d2ts,
			     float zskip ) const
{
    if ( mIsZero(zskip,mDefEps) )
	return;

    float highetszkip = mUdf(float);
    for ( int idx=0; idx<d2ts.size(); idx++ )
    {
	const TimeDepthModel& d2tmodel = *d2ts[idx];
	if ( d2tmodel.getTime(zskip)<highetszkip )
	    highetszkip = d2tmodel.getTime(zskip);
    }

    for ( int idx=0; idx<tbuf.size(); idx++ )
    {
	SeisTrc* trc = tbuf.get( idx );
	SeisTrc* newtrc = new SeisTrc( *trc );
	newtrc->info() = trc->info();
	const int startidx = trc->nearestSample( highetszkip );
	newtrc->reSize( trc->size()-startidx, false );
	newtrc->setStartPos( trc->samplePos(startidx) );
	for ( int sampidx=startidx; sampidx<trc->size(); sampidx++ )
	{
	    const float trcval =
		sampidx<0 || sampidx>=trc->size()-1 ? mUdf(float)
						    : trc->get(sampidx,0);
	    newtrc->set( sampidx-startidx, trcval, 0 );
	}
	delete tbuf.replace( idx, newtrc );
    }
}


void StratSynth::flattenTraces( SeisTrcBuf& tbuf ) const
{
    if ( tbuf.isEmpty() )
	return;

    float tmax = tbuf.get(0)->info().sampling_.start;
    float tmin = tbuf.get(0)->info().sampling_.atIndex( tbuf.get(0)->size() );
    for ( int idx=tbuf.size()-1; idx>=1; idx-- )
    {
	if ( mIsUdf(tbuf.get(idx)->info().pick_) ) continue;
	tmin = mMIN(tmin,tbuf.get(idx)->info().pick_);
	tmax = mMAX(tmax,tbuf.get(idx)->info().pick_);
    }

    for ( int idx=tbuf.size()-1; idx>=0; idx-- )
    {
	const SeisTrc* trc = tbuf.get( idx );
	const float start = trc->info().sampling_.start - tmax;
	const float stop  = trc->info().sampling_.atIndex(trc->size()-1) -tmax;
	SeisTrc* newtrc = trc->getRelTrc( ZGate(start,stop) );
	if ( !newtrc )
	{
	    newtrc = new SeisTrc( *trc );
	    newtrc->zero();
	}

	delete tbuf.replace( idx, newtrc );
    }
}


void StratSynth::decimateTraces( SeisTrcBuf& tbuf, int fac ) const
{
    for ( int idx=tbuf.size()-1; idx>=0; idx-- )
    {
	if ( idx%fac )
	    delete tbuf.remove( idx );
    }
}
