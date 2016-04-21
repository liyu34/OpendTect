#ifndef namedobj_h
#define namedobj_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Sep 1994, Aug 2006, Mar 2015
________________________________________________________________________

-*/

#include "basicmod.h"
#include "monitor.h"
#include "bufstring.h"


/*!\brief object with a name. */

mExpClass(Basic) NamedObject
{
public:

			NamedObject(const char* nm=0);
			NamedObject(const NamedObject&);
    virtual		~NamedObject();
    NamedObject&	operator =(const NamedObject&);
    bool		operator ==(const NamedObject&) const;

    virtual const OD::String& name() const		{ return name_; }
    virtual BufferString getName() const		{ return name_; }
    virtual void	setName( const char* nm )	{ name_ = nm; }

    bool		getNameFromPar(const IOPar&);
    void		putNameInPar(IOPar&) const;

protected:

    BufferString	name_;

};

/*!\brief Monitorable object with a name.  */

mExpClass(Basic) NamedMonitorable : public Monitorable
				  , public NamedObject
{
public:

			NamedMonitorable(const char* nm=0);
			NamedMonitorable(const NamedMonitorable&);
			NamedMonitorable(const NamedObject&);
    NamedMonitorable&	operator =(const NamedMonitorable&);
    NamedMonitorable&	operator =(const NamedObject&);
    virtual		~NamedMonitorable();
    bool		operator ==(const NamedMonitorable&) const;
    bool		operator ==(const NamedObject&) const;

    virtual BufferString getName() const;
    virtual void	setName(const char*);

    mDeclInstanceCreatedNotifierAccess( NamedMonitorable );

};


#endif
