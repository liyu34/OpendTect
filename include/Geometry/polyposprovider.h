#ifndef polyposprovider_h
#define polyposprovider_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Bert
 Date:          Feb 2008
 RCS:           $Id: polyposprovider.h,v 1.1 2008-02-05 15:52:50 cvsbert Exp $
________________________________________________________________________


-*/

#include "posprovider.h"
class HorSampling;
template <class T> class ODPolygon;

namespace Pos
{

/*!\brief Volume/Area provider based on CubeSampling */

class PolyProvider3D : public Provider3D
{
public:

			PolyProvider3D();
			PolyProvider3D(const PolyProvider3D&);
			~PolyProvider3D();
    PolyProvider3D&	operator =(const PolyProvider3D&);
    Provider*		clone() const	{ return new PolyProvider3D(*this); }

    virtual bool	initialize();
    virtual void	reset()		{ initialize(); }

    virtual bool	toNextPos();
    virtual bool	toNextZ();

    virtual BinID	curBinID() const	{ return curbid_; }
    virtual float	curZ() const		{ return curz_; }
    virtual bool	includes(const BinID&,float) const;
    virtual void	usePar(const IOPar&);
    virtual void	fillPar(IOPar&) const;

    virtual void	getExtent(BinID&,BinID&) const;
    virtual void	getZRange( Interval<float>& zrg ) const
						{ zrg = zrg_; }

    ODPolygon<float>&	polygon()		{ return poly_; }
    const ODPolygon<float>& polygon() const	{ return poly_; }
    StepInterval<float>& zRange()		{ return zrg_; }
    const StepInterval<float>& zRange() const	{ return zrg_; }
    HorSampling&	horSampling()		{ return hs_; }
    const HorSampling&	horSampling() const	{ return hs_; }

    static ODPolygon<float>* polyFromPar(const IOPar&,int nr=0);

protected:

    ODPolygon<float>&	poly_;
    StepInterval<float>	zrg_;
    HorSampling&	hs_;

    BinID		curbid_;
    float		curz_;

public:

    static void		initClass();
    static Provider3D*	create()		{ return new PolyProvider3D; }

};


} // namespace

#endif
