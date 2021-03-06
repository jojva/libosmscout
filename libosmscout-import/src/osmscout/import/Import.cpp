/*
  This source is part of the libosmscout library
  Copyright (C) 2009  Tim Teulings

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <osmscout/import/Import.h>

#include <iostream>

#include <osmscout/Types.h>


#include <osmscout/import/RawNode.h>
#include <osmscout/import/RawWay.h>
#include <osmscout/import/RawRelation.h>

#include <osmscout/import/GenRawNodeIndex.h>
#include <osmscout/import/GenRawWayIndex.h>
#include <osmscout/import/GenRawRelIndex.h>

#include <osmscout/RoutingService.h>
#include <osmscout/RouteNode.h>
#include <osmscout/Intersection.h>

#include <osmscout/import/GenTypeDat.h>

#include <osmscout/import/Preprocess.h>

#include <osmscout/import/GenNodeDat.h>
#include <osmscout/import/SortNodeDat.h>

#include <osmscout/import/GenRelAreaDat.h>
#include <osmscout/import/GenWayAreaDat.h>
#include <osmscout/import/MergeAreaData.h>
#include <osmscout/import/GenMergeAreas.h>

#include <osmscout/import/GenWayWayDat.h>
#include <osmscout/import/SortWayDat.h>

#include <osmscout/import/GenNumericIndex.h>

#include <osmscout/import/GenAreaAreaIndex.h>
#include <osmscout/import/GenAreaNodeIndex.h>
#include <osmscout/import/GenAreaWayIndex.h>

#include <osmscout/import/GenLocationIndex.h>
#include <osmscout/import/GenOptimizeAreaWayIds.h>
#include <osmscout/import/GenWaterIndex.h>

#include <osmscout/import/GenOptimizeAreasLowZoom.h>
#include <osmscout/import/GenOptimizeWaysLowZoom.h>

// Routing
#include <osmscout/import/GenRouteDat.h>

#if defined(OSMSCOUT_IMPORT_HAVE_LIB_MARISA)
#include <osmscout/import/GenTextIndex.h>
#endif

#include <osmscout/util/Progress.h>
#include <osmscout/util/StopClock.h>

namespace osmscout {

  static const size_t defaultStartStep=1;
#if defined(OSMSCOUT_IMPORT_HAVE_LIB_MARISA)
  static const size_t defaultEndStep=24;
#else
  static const size_t defaultEndStep=23;
#endif

  ImportParameter::Router::Router(uint8_t vehicleMask,
                                  const std::string& filenamebase)
  : vehicleMask(vehicleMask),
    filenamebase(filenamebase)
  {
     // no code
  }

  ImportParameter::ImportParameter()
   : typefile("map.ost"),
     startStep(defaultStartStep),
     endStep(defaultEndStep),
     strictAreas(false),
     sortObjects(true),
     sortBlockSize(40000000),
     sortTileMag(14),
     numericIndexPageSize(1024),
     coordDataMemoryMaped(false),
     rawNodeDataMemoryMaped(false),
     rawWayIndexMemoryMaped(true),
     rawWayDataMemoryMaped(false),
     rawWayIndexCacheSize(10000),
     rawWayBlockSize(500000),
     areaDataMemoryMaped(false),
     areaDataCacheSize(0),
     wayDataMemoryMaped(false),
     wayDataCacheSize(0),
     areaAreaIndexMaxMag(17),
     areaNodeMinMag(8),
     areaNodeIndexMinFillRate(0.1),
     areaNodeIndexCellSizeAverage(16),
     areaNodeIndexCellSizeMax(256),
     areaWayMinMag(11), // Should not be >= than optimizationMaxMag
     areaWayIndexMaxLevel(13),
     waterIndexMinMag(6),
     waterIndexMaxMag(14),
     optimizationMaxWayCount(1000000),
     optimizationMaxMag(10),
     optimizationMinMag(0),
     optimizationCellSizeAverage(64),
     optimizationCellSizeMax(255),
     optimizationWayMethod(TransPolygon::quality),
     routeNodeBlockSize(500000),
     assumeLand(true)
  {
    // no code
  }

  const std::list<std::string>& ImportParameter::GetMapfiles() const
  {
    return mapfiles;
  }

  std::string ImportParameter::GetTypefile() const
  {
    return typefile;
  }

  std::string ImportParameter::GetDestinationDirectory() const
  {
    return destinationDirectory;
  }

  size_t ImportParameter::GetStartStep() const
  {
    return startStep;
  }

  size_t ImportParameter::GetEndStep() const
  {
    return endStep;
  }

  const std::list<ImportParameter::Router>& ImportParameter::GetRouter() const
  {
    return router;
  }

  bool ImportParameter::GetStrictAreas() const
  {
    return strictAreas;
  }

  bool ImportParameter::GetSortObjects() const
  {
    return sortObjects;
  }

  size_t ImportParameter::GetSortBlockSize() const
  {
    return sortBlockSize;
  }

  size_t ImportParameter::GetSortTileMag() const
  {
    return sortTileMag;
  }

  size_t ImportParameter::GetNumericIndexPageSize() const
  {
    return numericIndexPageSize;
  }

  bool ImportParameter::GetCoordDataMemoryMaped() const
  {
    return coordDataMemoryMaped;
  }

  bool ImportParameter::GetRawNodeDataMemoryMaped() const
  {
    return rawNodeDataMemoryMaped;
  }

  bool ImportParameter::GetRawWayIndexMemoryMaped() const
  {
    return rawWayIndexMemoryMaped;
  }

  size_t ImportParameter::GetRawWayIndexCacheSize() const
  {
    return rawWayIndexCacheSize;
  }

  bool ImportParameter::GetRawWayDataMemoryMaped() const
  {
    return rawWayDataMemoryMaped;
  }

  size_t ImportParameter::GetRawWayBlockSize() const
  {
    return rawWayBlockSize;
  }

  size_t ImportParameter::GetAreaDataCacheSize() const
  {
    return areaDataCacheSize;
  }

  bool ImportParameter::GetAreaDataMemoryMaped() const
  {
    return areaDataMemoryMaped;
  }

  size_t ImportParameter::GetWayDataCacheSize() const
  {
    return wayDataCacheSize;
  }

  bool ImportParameter::GetWayDataMemoryMaped() const
  {
    return wayDataMemoryMaped;
  }

  size_t ImportParameter::GetAreaNodeMinMag() const
  {
    return areaNodeMinMag;
  }

  double ImportParameter::GetAreaNodeIndexMinFillRate() const
  {
    return areaNodeIndexMinFillRate;
  }

  size_t ImportParameter::GetAreaNodeIndexCellSizeAverage() const
  {
    return areaNodeIndexCellSizeAverage;
  }

  size_t ImportParameter::GetAreaNodeIndexCellSizeMax() const
  {
    return areaNodeIndexCellSizeMax;
  }

  size_t ImportParameter::GetAreaWayMinMag() const
  {
    return areaWayMinMag;
  }

  size_t ImportParameter::GetAreaWayIndexMaxLevel() const
  {
    return areaWayIndexMaxLevel;
  }

  size_t ImportParameter::GetAreaAreaIndexMaxMag() const
  {
    return areaAreaIndexMaxMag;
  }

  size_t ImportParameter::GetWaterIndexMinMag() const
  {
    return waterIndexMinMag;
  }

  size_t ImportParameter::GetWaterIndexMaxMag() const
  {
    return waterIndexMaxMag;
  }

  size_t ImportParameter::GetOptimizationMaxWayCount() const
  {
    return optimizationMaxWayCount;
  }

  size_t ImportParameter::GetOptimizationMaxMag() const
  {
    return optimizationMaxMag;
  }

  size_t ImportParameter::GetOptimizationMinMag() const
  {
    return optimizationMinMag;
  }

  size_t ImportParameter::GetOptimizationCellSizeAverage() const
  {
    return optimizationCellSizeAverage;
  }

  size_t ImportParameter::GetOptimizationCellSizeMax() const
  {
    return optimizationCellSizeMax;
  }

  TransPolygon::OptimizeMethod ImportParameter::GetOptimizationWayMethod() const
  {
    return optimizationWayMethod;
  }

  size_t ImportParameter::GetRouteNodeBlockSize() const
  {
    return routeNodeBlockSize;
  }

  bool ImportParameter::GetAssumeLand() const
  {
    return assumeLand;
  }

  void ImportParameter::SetMapfiles(const std::list<std::string>& mapfiles)
  {
    this->mapfiles=mapfiles;
  }

  void ImportParameter::SetTypefile(const std::string& typefile)
  {
    this->typefile=typefile;
  }

  void ImportParameter::SetDestinationDirectory(const std::string& destinationDirectory)
  {
    this->destinationDirectory=destinationDirectory;
  }

  void ImportParameter::SetStartStep(size_t startStep)
  {
    this->startStep=startStep;
    this->endStep=defaultEndStep;
  }

  void ImportParameter::SetSteps(size_t startStep, size_t endStep)
  {
    this->startStep=startStep;
    this->endStep=endStep;
  }

  void ImportParameter::ClearRouter()
  {
    router.clear();
  }

  void ImportParameter::AddRouter(const Router& router)
  {
    this->router.push_back(router);
  }

  void ImportParameter::SetStrictAreas(bool strictAreas)
  {
    this->strictAreas=strictAreas;
  }

  void ImportParameter::SetSortObjects(bool renumberIds)
  {
    this->sortObjects=renumberIds;
  }

  void ImportParameter::SetSortBlockSize(size_t sortBlockSize)
  {
    this->sortBlockSize=sortBlockSize;
  }

  void ImportParameter::SetSortTileMag(size_t sortTileMag)
  {
    this->sortTileMag=sortTileMag;
  }

  void ImportParameter::SetNumericIndexPageSize(size_t numericIndexPageSize)
  {
    this->numericIndexPageSize=numericIndexPageSize;
  }

  void ImportParameter::SetCoordDataMemoryMaped(bool memoryMaped)
  {
    this->coordDataMemoryMaped=memoryMaped;
  }

  void ImportParameter::SetRawNodeDataMemoryMaped(bool memoryMaped)
  {
    this->rawNodeDataMemoryMaped=memoryMaped;
  }

  void ImportParameter::SetRawWayIndexMemoryMaped(bool memoryMaped)
  {
    this->rawWayIndexMemoryMaped=memoryMaped;
  }

  void ImportParameter::SetRawWayDataMemoryMaped(bool memoryMaped)
  {
    this->rawWayDataMemoryMaped=memoryMaped;
  }

  void ImportParameter::SetRawWayIndexCacheSize(size_t wayIndexCacheSize)
  {
    this->rawWayIndexCacheSize=wayIndexCacheSize;
  }

  void ImportParameter::SetRawWayBlockSize(size_t blockSize)
  {
    this->rawWayBlockSize=blockSize;
  }

  void ImportParameter::SetAreaDataMemoryMaped(bool memoryMaped)
  {
    this->areaDataMemoryMaped=memoryMaped;
  }

  void ImportParameter::SetAreaDataCacheSize(size_t areaDataCacheSize)
  {
    this->areaDataCacheSize=areaDataCacheSize;
  }


  void ImportParameter::SetWayDataMemoryMaped(bool memoryMaped)
  {
    this->wayDataMemoryMaped=memoryMaped;
  }

  void ImportParameter::SetWayDataCacheSize(size_t wayDataCacheSize)
  {
    this->wayDataCacheSize=wayDataCacheSize;
  }

  void ImportParameter::SetAreaAreaIndexMaxMag(size_t areaAreaIndexMaxMag)
  {
    this->areaAreaIndexMaxMag=areaAreaIndexMaxMag;
  }

  void ImportParameter::SetAreaNodeMinMag(size_t areaNodeMinMag)
  {
    this->areaNodeMinMag=areaNodeMinMag;
  }

  void ImportParameter::SetAreaNodeIndexMinFillRate(double areaNodeIndexMinFillRate)
  {
    this->areaNodeIndexMinFillRate=areaNodeIndexMinFillRate;
  }

  void ImportParameter::SetAreaNodeIndexCellSizeAverage(size_t areaNodeIndexCellSizeAverage)
  {
    this->areaNodeIndexCellSizeAverage=areaNodeIndexCellSizeAverage;
  }

  void ImportParameter::SetAreaNodeIndexCellSizeMax(size_t areaNodeIndexCellSizeMax)
  {
    this->areaNodeIndexCellSizeMax=areaNodeIndexCellSizeMax;
  }

  void ImportParameter::SetAreaWayMinMag(size_t areaWayMinMag)
  {
    this->areaWayMinMag=areaWayMinMag;
  }

  void ImportParameter::SetAreaWayIndexMaxMag(size_t areaWayIndexMaxLevel)
  {
    this->areaWayIndexMaxLevel=areaWayIndexMaxLevel;
  }

  void ImportParameter::SetWaterIndexMinMag(size_t waterIndexMinMag)
  {
    this->waterIndexMinMag=waterIndexMinMag;
  }

  void ImportParameter::SetWaterIndexMaxMag(size_t waterIndexMaxMag)
  {
    this->waterIndexMaxMag=waterIndexMaxMag;
  }

  void ImportParameter::SetOptimizationMaxWayCount(size_t optimizationMaxWayCount)
  {
    this->optimizationMaxWayCount=optimizationMaxWayCount;
  }

  void ImportParameter::SetOptimizationMaxMag(size_t optimizationMaxMag)
  {
    this->optimizationMaxMag=optimizationMaxMag;
  }

  void ImportParameter::SetOptimizationMinMag(size_t optimizationMinMag)
  {
    this->optimizationMinMag=optimizationMinMag;
  }

  void ImportParameter::SetOptimizationCellSizeAverage(size_t optimizationCellSizeAverage)
  {
    this->optimizationCellSizeAverage=optimizationCellSizeAverage;
  }


  void ImportParameter::SetOptimizationCellSizeMax(size_t optimizationCellSizeMax)
  {
    this->optimizationCellSizeMax=optimizationCellSizeMax;
  }


  void ImportParameter::SetOptimizationWayMethod(TransPolygon::OptimizeMethod optimizationWayMethod)
  {
    this->optimizationWayMethod=optimizationWayMethod;
  }

  void ImportParameter::SetRouteNodeBlockSize(size_t blockSize)
  {
    this->routeNodeBlockSize=blockSize;
  }

  void ImportParameter::SetAssumeLand(bool assumeLand)
  {
    this->assumeLand=assumeLand;
  }

  ImportModule::~ImportModule()
  {
    // no code
  }

  static bool ExecuteModules(std::list<ImportModule*>& modules,
                            const ImportParameter& parameter,
                            Progress& progress,
                            const TypeConfigRef& typeConfig)
  {
    StopClock overAllTimer;
    size_t    currentStep=1;

    for (const auto& module : modules) {
      if (currentStep>=parameter.GetStartStep() &&
          currentStep<=parameter.GetEndStep()) {
        StopClock timer;
        bool      success;

        progress.SetStep(std::string("Step #")+
                         NumberToString(currentStep)+
                         " - "+
                         module->GetDescription());

        success=module->Import(typeConfig,
                               parameter,
                               progress);

        timer.Stop();

        progress.Info(std::string("=> ")+timer.ResultString()+" second(s)");

        if (!success) {
          progress.Error(std::string("Error while executing step '")+module->GetDescription()+"'!");
          return false;
        }
      }

      currentStep++;
    }

    overAllTimer.Stop();
    progress.Info(std::string("=> ")+overAllTimer.ResultString()+" second(s)");

    return true;
  }

  bool Import(const ImportParameter& parameter,
              Progress& progress)
  {
    // TODO: verify parameter

    TypeConfigRef            typeConfig(new TypeConfig());
    std::list<ImportModule*> modules;

    if (parameter.GetAreaWayMinMag()<=parameter.GetOptimizationMaxMag()) {
      progress.Error("Area way index minimum magnification is <= than optimization max magnification");
    }

    progress.SetStep("Loading type config");

    if (!typeConfig->LoadFromOSTFile(parameter.GetTypefile())) {
      progress.Error("Cannot load type configuration!");
      return false;
    }

    progress.Info("Number of types: "+NumberToString(typeConfig->GetTypes().size()));
    progress.Info("Number of node types: "+NumberToString(typeConfig->GetNodeTypes().size())+" "+NumberToString(typeConfig->GetNodeTypeIdBytes())+" byte(s)");
    progress.Info("Number of way types: "+NumberToString(typeConfig->GetWayTypes().size())+" "+NumberToString(typeConfig->GetWayTypeIdBytes())+" byte(s)");
    progress.Info("Number of area types: "+NumberToString(typeConfig->GetAreaTypes().size())+" "+NumberToString(typeConfig->GetAreaTypeIdBytes())+" byte(s)");

    typeConfig->RegisterNameTag("name",0);
    typeConfig->RegisterNameTag("place_name",1);

    /*
    typeConfig->RegisterNameAltTag("name:ru",0);
    typeConfig->RegisterNameAltTag("place_name:ru",1);
    */

    /* 1 */
    modules.push_back(new TypeDataGenerator());

    /* 2 */
    modules.push_back(new Preprocess());

    /* 3 */
    modules.push_back(new RawNodeIndexGenerator(AppendFileToDir(parameter.GetDestinationDirectory(),
                                                                "rawnodes.dat"),
                                                AppendFileToDir(parameter.GetDestinationDirectory(),
                                                                "rawnode.idx")));
    /* 4 */
    modules.push_back(new RawWayIndexGenerator(AppendFileToDir(parameter.GetDestinationDirectory(),
                                                               "rawways.dat"),
                                               AppendFileToDir(parameter.GetDestinationDirectory(),
                                                               "rawway.idx")));
    /* 5 */
    modules.push_back(new RawRelationIndexGenerator(AppendFileToDir(parameter.GetDestinationDirectory(),
                                                                    "rawrels.dat"),
                                                    AppendFileToDir(parameter.GetDestinationDirectory(),
                                                                    "rawrel.idx")));
    /* 6 */
    modules.push_back(new RelAreaDataGenerator());

    /* 7 */
    modules.push_back(new WayAreaDataGenerator());

    /* 8 */
    modules.push_back(new MergeAreaDataGenerator());

    /* 9 */
    modules.push_back(new MergeAreasGenerator());

    /* 10 */
    modules.push_back(new WayWayDataGenerator());

    /* 11 */
    modules.push_back(new OptimizeAreaWayIdsGenerator());

    /* 12 */
    modules.push_back(new NodeDataGenerator());

    /* 13 */
    modules.push_back(new SortNodeDataGenerator());

    /* 14 */
    modules.push_back(new SortWayDataGenerator());

    /* 15 */
    modules.push_back(new AreaNodeIndexGenerator());

    /* 16 */
    modules.push_back(new AreaWayIndexGenerator());

    /* 17 */
    modules.push_back(new AreaAreaIndexGenerator());

    /* 18 */
    modules.push_back(new WaterIndexGenerator());

    /* 19 */
    modules.push_back(new OptimizeAreasLowZoomGenerator());

    /* 20 */
    modules.push_back(new OptimizeWaysLowZoomGenerator());

    /* 21 */
    modules.push_back(new LocationIndexGenerator());

    /* 22 */
    modules.push_back(new RouteDataGenerator());

    /* 23 */
    modules.push_back(new NumericIndexGenerator<Id,Intersection>(std::string("Generating '")+RoutingService::FILENAME_INTERSECTIONS_IDX+"'",
                                                                 AppendFileToDir(parameter.GetDestinationDirectory(),
                                                                                 RoutingService::FILENAME_INTERSECTIONS_DAT),
                                                                 AppendFileToDir(parameter.GetDestinationDirectory(),
                                                                                 RoutingService::FILENAME_INTERSECTIONS_IDX)));

#if defined(OSMSCOUT_IMPORT_HAVE_LIB_MARISA)
    /* 24 */
    modules.push_back(new TextIndexGenerator());
#endif

    bool result=ExecuteModules(modules,
                               parameter,
                               progress,
                               typeConfig);

    for (const auto& module : modules) {
      delete module;
    }

    return result;
  }
}

