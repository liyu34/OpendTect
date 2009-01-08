#ifndef SoPlaneWellLog_h
#define SoPlaneWellLog_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
 RCS:		$Id: SoPlaneWellLog.h,v 1.19 2009-01-08 10:35:13 cvsbruno Exp $
________________________________________________________________________

-*/

#include <Inventor/nodekits/SoBaseKit.h>

#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoMFVec3f.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFShort.h>
#include <Inventor/SbLinear.h>

class SoBaseColor;
class SoCoordinate3;
class SoDrawStyle;
class SoFieldSensor;
class SoLineSet;
class SoTriangleStripSet;
class SoSensor;
class SoState;
class SoSwitch;

/*!\brief

*/

class COIN_DLL_API SoPlaneWellLog : public SoBaseKit
{
    typedef SoBaseKit inherited;
    SO_KIT_HEADER(SoPlaneWellLog);

public:
    				SoPlaneWellLog();
    				~SoPlaneWellLog();

    static void			initClass();

    void			setMaterial();
    void			setLineColor(const SbVec3f&,int);
    const SbVec3f&		lineColor(int) const;
    void			setLogFillColorTab(const float[][3],int);
    void			setNonIndexedShapeColor(const SbVec3f&,int);
    void			setLineWidth(float,int);
    float			lineWidth(int) const;
    void			showLog(bool,int);
    bool			logShown(int) const;
    void			clearLog(int);
    void			setRevScale( bool yn, int lognr ) 
    				{ (lognr == 1 ? revscale1 : revscale2) = yn; }
    void			setLogValue(int,const SbVec3f&,float,int);
    void   			setLogStyle(bool,int);
    bool 		        getLogStyle() const;
    void			setShift(float,int);
    void   			setLogFill(bool,int);
    void   			setLogFillTransparency(int);
    void   			setLineTransparency(int);
    void 			setFillLogValue(int,const SbVec3f&,
	                                        float,int);


    SoMFVec3f			path1;
    SoMFVec3f			path2;
    SoMFFloat			log1;
    SoMFFloat			log2;
    SoMFFloat			filllog1;
    SoMFFloat			filllog2;
    SoSFFloat			maxval1;
    SoSFFloat			maxval2;
    SoSFFloat			fillmaxval1;
    SoSFFloat			fillmaxval2;
    SoSFFloat			minval1;
    SoSFFloat			minval2;
    SoSFFloat			fillminval1;
    SoSFFloat			fillminval2;
    SoSFFloat			shift1;
    SoSFFloat			shift2;
    SoSFBool  			style1;
    SoSFBool  			style2;
    SoSFBool  			filling1;
    SoSFBool  			filling2;
    SoSFFloat			screenWidth;

    SO_KIT_CATALOG_ENTRY_HEADER(topSeparator);
    SO_KIT_CATALOG_ENTRY_HEADER(lineshape1);
    SO_KIT_CATALOG_ENTRY_HEADER(lineshape2);
    SO_KIT_CATALOG_ENTRY_HEADER(line1Switch);
    SO_KIT_CATALOG_ENTRY_HEADER(group1);
    SO_KIT_CATALOG_ENTRY_HEADER(linematerial1);
    SO_KIT_CATALOG_ENTRY_HEADER(col1);
    SO_KIT_CATALOG_ENTRY_HEADER(coltri1);
    SO_KIT_CATALOG_ENTRY_HEADER(coltriseis1);
    SO_KIT_CATALOG_ENTRY_HEADER(drawstyle1);
    SO_KIT_CATALOG_ENTRY_HEADER(coords1);
    SO_KIT_CATALOG_ENTRY_HEADER(lineset1);
    SO_KIT_CATALOG_ENTRY_HEADER(trishape1);
    SO_KIT_CATALOG_ENTRY_HEADER(NonIndexedtrishape1);
    SO_KIT_CATALOG_ENTRY_HEADER(Indexedtrishape1);
    SO_KIT_CATALOG_ENTRY_HEADER(coordtri1);
    SO_KIT_CATALOG_ENTRY_HEADER(coordtriseis1);
    SO_KIT_CATALOG_ENTRY_HEADER(triset1);
    SO_KIT_CATALOG_ENTRY_HEADER(triseisset1);
    SO_KIT_CATALOG_ENTRY_HEADER(material1);
    SO_KIT_CATALOG_ENTRY_HEADER(materialseis1);
    SO_KIT_CATALOG_ENTRY_HEADER(mbinding1);
    SO_KIT_CATALOG_ENTRY_HEADER(linembinding1);
    SO_KIT_CATALOG_ENTRY_HEADER(hints1);
    SO_KIT_CATALOG_ENTRY_HEADER(linehints1);
    SO_KIT_CATALOG_ENTRY_HEADER(line2Switch);
    SO_KIT_CATALOG_ENTRY_HEADER(group2);
    SO_KIT_CATALOG_ENTRY_HEADER(linematerial2);
    SO_KIT_CATALOG_ENTRY_HEADER(col2);
    SO_KIT_CATALOG_ENTRY_HEADER(drawstyle2);
    SO_KIT_CATALOG_ENTRY_HEADER(coords2);
    SO_KIT_CATALOG_ENTRY_HEADER(lineset2);
    SO_KIT_CATALOG_ENTRY_HEADER(trishape2);
    SO_KIT_CATALOG_ENTRY_HEADER(NonIndexedtrishape2);
    SO_KIT_CATALOG_ENTRY_HEADER(Indexedtrishape2);
    SO_KIT_CATALOG_ENTRY_HEADER(coordtri2);
    SO_KIT_CATALOG_ENTRY_HEADER(coordtriseis2);
    SO_KIT_CATALOG_ENTRY_HEADER(triset2);
    SO_KIT_CATALOG_ENTRY_HEADER(triseisset2);
    SO_KIT_CATALOG_ENTRY_HEADER(material2);
    SO_KIT_CATALOG_ENTRY_HEADER(materialseis2);
    SO_KIT_CATALOG_ENTRY_HEADER(mbinding2);
    SO_KIT_CATALOG_ENTRY_HEADER(hints2);
    SO_KIT_CATALOG_ENTRY_HEADER(linembinding2);
    SO_KIT_CATALOG_ENTRY_HEADER(linehints2);

    void			GLRender(SoGLRenderAction*);


protected:

    bool			valchanged;
    int				currentres;
    float			worldwidth;
    bool			revscale1, revscale2;
    int 			lognr;

    
    SoFieldSensor*		valuesensor;
    static void			valueChangedCB(void*,SoSensor*);
    
    
    void			buildLog(int,const SbVec3f&,int);
    void			buildSimpleLog(int,const SbVec3f&,int);
    void			buildSeismicLog(int,const SbVec3f&,int);
    void			buildFilledLog(int,const SbVec3f&,int);
    void			fillLogTriangles(const int,SoCoordinate3*,
	    						SbVec3f&,SbVec3f&);
    SbVec3f 			getProjCoords(const SoMFVec3f&,const int, 
	  				      const SbVec3f&, const SoSFFloat&,
					      const float, int lognr);
    SbVec3f			getNormal(const SbVec3f&,const SbVec3f&,
	    				  const SbVec3f&);
    bool			shouldGLRender(int);
    int				getResolution(SoState*);
};

#endif

