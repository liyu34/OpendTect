#pragma once

/*
 ________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	K. Tingdahl
 Date:		June 2012
 ________________________________________________________________________

 */


#include "seistrc.h"
#include "binidvalset.h"
#include "notify.h"
#include "multidimstorage.h"
#include "thread.h"
#include "refcount.h"

class SeisTrcTranslator;
class SeisTrcReader;

/* Request a trace anywhere, and it will become available to you as soon as
   soon as possible. All reading is done int the background.
*/

mExpClass(Seis) SeisRandomProvider : public CallBacker
{
public:
					SeisRandomProvider(const DBKey& mid);
					~SeisRandomProvider();


    void				requestTrace(const BinID&);

    Notifier<SeisRandomProvider>	traceAvailable;
    const SeisTrc&			getTrace() const { return curtrace_; }


protected:
    bool				readTraces();
    void				triggerWork();
    void				readFinished(CallBacker*);

    bool				isreading_;

    Threads::ConditionVar		lock_;

    SeisTrcReader*			reader_;
    SeisTrcTranslator*			translator_;

    SeisTrc				curtrace_;
    BinIDValueSet			wantedbids_;
};



mExpClass(Seis) SeisRandomRepository : public CallBacker
{
public:
				SeisRandomRepository( const DBKey& mid );

    void			addInterest(const BinID&);
    void			removeInterest(const BinID);

    const SeisTrc*			getTrace(const BinID&) const;
    Notifier<SeisRandomRepository>	traceAvailable;
    const BinID&			newTraceBid() const { return newtracebid_; }

protected:

    struct TraceHolder : public RefCount::Referenced
    {
    public:
				TraceHolder() : trc_( 0 ) {}

	SeisTrc*		trc_;
    protected:
			~TraceHolder();
    };

    void				incomingTrace( CallBacker* );


    MultiDimStorage<TraceHolder*>	storage_;
    BinID				newtracebid_;
};
