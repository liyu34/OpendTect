/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A.H. Bril
 * DATE     : 14-9-1998
 * FUNCTION : Batch Program 'driver'
-*/


#include "batchprog.h"

#include "envvars.h"
#include "commandlineparser.h"
#include "file.h"
#include "genc.h"
#include "dbman.h"
#include "dbdir.h"
#include "iopar.h"
#include "moddepmgr.h"
#include "strmprov.h"
#include "filepath.h"
#include "sighndl.h"
#include "hostdata.h"
#include "plugins.h"
#include "strmprov.h"
#include "ioobjctxt.h"
#include "jobcommunic.h"
#include "keystrs.h"
#include "ascstream.h"
#include "debug.h"

#include <iostream>

#ifndef __win__
# include "_execbatch.h"
#endif

#include "oddirs.h"


BatchProgram* BatchProgram::inst_ = 0;

BatchProgram& BP()
{
    if ( !BatchProgram::inst_ )
	BatchProgram::inst_ = new BatchProgram;
    return *BatchProgram::inst_;
}


void BatchProgram::deleteInstance()
{
    delete BatchProgram::inst_;
}


BatchProgram::BatchProgram()
    : NamedCallBacker("Batch Program")
    , stillok_(false)
    , finishmsg_("Finished batch processing.")
    , inbg_(false)
    , sdout_(*new StreamData)
    , iopar_(new IOPar)
    , comm_(0)
    , clparser_(0)
{
}


void BatchProgram::init()
{
    delete clparser_;

    OD::ModDeps().ensureLoaded( "Batch" );

    clparser_ = new CommandLineParser;
    clparser_->setKeyHasValue( OS::MachineCommand::sKeyMasterHost() );
    clparser_->setKeyHasValue( OS::MachineCommand::sKeyMasterPort() );
    clparser_->setKeyHasValue( OS::MachineCommand::sKeyJobID() );
    clparser_->setKeyHasValue( sKeyDataDir() );
    clparser_->setKeyHasValue( OS::CommandExecPars::sKeyPriority() );

    inbg_ = clparser_->hasKey( OS::MachineCommand::sKeyBG() );

    BufferString masterhost;
    clparser_->getVal( OS::MachineCommand::sKeyMasterHost(), masterhost );

    int masterport = -1;
    clparser_->getVal( OS::MachineCommand::sKeyMasterPort(), masterport );

    clparser_->getVal( OS::MachineCommand::sKeyJobID(), jobid_ );

    if ( masterhost.size() && masterport > 0 )  // both must be set.
	comm_ = new JobCommunic( masterhost, masterport, jobid_, sdout_ );

    BufferStringSet normalargs;
    clparser_->getNormalArguments( normalargs );

    BufferString parfilnm;
    for ( int idx=normalargs.size()-1; idx>=0; idx-- )
    {
	const File::Path parfp( normalargs.get(idx) );

	parfilnm = parfp.fullPath();
	parfilnm.replace( '%', ' ' );
	if ( File::exists(parfilnm) )
	    break;

	parfilnm.setEmpty();
    }

    if ( parfilnm.isEmpty() )
    {
	errorMsg( tr("%1: No existing parameter file name specified")
		 .arg(clparser_->getExecutableName()) );
	return;
    }

    setName( parfilnm );
    od_istream odstrm( parfilnm );
    if ( !odstrm.isOK() )
    {
	errorMsg( tr("%1: Cannot open parameter file: %2")
		    .arg( clparser_->getExecutableName() )
		    .arg( parfilnm ));
	return;
    }

    ascistream aistrm( odstrm, true );
    if ( sKey::Pars() != aistrm.fileType() )
    {
	errorMsg( tr("%1: Input file %2 is not a parameter file")
		    .arg( clparser_->getExecutableName() )
		    .arg( parfilnm ));

	od_cerr() << aistrm.fileType() << od_endl;
	return;
    }

    iopar_->getFrom( aistrm );
    odstrm.close();

    if ( iopar_->size() == 0 )
    {
	errorMsg( tr( "%1: Invalid input file %2")
		    .arg( clparser_->getExecutableName() )
		    .arg( parfilnm) );
        return;
    }

    BufferString res = iopar_->find( sKey::LogFile() );
    if ( !res )
	iopar_->set( sKey::LogFile(), StreamProvider::sStdErr() );

#define mSetDataRootVar(str) \
	SetEnvVar( __iswin__ ? "DTECT_WINDATA" : "DTECT_DATA", str );

    if ( clparser_->getVal(sKeyDataDir(),res) && File::isDirectory(res) )
    {
	mSetDataRootVar( res );
	iopar_->set( sKey::DataRoot(), res );
    }

    if ( !iopar_->get(sKey::Survey(),res) )
    {
	errorMsg( tr("Invalid paramater file %1\nSurvey key is missing.")
			.arg( parfilnm ) );
	return;
    }

    uiRetVal uirv = DBM().setDataSource( *iopar_ );
    if ( !uirv.isOK() )
	{ errorMsg( uirv ); return; }

    killNotify( true );
    stillok_ = true;
}


BatchProgram::~BatchProgram()
{
    infoMsg( finishmsg_ );
    DBM().applClosing();

    if ( comm_ )
    {

	JobCommunic::State s = comm_->state();

	bool isSet =  s == JobCommunic::AllDone
	           || s == JobCommunic::JobError
		   || s == JobCommunic::HostError;

	if ( !isSet )
	    comm_->setState( stillok_ ? JobCommunic::AllDone
				    : JobCommunic::HostError );

	bool ok = comm_->sendState( true );

	if ( ok )	infoMsg( "Successfully wrote final status" );
	else		infoMsg( "Could not write final status" );

	comm_->disConnect();
    }

    killNotify( false );

    sdout_.close();
    delete &sdout_;
    delete clparser_;

    // Do an explicit exitProgram() here, so we are sure the program
    // is indeed ended and we won't get stuck while cleaning up things
    // that we don't care about.
    ExitProgram( stillok_ ? 0 : 1 );

    // These will never be reached...
    delete iopar_;
    delete comm_;
}


void BatchProgram::progKilled( CallBacker* )
{
    infoMsg( "BatchProgram Killed." );

    if ( comm_ )
    {
	comm_->setState( JobCommunic::Killed );
	comm_->sendState( true );
    }

    killNotify( false );

#ifdef __debug__
    abort();
#endif
}


void BatchProgram::killNotify( bool yn )
{
    CallBack cb( mCB(this,BatchProgram,progKilled) );

    if ( yn )
	SignalHandling::startNotify( SignalHandling::Kill, cb );
    else
	SignalHandling::stopNotify( SignalHandling::Kill, cb );
}


bool BatchProgram::pauseRequested() const
    { return comm_ ? comm_->pauseRequested() : false; }


bool BatchProgram::errorMsg( const uiString& msg, bool cc_stderr )
{
    if ( sdout_.ostrm )
	*sdout_.ostrm << '\n' << msg.getFullString() << '\n' << std::endl;

    if ( !sdout_.ostrm || cc_stderr )
	std::cerr << '\n' << msg.getFullString() << '\n' << std::endl;

    if ( comm_ && comm_->ok() ) return comm_->sendErrMsg(msg.getFullString());

    return true;
}


bool BatchProgram::infoMsg( const char* msg, bool cc_stdout )
{
    if ( sdout_.ostrm )
	*sdout_.ostrm << '\n' << msg << '\n' << std::endl;

    if ( !sdout_.ostrm || cc_stdout )
	od_cout() << '\n' << msg << '\n' << od_endl;

    return true;
}



bool BatchProgram::initOutput()
{
    stillok_ = false;
    if ( comm_ && !comm_->sendPID(GetPID()) )
    {
	errorMsg( tr("Could not contact master. Exiting."), true );
	exit( 0 );
    }

    BufferString res = pars().find( sKey::LogFile() );
    if ( res == "stdout" ) res.setEmpty();

    if ( res && res=="window" )
    {
	BufferString cmd = "@";
	cmd += "\"";
	cmd += File::Path(GetExecPlfDir()).add("od_ProgressViewer").fullPath();
	cmd += "\" ";

	cmd += GetPID();
	StreamProvider sp( cmd );
	sdout_ = sp.makeOStream();
	if ( !sdout_.usable() )
	{
	    std::cerr << name() << ": Cannot open window for output"<<std::endl;
	    std::cerr << "Using std output instead" << std::endl;
	    res = 0;
	}
    }

    if ( res != "window" )
    {
	if ( res.isEmpty() )
	    res = StreamProvider::sStdErr();
	sdout_ = StreamProvider(res).makeOStream();
	if ( !sdout_.ostrm )
	{
	    std::cerr << name() << ": Cannot open log file" << std::endl;
	    std::cerr << "Using stderror instead" << std::endl;
	    sdout_.ostrm = &std::cerr;
	}
    }

    stillok_ = sdout_.usable();
    if ( stillok_ )
	PIM().loadAuto( true );
    return stillok_;
}


IOObj* BatchProgram::getIOObjFromPars(	const char* bsky, bool mknew,
					const IOObjContext& ctxt,
					bool msgiffail ) const
{
    uiString errmsg;
    IOObj* ioobj = DBM().getFromPar( pars(), bsky, ctxt, mknew, errmsg );
    if ( !ioobj && msgiffail )
    {
	if ( errmsg.isEmpty() )
	    errmsg = tr("Error getting DB info");
	*sdout_.ostrm << errmsg.getFullString().buf() << std::endl;
    }

    return ioobj;
}
