/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : A.H. Bril
 * DATE     : Nov 2006
-*/

static const char* rcsID = "$Id: seisimporter.cc,v 1.13 2007-10-17 04:46:59 cvsnanne Exp $";

#include "seisimporter.h"
#include "seisbuf.h"
#include "seistrc.h"
#include "seiscbvs.h"
#include "seiswrite.h"
#include "cbvsreadmgr.h"
#include "iostrm.h"
#include "conn.h"
#include "survinfo.h"
#include "binidsorting.h"
#include "ptrman.h"
#include "filegen.h"
#include "filepath.h"
#include "errh.h"


SeisImporter::SeisImporter( SeisImporter::Reader* r, SeisTrcWriter& w,
			    Seis::GeomType gt )
    	: Executor("Importing seismic data")
    	, rdr_(r)
    	, wrr_(w)
    	, geomtype_(gt)
    	, buf_(*new SeisTrcBuf(false))
    	, trc_(*new SeisTrc)
    	, state_( Seis::isPS(gt) ? ReadWrite : ReadBuf )
    	, nrread_(0)
    	, nrwritten_(0)
    	, nrskipped_(0)
    	, postproc_(0)
	, sortanal_(new BinIDSortingAnalyser(Seis::is2D(gt)))
	, sorting_(0)
	, prevbid_(*new BinID(mUdf(int),mUdf(int)))
{
}


SeisImporter::~SeisImporter()
{
    buf_.deepErase();

    delete rdr_;
    delete sorting_;
    delete sortanal_;
    delete postproc_;

    delete &buf_;
    delete &trc_;
    delete &prevbid_;
}


const char* SeisImporter::message() const
{
    if ( postproc_ ) return postproc_->message();
    if ( !errmsg_.isEmpty() )
	return errmsg_;

    if ( hndlmsg_.isEmpty() )
	{ hndlmsg_ = "Importing from "; hndlmsg_ += rdr_->name(); }
    return hndlmsg_;
}


int SeisImporter::nrDone() const
{
    if ( postproc_ ) return postproc_->nrDone();
    return state_ == ReadBuf ? nrread_ : nrwritten_;
}


const char* SeisImporter::nrDoneText() const
{
    if ( postproc_ ) return postproc_->nrDoneText();
    return state_ == ReadBuf ? "Traces read" : "Traces written";
}


int SeisImporter::totalNr() const
{
    if ( postproc_ ) return postproc_->totalNr();
    return rdr_->totalNr();
}


#define mDoRead(trc) \
    bool atend = false; \
    if ( rdr_->fetch(trc) ) \
	nrread_++; \
    else \
    { \
	errmsg_ = rdr_->errmsg_; \
	if ( !errmsg_.isEmpty() ) \
	    return Executor::ErrorOccurred; \
	atend = true; \
    }


int SeisImporter::nextStep()
{
    if ( postproc_ ) return postproc_->doStep();

    if ( state_ == WriteBuf )
    {
	if ( buf_.isEmpty() )
	    state_ = ReadWrite;
	else
	{
	    PtrMan<SeisTrc> trc = buf_.remove( ((int)0) );
	    return doWrite( *trc );
	}
    }

    if ( state_ == ReadWrite )
    {
	mDoRead( trc_ )
	if ( !atend )
	    return sortingOk(trc_) ? doWrite(trc_) : Executor::ErrorOccurred;

	postproc_ = mkPostProc();
	if ( !errmsg_.isEmpty() )
	    return Executor::ErrorOccurred;
	return postproc_ ? Executor::MoreToDo : Executor::Finished;
    }

    return readIntoBuf();
}


bool SeisImporter::needInlCrlSwap() const
{
    return !Seis::is2D(geomtype_) && sorting_ && !sorting_->inlSorted();
}


int SeisImporter::doWrite( SeisTrc& trc )
{
    if ( needInlCrlSwap() )
	Swap( trc.info().binid.inl, trc.info().binid.crl );

    if ( wrr_.put(trc) )
    {
	nrwritten_++;
	return Executor::MoreToDo;
    }

    errmsg_ = wrr_.errMsg();
    if ( errmsg_.isEmpty() )
    {
	pErrMsg( "Need an error message from writer" );
	errmsg_ = "Cannot write trace";
    }

    return Executor::ErrorOccurred;
}


int SeisImporter::readIntoBuf()
{
    SeisTrc* trc = new SeisTrc;
    mDoRead( *trc )
    if ( atend )
    {
	delete trc;
	if ( nrread_ == 0 )
	{
	    errmsg_ = "No valid traces in input";
	    return Executor::ErrorOccurred;
	}

	state_ = buf_.isEmpty() ? ReadWrite : WriteBuf;
	return Executor::MoreToDo;
    }

    if ( !Seis::is2D(geomtype_) && !SI().isReasonable(trc->info().binid) )
    {
	delete trc;
	nrskipped_++;
    }
    else
    {
	buf_.add( trc );
	if ( !sortingOk(*trc) )
	    return Executor::ErrorOccurred;
	if ( !sortanal_ )
	    state_ = WriteBuf;
    }

    return Executor::MoreToDo;
}


bool SeisImporter::sortingOk( const SeisTrc& trc )
{
    const bool is2d = Seis::is2D(geomtype_);
    BinID bid( trc.info().binid );
    if ( is2d )
    {
	bid.crl = trc.info().nr;
	bid.inl = prevbid_.inl;
	if ( mIsUdf(bid.inl) )
	    bid.inl = 0;
	else if ( trc.info().new_packet )
	    bid.inl = prevbid_.inl + 1;
    }

    bool rv = true;
    if ( sorting_ )
    {
	if ( !sorting_->isValid(prevbid_,bid) )
	{
	    if ( is2d )
	    {
		errmsg_ = "Importing stopped at trace number: ";
		errmsg_ += trc.info().nr;
		errmsg_ += "\nbecause before this trace, the rule was:\n";
	    }
	    else
	    {
		char buf[30]; bid.fill( buf );
		errmsg_ = "Importing stopped because trace position found: ";
		errmsg_ += buf;
		errmsg_ += "\nviolates previous trace sorting:\n";
	    }
	    errmsg_ += sorting_->description();
	    rv = false;
	}
    }
    else
    {
	if ( sortanal_->add(bid) )
	{
	    sorting_ = new BinIDSorting( sortanal_->getSorting() );
	    delete sortanal_; sortanal_ = 0;
	}
	else if ( *sortanal_->errMsg() )
	{
	    errmsg_ = sortanal_->errMsg();
	    rv = false;
	}
    }

    prevbid_ = bid;
    return rv;
}


class SeisInlCrlSwapper : public Executor
{
public:

SeisInlCrlSwapper( const char* inpfnm, IOObj* out, int nrtrcs )
    : Executor( "Swapping In/X-line" )
    , tri_(CBVSSeisTrcTranslator::getInstance())
    , targetfnm_(inpfnm)
    , wrr_(0)
    , out_(out)
    , nrdone_(0)
    , totnr_(nrtrcs)
{
    if ( !tri_->initRead(new StreamConn(inpfnm,Conn::Read)) )
	{ errmsg_ = tri_->errMsg(); return; }
    geom_ = &tri_->readMgr()->info().geom;
    linenr_ = geom_->start.crl;
    trcnr_ = geom_->start.inl - geom_->step.inl;

    wrr_ = new SeisTrcWriter( out_ );
    if ( *wrr_->errMsg() )
	{ errmsg_ = wrr_->errMsg(); return; }

    nrdone_++;
}

~SeisInlCrlSwapper()
{
    delete tri_;
    delete wrr_;
    delete out_;
}

const char* message() const
{
    return errmsg_.isEmpty() ? "Re-sorting traces" : ((const char*)errmsg_);
}
const char* nrDoneText() const	{ return "Traces handled"; }
int nrDone() const		{ return nrdone_; }
int totalNr() const		{ return totnr_; }

int nextStep()
{
    if ( !errmsg_.isEmpty() )
	return Executor::ErrorOccurred;

    trcnr_ += geom_->step.inl;
    if ( trcnr_ > geom_->stop.inl )
    {
	linenr_ += geom_->step.crl;
	if ( linenr_ > geom_->stop.crl )
	    return doFinal();
	else
	    trcnr_ = geom_->start.inl;
    }

    if ( tri_->goTo(BinID(trcnr_,linenr_)) )
    {
	if ( !tri_->read(trc_) )
	{
	    errmsg_ = "Cannot read ";
	    errmsg_ += linenr_; errmsg_ += "/"; errmsg_ += trcnr_;
	    errmsg_ += ":\n"; errmsg_ += tri_->errMsg();
	    return Executor::ErrorOccurred;
	}

	Swap( trc_.info().binid.inl, trc_.info().binid.crl );
	trc_.info().coord = SI().transform( trc_.info().binid );

	if ( !wrr_->put(trc_) )
	{
	    errmsg_ = "Cannot write ";
	    errmsg_ += linenr_; errmsg_ += "/"; errmsg_ += trcnr_;
	    errmsg_ += ":\n"; errmsg_ += wrr_->errMsg();
	    return Executor::ErrorOccurred;
	}
	nrdone_++;
    }
    return Executor::MoreToDo;
}

int doFinal()
{
    delete wrr_; wrr_ = 0;
    const BufferString tmpfnm( out_->fullUserExpr(false) );

    if ( nrdone_ < 1 )
    {
	errmsg_ = "No traces written during re-sorting.\n";
	errmsg_ += "The imported cube remains to have swapped in/crosslines";
	File_remove( tmpfnm, NO );
	return Executor::ErrorOccurred;
    }

    if ( !File_remove(targetfnm_,NO) || !File_rename(tmpfnm,targetfnm_) )
    {
	errmsg_ = "Cannot rename the swapped in/crossline cube";
	errmsg_ += "Please rename (by hand):\n";
	errmsg_ += tmpfnm; errmsg_ += "\nto:"; errmsg_ += targetfnm_;
	return Executor::ErrorOccurred;
    }

    return Executor::Finished;
}

    BufferString		targetfnm_;
    CBVSSeisTrcTranslator*	tri_;
    SeisTrcWriter*		wrr_;
    const CBVSInfo::SurvGeom*	geom_;
    IOObj*			out_;
    SeisTrc			trc_;

    BufferString		errmsg_;
    int				nrdone_;
    int				totnr_;
    int				linenr_;
    int				trcnr_;

};


Executor* SeisImporter::mkPostProc()
{
    errmsg_ = "";
    if ( needInlCrlSwap() )
    {
	const IOObj* targetioobj = wrr_.ioObj();
	mDynamicCastGet(const IOStream*,targetiostrm,targetioobj)
	if ( !targetiostrm )
	{
	    errmsg_ = "Your data was loaded with swapped inline/crossline\n"
		      "Please use the batch program 'cbvs_swap_inlcrl' now\n";
	    return 0;
	}

	FilePath fp( targetiostrm->getExpandedName(false) );
	const BufferString targetfnm( fp.fullPath() );
	FilePath fptemp( FilePath::getTempName("cbvs") );
	fp.setFileName( fptemp.fileName() );
	const BufferString tmpfnm( fp.fullPath() );
	IOStream* tmpiostrm = (IOStream*)targetiostrm->clone();
	tmpiostrm->setFileName( tmpfnm );
	return new SeisInlCrlSwapper( targetfnm, tmpiostrm, nrwritten_ );
    }
    return 0;
}
