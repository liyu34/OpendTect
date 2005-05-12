/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : Mar 2000
-*/

static const char* rcsID = "$Id: od_process_attrib.cc,v 1.2 2005-05-12 10:54:50 cvshelene Exp $";

#include "attribstorprovider.h"
#include "attribdescset.h"
#include "attribdescsettr.h"
#include "attribprocessor.h"
#include "attribengman.h"
#include "attriboutput.h"
#include "seis2dline.h"
#include "seisjobexecprov.h"
#include "iopar.h"
#include "ioman.h"
#include "ioobj.h"
#include "ptrman.h"
#include "progressmeter.h"
#include "batchprog.h"
#include "hostdata.h"
#include "separstr.h"
#include "socket.h"
#include "timefun.h"
#include "filegen.h"
#include "filepath.h"

#include "attribfactory.h"
#include "attrfact.h"

#define mDestroyWorkers \
	{ delete proc; proc = 0; }

defineTranslatorGroup(AttribDescSet,"Attribute definitions");
/*	
    { delete proc; proc = 0; }
    
    Don't delete AttribDescSetProcessor, because it takes too much time 
    ( >2 min. ), especially the destruction of the SeisTrcBuf. 
*/


#define mErrRet(s) \
{ \
    strm << ( stepout ? "0 0" : "0" ) << std::endl; \
    bp.errorMsg(s,true); \
    return false; \
}

static bool attribSetQuery( std::ostream& strm, const IOPar& iopar,
			    BatchProgram& bp, bool stepout )
{
    Attrib::DescSet initialset;
    PtrMan<IOPar> attribs = iopar.subselect("Attributes");
    if ( !initialset.usePar( *attribs ) )
	mErrRet( initialset.errMsg() )

    const char* res = iopar.find( "Output.1.Attributes.0" );
    if ( !res )
	mErrRet( "No target attribute found" )
    int outid = atoi( res );
    if ( !initialset.getDesc(outid) )
	mErrRet( "Target attribute not present in attribute set" )

    return true;
}


#define mRetError(s)	{ errorMsg(s); mDestroyWorkers; return false; }

#define mRetHostErr(s)	{  \
    if ( comm ) comm->setState( MMSockCommunic::HostError ); \
    mRetError(s) \
}

#define mRetJobErr(s)	 {  \
    if ( comm ) comm->setState( MMSockCommunic::JobError ); \
    mRetError(s) \
}
	  
bool BatchProgram::go( std::ostream& strm )
{
    Attrib::initAttribClasses();
    if ( cmdLineOpts().size() )
    {
	BufferString opt = *cmdLineOpts()[0];
	bool ismaxstepout = opt == "maxstepout";
	if ( ismaxstepout || opt == "validate" )
	    return attribSetQuery( strm, pars(), *this, ismaxstepout );
    }

    strm << "Processing on " << HostData::localHostName();
    strm << '.' << std::endl;

    Attrib::Processor* proc = 0;
    
    const char* tempdir = pars().find(SeisJobExecProv::sKeyTmpStor);

    if ( tempdir && *tempdir )
    {
	if ( !File_exists(tempdir) )
	{
	    BufferString msg(SeisJobExecProv::sKeyTmpStor);
	    msg += " ("; msg += tempdir; msg += ") does not exist.";
	    mRetHostErr( msg );
	}

	if ( !File_isDirectory(tempdir) )
	{
	    BufferString msg(SeisJobExecProv::sKeyTmpStor);
	    msg += " ("; msg += tempdir; msg += ") is not a directory.";
	    mRetHostErr( msg );
	}

	if ( !File_isWritable(tempdir) )
	{
	    BufferString msg(SeisJobExecProv::sKeyTmpStor);
	    msg += " ("; msg += tempdir; msg += ") is not writeable.";
	    mRetHostErr( msg );
	}
    }

    Seis2DLineSet::installPreSet( pars(), SeisJobExecProv::sKeyOutputLS,
	    			  SeisJobExecProv::sKeyWorkLS );

    const char* selspec = pars().find( "Output.1.In-line range" );
    if ( selspec && *selspec )
    {
	FileMultiString fms( selspec );
	int lnr = atoi( fms[0] );
	if ( lnr == atoi( fms[1] ) )
	    strm << "Calculating for in-line " << lnr << '.' << std::endl;
    }
    strm << std::endl;

    strm << "Preparing processing"; strm.flush();
    const char* seisid = pars().find( "Output.1.Seismic ID" );
    if ( !seisid )
	strm << " ..." << std::endl;
    else
    {
	PtrMan<IOObj> ioobj = IOM().get( seisid );
	if ( !ioobj )
	{
	    BufferString msg("Cannot find output Seismic Object! for '");
	    msg += seisid;
	    msg += "' ...";
	    mRetHostErr( msg );
	}

	FilePath fp( ioobj->fullUserExpr(false) );
	if ( !fp.isAbsolute() )
	{
	    fp.set( ioobj->dirName() );
	    fp.add( ioobj->fullUserExpr(false) );
	}
	BufferString dirnm = fp.pathOnly();
	if ( !File_isDirectory(dirnm) || !File_isWritable(dirnm) )
	{
	    BufferString msg("Output directory for '");
	    msg += ioobj->name();
	    msg += "' (";
	    msg += dirnm;
	    msg += File_isDirectory(dirnm) ? ") is not writeable." 
					   : ") does not exist.";
	    mRetHostErr( msg );
	}
    }
    strm.flush();

    Attrib::StorageProvider::initClass();
    Attrib::DescSet attribset;
    const char* setid = pars().find("Attribute Set");
    if ( setid && *setid )
    {
	PtrMan<IOObj> ioobj = IOM().get( setid );
	if ( !ioobj )
	    mRetHostErr( "Cannot find provided attrib set ID" )
	BufferString msg;
	if ( !AttribDescSetTranslator::retrieve(attribset,ioobj,msg) )
	    mRetJobErr( msg );
    }
    else
    {
	PtrMan<IOPar> attribs = pars().subselect("Attributes");
	if ( !attribset.usePar( *attribs ) )
	    mRetJobErr( attribset.errMsg() )
    }

    PtrMan<IOPar> outputs = pars().subselect("Output");

    if ( !outputs )
	mRetJobErr( "No outputs found" )

    Attrib::EngineMan* attrengman = new Attrib::EngineMan();
    //from here delete things related to mult 2dlines next line to inprove
    int indexoutp = 0;
    const char* linename;
    while ( true )
    {
        BufferString multoutpstr = IOPar::compKey( "Output", indexoutp );
        PtrMan<IOPar> output = pars().subselect( multoutpstr );
        if ( !output )
        {
            if ( !indexoutp )
		{ indexoutp++; continue; }
	    else
	        break;
	}
	linename = output->find("Line key");
	indexoutp++;
    }    

    proc = attrengman->usePar( pars(), attribset, linename );
    
    ProgressMeter progressmeter(strm);

    bool cont = true;
    bool loading = true;

    if ( comm )
    {
	comm->setState(MMSockCommunic::Working);   
	if( !comm->sendState() ) mRetHostErr( comm->errMsg() )	    
    }

    const double sleeptime = getenv("OD_BATCH_SLEEP_TIME") ?
			     atof( getenv("OD_BATCH_SLEEP_TIME") ) : 1;
    double startup_wait = 0;
    pars().get( "Startup delay time", startup_wait );
    Time_sleep( startup_wait );  

    int nriter = 0;
    while ( true )
    {
	bool paused = false;

	if ( pauseRequested() )
	{ 
	    paused = true;

	    if ( comm )
	    {
		comm->setState( MMSockCommunic::Paused );
		if ( !comm->updateState() ) mRetHostErr( comm->errMsg() )
	    }

	    Time_sleep( sleeptime );  
	}
	else
	{
	    if ( paused )
	    {
		paused = false;
		if ( comm )
		{
		    comm->setState( MMSockCommunic::Working );
		    if ( !comm->updateState() ) mRetHostErr( comm->errMsg())
		}
	    }
	    int res = proc->nextStep();

	    if ( nriter==0 )
	    {
		strm << std::endl;
		strm << "Estimated number of positions to be processed"
		     <<"(regular survey): "<< proc->totalNr() 
		     << std::endl;
		strm << "Loading cube data ..." << std::endl;
	    }

	    if ( res > 0 )
	    {
		if ( loading )
		{
		    loading = false;
		    strm << "\n["<<getPID()<<"]: Processing started."
			 << std::endl;
		}

		if ( comm && !comm->updateProgress( nriter + 1 ) )
		    mRetHostErr( comm->errMsg() )

		++progressmeter;
	    }
	    else
	    {
		if ( res == -1 )
		    mRetJobErr( "Cannot reach next position" )
		break;
	    }
	}
	nriter++;
    }

    strm << "\n["<<getPID()<<"]: Processing done. Closing up." << std::endl;

    // It is VERY important workers are destroyed BEFORE the last sendState!!!
    mDestroyWorkers
    progressmeter.finish();

    strm << "\n["<<getPID()<<"]: Threads closed. Writing finish status"
	 << std::endl;

    if ( !comm ) return true;

    comm->setState( MMSockCommunic::Finished );
    bool ret = comm->sendState();

    if ( ret )
	strm << "[" <<getPID()<< "]: Successfully wrote finish status."
	     << std::endl;
    else
    strm << "[" <<getPID()<< "]: Could not write finish status."
	 << std::endl;

    return ret;
}


