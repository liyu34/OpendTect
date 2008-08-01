/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : Raman Singh
 * DATE     : July 2008
-*/

static const char* rcsID = "$Id: gmtpi.cc,v 1.1 2008-08-01 08:28:27 cvsraman Exp $";

#include "initstdgmt.h"
#include "plugins.h"

extern "C" int GetGMTPluginType()
{
    return PI_AUTO_INIT_LATE;
}


extern "C" PluginInfo* GetGMTPluginInfo()
{
    static PluginInfo retpi = {
	"GMT Base",
	"dGB (Raman)",
	"3.2",
    	"Plots Surface data using GMT mapping tool" };
    return &retpi;
}


extern "C" const char* InitGMTPlugin( int, char** )
{
    initStdGMTClasses();

    return 0;
}
