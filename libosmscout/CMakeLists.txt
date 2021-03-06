cmake_minimum_required(VERSION 3.1)
project(libosmscout)

include_directories(include)

set(HEADER_FILES
    include/osmscout/ost/Parser.h
    include/osmscout/ost/Scanner.h
    include/osmscout/private/Config.h
    include/osmscout/private/CoreImportExport.h
    include/osmscout/system/Assert.h
    include/osmscout/system/Math.h
    include/osmscout/system/SSEMath.h
    include/osmscout/system/SSEMathPublic.h
    include/osmscout/system/Types.h
    include/osmscout/util/Breaker.h
    include/osmscout/util/Cache.h
    include/osmscout/util/Color.h
    include/osmscout/util/Exception.h
    include/osmscout/util/File.h
    include/osmscout/util/FileScanner.h
    include/osmscout/util/FileWriter.h
    include/osmscout/util/GeoBox.h
    include/osmscout/util/Geometry.h
    include/osmscout/util/Logger.h
    include/osmscout/util/Magnification.h
    include/osmscout/util/NodeUseMap.h
    include/osmscout/util/Number.h
    include/osmscout/util/NumberSet.h
    include/osmscout/util/Parsing.h
    include/osmscout/util/Progress.h
    include/osmscout/util/Projection.h
    include/osmscout/util/StopClock.h
    include/osmscout/util/String.h
    include/osmscout/util/Tiling.h
    include/osmscout/util/Transformation.h
    include/osmscout/util/WorkQueue.h
    include/osmscout/Area.h
    include/osmscout/AreaAreaIndex.h
    include/osmscout/AreaDataFile.h
    include/osmscout/AreaNodeIndex.h
    include/osmscout/AreaWayIndex.h
    include/osmscout/Coord.h
    include/osmscout/CoordDataFile.h
    include/osmscout/CoreFeatures.h
    include/osmscout/Database.h
    include/osmscout/DataFile.h
    include/osmscout/DebugDatabase.h
    include/osmscout/GeoCoord.h
    include/osmscout/GroundTile.h
    include/osmscout/Intersection.h
    include/osmscout/Location.h
    include/osmscout/LocationIndex.h
    include/osmscout/LocationService.h
    include/osmscout/Navigation.h
    include/osmscout/Node.h
    include/osmscout/NodeDataFile.h
    include/osmscout/NumericIndex.h
    include/osmscout/ObjectRef.h
    include/osmscout/OptimizeAreasLowZoom.h
    include/osmscout/OptimizeWaysLowZoom.h
    include/osmscout/Path.h
    include/osmscout/Pixel.h
    include/osmscout/Point.h
    include/osmscout/POIService.h
    include/osmscout/Route.h
    include/osmscout/RouteData.h
    include/osmscout/RouteNode.h
    include/osmscout/RoutePostprocessor.h
    include/osmscout/RoutingProfile.h
    include/osmscout/RoutingService.h
    include/osmscout/SRTM.h
    include/osmscout/Tag.h
    include/osmscout/TextSearchIndex.h
    include/osmscout/TurnRestriction.h
    include/osmscout/TypeConfig.h
    include/osmscout/TypeFeatures.h
    include/osmscout/Types.h
    include/osmscout/WaterIndex.h
    include/osmscout/Way.h
    include/osmscout/WayDataFile.h)

set(SOURCE_FILES
    src/osmscout/ost/Parser.cpp
    src/osmscout/ost/Scanner.cpp
    src/osmscout/system/SSEMath.cpp
    src/osmscout/util/Breaker.cpp
    src/osmscout/util/Cache.cpp
    src/osmscout/util/Color.cpp
    src/osmscout/util/Exception.cpp
    src/osmscout/util/File.cpp
    src/osmscout/util/FileScanner.cpp
    src/osmscout/util/FileWriter.cpp
    src/osmscout/util/GeoBox.cpp
    src/osmscout/util/Geometry.cpp
    src/osmscout/util/Logger.cpp
    src/osmscout/util/Magnification.cpp
    src/osmscout/util/NodeUseMap.cpp
    src/osmscout/util/Number.cpp
    src/osmscout/util/NumberSet.cpp
    src/osmscout/util/Parsing.cpp
    src/osmscout/util/Progress.cpp
    src/osmscout/util/Projection.cpp
    src/osmscout/util/StopClock.cpp
    src/osmscout/util/String.cpp
    src/osmscout/util/Tiling.cpp
    src/osmscout/util/Transformation.cpp
    src/osmscout/util/WorkQueue.cpp
    src/osmscout/Area.cpp
    src/osmscout/AreaAreaIndex.cpp
    src/osmscout/AreaNodeIndex.cpp
    src/osmscout/AreaWayIndex.cpp
    src/osmscout/Coord.cpp
    src/osmscout/CoordDataFile.cpp
    src/osmscout/Database.cpp
    src/osmscout/DebugDatabase.cpp
    src/osmscout/GeoCoord.cpp
    src/osmscout/GroundTile.cpp
    src/osmscout/Intersection.cpp
    src/osmscout/Location.cpp
    src/osmscout/LocationIndex.cpp
    src/osmscout/LocationService.cpp
    src/osmscout/Node.cpp
    src/osmscout/NodeDataFile.cpp
    src/osmscout/NumericIndex.cpp
    src/osmscout/ObjectRef.cpp
    src/osmscout/OptimizeAreasLowZoom.cpp
    src/osmscout/OptimizeWaysLowZoom.cpp
    src/osmscout/Path.cpp
    src/osmscout/Pixel.cpp
    src/osmscout/Point.cpp
    src/osmscout/POIService.cpp
    src/osmscout/Route.cpp
    src/osmscout/RouteData.cpp
    src/osmscout/RouteNode.cpp
    src/osmscout/RoutePostprocessor.cpp
    src/osmscout/RoutingProfile.cpp
    src/osmscout/RoutingService.cpp
    src/osmscout/SRTM.cpp
    src/osmscout/Tag.cpp
    src/osmscout/TextSearchIndex.cpp
    src/osmscout/TurnRestriction.cpp
    src/osmscout/TypeConfig.cpp
    src/osmscout/TypeFeatures.cpp
    src/osmscout/Types.cpp
    src/osmscout/WaterIndex.cpp
    src/osmscout/Way.cpp
#    tests/AccessParse.cpp
 #   tests/EncodeNumber.cpp
#    tests/FileScannerWriter.cpp
#    tests/GeoCoordParse.cpp
#    tests/NumberSet.cpp
#    tests/ScanConversion.cpp
)

add_library(libosmscout SHARED ${SOURCE_FILES} ${HEADER_FILES})
set_property(TARGET libosmscout PROPERTY CXX_STANDARD 11)

include(CheckCXXSourceCompiles)
include(CheckPrototypeDefinition)

CHECK_CXX_SOURCE_COMPILES("
#include <string>

int main()
{
  std::wstring value=L\"Hello\";
}
" OSMSCOUT_HAVE_STD_WSTRING)
