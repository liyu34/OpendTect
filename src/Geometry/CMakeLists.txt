#_______________________Pmake___________________________________________________
#
#	Makefile : 	Geometry-related classes
# 	July 1995	A.H. Bril
#_______________________________________________________________________________


set(OD_MODULE_DEPS General )
set(OD_MODULE_SOURCES
		arraytesselator.cc
		binidsurface.cc
		cubicbeziercurve.cc
		cubicbeziersurface.cc
		explfaultsticksurface.cc
		explicitmarchingcubes.cc
		explplaneintersection.cc
		explpolygonsurface.cc
		faulthorintersect.cc
		faultstickset.cc
		faultsticksurface.cc
		geeditor.cc
		geeditorimpl.cc
		geom2dintersections.cc
		geomelement.cc
		impbodyplaneintersect.cc
		indexedshape.cc
		initgeometry.cc
		marchingcubes.cc
		marchingcubeseditor.cc
		horizon2dline.cc
		horizon3dtesselator.cc
		parametriccurve.cc
		parametricsurface.cc
		picksetio.cc
		picksetmanager.cc
		picksettr.cc
		polygonsurface.cc
		polygonsurfaceedit.cc
		polyposprovider.cc
		posvecdatasettr.cc
		probdenfunctr.cc
		randomlinegeom.cc
		randomlineprobe.cc
		randomlinetr.cc
		rowcolsurface.cc
		stickseteditor.cc
		survgeometry.cc
		tableposprovider.cc
)

OD_ADD_SOURCE_FILES( marchingcubeslist.cc )
OD_INIT_MODULE()
