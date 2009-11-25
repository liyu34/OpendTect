#ifndef uiseiswvltattr_h
#define uiseiswvltattr_h
/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bruno
 Date:          Mar 2009
 RCS:           $Id: uiseiswvltattr.h,v 1.13 2009-11-25 14:09:20 cvsbruno Exp $
________________________________________________________________________

-*/

#include "uidialog.h"
#include "uislider.h"

class ArrayNDWindow;
class Wavelet;
class uiCheckBox;
class uiFuncTaperDisp;
class uiFunctionDisplay;
class uiFreqTaperGrp;
class uiGenInput;
class uiWaveletDispProp;
class WaveletAttrib;

template <class T> class Array1DImpl;

mClass uiSeisWvltSliderDlg : public uiDialog 
{
public:
				~uiSeisWvltSliderDlg();

    Notifier<uiSeisWvltSliderDlg> acting;
    const Wavelet*              getWavelet() const  { return wvlt_; }

    WaveletAttrib*		wvltattr_;
    uiSliderExtra*		sliderfld_;
    Wavelet* 			wvlt_;
    const Wavelet* 		orgwvlt_;

    virtual void		act(CallBacker*) {}
    void			constructSlider(uiSliderExtra::Setup&,
	    					const Interval<float>&);
protected:
				uiSeisWvltSliderDlg(uiParent*,Wavelet&);
};


mClass uiSeisWvltRotDlg : public uiSeisWvltSliderDlg 
{
public:
				uiSeisWvltRotDlg(uiParent*,Wavelet&);
protected:

    void			act(CallBacker*);
};


mClass uiSeisWvltTaperDlg : public uiSeisWvltSliderDlg 
{
public:
				uiSeisWvltTaperDlg(uiParent*,Wavelet&);
				~uiSeisWvltTaperDlg();
protected: 
    
    bool			isfreqtaper_;
    int				wvltsz_;

    Array1DImpl<float>* 	wvltvals_;
    Array1DImpl<float>* 	freqvals_;
    Interval<float>		timerange_;
    Interval<float>		freqrange_;

    uiFuncTaperDisp*		timedrawer_;
    uiFuncTaperDisp*		freqdrawer_;
    uiFreqTaperGrp*		freqtaper_;

    uiGenInput*			typefld_;
    uiCheckBox*			mutefld_;

    void			setFreqData();
    void			setTimeData();

    void			act(CallBacker*);
    void			typeChoice(CallBacker*);
};



mClass uiWaveletDispProp : public uiGroup
{
public:

				uiWaveletDispProp(uiParent*,const Wavelet&);
				~uiWaveletDispProp();

    void                        setAttrCurves(const Wavelet&);
    Interval<float>             getFreqRange() const { return freqrange_; }
    Interval<float>             getTimeRange() const { return timerange_; }

private:

    int                         wvltsz_;
    ObjectSet<uiFunctionDisplay>  attrdisps_;
    ObjectSet< Array1DImpl<float> > attrarrays_;
    
    WaveletAttrib*		wvltattr_;
    
    Interval<float>		timerange_;
    Interval<float>		freqrange_;

    void			addAttrDisp(bool);
};


mClass uiWaveletDispPropDlg : public uiDialog
{
public:
				uiWaveletDispPropDlg(uiParent*,const Wavelet&);
				~uiWaveletDispPropDlg();
protected:

    uiWaveletDispProp*		properties_;
};

#endif
