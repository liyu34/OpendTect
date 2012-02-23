#_______________________Pmake__________________________________________________
#
#	CopyRight:	dGB Beheer B.V.
# 	Jan 2012	K. Tingdahl
#	RCS :		$Id: ODCoinUtils.cmake,v 1.6 2012-02-23 13:28:43 cvskris Exp $
#_______________________________________________________________________________

SET( OD_COINDIR_ENV $ENV{OD_COINDIR})

FIND_PACKAGE( OpenGL REQUIRED )

IF(OD_COINDIR_ENV)
    SET(COINDIR ${OD_COINDIR_ENV})
ELSE()
    SET(COINDIR "" CACHE PATH "COINDIR location")
ENDIF()

MACRO(OD_SETUP_COIN)
    IF(OD_USECOIN)
	IF ( OD_EXTRA_COINFLAGS )
	    ADD_DEFINITIONS( ${OD_EXTRA_COINFLAGS} )
	ENDIF( OD_EXTRA_COINFLAGS )

	LIST(APPEND OD_MODULE_INCLUDEPATH ${COINDIR}/include )
	IF(WIN32)
	    FIND_LIBRARY(COINLIB NAMES Coin3 PATHS ${COINDIR}/lib REQUIRED )
	    FIND_LIBRARY(SOQTLIB NAMES SoQt1 PATHS ${COINDIR}/lib REQUIRED )
	ELSE()
	    FIND_LIBRARY(COINLIB NAMES Coin PATHS ${COINDIR}/lib REQUIRED )
	    FIND_LIBRARY(SOQTLIB NAMES SoQt PATHS ${COINDIR}/lib REQUIRED )
	ENDIF()

	IF(WIN32)
	    FIND_LIBRARY(OD_SIMVOLEON_LIBRARY NAMES SimVoleon2
		         PATHS ${COINDIR}/lib REQUIRED )
	ELSE()
	    SET(TMPVAR ${CMAKE_FIND_LIBRARY_SUFFIXES})
	    SET(CMAKE_FIND_LIBRARY_SUFFIXES ${OD_STATIC_EXTENSION})
	    FIND_LIBRARY(OD_SIMVOLEON_LIBRARY NAMES SimVoleon
		         PATHS ${COINDIR}/lib REQUIRED )
	    SET(CMAKE_FIND_LIBRARY_SUFFIXES ${TMPVAR})
	ENDIF()

	SET(OD_COIN_LIBS ${COINLIB} ${SOQTLIB} ${OPENGL_gl_LIBRARY} )
    ENDIF()

    LIST(APPEND OD_MODULE_EXTERNAL_LIBS ${OD_COIN_LIBS} )
ENDMACRO(OD_SETUP_COIN)
