#ifndef vissurvpickset_h
#define vissurvpickset_h

/*+
________________________________________________________________________

 CopyRight:	(C) de Groot-Bril Earth Sciences B.V.
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
 RCS:		$Id: vispicksetdisplay.h,v 1.21 2002-10-14 15:10:53 niclas Exp $
________________________________________________________________________


-*/

#include "visobject.h"
#include "vissurvobj.h"

class Color;
class IOPar;

namespace visBase { class SceneObjectGroup; class EventCatcher; };
namespace Geometry { class Pos; }

namespace visSurvey
{

class Scene;

/*!\brief


*/

class PickSetDisplay :	public visBase::VisualObjectImpl,
			public visSurvey::SurveyObject
{
public:
    static PickSetDisplay*	create()
				mCreateDataObj0arg(PickSetDisplay);

    int				nrPicks() const;
    Coord3			getPick( int idx ) const;
    void			addPick( const Coord3& );
    void			removePick( const Coord3& );
    void			removeAll();

    float			getInitSize() const	{ return initsz; }
    float			getPickSize() const 	{ return picksz; }
    void			setSize(float);

    void			setColor(const Color&);
    const Color&		getColor() const;

    void			setType(int);
    int				getType() const;
    void			getTypeNames(TypeSet<char*>&);

    void			showAll(bool yn);
    bool			allShown()		{ return showall; }
    void			filterPicks( ObjectSet<SurveyObject>&,
	    				     float dist );

    virtual void                fillPar( IOPar&, TypeSet<int>& ) const;
    virtual int                 usePar( const IOPar& );

    Notifier<PickSetDisplay>	changed;

protected:
    virtual			~PickSetDisplay();

    void			pickCB( CallBacker* =0 );
    void			updatePickSz( CallBacker* = 0);

    int				useOldPar(const IOPar&);

    int				picktype;
    float			picksz;
    const float			initsz;

    bool			showall;

    int				mousepressid;
    Coord3		        mousepressposition;

    visBase::SceneObjectGroup*	group;
    visBase::EventCatcher*	eventcatcher;

    static const char*		nopickstr;
    static const char*		pickprefixstr;
    static const char*		showallstr;
    static const char*		shapestr;
    static const char*		sizestr;
};

};


#endif
