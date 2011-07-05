#ifndef synthseis_h
#define synthseis_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A.H. Bril
 Date:		24-3-1996
 RCS:		$Id: synthseis.h,v 1.22 2011-07-05 08:24:39 cvsbruno Exp $
________________________________________________________________________

-*/

#include "ailayer.h"
#include "cubesampling.h"
#include "factory.h"
#include "odmemory.h"
#include "raytrace1d.h"
#include "samplingdata.h"
#include "task.h"
#include "executor.h"

#include "complex"

class RayTracer1D;
class SeisTrc;
class TimeDepthModel;
class Wavelet;

typedef std::complex<float> float_complex;
namespace Fourier { class CC; };
namespace PreStack { class Gather; }

namespace Seis
{

/* brief generates synthetic traces.The SynthGenerator performs the basic 
   convolution with a reflectivity series and a wavelet. If you have AI layers 
   and want directly some synthetics out of them, then you should use the 
   RayTraceSynthGenerator.

   The different constructors and generate() functions will optimize for
   different situations. For example, if your Reflectivity/AI Model is fixed 
   and you need to generate for multiple wavelets, then you benefit from only 
   one anti-alias being done.
*/


mClass SynthGenBase 
{
public:
    virtual bool		setWavelet(const Wavelet*,OD::PtrPolicy pol);
    virtual bool		setOutSampling(const StepInterval<float>&);

    const char*			errMsg() const	
    				{ return errmsg_.isEmpty() ? 0 : errmsg_.buf();}

    virtual void		fillPar(IOPar&) const;
    virtual bool		usePar(const IOPar&);

    static const char*		sKeyFourier() 	{ return "Convolution Domain"; }
    static const char* 		sKeyNMO() 	{ return "Use NMO"; }

protected:
    				SynthGenBase();
    				~SynthGenBase();

    bool			isfourier_;
    bool			usenmotimes_;
    bool			waveletismine_;
    const Wavelet*		wavelet_;
    StepInterval<float>		outputsampling_;

    BufferString		errmsg_;
};



mClass SynthGenerator : public SynthGenBase
{
public:
    				SynthGenerator();
    				~SynthGenerator();

    virtual bool		setWavelet(const Wavelet*,OD::PtrPolicy pol);
    virtual bool		setOutSampling(const StepInterval<float>&);
    bool			setModel(const ReflectivityModel&);

    bool                        doPrepare();
    bool			doWork();
    const SeisTrc&		result() const		{ return outtrc_; }

    void 			getSampledReflectivities(TypeSet<float>&) const;

protected:

    bool 			computeTrace(float* result); 
    bool 			doTimeConvolve(float* result); 
    bool 			doFFTConvolve(float* result);
    void 			setConvolDomain(bool fourier);

    const ReflectivityModel*	refmodel_;

    Fourier::CC*                fft_;
    int				fftsz_;
    float_complex*		freqwavelet_;
    bool			needprepare_;	
    TypeSet<float_complex>	cresamprefl_;
    SeisTrc&			outtrc_;
};


mClass MultiTraceSynthGenerator : public ParallelTask, public SynthGenBase
{
public:
    				~MultiTraceSynthGenerator();

    void 			setModels(
				    const ObjectSet<const ReflectivityModel>&);

    void 			result(ObjectSet<const SeisTrc>&) const;
    void 			getSampledReflectivities(TypeSet<float>&) const;

protected:

    od_int64            	nrIterations() const;
    virtual bool        	doWork(od_int64,od_int64,int);

    ObjectSet<SynthGenerator>	synthgens_;
};



mClass RaySynthGenerator : public SynthGenBase, public Executor 
{
public:
    mDefineFactoryInClass( RaySynthGenerator, factory );

    virtual bool		addModel(const AIModel&);
				/*!<you can have more than one model!*/
    virtual bool		setRayParams(const TypeSet<float>& offs,
					const RayTracer1D::Setup&,bool isnmo);

    int				nextStep();
    od_int64            	totalNr() const { return raymodels_.size(); }
    od_int64            	nrDone() const 	{ return nrdone_; }
    const char*         	message() const;

    mStruct RayModel
    {
			RayModel(const RayTracer1D& rt1d,int nroffsets);
			~RayModel();	

	void 		getTraces(ObjectSet<const SeisTrc>&,bool steal);
	void		getRefs(ObjectSet<const ReflectivityModel>&,bool steal);
	void		getD2T(ObjectSet<const TimeDepthModel>&,bool steal);

	const SeisTrc*	stackedTrc() const;

    protected:
	ObjectSet<const SeisTrc>		outtrcs_; //this is a gather
	ObjectSet<const TimeDepthModel> 	t2dmodels_;
	ObjectSet<const ReflectivityModel> 	refmodels_;
	TypeSet<float>  			sampledrefs_;

    public:
	void		getSampledRefs(TypeSet<float>&) const;

	friend class 				RaySynthGenerator;
    };

    //available after execution
    RayModel&			result(int id) { return *raymodels_[id]; }
    const RayModel&		result(int id) const { return *raymodels_[id]; }

protected:
    				RaySynthGenerator();
    virtual 			~RaySynthGenerator();

    void			clean();
    int				nrdone_;
    bool			israytracing_;

    bool			doRayTracing();
    int				doSynthetics();

    StepInterval<float>		raysampling_;
    TypeSet<float>		offsets_;
    TypeSet<AIModel>		aimodels_;
    ObjectSet<RayModel>		raymodels_;
    RayTracer1D::Setup 		raysetup_;

public:
    void			reSet( bool all ); 
};



mClass ODRaySynthGenerator : public RaySynthGenerator
{
protected:

    static void         	initClass() 
				{factory().addCreator(create,"Fast Generator");}
    static RaySynthGenerator* 	create() { return new ODRaySynthGenerator; }
};


} //namespace


#endif

