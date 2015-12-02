#ifndef uisellinest_h
#define uisellinest_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        A.H. Lammertink
 Date:          08/08/2000
 RCS:           $Id$
________________________________________________________________________

-*/

#include "uitoolsmod.h"
#include "uigroup.h"
#include "uistrings.h"

class uiComboBox;
class uiColorInput;
class uiLabeledSpinBox;
class LineStyle;


/*!\brief Group for defining line properties
Provides selection of linestyle, linecolor and linewidth
*/

mExpClass(uiTools) uiSelLineStyle : public uiGroup
{ mODTextTranslationClass(uiSelLineStyle);
public:

    mExpClass(uiTools) Setup
    {
    public:
			Setup( const uiString& 
			       lbltxt=uiStrings::sEmptyString() )
			    // lbltxt null or "" => "Line style"
			    // lbltxt "-" => no label
			    : txt_(lbltxt)
			    , drawstyle_(true)
			    , color_(true)
			    , width_(true)
			    , transparency_(false)	{}

	mDefSetupMemb(uiString,txt)
	mDefSetupMemb(bool,drawstyle)
	mDefSetupMemb(bool,color)
	mDefSetupMemb(bool,width)
	mDefSetupMemb(bool,transparency)

    };

			uiSelLineStyle(uiParent*,const LineStyle&,
			 const uiString& lbltxt=uiString::emptyString());
			uiSelLineStyle(uiParent*,const LineStyle&,
					       const Setup&);
			~uiSelLineStyle();

    void		setStyle(const LineStyle&);
    const LineStyle&	getStyle() const;

    void		setColor(const Color&);
    const Color&	getColor() const;
    void		setWidth(int);
    int			getWidth() const;
    void		setLineWidthBounds( int min, int max );
    void		setType(int);
    int			getType() const;

    Notifier<uiSelLineStyle>	changed;

protected:

    uiComboBox*			stylesel_;
    uiColorInput*		colinp_;
    uiLabeledSpinBox*		widthbox_;

    LineStyle&			linestyle_;

    void			changeCB(CallBacker*);
private:

    void			init(const Setup&);

};

#endif
