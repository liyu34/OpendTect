#ifndef datapack_h
#define datapack_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	A.H.Bril
 Date:		Jan 2007
 RCS:		$Id: datapack.h,v 1.1 2009-02-10 15:22:13 cvsbert Exp $
________________________________________________________________________

-*/

#include "namedobj.h"
#include "sets.h"
#include "thread.h"
#include <iosfwd>

class DataPackMgr;
class IOPar;

/*!\brief A data packet: data+positioning and more that needs to be shared.

  The 'category' is meant like:
  'Pre-Stack gather'
  'Wavelet'
  'Fault surface'

 */ 


mClass DataPack : public NamedObject
{
public:

    typedef int		ID;

    			DataPack( const char* categry )
			    : NamedObject("<?>")
			    , category_(categry)
			    , nrusers_( 0 )
			    , id_(getNewID())	{}
			DataPack( const DataPack& dp )
			    : NamedObject( dp.name().buf() )
			    , category_( dp.category_ )
			    , nrusers_( 0 )
			    , id_(getNewID())	{}
    virtual		~DataPack()		{}

    ID			id() const		{ return id_; }
    virtual const char*	category() const	{ return category_.buf(); }

    virtual float	nrKBytes() const	= 0;
    virtual void	dumpInfo(IOPar&) const;

    static const char*	sKeyCategory();
    static const ID	cNoID()		    { return 0; }

protected:

    const ID		id_;
    const BufferString	category_;
    int			nrusers_;
    Threads::Mutex	nruserslock_;

    static ID		getNewID(); 	//!< ensures a global data pack ID
    static const float	sKb2MbFac();	//!< 1 / 1024

    void		setCategory( const char* c )
    			{ *const_cast<BufferString*>(&category_) = c; }

    friend class	DataPackMgr;
};


/*!\brief Manages DataPacks

  Data Packs will be managed with everything in it. If you add a Pack, you
  will get the ID of the pack.

  When you obtain the data for looking at it, you can choose to 'only observe'.
  In that case, you'd better use the packToBeRemoved notifier, as the data may
  be deleted at any time. Normally, you want to obtain a reference whilst
  making sure the data is not thrown away.

  This means you *must* release the data pack once you no longer use it, but
  *NEVER* release a pack when you used the 'observing_only' option.

  You can get an appropriate DataPackMgr from the DPM() function.

   */

mClass DataPackMgr : public CallBacker
{
public:
			// You can, but normally should not, construct
			// a manager. In general, leave it to DPM() - see below.

    typedef int		ID;		//!< Each Mgr has its own ID

    void		add(DataPack*);
    			//!< The pack becomes mine
    DataPack*		addAndObtain(DataPack*);
    			/*!< The pack becomes mines. Pack is obtained
			     during the lock, i.e. threadsafe. */

    DataPack*		obtain( DataPack::ID dpid, bool observing_only=false )
			{ return doObtain(dpid,observing_only); }
    const DataPack*	obtain( DataPack::ID dpid, bool obsrv=false ) const
			{ return doObtain(dpid,obsrv); }

    void		release(DataPack::ID);
    void		release( const DataPack* dp )
    			{ if ( dp ) release( dp->id() ); }
    void		releaseAll(bool donotify);

    Notifier<DataPackMgr> newPack;		//!< Passed CallBacker* = Pack
    Notifier<DataPackMgr> packToBeRemoved;	//!< Passed CallBacker* = Pack

			// Standard mgr IDs take the low integer numbers
    static const ID	PointID();	//!< Sets of 'unconnected' points: 1
    static const ID	CubeID();	//!< Cube/Block (N1xN2xN3) data: 2
    static const ID	FlatID();	//!< Flat (N1xN2) data: 3
    static const ID	SurfID();	//!< Surface (triangulated) data: 4

    			// Convenience to get info without any obtain()
    const char*		nameOf(DataPack::ID) const;
    const char*		categoryOf(DataPack::ID) const;
    virtual float	nrKBytesOf(DataPack::ID) const;
    virtual void	dumpInfoFor(DataPack::ID,IOPar&) const;

    ID			id() const		{ return id_; }
    void		dumpInfo(std::ostream&) const;
    float		nrKBytes() const;

    const ObjectSet<DataPack>&	packs() const	{ return packs_; }

    			DataPackMgr(ID);
			//!< You can, but normally should not, construct
			//!< a manager. In general, leave it to DPM().
    			~DataPackMgr();
			//!< Delete a DataPackMgr only when you have
			//!< created it with the constructor.

    static DataPackMgr&	DPM(ID);
    static void		dumpDPMs(std::ostream&);

protected:

    ID				id_;
    ObjectSet<DataPack>		packs_;

    DataPack*				doObtain(ID,bool) const;
    int					indexOf(ID) const;
    					//!<Object should be readlocked
    mutable Threads::ReadWriteLock	lock_;

    static Threads::Mutex		mgrlistlock_;
    static ObjectSet<DataPackMgr> 	mgrs_;
};


mGlobal DataPackMgr& DPM(DataPackMgr::ID);


#define mObtainDataPack( var, type, mgrid, newid ) \
{ \
    if ( var ) \
    { \
	DPM( mgrid ).release( var->id() ); \
	var = 0; \
    } \
 \
    DataPack* __dp = DPM( mgrid ).obtain( newid ); \
    mDynamicCastGet( type, __dummy, __dp ); \
    if ( !__dummy && __dp ) \
	 DPM( mgrid ).release( __dp->id() ); \
    else \
	var = __dummy; \
} 


#define mObtainDataPackToLocalVar( var, type, mgrid, newid ) \
type var = 0; \
mObtainDataPack( var, type, mgrid, newid ); \

#endif
