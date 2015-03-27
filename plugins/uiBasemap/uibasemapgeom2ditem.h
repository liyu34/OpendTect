#ifndef uibasemapgeom2ditem_h
#define uibasemapgeom2ditem_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2014
 RCS:		$Id$
________________________________________________________________________

-*/

#include "uibasemapmod.h"
#include "uibasemapitem.h"

class uiSelLineStyle;

mExpClass(uiBasemap) uiBasemapGeom2DGroup : public uiBasemapIOObjGroup
{
public:
			uiBasemapGeom2DGroup(uiParent*,bool isadd);
			~uiBasemapGeom2DGroup();

    bool		acceptOK();
    bool		fillItemPar(int idx,IOPar&) const;
    bool		usePar(const IOPar&);

protected:
    virtual uiObject*	lastObject();
    uiSelLineStyle*	lsfld_;
};



mExpClass(uiBasemap) uiBasemapGeom2DTreeItem : public uiBasemapTreeItem
{
public:
			uiBasemapGeom2DTreeItem(const char*);
			~uiBasemapGeom2DTreeItem();

    bool		usePar(const IOPar&);

protected:

    bool		showSubMenu();
    bool		handleSubMenu(int);
    const char*		parentType() const
			{ return typeid(uiBasemapTreeTop).name(); }
};



mExpClass(uiBasemap) uiBasemapGeom2DItem : public uiBasemapItem
{
public:
			mDefaultFactoryInstantiation(
				uiBasemapItem,
				uiBasemapGeom2DItem,
				"2D Lines",
				sFactoryKeyword())

    int			defaultZValue() const;
    const char*		iconName() const;
    uiBasemapGroup*	createGroup(uiParent*,bool isadd);
    uiBasemapTreeItem*	createTreeItem(const char*);
};

#endif