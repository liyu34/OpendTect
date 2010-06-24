#ifndef emhorizonpainter2d_h
#define emhorizonpainter2d_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Umesh Sinha
 Date:		May 2010
 RCS:		$Id: emhorizonpainter2d.h,v 1.1 2010-06-24 08:44:53 cvsumesh Exp $
________________________________________________________________________

-*/

#include "cubesampling.h"
#include "emposid.h"
#include "flatview.h"

namespace EM
{

mClass HorizonPainter2D : public CallBacker
{
public:
    			HorizonPainter2D(FlatView::Viewer&,const EM::ObjectID&);
			~HorizonPainter2D();

    void		setCubeSampling(const CubeSampling&,bool upd=false);
    void		setLineName(const char*);

    void		enableLine(bool);
    void		enableSeed(bool);

    TypeSet<int>&       getTrcNos()			{ return trcnos_; }
    TypeSet<float>&	getDistances()			{ return distances_; }

    void		paint();

    	mStruct	Marker2D
	{
	    			Marker2D()
				    : marker_(0)
				    , sectionid_(-1)
				{}
				~Marker2D()
				{ delete marker_; }

	    FlatView::Annotation::AuxData*      marker_;
	    EM::SectionID			sectionid_;
	};

protected:

    bool		addPolyLine();
    void		removePolyLine();

    void		horChangeCB(CallBacker*);
    void		changePolyLineColor();
    void		changePolyLinePosition( const EM::PosID& pid );
    void		repaintHorizon();

    EM::ObjectID	id_;
    CubeSampling	cs_;

    LineStyle           markerlinestyle_;
    MarkerStyle2D       markerstyle_;
    FlatView::Viewer&   viewer_;

    const char*		linenm_;
    TypeSet<int>	trcnos_;
    TypeSet<float>	distances_;

    typedef ObjectSet<Marker2D> 	SectionMarker2DLine;
    ObjectSet<SectionMarker2DLine>	markerline_;
    Marker2D*				markerseeds_;

    bool		linenabled_;
    bool		seedenabled_;
};

}; //namespace EM


#endif
