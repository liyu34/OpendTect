/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Nanne Hemstra
 Date:          January 2016
________________________________________________________________________

-*/

#include "uirgbattrseldlg.h"

#include "uiattrsel.h"
#include "attribsel.h"


uiRGBAttrSelDlg::uiRGBAttrSelDlg( uiParent* p, const Attrib::DescSet& ds )
    : uiDialog(p,Setup(tr("Select RGB Attributes"),mNoDlgTitle,mTODOHelpKey))
{
    rfld_ = new uiAttrSel( this, ds );
    rfld_->setLabelText( tr("Red Attribute") );

    gfld_ = new uiAttrSel( this, ds );
    gfld_->setLabelText( tr("Green Attribute") );
    gfld_->attach( alignedBelow, rfld_ );

    bfld_ = new uiAttrSel( this, ds );
    bfld_->setLabelText( tr("Blue Attribute") );
    bfld_->attach( alignedBelow, gfld_ );

    tfld_ = new uiAttrSel( this, ds );
    tfld_->setLabelText( tr("Transparency Attribute") );
    tfld_->attach( alignedBelow, bfld_ );
}


uiRGBAttrSelDlg::~uiRGBAttrSelDlg()
{
}


void uiRGBAttrSelDlg::setSelSpec( const TypeSet<Attrib::SelSpec>& as )
{
    if ( as.size() != 4 )
	return;

    rfld_->setSelSpec( &as[0] );
    gfld_->setSelSpec( &as[1] );
    bfld_->setSelSpec( &as[2] );
    tfld_->setSelSpec( &as[3] );
}


void uiRGBAttrSelDlg::fillSelSpec( TypeSet<Attrib::SelSpec>& as ) const
{
    if ( as.size() != 4 )
	as.setSize( 4 );

    rfld_->fillSelSpec( as[0] );
    gfld_->fillSelSpec( as[1] );
    bfld_->fillSelSpec( as[2] );
    tfld_->fillSelSpec( as[3] );
}


bool uiRGBAttrSelDlg::acceptOK()
{
    return true;
}
