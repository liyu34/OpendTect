#ifndef uidirectionalplot_h
#define uidirectionalplot_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Bert
 Date:          Mar 2009
 RCS:           $Id: uidirectionalplot.h,v 1.2 2009-04-01 14:35:39 cvsbert Exp $
________________________________________________________________________

-*/

#include "uigraphicsview.h"
#include "statdirdata.h"
#include "draw.h"
class uiTextItem;
class uiEllipseItem;
class uiGraphicsItem;
class uiGraphicsItemGroup;

/*!\brief creates a directional plot: Rose diagrams and more.

  * For Type Rose, the sector part data's val_ will be interpreted as a count;
    the 'pos_' is ignored.
  * For Type Scatter, (pos_,val_) are the polar coordinates.
  * For Type Vals, the pos_ will be used as the 'R' of a polar coordinate. The
    'val_' will be used for a color table lookup.

 */

mClass uiDirectionalPlot : public uiGraphicsView
{
public:

    struct Setup
    {
	enum Type		{ Rose, Scatter, Vals };
				Setup( Type t=Rose )
				    : type_(t)
				    , circlels_(LineStyle::Solid)
				    , sectorls_(LineStyle::Solid)
				    , equils_(LineStyle::None)
				    , drawdirannot_(true)
				    , drawposannot_(false)
				    , prefsize_(400,400)	{}

	mDefSetupMemb(Type,type)
	mDefSetupMemb(LineStyle,circlels)
	mDefSetupMemb(LineStyle,sectorls)
	mDefSetupMemb(LineStyle,equils)
	mDefSetupMemb(bool,drawdirannot)
	mDefSetupMemb(bool,drawposannot)
	mDefSetupMemb(uiSize,prefsize)
    };

				uiDirectionalPlot(uiParent*,const Setup&);
				~uiDirectionalPlot();

    void			setVals(const Stats::DirectionalData&);
    void			setVals(const float*,int);	//!< Rose dgrm
    Stats::DirectionalData::Setup& dataSetup()	{ return data_.setup_; }

    Setup&			setup()		{ return setup_; }
    const Stats::DirectionalData& data() const	{ return data_; }

    int				nrSectors() const { return data().size(); }
    int				size() const	{ return nrSectors(); }
    float			angle( int s ) const { return data_.angle(s); }

    Notifier<uiDirectionalPlot>	sectorPartSelected;
    int				selSector() const { return selsector_; }
    int				selPart() const	{ return selpart_; }

protected:

    Setup			setup_;
    Stats::DirectionalData	data_;

    bool			isempty_;
    Interval<float>		valrg_;
    Interval<float>		posrg_;
    uiPoint			center_;
    int				radius_;
    mutable int			selsector_;
    mutable int			selpart_;

    uiEllipseItem*		outercircleitm_;
    ObjectSet<uiEllipseItem>	equicircles_;
    ObjectSet<uiTextItem>	dirtxtitms_;
    uiGraphicsItemGroup&	sectorlines_;

    void			mouseRelease(CallBacker*);
    void			reSized(CallBacker*);

    void			gatherInfo();
    void			draw();
    void			drawGrid();
    void			drawAnnot();

};


#endif
