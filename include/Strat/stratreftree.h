#ifndef stratreftree_h
#define stratreftree_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert Bril
 Date:		Dec 2003
 RCS:		$Id: stratreftree.h,v 1.19 2010-09-07 16:03:06 cvsbruno Exp $
________________________________________________________________________

-*/

#include "stratunitref.h"
#include "repos.h"

namespace Strat
{

class Lithology;


/*!\brief Tree of UnitRef's  */

mClass RefTree : public NodeUnitRef
{
public:

    			RefTree( const char* nm, Repos::Source src )
			: NodeUnitRef(0,"","Top Node")
			, treename_(nm)
			, src_(src)			
			, botlvlid_(-1)
			{}
			~RefTree(){};

    const BufferString&	treeName() const		{ return treename_; }
    void		setTreeName( const char* nm )	{ treename_ = nm; }
    Repos::Source	source() const			{ return src_; }
    
    Strat::UnitRef*     getByID(int id) 		{ return fnd(id); }
    const Strat::UnitRef* getByID(int id) const 	{ return fnd(id); } 
     
    int			getID(const char* code) const;
    void		getUnitIDs(TypeSet<int>&) const;
    
    void		assignEqualTimesToUnits(Interval<float>) const;
    void		getLeavesTimeGaps(const NodeUnitRef&,
					   TypeSet< Interval<float> >&) const;

    bool		addCopyOfUnit(const UnitRef&,bool rev=false);
    bool		addUnit(const char*,const char* unit_dump,
	    			bool rev=false);
    void		removeEmptyNodes(); //!< recommended after add
    bool		write(std::ostream&) const;
    				//!< for printing, export or something.
    				//!< otherwise, use UnitRepository::write()
    int			botLvlID() const	{ return botlvlid_; }
    void		setBotLvlID(int id) 	{ botlvlid_ = id; }

protected:

    void		constrainUnits(Strat::UnitRef&) const;
    void		constrainUnitTimes(NodeUnitRef&) const;
    void		constrainUnitLvls(UnitRef&) const;
    void		gatherLeavesByTime(const NodeUnitRef&,
	    					ObjectSet<UnitRef>&) const;
    
    Strat::UnitRef*     fnd(int id) const;

    int			botlvlid_;

    Repos::Source	src_;
    BufferString	treename_;

};


}; // namespace Strat


#endif
