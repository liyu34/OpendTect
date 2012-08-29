#ifndef externalattrib_h
#define externalattrib_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A.H. Bril
 Date:		Nov 2004
 RCS:		$Id: externalattrib.h,v 1.15 2012-08-29 06:25:40 cvskris Exp $
________________________________________________________________________

-*/

#include "attributeenginemod.h"
#include "factory.h"
#include "datapack.h"
#include "sets.h"

class BinIDValueSet;
class CubeSampling;
class LineKey;
class SeisTrcBuf;
class TaskRunner;


namespace Attrib
{

class SelSpec;
class DataCubes;

/*! Generic class for attribs that does not come from the attribute engine. */


mClass(AttributeEngine) ExtAttribCalc
{
public:
    virtual			~ExtAttribCalc()			{}
    virtual bool		setTargetSelSpec(const SelSpec&)	= 0;
    				/*!<\returns if this object can 
				     compute it or not. */

    virtual DataPack::ID	createAttrib(const CubeSampling&,
					     DataPack::ID, TaskRunner*);
    virtual bool		createAttrib(ObjectSet<BinIDValueSet>&,
	    				     TaskRunner*);
    virtual bool		createAttrib(const BinIDValueSet&, SeisTrcBuf&,
					     TaskRunner*);
    virtual DataPack::ID	createAttrib(const CubeSampling&,
	    				     const LineKey&,TaskRunner*);

    virtual bool		isIndexes() const	{ return false; }

    BufferString		errmsg_;
};


mDefineFactory1Param( AttributeEngine, ExtAttribCalc, const Attrib::SelSpec&, ExtAttrFact );


} // namespace Attrib

#endif

