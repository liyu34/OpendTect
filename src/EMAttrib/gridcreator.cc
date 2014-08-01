/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2010
________________________________________________________________________

-*/

static const char* rcsID mUsedVar = "$Id$";


#include "gridcreator.h"

#include "cubesampling.h"
#include "emmanager.h"
#include "emhorizon3d.h"
#include "emhorizon2d.h"
#include "grid2d.h"
#include "hor2dfrom3dcreator.h"
#include "ioman.h"
#include "ioobj.h"
#include "iopar.h"
#include "keystrs.h"
#include "multiid.h"
#include "randomlinegeom.h"
#include "seisrandlineto2d.h"
#include "seisread.h"
#include "seisselectionimpl.h"
#include "seis2dline.h"
#include "seistrc.h"
#include "seiswrite.h"
#include "separstr.h"
#include "survgeom2d.h"


const char* Seis2DGridCreator::sKeyOverWrite()	{ return "Do Overwrite"; }
const char* Seis2DGridCreator::sKeyInput()	{ return "Input ID"; }
const char* Seis2DGridCreator::sKeyOutput()	{ return "Output ID"; }
const char* Seis2DGridCreator::sKeySelType()	{ return "Selection Type"; }
const char* Seis2DGridCreator::sKeyOutpAttrib() { return "Output Attribute"; }
const char* Seis2DGridCreator::sKeyInlSelType() { return "Inl Sel Type"; }
const char* Seis2DGridCreator::sKeyCrlSelType() { return "Crl Sel Type"; }

const char* Seis2DGridCreator::sKeyBaseLine()	{ return "BaseLine"; }
const char* Seis2DGridCreator::sKeyStartBinID() { return "Start BinID"; }
const char* Seis2DGridCreator::sKeyStopBinID()	{ return "Stop BinID"; }
const char* Seis2DGridCreator::sKeyInlSpacing() { return "Inl Spacing"; }
const char* Seis2DGridCreator::sKeyCrlSpacing() { return "Crl Spacing"; }

const char* Seis2DGridCreator::sKeyInlPrefix()	{ return "Inl Prefix"; }
const char* Seis2DGridCreator::sKeyCrlPrefix()	{ return "Crl Prefix"; }

class Seis2DLineCreator : public Executor
{
public:
			Seis2DLineCreator(const IOObj& input,
					  const CubeSampling&,
					  const IOObj& output,
					  Pos::GeomID gepmid);
			~Seis2DLineCreator();

    uiString		uiMessage() const	{ return msg_; }
    virtual od_int64	nrDone() const		{ return nrdone_; }
    virtual od_int64	totalNr() const		{ return totalnr_; }
    virtual int		nextStep();

protected:
    od_int64		nrdone_;
    od_int64		totalnr_;
    uiString		msg_;

    SeisTrcReader*	rdr_;
    SeisTrcWriter*	wrr_;
};


Seis2DLineCreator::Seis2DLineCreator( const IOObj& input,
    const CubeSampling& cs, const IOObj& output, Pos::GeomID geomid )
    : Executor("Creating 2D line")
    , nrdone_(0)
    , totalnr_(cs.hrg.totalNr())
{
    rdr_ = new SeisTrcReader( &input );
    rdr_->prepareWork();
    rdr_->setSelData( new Seis::RangeSelData(cs) );

    wrr_ = new SeisTrcWriter( &output );
    Seis::SelData* seldata = Seis::SelData::get( Seis::Range );
    if ( seldata )
    {
	seldata->setGeomID( geomid );
	wrr_->setSelData( seldata );
    }
}


Seis2DLineCreator::~Seis2DLineCreator()
{
    delete rdr_;
    delete wrr_;
}


int Seis2DLineCreator::nextStep()
{
    SeisTrc trc;
    const int res = rdr_->get( trc.info() );
    if ( res == -1 )
	{ msg_ = rdr_->errMsg(); return ErrorOccurred(); }
    if ( res == 0 )
	return Finished();
    if ( res == 2 )
	return MoreToDo();

    if ( !rdr_->get(trc) )
    {
	msg_ = "Error reading input trace\n";
	msg_.append( rdr_->errMsg() );
	return ErrorOccurred();
    }

    if ( !wrr_->put(trc) )
    {
	msg_ = "Error writing output trace\n";
	msg_.append( wrr_->errMsg() );
	return ErrorOccurred();
    }

    nrdone_++;
    return MoreToDo();
}



Seis2DGridCreator::Seis2DGridCreator( const IOPar& par )
    : ExecutorGroup("Creating Seis 2D Grid")
{
    init( par );
}


Seis2DGridCreator::~Seis2DGridCreator()
{}

uiString Seis2DGridCreator::uiNrDoneText() const
{ return "Traces done"; }

bool Seis2DGridCreator::init( const IOPar& par )
{
    MultiID key;
    par.get( Seis2DGridCreator::sKeyInput(), key );
    PtrMan<IOObj> input = IOM().get( key );

    par.get( Seis2DGridCreator::sKeyOutput(), key );
    PtrMan<IOObj> output = IOM().get( key );
    if ( !input || !output )
	return false;

    CubeSampling bbox;
    PtrMan<IOPar> subselpar = par.subselect( sKey::Subsel() );
    if ( subselpar )
	bbox.usePar( *subselpar );

    BufferString seltype;
    par.get( sKeySelType(), seltype );
    return seltype == "InlCrl" ? initFromInlCrl(par,*input,*output,bbox)
			       : initFromRandomLine(par,*input,*output,bbox);
}

#define mHandleLineGeom \
uiString errmsg; \
Pos::GeomID geomid = Survey::GM().getGeomID( linenm ); \
const bool islinepresent = geomid != Survey::GeometryManager::cUndefGeomID(); \
if ( !islinepresent ) \
{ \
    PosInfo::Line2DData* l2d = new PosInfo::Line2DData( linenm ); \
    Survey::Geometry2D* newgoem2d = new Survey::Geometry2D( l2d ); \
    newgoem2d->ref(); \
    geomid = Survey::GMAdmin().addNewEntry( newgoem2d, errmsg ); \
    newgoem2d->unRef(); \
    if ( geomid == Survey::GeometryManager::cUndefGeomID() ) \
    { \
	failedlines_.add( linenm ); \
	continue; \
    } \
} \
else if ( islinepresent && dooverwrite ) \
{ \
    Survey::Geometry* geom = Survey::GMAdmin().getGeometry( geomid ); \
    mDynamicCastGet(Survey::Geometry2D*,geom2d,geom); \
    geom2d->dataAdmin().setEmpty(); \
}

bool Seis2DGridCreator::initFromInlCrl( const IOPar& par,
					const IOObj& input, const IOObj& output,
					const CubeSampling& bbox )
{
    BufferString attribname;
    par.get( sKey::Attribute(), attribname );

    TypeSet<int> inlines;
    BufferString mode;
    par.get( Seis2DGridCreator::sKeyInlSelType(), mode );
    if ( mode == sKey::Range() )
    {
	StepInterval<int> range;
	par.get( sKey::InlRange(), range );
	for ( int idx=0; idx<=range.nrSteps(); idx++ )
	    inlines += range.atIndex( idx );
    }
    else
    {
	SeparString str( par.find(sKey::InlRange()) );
	const int nrinls = str.size();
	for ( int idx=0; idx<nrinls; idx++ )
	    inlines += str.getIValue(idx);
    }

    bool dooverwrite = false;
    if ( par.find(sKeyOverWrite()) )
	par.getYN( sKeyOverWrite(), dooverwrite );
    FixedString inlstr = par.find( sKeyInlPrefix() );
    deepErase( failedlines_ );
    for ( int idx=0; idx<inlines.size(); idx++ )
    {
	CubeSampling cs = bbox;
	cs.hrg.start.inl() = cs.hrg.stop.inl() = inlines[idx];
	BufferString linenm( inlstr.str() );
	linenm.add( inlines[idx] );

	mHandleLineGeom
	add( new Seis2DLineCreator(input,cs,output,geomid) );
    }

    TypeSet<int> crosslines;
    par.get( Seis2DGridCreator::sKeyCrlSelType(), mode );
    if ( mode == sKey::Range() )
    {
	StepInterval<int> range;
	par.get( sKey::CrlRange(), range );
	for ( int idx=0; idx<=range.nrSteps(); idx++ )
	    crosslines += range.atIndex( idx );
    }
    else
    {
	SeparString str( par.find(sKey::CrlRange()) );
	for ( int idx=0; idx<str.size(); idx++ )
	    crosslines += str.getIValue(idx);
    }

    FixedString crlstr = par.find( sKeyCrlPrefix() );
    for ( int idx=0; idx<crosslines.size(); idx++ )
    {
	CubeSampling cs = bbox;
	cs.hrg.start.crl() = cs.hrg.stop.crl() = crosslines[idx];
	BufferString linenm( crlstr.str() );
	linenm.add( crosslines[idx] );
	mHandleLineGeom
	add( new Seis2DLineCreator(input,cs,output,geomid) );
    }

    return true;
}


bool Seis2DGridCreator::initFromRandomLine( const IOPar& par,
					const IOObj& input, const IOObj& output,
					const CubeSampling& bbox )
{
    PtrMan<IOPar> baselinepar = par.subselect( sKeyBaseLine() );
    if ( !baselinepar )
	return false;

    BinID start, stop;
    if ( !baselinepar->get(sKeyStartBinID(),start)
	    || !baselinepar->get(sKeyStopBinID(),stop) )
	return false;

    Grid2D::Line baseline( start, stop );
    double pardist, perdist;
    if ( !par.get(sKeyInlSpacing(),pardist)
	    || !par.get(sKeyCrlSpacing(),perdist) )
	return false;

    Grid2D grid;
    grid.set( baseline, pardist, perdist, bbox.hrg );
    if ( !grid.totalSize() )
	return false;

    deepErase( failedlines_ );
    BufferString attribname;
    par.get( sKey::Attribute(), attribname );
    bool dooverwrite = false;
    if ( par.find(sKeyOverWrite()) )
	par.getYN( sKeyOverWrite(), dooverwrite );

    FixedString parstr = par.find( sKeyInlPrefix() );
    for ( int idx=0; idx<grid.size(true); idx++ )
    {
	const Grid2D::Line* line = grid.getLine( idx, true );
	if ( !line )
	    continue;

	BufferString linenm( parstr.str() );
	linenm.add( idx );
	mHandleLineGeom

	Geometry::RandomLine rdl;
	rdl.addNode( line->start_ );
	rdl.addNode( line->stop_ );
	add( new SeisRandLineTo2D(input,output,geomid,1,rdl) );
    }

    FixedString perstr = par.find( sKeyCrlPrefix() );
    for ( int idx=0; idx<grid.size(false); idx++ )
    {
	const Grid2D::Line* line = grid.getLine( idx, false );
	if ( !line )
	    continue;

	BufferString linenm( perstr.str() );
	linenm.add( idx );
	mHandleLineGeom
	Geometry::RandomLine rdl;
	rdl.addNode( line->start_ );
	rdl.addNode( line->stop_ );
	add( new SeisRandLineTo2D(input,output,geomid,1,rdl) );
    }

    return true;
}



bool Seis2DGridCreator::hasWarning( BufferString& msg ) const
{
    msg.setEmpty();
    if ( failedlines_.isEmpty() )
	return false;
    msg.set( "Warning : Following lines could not be created " );
    msg += failedlines_.getDispString();
    return true;
}


// ---- Horizon2DGridCreator ----
const char* Horizon2DGridCreator::sKeyInputIDs() { return "Input IDs"; }
const char* Horizon2DGridCreator::sKeySeisID()	 { return "Seis ID"; }
const char* Horizon2DGridCreator::sKeyPrefix()	 { return "Name Prefix"; }

Horizon2DGridCreator::Horizon2DGridCreator()
    : ExecutorGroup("Extracting 2D horizons")
{}

Horizon2DGridCreator::~Horizon2DGridCreator()
{
    deepUnRef ( horizons_ );
}

od_int64 Horizon2DGridCreator::totalNr() const
{ return totalnr_; }

od_int64 Horizon2DGridCreator::nrDone() const
{ return nrdone_; }

uiString Horizon2DGridCreator::uiNrDoneText() const
{ return "Positions done"; }


bool Horizon2DGridCreator::init( const IOPar& par, TaskRunner* taskrunner )
{
    BufferString prefix;
    par.get( Horizon2DGridCreator::sKeyPrefix(), prefix );

    MultiID lsid;
    par.get( Horizon2DGridCreator::sKeySeisID(), lsid );
    PtrMan<IOObj> lsioobj = IOM().get( lsid );
    if ( !lsioobj ) return false;

    BufferStringSet linenames;
    Seis2DLineSet seislineset( *lsioobj );
    for ( int idx=0; idx<seislineset.nrLines(); idx++ )
	linenames.add( seislineset.lineName(idx) );

    BufferStringSet horids;
    par.get( Horizon2DGridCreator::sKeyInputIDs(), horids );

    EM::EMManager& em = EM::EMM();
    for ( int idx=0; idx<horids.size(); idx++ )
    {
	MultiID mid( horids.get(idx) );
	RefMan<EM::EMObject> emobj = em.loadIfNotFullyLoaded( mid, taskrunner );

	mDynamicCastGet(EM::Horizon3D*,horizon3d,emobj.ptr());
	if ( !horizon3d ) continue;

	horizon3d->ref();
	BufferString hornm( prefix, " ", horizon3d->name() );
	EM::ObjectID emid = em.createObject( EM::Horizon2D::typeStr(),hornm );
	mDynamicCastGet(EM::Horizon2D*,horizon2d,em.getObject(emid));
	if ( !horizon2d ) continue;

	horizon2d->ref();
	horizons_ += horizon2d;
	Hor2DFrom3DCreatorGrp* creator =
	    new Hor2DFrom3DCreatorGrp( *horizon3d, *horizon2d );
	creator->init( linenames, lsioobj->name() );
	add( creator );
	horizon3d->unRef();
    }

    return true;
}


bool Horizon2DGridCreator::finish( TaskRunner* taskrunner )
{
    for ( int idx=0; idx<horizons_.size(); idx++ )
    {
	PtrMan<Executor> saver = horizons_[idx]->saver();
	TaskRunner::execute( taskrunner, *saver );
    }

    return true;
}

