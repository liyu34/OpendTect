/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        A.H. Lammertink
 Date:          08/08/2000
 RCS:           $Id: uisellinest.cc,v 1.22 2008-08-22 06:57:14 cvsnanne Exp $
________________________________________________________________________

-*/

#include "uisellinest.h"
#include "draw.h"
#include "uilabel.h"
#include "uicombobox.h"
#include "uicolor.h"
#include "uispinbox.h"
#include "bufstringset.h"


static const int sMinWidth = 1;
static const int sMaxWidth = 10;

uiSelLineStyle::uiSelLineStyle( uiParent* p, const LineStyle& ls,
				const char* txt, bool wdraw, bool wcol, 
				bool wwidth )
    : uiGroup(p,"Line style selector")
    , linestyle(*new LineStyle(ls))
    , stylesel(0)
    , colinp(0)
    , widthbox(0)
    , changed(this)
{
    if ( wdraw )
    {
	BufferStringSet itms( LineStyle::TypeNames );
	stylesel = new uiComboBox( this, itms, "Line Style" );
	stylesel->setCurrentItem( (int)linestyle.type_ );
	stylesel->selectionChanged.notify( mCB(this,uiSelLineStyle,changeCB) );
	new uiLabel( this, txt, stylesel );
    }

    if ( wcol )
    {
	colinp = new uiColorInput( this,
				   uiColorInput::Setup(linestyle.color_).
				   lbltxt(wdraw ? "Color" : "Line color") );
	colinp->colorchanged.notify( mCB(this,uiSelLineStyle,changeCB) );
	if ( stylesel )
	    colinp->attach( rightTo, stylesel );
    }

    if ( wwidth )
    {
	widthbox = new uiLabeledSpinBox( this, wcol || wdraw ? "Width" 
							     : "Line width" );
	widthbox->box()->valueChanging.notify( 
					mCB(this,uiSelLineStyle,changeCB) );
	widthbox->box()->setValue( linestyle.width_ );
	widthbox->box()->setMinValue( sMinWidth );
  	widthbox->box()->setMaxValue( sMaxWidth );
	if ( colinp )
	    widthbox->attach( rightTo, colinp );
	else if ( stylesel )
	    widthbox->attach( rightTo, stylesel );
    }

    setHAlignObj( stylesel ? (uiObject*)stylesel 
	    		   : ( colinp ? (uiObject*)colinp->attachObj() 
			       	      : (uiObject*)widthbox ) );
    setHCentreObj( stylesel ? (uiObject*)stylesel
	    		    : ( colinp ? (uiObject*)colinp->attachObj()
				       : (uiObject*)widthbox ) );
}


uiSelLineStyle::~uiSelLineStyle()
{
    delete &linestyle;
}


const LineStyle& uiSelLineStyle::getStyle() const
{
    return linestyle;
}


void uiSelLineStyle::setStyle( const LineStyle& ls )
{
    setColor( ls.color_ );
    setWidth( ls.width_ );
    setType( (int)ls.type_ );
    if ( colinp )
	colinp->setSensitive( linestyle.type_ != LineStyle::None );

    if ( widthbox )
	widthbox->setSensitive( linestyle.type_ != LineStyle::None );
}


void uiSelLineStyle::setColor( const Color& col )
{
    linestyle.color_ = col;
    if ( colinp ) colinp->setColor( col );
}


const Color& uiSelLineStyle::getColor() const
{
    return linestyle.color_;
}


void uiSelLineStyle::setWidth( int width )
{
    linestyle.width_ = width;
    if ( widthbox ) widthbox->box()->setValue( width );
}


int uiSelLineStyle::getWidth() const
{
    return linestyle.width_;
}


void uiSelLineStyle::setType( int tp )
{
    linestyle.type_ = (LineStyle::Type)tp;
    if ( stylesel ) stylesel->setCurrentItem( tp );
}


int uiSelLineStyle::getType() const
{
    return (int)linestyle.type_;
}


void uiSelLineStyle::changeCB( CallBacker* cb )
{
    if ( stylesel )
	linestyle.type_ = (LineStyle::Type)stylesel->currentItem();

    if ( colinp )
    {
	linestyle.color_ = colinp->color();
	colinp->setSensitive( linestyle.type_ != LineStyle::None );
    }

    if ( widthbox )
    {
	linestyle.width_ = widthbox->box()->getValue();
	widthbox->setSensitive( linestyle.type_ != LineStyle::None );
    }

    changed.trigger(cb);
}


// ========================================================

LineStyleDlg::LineStyleDlg( uiParent* p, const LineStyle& ls, const char* lbl,
			    bool wdraw, bool wcol, bool wwidth )
        : uiDialog(p,uiDialog::Setup("Display","Select linestyle",0))
{   
    lsfld = new uiSelLineStyle( this, ls, lbl ? lbl : "Line style", 
				wdraw, wcol, wwidth ); 
}


const LineStyle& LineStyleDlg::getLineStyle() const
{   
    return lsfld->getStyle();
}

