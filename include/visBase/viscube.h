#ifndef viscube_h
#define viscube_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
 RCS:		$Id: viscube.h,v 1.13 2004-11-16 09:29:17 kristofer Exp $
________________________________________________________________________


-*/

#include "visshape.h"
#include "position.h"

class SoCube;
class SoTranslation;

namespace Geometry { class Pos; };

namespace visBase
{
class Scene;

/*!\brief

Cube is a basic cube that is settable in size.

*/

class Cube : public Shape
{
public:
    static Cube*	create()
			mCreateDataObj(Cube);

    void		setCenterPos( const Coord3& );
    Coord3		centerPos() const;
    
    void		setWidth( const Coord3& );
    Coord3		width() const;

    void		setDisplayTransformation( Transformation* );
    Transformation*	getDisplayTransformation() { return transformation; }

    int			usePar( const IOPar& );
    void		fillPar( IOPar&, TypeSet<int>& ) const;

protected:
    			~Cube();
    Transformation*	transformation;
    SoTranslation*	position;
    static const char*	centerposstr;
    static const char*	widthstr;
};

};


#endif
