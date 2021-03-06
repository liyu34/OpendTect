/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          May 2002
________________________________________________________________________

-*/

#include "uiseedpropdlg.h"

#include "uimarkerstyle.h"


uiSeedPropDlg::uiSeedPropDlg( uiParent* p, EM::EMObject* emobj )
    : uiVisMarkerStyleDlg( p, tr("Seed properties") )
    , emobject_( emobj )
    , markerstyle_(emobject_->getPosAttrMarkerStyle(EM::EMObject::sSeedNode()))
{
}


void uiSeedPropDlg::doFinalise( CallBacker* )
{
    stylefld_->setMarkerStyle( markerstyle_ );
}


void uiSeedPropDlg::sizeChg( CallBacker* )
{
    OD::MarkerStyle3D style;
    stylefld_->getMarkerStyle( style );
    if ( markerstyle_ == style )
	return;

    markerstyle_ = style;
    updateMarkerStyle();
}


void uiSeedPropDlg::typeSel( CallBacker* )
{
    OD::MarkerStyle3D style;
    stylefld_->getMarkerStyle( style );
    if ( markerstyle_==style )
	return;

    markerstyle_ = style;

    updateMarkerStyle();
}


void uiSeedPropDlg::colSel( CallBacker* )
{
    OD::MarkerStyle3D style;
    stylefld_->getMarkerStyle( style );
    if ( markerstyle_==style )
	return;

    markerstyle_ = style;

    updateMarkerStyle();
}


void uiSeedPropDlg::updateMarkerStyle()
{
    emobject_->setPosAttrMarkerStyle( EM::EMObject::sSeedNode(), markerstyle_ );
}
