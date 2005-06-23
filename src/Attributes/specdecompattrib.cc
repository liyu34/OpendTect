/*+
________________________________________________________________________

 CopyRight:     (C) de Groot-Bril Earth Sciences B.V.
 Author:        Nanne Hemstra
 Date:          January 2004
 RCS:           $Id: specdecompattrib.cc,v 1.1 2005-06-23 09:08:24 cvshelene Exp $
________________________________________________________________________

-*/

#include "specdecompattrib.h"
#include "attribdataholder.h"
#include "attribdesc.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "datainpspec.h"
#include "genericnumer.h"
#include "survinfo.h"
#include "simpnumer.h"
#include "threadwork.h"
#include "basictask.h"
#include "attriblinebuffer.h"

#include <math.h>
#include <iostream>


#define mTransformTypeFourier		0
#define mTransformTypeDiscrete  	1
#define mTransformTypeContinuous	2

namespace Attrib
{

void SpecDecomp::initClass()
{
    Desc* desc = new Desc( attribName(), updateDesc );
    desc->ref();

    EnumParam* ttype = new EnumParam(transformTypeStr());
    //Note: Ordering must be the same as numbering!
    ttype->addEnum(transTypeNamesStr(mTransformTypeFourier));
    ttype->addEnum(transTypeNamesStr(mTransformTypeDiscrete));
    ttype->addEnum(transTypeNamesStr(mTransformTypeContinuous));
    ttype->setRequired(false);
    ttype->setDefaultValue("0");
    desc->addParam(ttype);

    EnumParam* window = new EnumParam(windowStr());
    window->addEnums(ArrayNDWindow::WindowTypeNames);
    window->setRequired(false);
    window->setDefaultValue("5");
    desc->addParam(window);

    ZGateParam* gate = new ZGateParam(gateStr());
    gate->setLimits( Interval<float>(-mLargestZGate,mLargestZGate) );
    desc->addParam( gate );

    FloatInpSpec* floatspec = new FloatInpSpec();
    ValParam* deltafreq = new ValParam( deltafreqStr(), floatspec );
    deltafreq->setRequired( false );
    deltafreq->setDefaultValue("5");
    desc->addParam( deltafreq );

    EnumParam* dwtwavelet = new EnumParam(dwtwaveletStr());
    dwtwavelet->addEnums(WaveletTransform::WaveletTypeNames);
    dwtwavelet->setRequired(false);
    dwtwavelet->setDefaultValue("Haar");
    desc->addParam(dwtwavelet);

    EnumParam* cwtwavelet = new EnumParam(cwtwaveletStr());
    cwtwavelet->addEnums(CWT::WaveletTypeNames);
    cwtwavelet->setRequired(false);
    cwtwavelet->setDefaultValue("Morlet");
    desc->addParam(cwtwavelet);

    desc->addOutputDataType( Seis::UnknowData );

    InputSpec reinputspec( 
	 "Real data on which the Frequency spectrum should be measured", true );
    desc->addInput( reinputspec );

    InputSpec iminputspec( 
	 "Imag data on which the Frequency spectrum should be measured", true );
    desc->addInput( iminputspec );

    desc->init();

    PF().addDesc( desc, createInstance );
    desc->unRef();
}


Provider* SpecDecomp::createInstance( Desc& ds )
{
    SpecDecomp* res = new SpecDecomp( ds );
    res->ref();

    if ( !res->isOK() )
    {
	res->unRef();
	return 0;
    }

    res->unRefNoDelete();
    return res;
}


void SpecDecomp::updateDesc( Desc& desc )
{
    const ValParam* transformtype=(ValParam*)desc.getParam(transformTypeStr());
    if ( !strcmp(transformtype->getStringValue(0),
		 transTypeNamesStr(mTransformTypeFourier)))
    {
	desc.setParamEnabled(gateStr(),true);
	desc.setParamEnabled(windowStr(),true);
    }
    else
    {
	desc.setParamEnabled(gateStr(),false);
	desc.setParamEnabled(windowStr(),false);
    }
    
    if ( !strcmp(transformtype->getStringValue(0),
		 transTypeNamesStr(mTransformTypeDiscrete)))
	desc.setParamEnabled(dwtwaveletStr(),true);
    else
	desc.setParamEnabled(dwtwaveletStr(),false);

    if ( !strcmp(transformtype->getStringValue(0),
		 transTypeNamesStr(mTransformTypeContinuous)))
	desc.setParamEnabled(cwtwaveletStr(),true);
    else
	desc.setParamEnabled(cwtwaveletStr(),false);

    //HERE see what to do when SI().zRange().step != refstep !!!
    float dfreq;
    mGetFloat( dfreq, deltafreqStr() );
    float nyqfreq = 0.5 / SI().zRange().step;
    int nrattribs = mNINT( nyqfreq / dfreq );
    desc.setNrOutputs( Seis::UnknowData, nrattribs-1 );
}


const char* SpecDecomp::transTypeNamesStr(int type)
{
    if ( type==mTransformTypeFourier ) return "DFT";
    if ( type==mTransformTypeDiscrete ) return "DWT";
    return "CWT";
}


SpecDecomp::SpecDecomp( Desc& desc_ )
    : Provider( desc_ )
    , window(0)
    , fftisinit(false)
    , timedomain(0)
    , freqdomain(0)
    , signal(0)
    , scalelen(0)
{ 
    if ( !isOK() ) return;

    mGetEnum( transformtype, transformTypeStr() );
    mGetFloat( deltafreq, deltafreqStr() );

    if ( transformtype == mTransformTypeFourier )
    {
	int wtype;
	mGetEnum( wtype, windowStr() );
	windowtype = (ArrayNDWindow::WindowType)wtype;
	
	mGetFloatInterval( gate, gateStr() );
	gate.start = gate.start / zFactor(); gate.stop = gate.stop / zFactor();
    }
    else if ( transformtype == mTransformTypeDiscrete )
    {
	int dwave;
	mGetEnum( dwave, dwtwaveletStr() );
	dwtwavelet = (WaveletTransform::WaveletType) dwave;
    }
    else 
    {
	int cwave;
	mGetEnum( cwave, cwtwaveletStr() );
	cwtwavelet = (CWT::WaveletType) cwave;
    }
}


SpecDecomp::~SpecDecomp()
{
    delete window;
    
    for ( int idx=0; idx<inputs.size(); idx++ )
	if ( inputs[idx] ) inputs[idx]->unRef();
	    inputs.erase();

    desc.unRef();

    delete threadmanager;
    deepErase( computetasks );

    delete linebuffer;
    delete &seldata_;
}


bool SpecDecomp::getInputOutput( int input, TypeSet<int>& res ) const
{
    return Provider::getInputOutput( input, res );
}


bool SpecDecomp::getInputData( const BinID& relpos, int idx )
{
    redata = inputs[0]->getData( relpos, idx );
    if ( !redata ) return false;

    imdata = inputs[1]->getData( relpos, idx );
    if ( !imdata ) return false;
    
    return true;
}


bool SpecDecomp::computeData( const DataHolder& output, const BinID& relpos,
	                          int t0, int nrsamples ) const
{
    if ( !fftisinit )
    {
	if ( transformtype == mTransformTypeFourier )
	{
	    const_cast<SpecDecomp*>(this)->samplegate = 
	     Interval<int>(mNINT(gate.start/refstep), mNINT(gate.stop/refstep));
	    const_cast<SpecDecomp*>(this)->sz = samplegate.width()+1;

	    const float fnyq = 0.5 / refstep;
	    const int minsz = mNINT( 2*fnyq/deltafreq );
	    const_cast<SpecDecomp*>(this)->fftsz = sz > minsz ? sz : minsz;
	    const_cast<SpecDecomp*>(this)->
			fft.setInputInfo(Array1DInfoImpl(fftsz));
	    const_cast<SpecDecomp*>(this)->fft.setDir(true);
	    const_cast<SpecDecomp*>(this)->fft.init();
	    const_cast<SpecDecomp*>(this)->df = FFT::getDf( refstep, fftsz );

	    const_cast<SpecDecomp*>(this)->window = 
		new ArrayNDWindow( Array1DInfoImpl(sz), false, windowtype );
	}
	else
	    const_cast<SpecDecomp*>(this)->scalelen = 1024;

	const_cast<SpecDecomp*>(this)->fftisinit = true;
    }
    
    if ( transformtype == mTransformTypeFourier )
    {
	const_cast<SpecDecomp*>(this)->
	    		signal = new Array1DImpl<float_complex>( sz );

	const_cast<SpecDecomp*>(this)->
	    timedomain = new Array1DImpl<float_complex>( fftsz );
	const int tsz = timedomain->info().getTotalSz();
	memset(timedomain->getData(),0, tsz*sizeof(float_complex));

	const_cast<SpecDecomp*>(this)->
			freqdomain = new Array1DImpl<float_complex>( fftsz );
    }
    
    bool res;
    if ( transformtype == mTransformTypeFourier )
	res = calcDFT(output, t0, nrsamples);
    else if ( transformtype == mTransformTypeDiscrete )
	res = calcDWT(output, t0, nrsamples);
    else if ( transformtype == mTransformTypeContinuous )
	res = calcCWT(output, t0, nrsamples);

    
    delete signal;
    delete timedomain;
    delete freqdomain;
    return res;
}


bool SpecDecomp::calcDFT(const DataHolder& output, int t0, int nrsamples ) const
{
    for ( int idx=0; idx<nrsamples; idx++ )
    {
	int cursample = t0 - redata->t0_ + idx;
	int sample = cursample + samplegate.start;
	for ( int ids=0; ids<sz; ids++ )
	{
	    float real = redata->item(0)? redata->item(0)->value( sample ) : 0;
	    float imag = imdata->item(0)? -imdata->item(0)->value( sample ) : 0;

	    signal->set( ids, float_complex(real,imag) );
	    sample++;
	}

	removeBias( signal );
	window->apply( signal );

	const int diff = (int)(fftsz - sz)/2;
	for ( int idy=0; idy<sz; idy++ )
	    timedomain->set( diff+idy, signal->get(idy) );

	fft.transform( *timedomain, *freqdomain );

	for ( int idf=0; idf<outputinterest.size(); idf++ )
	{
	    if ( !outputinterest[idf] ) continue;

	    float_complex val = freqdomain->get( idf );
	    float real = val.real();
	    float imag = val.imag();
	    output.item(idf)->setValue( idx, sqrt(real*real+imag*imag) );
	}
    }

    return true;
}


bool SpecDecomp::calcDWT(const DataHolder& output, int t0, int nrsamples ) const
{
    int len = nrsamples + scalelen;
    while ( !isPower( len, 2 ) ) len++;

    Array1DImpl<float> inputdata( len );
    if ( !redata->item(0) ) return false;
    
    int off = (len-nrsamples)/2;
    for ( int idx=0; idx<len; idx++ )
    {
	int cursample = t0 - redata->t0_ + idx-off;
        inputdata.set( idx, redata->item(0)->value(cursample) );
    }

    Array1DImpl<float> transformed( len );
    ::DWT dwt(dwtwavelet );// what does that really mean?

    dwt.setInputInfo( inputdata.info() );
    dwt.init();
    dwt.transform( inputdata, transformed );

    const int nrscales = isPower( len, 2 ) + 1;
    ArrPtrMan<float> spectrum =  new float[nrscales];
    spectrum[0] = fabs(transformed.get(0)); // scale 0 (dc)
    spectrum[1] = fabs(transformed.get(1)); // scale 1

    for ( int idx=0; idx<nrsamples; idx++ )
    {
        for ( int scale=2; scale<nrscales; scale++ )
        {
            int scalepos = intpow(2,scale-1) + ((idx+off) >> (nrscales-scale));
            spectrum[scale] = fabs(transformed.get(scalepos));

	    if ( !outputinterest[scale] ) continue;
	    output.item(scale)->setValue( idx, spectrum[scale] );
        }
    }

    return true;
}


#define mGetNextPow2(inp) \
    int nr = 2; \
    while ( nr < inp ) \
        nr *= 2; \
    inp = nr;


bool SpecDecomp::calcCWT(const DataHolder& output, int t0, int nrsamples ) const
{
    int nrsamp = nrsamples;
    if ( nrsamples == 1 ) nrsamp = 256;
    mGetNextPow2( nrsamp );
    if ( !redata->item(0) || !imdata->item(0) ) return false;

    int off = (nrsamp-nrsamples)/2;
    Array1DImpl<float_complex> inputdata( nrsamp );
    for ( int idx=0; idx<nrsamp; idx++ )
    {
	int cursample = t0 - redata->t0_ + idx - off;
	float real = redata->item(0)->value( cursample );
	float imag = - imdata->item(0)->value( cursample );
        inputdata.set( idx, float_complex(real,imag) );
    }
    
    CWT& cwt = const_cast<CWT&>(cwt);
    cwt.setInputInfo( Array1DInfoImpl(nrsamp) );
    cwt.setDir( true );
    cwt.setWavelet( cwtwavelet );
    cwt.setDeltaT( refstep );

    float nrattribs = mNINT(0.5 * refstep / deltafreq);
    const float freqstop = deltafreq*nrattribs;
    cwt.setTransformRange( StepInterval<float>(deltafreq,freqstop,deltafreq) );
    cwt.init();

    Array2DImpl<float> outputdata(0,0);
    cwt.transform( inputdata, outputdata );

    const int nrscales = outputdata.info().getSize(1);
    if ( getenv("DTECT_PRINT_SPECDECOMP") )
    {
	for ( int idx=0; idx<nrsamp; idx++ )
	{
	    for ( int idy=0; idy<nrscales; idy++ )
		std::cerr << idx << '\t' << idy << '\t'
		    	  << outputdata.get(idx,idy)<<'\n';
	}
    }
    
    for ( int idx=0; idx<nrscales; idx++ )
    {
	if ( !outputinterest[idx] ) continue;
	for ( int ids=0; ids<nrsamples; ids++ )
	    output.item(0)->setValue( ids, outputdata.get( ids+off, idx ) );
    }

    return true;
}


const Interval<float>* SpecDecomp::reqZMargin( int inp, int ) const
{ return transformtype != mTransformTypeFourier ? 0 : &gate; }

}//namespace
