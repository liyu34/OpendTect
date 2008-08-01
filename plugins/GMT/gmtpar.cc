/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Raman Singh
 Date:          July 2008
 RCS:           $Id: gmtpar.cc,v 1.1 2008-08-01 08:28:27 cvsraman Exp $
________________________________________________________________________

-*/


#include "gmtpar.h"
#include "keystrs.h"


DefineNameSpaceEnumNames(ODGMT,Shape,3,"Shapes")
{ "Star", "Circle", "Diamond", "Square", "Triangle", "Cross", 0 };


GMTParFactory& GMTPF()
{
    static PtrMan<GMTParFactory> inst = 0;
    if ( !inst )
	inst = new GMTParFactory;

    return *inst;
}


int GMTParFactory::add( const char* nm, GMTParCreateFunc fn )
{
    Entry* entry = getEntry( nm );
    if ( entry )
	entry->crfn_ = fn;
    else
    {
	entry = new Entry( nm, fn );
	entries_ += entry;
    }

    return entries_.size() - 1;
}


GMTPar* GMTParFactory::create( const IOPar& iop ) const
{
    const char* grpname = iop.find( ODGMT::sKeyGroupName );
    if ( !grpname || !*grpname ) return 0;

    Entry* entry = getEntry( grpname );
    if ( !entry ) return 0;

    GMTPar* grp = entry->crfn_( iop );
    return grp;
}


const char* GMTParFactory::name( int idx ) const
{
    if ( idx < 0 || idx >= entries_.size() )
	return 0;

    return entries_[idx]->name_.buf();
}


GMTParFactory::Entry* GMTParFactory::getEntry( const char* nm ) const
{
    for ( int idx=0; idx<entries_.size(); idx++ )
    {
	if ( entries_[idx]->name_ == nm )
	    return const_cast<GMTParFactory*>( this )->entries_[idx];
    }

    return 0;
}

