/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : Jan 2002
-*/

static const char* rcsID = "$Id: visselman.cc,v 1.13 2004-04-14 09:42:03 nanne Exp $";

#include "visselman.h"
#include "visscene.h"
#include "visdataman.h"
#include "thread.h"

visBase::SelectionManager::SelectionManager()
    : selnotifer( this )
    , deselnotifer( this )
    , allowmultiple( false )
    , mutex( *new Threads::Mutex )
{}


visBase::SelectionManager::~SelectionManager()
{ 
    delete &mutex;
}


void visBase::SelectionManager::setAllowMultiple( bool yn )
{
    Threads::MutexLocker lock( mutex );
    while ( !yn && selectedids.size()>1 ) deSelect( selectedids[0], false );
    allowmultiple = yn;
}


void visBase::SelectionManager::rightClicked( int id )
{
    DataObject* dataobj = visBase::DM().getObj( id );
    if ( !dataobj ) return;
    dataobj->triggerRightClick();
}


void visBase::SelectionManager::select( int newid, bool keepoldsel, bool lock )
{
    if ( lock ) mutex.lock();
    if ( selectedids.indexOf( newid ) == -1 )
    {
	if ( !allowmultiple || !keepoldsel )
	    deSelectAll(false);

	DataObject* dataobj = visBase::DM().getObj( newid );

	if ( dataobj )
	{
	    selectedids += newid;
	    dataobj->triggerSel();
	    selnotifer.trigger(newid);
	}
    }

    if ( lock ) mutex.unlock();
}


void visBase::SelectionManager::deSelect( int id, bool lock )
{
    if ( lock ) mutex.lock();

    int idx = selectedids.indexOf( id );
    if ( idx!=-1 )
    {
	selectedids.remove( idx );

	DataObject* dataobj = visBase::DM().getObj( id );
	if ( dataobj )
	{
	    dataobj->triggerDeSel();
	    deselnotifer.trigger(id);
	}

    }

    if ( lock ) mutex.unlock();
}


void visBase::SelectionManager::deSelectAll(bool lock)
{
    if ( lock ) mutex.lock();
    while ( selectedids.size() ) deSelect( selectedids[0], false );
    if ( lock ) mutex.unlock();
}
