/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        A.H. Lammertink
 Date:          07/02/2002
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "ui3dviewer.h"

#include "uicursor.h"
#include "ui3dviewerbody.h"
#include "ui3dindirectviewer.h"
#include "uirgbarray.h"

#include <osgQt/GraphicsWindowQt>
#include <osgViewer/View>
#include <osgViewer/CompositeViewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/TrackballManipulator>
#include <osg/ComputeBoundsVisitor>

#include "envvars.h"
#include "iopar.h"
#include "keybindings.h"
#include "keystrs.h"
#include "math2.h"	
#include "uicursor.h"
#include "ptrman.h"
#include "settings.h"
#include "survinfo.h"
#include "SoAxes.h"
#include "ViewportRegion.h"

#include "uiobjbody.h"
#include "viscamera.h"
#include "viscamerainfo.h"
#include "visdatagroup.h"
#include "visdataman.h"
#include "visscene.h"
#include "vissurvscene.h"

#include <iostream>
#include <math.h>

#include "i_layout.h"
#include "i_layoutitem.h"

#include <QTabletEvent>
#include <QPainter>
#include "SoPolygonSelect.h"

 
#define col2f(rgb) float(col.rgb())/255

#include "uiosgviewer.h"

#include "uiobjbody.h"
#include "keystrs.h"

#include "survinfo.h"
#include "viscamera.h"
#include "vissurvscene.h"
#include "visdatagroup.h"

DefineEnumNames(ui3DViewer,StereoType,0,"StereoType")
{ sKey::None().str(), "RedCyan", "QuadBuffer", 0 };


class uiDirectViewBody : public ui3DViewerBody
{
public:
				uiDirectViewBody(ui3DViewer&,uiParent*);

    const mQtclass(QWidget)* 	qwidget_() const;
    

    virtual uiSize		minimumSize() const 
    				{ return uiSize(200,200); }

protected:
    void			updateActModeCursor();
    osgGA::GUIActionAdapter&	getActionAdapter() { return *graphicswin_.get();}
    osg::GraphicsContext*	getGraphicsContext() {return graphicswin_.get();}

    bool			mousebutdown_;
    float			zoomfactor_;

    //visBase::CameraInfo*	camerainfo_;
    MouseCursor			actmodecursor_;

    osg::ref_ptr<osgQt::GraphicsWindowQt>	graphicswin_;
};


ui3DViewerBody::ui3DViewerBody( ui3DViewer& h, uiParent* parnt )
    : uiObjectBody( parnt, 0 )
    , handle_( h )
    , printpar_(*new IOPar)
    , viewport_( 0 )
{}


ui3DViewerBody::~ui3DViewerBody()
{			
    handle_.destroyed.trigger(handle_);
    delete &printpar_;
    view_.detachView();
    viewport_->unref();
}


uiObject& ui3DViewerBody::uiObjHandle()
{ return handle_; }


osg::Camera* ui3DViewerBody::getOsgCamera()
{
    if ( !view_.getOsgView() )
	return 0;

    return view_.getOsgView()->getCamera();
}


const osg::Camera* ui3DViewerBody::getOsgCamera() const
{
    return const_cast<ui3DViewerBody*>( this )->getOsgCamera();
}


void ui3DViewerBody::reSizeEvent(CallBacker*)
{
    const mQtclass(QWidget)* widget = qwidget();
    if ( !widget )
	return;

    if ( !camera_ )
	return;

    osg::ref_ptr<osg::Camera> osgcamera = getOsgCamera();

    if ( !osgcamera )
	return;

    const double aspectratio = static_cast<double>(widget->width())/
	static_cast<double>(widget->height());

    osgcamera->setProjectionMatrixAsPerspective( 30.0f, aspectratio,
						  1.0f, 10000.0f );
}



void ui3DViewerBody::toggleViewMode(CallBacker* cb )
{
    setViewMode( !isViewMode(), true );
}


class ODDirectWidget : public osgQt::GLWidget, public CallBacker
{
public:
    			ODDirectWidget(QWidget* p)
			    : osgQt::GLWidget(p)
			    , resize( this )
			    , escPressed( this )
			{}

    bool		event(QEvent* ev)
			{
			    if ( ev->type()==QEvent::Resize )
			    {
				resize.trigger();
			    }
			    else if ( ev->type()==QEvent::KeyPress )
			    {
				QKeyEvent* keyevent = (QKeyEvent*) ev;
				if ( keyevent->key()==Qt::Key_Escape )
				{
				    escPressed.trigger();
				}
				
				return true;
			    }
			    else if ( ev->type()==QEvent::KeyRelease )
				return true;

			    return osgQt::GLWidget::event( ev );
			}

    Notifier<ODDirectWidget>	resize;
    Notifier<ODDirectWidget>	escPressed;

};




uiDirectViewBody::uiDirectViewBody( ui3DViewer& hndl, uiParent* parnt )
    : ui3DViewerBody( hndl, parnt )
    , mousebutdown_(false)
    , zoomfactor_( 1 )
{
    osg::ref_ptr<osg::DisplaySettings> ds = osg::DisplaySettings::instance();
    ODDirectWidget* glw = new ODDirectWidget( parnt->pbody()->managewidg() );
    mAttachCB( glw->resize, ui3DViewerBody, reSizeEvent );
    mAttachCB( glw->escPressed, ui3DViewerBody, toggleViewMode );

    graphicswin_ = new osgQt::GraphicsWindowQt( glw );
    setStretch(2,2);
}


const mQtclass(QWidget)* uiDirectViewBody::qwidget_() const
{ return graphicswin_->getGLWidget(); }


void uiDirectViewBody::updateActModeCursor()
{
    /*
    if ( isViewing() )
	return;

    if ( !isCursorEnabled() )
	return;

    QCursor qcursor;
    uiCursorManager::fillQCursor( actmodecursor_, qcursor );

    getGLWidget()->setCursor( qcursor );
    */
}


bool ui3DViewerBody::isViewMode() const
{
    return scene_ && !scene_->isTraversalEnabled( visBase::EventTraversal );
}


#define ROTATE_WIDTH 16
#define ROTATE_HEIGHT 16
#define ROTATE_BYTES ((ROTATE_WIDTH + 7) / 8) * ROTATE_HEIGHT
#define ROTATE_HOT_X 6
#define ROTATE_HOT_Y 8

static unsigned char rotate_bitmap[ROTATE_BYTES] = {
    0xf0, 0xef, 0x18, 0xb8, 0x0c, 0x90, 0xe4, 0x83,
    0x34, 0x86, 0x1c, 0x83, 0x00, 0x81, 0x00, 0xff,
    0xff, 0x00, 0x81, 0x00, 0xc1, 0x38, 0x61, 0x2c,
    0xc1, 0x27, 0x09, 0x30, 0x1d, 0x18, 0xf7, 0x0f
};

static unsigned char rotate_mask_bitmap[ROTATE_BYTES] = {
    0xf0,0xef,0xf8,0xff,0xfc,0xff,0xfc,0xff,0x3c,0xfe,0x1c,0xff,0x00,0xff,0x00,
    0xff,0xff,0x00,0xff,0x00,0xff,0x38,0x7f,0x3c,0xff,0x3f,0xff,0x3f,0xff,0x1f,
    0xf7,0x0f
};



void ui3DViewerBody::setViewMode( bool yn, bool trigger )
{
    if ( scene_ )
	scene_->enableTraversal( visBase::EventTraversal, !yn );
    
    MouseCursor cursor;
    if ( yn )
    {
	cursor.shape_ = MouseCursor::Bitmap;
	
	uiRGBArray* cursorimage = new uiRGBArray( true );
	cursorimage->setSize( ROTATE_WIDTH, ROTATE_HEIGHT );
	cursorimage->putFromBitmap( rotate_bitmap, rotate_mask_bitmap );
	
	cursor.image_ = cursorimage;
	cursor.hotx_ = ROTATE_HOT_X;
	cursor.hoty_ = ROTATE_HOT_Y;
	
    }
    else
    {
	cursor.shape_ = MouseCursor::Arrow;
    }
    
    mQtclass(QCursor) qcursor;
    uiCursorManager::fillQCursor( cursor, qcursor );
    qwidget()->setCursor( qcursor );
    
    if ( trigger )
	handle_.viewmodechanged.trigger( handle_ );

}


Coord3 ui3DViewerBody::getCameraPosition() const
{
    osg::ref_ptr<const osg::Camera> cam = getOsgCamera();
    if ( !cam )
	return Coord3::udf();

    osg::Vec3 eye, center, up;
    cam->getViewMatrixAsLookAt( eye, center, up );
    return Coord3( eye.x(), eye.y(), eye.z() );
}


void ui3DViewerBody::setSceneID( int sceneid )
{
    visBase::DataObject* obj = visBase::DM().getObject( sceneid );
    mDynamicCastGet(visBase::Scene*,newscene,obj)
    if ( !newscene ) return;

    if ( !view_.getOsgView() )
    {
	camera_ = visBase::Camera::create();

	mDynamicCastGet(osg::Camera*, osgcamera, camera_->osgNode() );
	osgcamera->setGraphicsContext( getGraphicsContext() );
	osgcamera->setClearColor( osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f) );
	if ( viewport_ ) viewport_->unref();
	viewport_ = new osg::Viewport(0, 0, 600, 400 );
	viewport_->ref();
	osgcamera->setViewport( viewport_ );

	osg::ref_ptr<osgViewer::View> view = new osgViewer::View;
	view->setCamera( osgcamera );
	view->setSceneData( newscene->osgNode() );
	view->addEventHandler( new osgViewer::StatsHandler );

	osg::ref_ptr<osgGA::TrackballManipulator> manip =
	    new osgGA::TrackballManipulator(
		osgGA::StandardManipulator::DEFAULT_SETTINGS |
		osgGA::StandardManipulator::SET_CENTER_ON_WHEEL_FORWARD_MOVEMENT
	    );

	manip->setAutoComputeHomePosition( false );

	view->setCameraManipulator( manip.get() );
	view_.setOsgView( view );

	// To put exaggerated bounding sphere radius offside
	manip->setMinimumDistance( 0 );

	// Camera projection must be initialized before computing home position
	reSizeEvent( 0 );
    }

    scene_ = newscene;
}


void ui3DViewerBody::align()
{
    /*
    SoCamera* cam = getCamera();
    if ( !cam ) return;
    osg::Vec3f dir;
    cam->orientation.getValue().multVec( osg::Vec3f(0,0,-1), dir );

    osg::Vec3f focalpoint = cam->position.getValue() +
					cam->focalDistance.getValue() * dir;
					osg::Vec3f upvector( dir[0], dir[1], 1 );
    cam->pointAt( focalpoint, upvector );
    */
}


Geom::Size2D<int> ui3DViewerBody::getViewportSizePixels() const
{
    osg::ref_ptr<const osg::Camera> camera = getOsgCamera();
    osg::ref_ptr<const osg::Viewport> vp = camera->getViewport();

    return Geom::Size2D<int>( mNINT32(vp->width()), mNINT32(vp->height()) );
}


void ui3DViewerBody::setCameraPos( const osg::Vec3f& updir, 
				  const osg::Vec3f& focusdir,
				  bool usetruedir )
{
    /*
    osg::Camera* cam = getOsgCamera();
    if ( !cam ) return;

    osg::Vec3f dir;
    cam->orientation.getValue().multVec( osg::Vec3f(0,0,-1), dir );

#define mEps 1e-6
    const float angletofocus = Math::ACos( dir.dot(focusdir) );
    if ( mIsZero(angletofocus,mEps) ) return;

    const float diffangle = fabs(angletofocus) - M_PI_2;
    const int sign = usetruedir || mIsZero(diffangle,mEps) || diffangle < 0 ? 1 : -1;
    osg::Vec3f focalpoint = cam->position.getValue() +
			 cam->focalDistance.getValue() * sign * focusdir;
    cam->pointAt( focalpoint, updir );

    cam->position = focalpoint + cam->focalDistance.getValue() * sign *focusdir;
    viewAll();
    */
}


void ui3DViewerBody::viewPlaneX()
{
    setCameraPos( osg::Vec3f(0,0,1), osg::Vec3f(1,0,0), false );
}


void ui3DViewerBody::viewAll()
{
    if ( !view_.getOsgView() )
	return;

    osg::ref_ptr<osgGA::CameraManipulator> manip =
	static_cast<osgGA::CameraManipulator*>(
	    view_.getOsgView()->getCameraManipulator() );

    if ( !manip )
	return;

    osg::ref_ptr<osgGA::GUIEventAdapter>& ea =
	osgGA::GUIEventAdapter::getAccumulatedEventState();
    if ( !ea )
	return;

    computeViewAllPosition();
    manip->home( *ea, getActionAdapter() );
}



void ui3DViewerBody::computeViewAllPosition()
{
    if ( !view_.getOsgView() )
	return;
    
    osg::ref_ptr<osgGA::CameraManipulator> manip =
    static_cast<osgGA::CameraManipulator*>(
				view_.getOsgView()->getCameraManipulator() );
    
    osg::ref_ptr<osg::Node> node = manip ? manip->getNode() : 0;
    if ( !node )
	return;

    osg::ComputeBoundsVisitor visitor(
			    osg::NodeVisitor::TRAVERSE_ACTIVE_CHILDREN);
    visitor.setNodeMaskOverride( visBase::BBoxTraversal );
    node->accept(visitor);
    osg::BoundingBox &bb = visitor.getBoundingBox();
    
    osg::BoundingSphere boundingsphere;
    
    
    if ( bb.valid() ) boundingsphere.expandBy(bb);
    else boundingsphere = node->getBound();
    
    // set dist to default
    double dist = 3.5f * boundingsphere.radius();
    
    // try to compute dist from frustrum
    double left,right,bottom,top,znear,zfar;
    if ( getOsgCamera()->getProjectionMatrixAsFrustum(
					    left,right,bottom,top,znear,zfar) )
    {
	double vertical2 = fabs(right - left) / znear / 2.;
	double horizontal2 = fabs(top - bottom) / znear / 2.;
	double dim = horizontal2 < vertical2 ? horizontal2 : vertical2;
	double viewangle = atan2(dim,1.);
	dist = boundingsphere.radius() / sin(viewangle);
    }
    else
    {
	// try to compute dist from ortho
	if ( getOsgCamera()->getProjectionMatrixAsOrtho(
					    left,right,bottom,top,znear,zfar) )
	{
	    dist = fabs(zfar - znear) / 2.;
	}
    }
    
    // set home position
    manip->setHomePosition(boundingsphere.center() + osg::Vec3d(0.0,-dist,0.0f),
		    boundingsphere.center(),
		    osg::Vec3d(0.0f,0.0f,1.0f),
		    false );
}


void ui3DViewerBody::viewPlaneY()
{
    setCameraPos( osg::Vec3f(0,0,1), osg::Vec3f(0,1,0), false );
}


void ui3DViewerBody::setBackgroundColor( const Color& col )
{
    getOsgCamera()->setClearColor( osg::Vec4(col2f(r),col2f(g),col2f(b), 1.0) );
}


Color ui3DViewerBody::getBackgroundColor() const
{
    const osg::Vec4 col = getOsgCamera()->getClearColor();
    return Color( mNINT32(col.r()*255), mNINT32(col.g()*255),
		  mNINT32(col.b()*255) );
}


void ui3DViewerBody::viewPlaneN()
{
    setCameraPos( osg::Vec3f(0,0,1), osg::Vec3f(0,1,0), true );
}


void ui3DViewerBody::viewPlaneZ()
{
    osg::Camera* cam = getOsgCamera();
    if ( !cam ) return;

    osg::Vec3f curdir;
    //cam->orientation.getValue().multVec( osg::Vec3f(0,0,-1), curdir );
    //setCameraPos( curdir, osg::Vec3f(0,0,-1), true );
}


static void getInlCrlVec( osg::Vec3f& vec, bool inl )
{
    const RCol2Coord& b2c = SI().binID2Coord();
    const RCol2Coord::RCTransform& xtr = b2c.getTransform(true);
    const RCol2Coord::RCTransform& ytr = b2c.getTransform(false);
    const float det = xtr.det( ytr );
    if ( inl )
	vec = osg::Vec3f( ytr.c/det, -xtr.c/det, 0 );
    else
	vec = osg::Vec3f( -ytr.b/det, xtr.b/det, 0 );
    vec.normalize();
}


void ui3DViewerBody::viewPlaneInl()
{
    osg::Vec3f inlvec;
    getInlCrlVec( inlvec, true );
    setCameraPos( osg::Vec3f(0,0,1), inlvec, false );
}


void ui3DViewerBody::viewPlaneCrl()
{
    osg::Vec3f crlvec;
    getInlCrlVec( crlvec, false );
    setCameraPos( osg::Vec3f(0,0,1), crlvec, false );
}


bool ui3DViewerBody::isCameraPerspective() const
{
    return !camera_->isOrthogonal();
}


bool ui3DViewerBody::isCameraOrthographic() const
{
    return camera_->isOrthogonal();
}


void ui3DViewerBody::toggleCameraType()
{
    //TODO
}


void ui3DViewerBody::setHomePos()
{
    if ( !camera_ ) return;

    PtrMan<IOPar> homepospar =
	SI().pars().subselect( ui3DViewer::sKeyHomePos() );
    if ( !homepospar )
	return;

    camera_->usePar( *homepospar );
}



void ui3DViewerBody::resetToHomePosition()
{
}


void ui3DViewerBody::uiRotate( float angle, bool horizontal )
{
    mDynamicCastGet( osgGA::StandardManipulator*, manip,
		     view_.getOsgView()->getCameraManipulator() ); 
    if ( !manip )
	return;

    osg::Vec3d eye, center, up;
    manip->getTransformation( eye, center, up );
    osg::Matrixd mat;
    osg::Vec3d axis = horizontal ? up : (eye-center)^up;
    mat.makeRotate( angle, axis );
    mat.preMultTranslate( -center );
    mat.postMultTranslate( center );
    manip->setTransformation( eye*mat, center, up );
}


void ui3DViewerBody::uiZoom( float rel, const osg::Vec3f* dir )
{
    mDynamicCastGet( osgGA::StandardManipulator*, manip,
		     view_.getOsgView()->getCameraManipulator() );

    osg::ref_ptr<const osg::Camera> cam = getOsgCamera();

    if ( !manip || !cam )
	return;

    osg::Vec3d eye, center, up;
    manip->getTransformation( eye, center, up );

    double multiplicator = exp( rel );
    
    double l, r, b, t, znear, zfar;
    if ( cam->getProjectionMatrixAsFrustum(l,r,b,t,znear,zfar) )
    {
	osg::Vec3d olddir = center - eye; 
	osg::Vec3d newdir = olddir; 
	if ( dir )
	    newdir = *dir;

	double movement = (multiplicator-1.0) * olddir.length();
	const double minmovement = fabs(znear-zfar) * 0.01;

	if ( fabs(movement) < minmovement )
	{
	    if ( movement < 0.0 )	// zoom in
		return;

	    movement = minmovement;
	}

	olddir.normalize();
	newdir.normalize();
	if ( !newdir.valid() )
	    return;

	eye -= newdir * movement;
	center -= (olddir*(newdir*olddir) - newdir) * movement;
    }
    else if ( cam->getProjectionMatrixAsOrtho(l,r,b,t,znear,zfar) )
    {
	// TODO
    }

    manip->setTransformation( eye, center, up );
}




//------------------------------------------------------------------------------


ui3DViewer::ui3DViewer( uiParent* parnt, bool direct, const char* nm )
    : uiObject(parnt,nm,mkBody(parnt, direct, nm) )
    , destroyed(this)
    , viewmodechanged(this)
    , pageupdown(this)
    , vmcb(0)
{
    PtrMan<IOPar> homepospar = SI().pars().subselect( sKeyHomePos() );
    if ( homepospar )
	homepos_ = *homepospar;

    setViewMode( false );  // switches between view & interact mode
}


ui3DViewer::~ui3DViewer()
{
    delete osgbody_;
}


uiObjectBody& ui3DViewer::mkBody( uiParent* parnt, bool direct, const char* nm )
{
    osgQt::initQtWindowingSystem();

    osgbody_ = direct 
	? (ui3DViewerBody*) new uiDirectViewBody( *this, parnt )
	: (ui3DViewerBody*) new ui3DIndirectViewBody( *this, parnt );
    
    return *osgbody_; 
}

void ui3DViewer::viewAll()
{
    mDynamicCastGet(visSurvey::Scene*,survscene,getScene());
    if ( !survscene )
    {
	osgbody_->viewAll();
    }
    else
    {
	bool showtext = survscene->isAnnotTextShown();
	bool showscale = survscene->isAnnotScaleShown();
	if ( !showtext && !showscale )
	{
	    osgbody_->viewAll();
	}
	else
	{
	    survscene->showAnnotText( false );
	    survscene->showAnnotScale( false );
	    osgbody_->viewAll();
	    survscene->showAnnotText( showtext );
	    survscene->showAnnotScale( showscale );
	}
    }
}


void ui3DViewer::setBackgroundColor( const Color& col )
{
    osgbody_->setBackgroundColor( col );
}


void ui3DViewer::setAxisAnnotColor( const Color& col )
{
}


Color ui3DViewer::getBackgroundColor() const
{
    return osgbody_->getBackgroundColor();
}


void ui3DViewer::getAllKeyBindings( BufferStringSet& keys )
{
}


void ui3DViewer::setKeyBindings( const char* keybindname )
{
}


const char* ui3DViewer::getCurrentKeyBindings() const
{
    return 0;
}


void ui3DViewer::viewPlane( PlaneType type )
{
    switch ( type )
    {
	    case X: osgbody_->viewPlaneX(); break;
	    case Y: osgbody_->viewPlaneN(); break;
	    case Z: osgbody_->viewPlaneZ(); break;
	    case Inl: osgbody_->viewPlaneInl(); break;
	    case Crl: osgbody_->viewPlaneCrl(); break;
	    case YZ: osgbody_->viewPlaneN(); osgbody_->viewPlaneZ(); break;
    }

    viewAll();
}


bool ui3DViewer::isCameraPerspective() const
{
    return osgbody_->isCameraPerspective();
}


bool ui3DViewer::setStereoType( StereoType type )
{
    pErrMsg( "Not impl yet" ); 
    return false;
}


ui3DViewer::StereoType ui3DViewer::getStereoType() const
{
    return None;
} 


void ui3DViewer::setStereoOffset( float offset )
{
    pErrMsg( "Not impl yet" ); 
}


float ui3DViewer::getStereoOffset() const
{
    pErrMsg( "Not impl yet" ); 
    return 0;
}


void ui3DViewer::setSceneID( int sceneid )
{
    osgbody_->setSceneID( sceneid );
}


int ui3DViewer::sceneID() const
{
    return osgbody_->getScene() ? osgbody_->getScene()->id() : -1;
}


void ui3DViewer::setViewMode( bool yn )
{
    if ( yn==isViewMode() )
	return;
    
    osgbody_->setViewMode( yn, false );
}


bool ui3DViewer::isViewMode() const
{
    return osgbody_->isViewMode();
}


void ui3DViewer::rotateH( float angle )
{ osgbody_->uiRotate( angle, true ); }


void ui3DViewer::rotateV( float angle )
{ osgbody_->uiRotate( angle, false ); }


void ui3DViewer::dolly( float rel )
{ osgbody_->uiZoom( rel ); }


float ui3DViewer::getCameraZoom()
{
    return 1;
}

const Coord3 ui3DViewer::getCameraPosition() const
{
    return osgbody_->getCameraPosition();
}


void ui3DViewer::setCameraZoom( float val )
{
	//TODO
}

void ui3DViewer::anyWheelStart()
{
}

void ui3DViewer::anyWheelStop()
{
}


void ui3DViewer::align()
{
    osgbody_->align();
}


void ui3DViewer::toHomePos()
{
    RefMan<visBase::Camera> camera = osgbody_->getVisCamera();
    if ( !camera ) return;

    camera->usePar( homepos_ );
}


void ui3DViewer::saveHomePos()
{
    RefMan<visBase::Camera> camera = osgbody_->getVisCamera();

    if ( !camera ) return;

    TypeSet<int> dummy;
    camera->fillPar( homepos_, dummy );
    homepos_.removeWithKey( sKey::Type() );
    SI().getPars().mergeComp( homepos_, sKeyHomePos() );
    SI().savePars();
}


void ui3DViewer::showRotAxis( bool yn )
{
    pErrMsg("Not impl");
}


bool ui3DViewer::rotAxisShown() const
{
    pErrMsg("Not impl");
    return false;
}

void ui3DViewer::toggleCameraType()
{
    osgbody_->toggleCameraType();
}


Geom::Size2D<int> ui3DViewer::getViewportSizePixels() const
{
    return osgbody_->getViewportSizePixels();
}


void ui3DViewer::fillPar( IOPar& par ) const
{
    par.set( sKeySceneID(), getScene()->id() );

    par.set( sKeyBGColor(), (int)getBackgroundColor().rgb() );

    par.set( sKeyStereo(), toString( getStereoType() ) );
    float offset = getStereoOffset();
    par.set( sKeyStereoOff(), offset );

    par.setYN( sKeyPersCamera(), isCameraPerspective() );

    TypeSet<int> dummy;
    RefMan<visBase::Camera> camera = osgbody_->getVisCamera();

    camera->fillPar( par, dummy );

    par.mergeComp( homepos_, sKeyHomePos() );
}


bool ui3DViewer::usePar( const IOPar& par )
{
    int sceneid;
    if ( !par.get(sKeySceneID(),sceneid) ) return false;

    setSceneID( sceneid );
    if ( !getScene() ) return false;

    int bgcol;
    if ( par.get(sKeyBGColor(),bgcol) )
    {
	Color newcol; newcol.setRgb( bgcol );
	setBackgroundColor( newcol );
    }

    StereoType stereotype;
    if ( parseEnum( par, sKeyStereo(), stereotype ) )
	setStereoType( stereotype );

    float offset;
    if ( par.get( sKeyStereoOff(), offset )  )
	setStereoOffset( offset );

    PtrMan<IOPar> homepos = par.subselect( sKeyHomePos() );

    if ( homepos )
    {
	homepos_ = *homepos;
	saveHomePos();
    }

    RefMan<visBase::Camera> camera = osgbody_->getVisCamera();

    bool res = camera->usePar( par ) == 1;
    
    PtrMan<IOPar> survhomepospar = SI().pars().subselect( sKeyHomePos() );
    if ( !survhomepospar )
	saveHomePos();

    return res;
}



float ui3DViewer::getHeadOnLightIntensity() const
{
    return -1;
}

void ui3DViewer::setHeadOnLightIntensity( float value )
{
}


visBase::Scene* ui3DViewer::getScene()
{
    return osgbody_->getScene();
}


const visBase::Scene* ui3DViewer::getScene() const
{ return const_cast<ui3DViewer*>(this)->getScene(); }
