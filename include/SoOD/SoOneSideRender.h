#ifndef SoOneSideRender_h
#define SoOneSideRender_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
 RCS:		$Id: SoOneSideRender.h,v 1.3 2012-08-27 13:16:48 cvskris Exp $
________________________________________________________________________


-*/

//#include "soodmod.h"
#include "Inventor/nodes/SoShape.h"
//#include "Inventor/fields/SoSFShort.h"
#include "Inventor/fields/SoMFNode.h"
#include "Inventor/fields/SoMFVec3f.h"

#include "Inventor/nodes/SoSubNode.h"

#include "soodbasic.h"

class SoState;

//namespace MeshSurfImpl { class MeshSurfacePart; };

/*!
A class that renders its sub-nodes if light comes from one side. */



mSoODClass SoOneSideRender : public SoNode
{
    SO_NODE_HEADER(SoOneSideRender);
public:
    			SoOneSideRender();
    static void		initClass();

    SoMFNode		nodes;
    SoMFVec3f		positions;
    SoMFVec3f		normals;

protected:
    bool		shouldRender(int idx,SoState*) const;

    void		getBoundingBox(SoGetBoundingBoxAction*);
    void 		GLRender(SoGLRenderAction*);
    void 		callback(SoCallbackAction*);
    void 		getMatrix(SoGetMatrixAction*);
    void 		handleEvent (SoHandleEventAction*);
    void 		pick(SoPickAction*);
    void 		rayPick(SoRayPickAction*);
    void 		getPrimitiveCount(SoGetPrimitiveCountAction*);
};

#endif
    

