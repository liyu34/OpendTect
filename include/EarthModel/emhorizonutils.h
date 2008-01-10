#ifndef emhorizonutils_h
#define emhorizonutils_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Helene Payraudeau
 Date:          September 2005
 RCS:           $Id: emhorizonutils.h,v 1.6 2008-01-10 08:49:18 cvshelene Exp $
________________________________________________________________________

-*/

#include "sets.h"
#include "ranges.h"
#include "geometry.h"

class RowCol;
class MultiID;
class BinIDValueSet;
class HorSampling;
class BufferStringSet;

namespace EM
{

/*! \brief
Group of utilities for horizons : here are all functions required in 
process_attrib_em for computing data on, along or between 2 horizons.
*/

class Surface;

class HorizonUtils
{
public:
			HorizonUtils(){};
				
    static float 	getZ(const RowCol&,const Surface*);
    static float 	getMissingZ(const RowCol&,const Surface*,int);
    static Surface* 	getSurface(const MultiID&);
    static void 	getPositions(std::ostream&,const MultiID&,
				     ObjectSet<BinIDValueSet>&,
				     const Geom::PosRectangle<double>* xyrg=0);
    static void 	getWantedPositions(std::ostream&,ObjectSet<MultiID>&,
					   BinIDValueSet&,const HorSampling&,
					   const Interval<float>& extraz,
					   int nrinterpsamp,int mainhoridx,
					   float extrawidth);
    static void 	getWantedPos2D(std::ostream&,ObjectSet<MultiID>&,
				       BinIDValueSet&,
				       const Geom::PosRectangle<double>*,
				       const Interval<float>& extraz);
    static bool		getZInterval(int idi,int idc,Surface*,Surface*,
	    			     float& topz,float& botz,int nrinterpsamp,
				     int mainhoridx,float& lastzinterval,
				     float extrawidth);
	
    static bool		SolveIntersect(float& topz,float& botz,int nrinterpsamp,
	    			       int is1main,float extrawidth,
				       bool is1interp,bool is2interp);
    static void 	addSurfaceData(const MultiID&,const BufferStringSet&,
				       const ObjectSet<BinIDValueSet>&);

protected:

};

};//namespace

#endif
