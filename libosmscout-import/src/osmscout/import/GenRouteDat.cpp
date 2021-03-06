/*
  This source is part of the libosmscout library
  Copyright (C) 2012  Tim Teulings

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

#include <osmscout/import/GenRouteDat.h>

#include <algorithm>

#include <osmscout/ObjectRef.h>

#include <osmscout/CoordDataFile.h>

#include <osmscout/RoutingService.h>

#include <osmscout/system/Assert.h>
#include <osmscout/system/Math.h>

#include <osmscout/util/Geometry.h>
#include <osmscout/util/StopClock.h>
#include <osmscout/util/String.h>

#include <osmscout/import/GenNumericIndex.h>

namespace osmscout {

  RouteDataGenerator::RouteDataGenerator()
  {
    // no code
  }

  std::string RouteDataGenerator::GetDescription() const
  {
    return "Generate routing graphs";
  }

  AccessFeatureValue RouteDataGenerator::GetAccess(const FeatureValueBuffer& buffer) const
  {
    AccessFeatureValue *accessValue=accessReader->GetValue(buffer);

    if (accessValue!=NULL) {
      return *accessValue;
    }
    else {
      return AccessFeatureValue (buffer.GetType()->GetDefaultAccess());
    }
  }

  uint8_t RouteDataGenerator::GetMaxSpeed(const Way& way) const
  {
    MaxSpeedFeatureValue *maxSpeedValue=maxSpeedReader->GetValue(way.GetFeatureValueBuffer());

    if (maxSpeedValue!=NULL) {
      return maxSpeedValue->GetMaxSpeed();
    }
    else {
      return 0;
    }
  }

  uint8_t RouteDataGenerator::GetGrade(const Way& way) const
  {
    GradeFeatureValue *gradeValue=gradeReader->GetValue(way.GetFeatureValueBuffer());

    if (gradeValue!=NULL) {
      return gradeValue->GetGrade();
    }
    else {
      return 1;
    }
  }

  uint8_t RouteDataGenerator::CopyFlags(const Area::Ring& ring) const
  {
    uint8_t                      flags=0;
    AccessFeatureValue           access=GetAccess(ring.GetFeatureValueBuffer());
    AccessRestrictedFeatureValue *accessRestrictedValue=accessRestrictedReader->GetValue(ring.GetFeatureValueBuffer());


    if (accessRestrictedValue!=NULL) {
      if (!accessRestrictedValue->CanAccessFoot()) {
        flags|=RouteNode::restrictedForFoot;
      }

      if (!accessRestrictedValue->CanAccessBicycle()) {
        flags|=RouteNode::restrictedForFoot;
      }

      if (!accessRestrictedValue->CanAccessCar()) {
        flags|=RouteNode::restrictedForCar;
      }
    }

    if (access.CanRouteFoot()) {
      flags|=RouteNode::usableByFoot;
    }

    if (access.CanRouteBicycle()) {
      flags|=RouteNode::usableByBicycle;
    }

    if (access.CanRouteCar()) {
      flags|=RouteNode::usableByCar;
    }

    return flags;
  }

  uint8_t RouteDataGenerator::CopyFlagsForward(const Way& way) const
  {
    uint8_t                      flags=0;
    AccessFeatureValue           access=GetAccess(way.GetFeatureValueBuffer());
    AccessRestrictedFeatureValue *accessRestrictedValue=accessRestrictedReader->GetValue(way.GetFeatureValueBuffer());

    if (accessRestrictedValue!=NULL) {
      if (!accessRestrictedValue->CanAccessFoot()) {
        flags|=RouteNode::restrictedForFoot;
      }

      if (!accessRestrictedValue->CanAccessBicycle()) {
        flags|=RouteNode::restrictedForFoot;
      }

      if (!accessRestrictedValue->CanAccessCar()) {
        flags|=RouteNode::restrictedForCar;
      }
    }

    if (access.CanRouteFootForward()) {
      flags|=RouteNode::usableByFoot;
    }

    if (access.CanRouteBicycleForward()) {
      flags|=RouteNode::usableByBicycle;
    }

    if (access.CanRouteCarForward()) {
      flags|=RouteNode::usableByCar;
    }

    return flags;
  }

  uint8_t RouteDataGenerator::CopyFlagsBackward(const Way& way) const
  {
    uint8_t            flags=0;
    AccessFeatureValue access=GetAccess(way.GetFeatureValueBuffer());
    AccessRestrictedFeatureValue *accessRestrictedValue=accessRestrictedReader->GetValue(way.GetFeatureValueBuffer());

    if (accessRestrictedValue!=NULL) {
      if (!accessRestrictedValue->CanAccessFoot()) {
        flags|=RouteNode::restrictedForFoot;
      }

      if (!accessRestrictedValue->CanAccessBicycle()) {
        flags|=RouteNode::restrictedForFoot;
      }

      if (!accessRestrictedValue->CanAccessCar()) {
        flags|=RouteNode::restrictedForCar;
      }
    }

    if (access.CanRouteFootBackward()) {
      flags|=RouteNode::usableByFoot;
    }

    if (access.CanRouteBicycleBackward()) {
      flags|=RouteNode::usableByBicycle;
    }

    if (access.CanRouteCarBackward()) {
      flags|=RouteNode::usableByCar;
    }

    return flags;
  }

  uint16_t RouteDataGenerator::RegisterOrUseObjectVariantData(std::map<ObjectVariantData,uint16_t>& routeDataMap,
                                                              const TypeInfoRef& type,
                                                              uint8_t maxSpeed,
                                                              uint8_t grade)
  {
    ObjectVariantData routeData;
    uint16_t          objectVariantIndex;

    routeData.type=type;
    routeData.maxSpeed=maxSpeed;
    routeData.grade=grade;

    const auto entry=routeDataMap.find(routeData);

    if (entry==routeDataMap.end()) {
      objectVariantIndex=routeDataMap.size();
      routeDataMap.insert(std::make_pair(routeData,objectVariantIndex));
    }
    else {
      objectVariantIndex=entry->second;
    }

    return objectVariantIndex;
  }

  bool RouteDataGenerator::IsAnyRoutable(Progress& progress,
                                         const std::list<ObjectFileRef>& objects,
                                         const std::unordered_map<FileOffset,WayRef>& waysMap,
                                         const std::unordered_map<FileOffset,AreaRef>&  areasMap,
                                         VehicleMask vehicles) const
  {
    for (const auto& ref : objects) {
      if (ref.GetType()==refWay) {
        const auto& wayEntry=waysMap.find(ref.GetFileOffset());

        if (wayEntry==waysMap.end()) {
          progress.Error("Error while loading way at offset "+
                         NumberToString(ref.GetFileOffset()) +
                         " (Internal error?)");
          continue;
        }

        if (GetAccess(*wayEntry->second).CanRoute(vehicles)) {
          return true;
        }

      }
      else if (ref.GetType()==refArea) {
        const auto& areaEntry=areasMap.find(ref.GetFileOffset());

        if (areaEntry==areasMap.end()) {
          progress.Error("Error while loading areaEntry at offset "+
                         NumberToString(ref.GetFileOffset()) +
                         " (Internal error?)");
          continue;
        }

        if (areaEntry->second->GetType()->CanRoute()) {
          return true;
        }
      }
    }

    return false;
  }

  bool RouteDataGenerator::ReadTurnRestrictionIds(const ImportParameter& parameter,
                                                  Progress& progress,
                                                  std::map<OSMId,FileOffset>& wayIdOffsetMap,
                                                  std::map<OSMId,Id>& nodeIdMap)
  {
    FileScanner scanner;
    uint32_t    restrictionCount=0;

    progress.Info("Reading turn restriction way ids");

    if (!scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                      "turnrestr.dat"),
                      FileScanner::Sequential,
                      true)) {
      progress.Error("Cannot open '"+scanner.GetFilename()+"'");
      return false;
    }

    if (!scanner.Read(restrictionCount)) {
      progress.Error("Error while reading number of data entries in file");
      return false;
    }

    for (uint32_t r=1; r<=restrictionCount; r++) {
      progress.SetProgress(r,restrictionCount);

      TurnRestrictionRef restriction=std::make_shared<TurnRestriction>();

      if (!restriction->Read(scanner)) {
        progress.Error(std::string("Error while reading data entry ")+
                       NumberToString(r)+" of "+
                       NumberToString(restrictionCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      wayIdOffsetMap.insert(std::make_pair(restriction->GetFrom(),0));
      wayIdOffsetMap.insert(std::make_pair(restriction->GetTo(),0));

      nodeIdMap.insert(std::make_pair(restriction->GetVia(),0));
    }

    if (!scanner.Close()) {
      progress.Error("Cannot close file 'turnrestr.dat'");
      return false;
    }

    return true;
  }

  bool RouteDataGenerator::ResolveWayIds(const ImportParameter& parameter,
                                         Progress& progress,
                                         std::map<OSMId,FileOffset>& wayIdOffsetMap)
  {
    FileScanner scanner;
    uint32_t    wayCount=0;
    uint32_t    resolveCount=0;

    progress.Info("Resolving turn restriction way ids to way file offsets");

    if (!scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                      "ways.idmap"),
                      FileScanner::Sequential,
                      parameter.GetWayDataMemoryMaped())) {
      progress.Error("Cannot open '"+scanner.GetFilename()+"'");

      return false;
    }

    if (!scanner.Read(wayCount)) {
      progress.Error("Error while reading number of data entries in file");
      return false;
    }

    for (uint32_t w=1; w<=wayCount; w++) {
      progress.SetProgress(w,wayCount);

      OSMId      wayId;
      uint8_t    typeByte;
      OSMRefType type;
      FileOffset wayOffset;

      if (!scanner.Read(wayId) ||
          !scanner.Read(typeByte) ||
          !scanner.ReadFileOffset(wayOffset)) {
        progress.Error(std::string("Error while reading idmap file entry ")+
                       NumberToString(w)+" of "+
                       NumberToString(wayCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      type=(OSMRefType)typeByte;

      if (type!=osmRefWay) {
        continue;
      }

      std::map<OSMId,FileOffset>::iterator idOffsetEntry;

      idOffsetEntry=wayIdOffsetMap.find(wayId);

      if (idOffsetEntry!=wayIdOffsetMap.end()) {
        idOffsetEntry->second=wayOffset;
        resolveCount++;
      }
    }

    if (!scanner.Close()) {
      progress.Error(std::string("Cannot close file '")+scanner.GetFilename()+"'");
      return false;
    }

    progress.Info(NumberToString(resolveCount)+" id(s) resolved");

    return true;
  }

  bool RouteDataGenerator::ResolveNodeIds(const ImportParameter& parameter,
                                          Progress& progress,
                                          std::map<OSMId,Id>& nodeIdMap)
  {
    progress.Info("Resolving turn restriction OSM node ids to node ids");

    CoordDataFile coordDataFile("coord.dat");
    uint32_t      resolveCount=0;

    if (!coordDataFile.Open(parameter.GetDestinationDirectory(),
                            parameter.GetCoordDataMemoryMaped())) {
      std::cerr << "Cannot open coord data file!" << std::endl;
      return false;
    }

    std::set<OSMId>               nodeIds;
    CoordDataFile::CoordResultMap coordsMap;

    for (const auto& entry : nodeIdMap) {
      nodeIds.insert(entry.first);
    }

    if (!coordDataFile.Get(nodeIds,
                           coordsMap)) {
      std::cerr << "Cannot read nodes!" << std::endl;
      return false;
    }

    for (const auto& entry : coordsMap) {
      auto nodeIdEntry=nodeIdMap.find(entry.first);

      if (nodeIdEntry!=nodeIdMap.end()) {
        nodeIdEntry->second=entry.second.point.GetId();
        resolveCount++;
      }
    }

    if (!coordDataFile.Close()) {
      progress.Error(std::string("Cannot close file '")+coordDataFile.GetFilename()+"'");
      return false;
    }

    progress.Info(NumberToString(resolveCount)+" id(s) resolved");

    return true;
  }

  bool RouteDataGenerator::ReadTurnRestrictionData(const ImportParameter& parameter,
                                                   Progress& progress,
                                                   const std::map<OSMId,Id>& nodeIdMap,
                                                   const std::map<OSMId,FileOffset>& wayIdOffsetMap,
                                                   ViaTurnRestrictionMap& restrictions)
  {
    FileScanner scanner;
    uint32_t    restrictionCount=0;

    progress.Info("Creating turn restriction data structures");

    if (!scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                      "turnrestr.dat"),
                      FileScanner::Sequential,
                      true)) {
      progress.Error("Cannot open '"+scanner.GetFilename()+"'");
      return false;
    }

    if (!scanner.Read(restrictionCount)) {
      progress.Error("Error while reading number of data entries in file");
      return false;
    }

    for (uint32_t r=1; r<=restrictionCount; r++) {
      progress.SetProgress(r,restrictionCount);

      TurnRestrictionRef restriction=std::make_shared<TurnRestriction>();

      if (!restriction->Read(scanner)) {
        progress.Error(std::string("Error while reading data entry ")+
                       NumberToString(r)+" of "+
                       NumberToString(restrictionCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      TurnRestrictionData                        data;
      std::map<OSMId,FileOffset>::const_iterator idOffsetEntry;
      std::map<OSMId,Id>::const_iterator         nodeIdEntry;


      idOffsetEntry=wayIdOffsetMap.find(restriction->GetFrom());

      if (idOffsetEntry==wayIdOffsetMap.end() || idOffsetEntry->second==0) {
        progress.Error(std::string("Error while retrieving way offset for way id ")+
                       NumberToString(restriction->GetFrom()));
        continue;
      }

      data.fromWayOffset=idOffsetEntry->second;

      nodeIdEntry=nodeIdMap.find(restriction->GetVia());

      if (nodeIdEntry==nodeIdMap.end() || nodeIdEntry->second==0) {
        progress.Error(std::string("Error while retrieving node id for node OSM id ")+
                       NumberToString(restriction->GetVia()));
        continue;
      }

      data.viaNodeId=nodeIdEntry->second;

      idOffsetEntry=wayIdOffsetMap.find(restriction->GetTo());

      if (idOffsetEntry==wayIdOffsetMap.end() || idOffsetEntry->second==0) {
        progress.Error(std::string("Error while retrieving way offset for way id ")+
                       NumberToString(restriction->GetTo()));
        continue;
      }

      data.toWayOffset=idOffsetEntry->second;

      switch (restriction->GetType()) {
      case TurnRestriction::Allow:
        data.type=TurnRestrictionData::Allow;
        break;
      case TurnRestriction::Forbit:
        data.type=TurnRestrictionData::Forbit;
        break;
      }

      restrictions[data.viaNodeId].push_back(data);
    }

    if (!scanner.Close()) {
      progress.Error("Cannot close file 'turnrestr.dat'");
      return false;
    }

    progress.Info(std::string("Read ")+NumberToString(restrictionCount)+" turn restrictions");

    return true;
  }

  bool RouteDataGenerator::ReadTurnRestrictions(const ImportParameter& parameter,
                                                Progress& progress,
                                                ViaTurnRestrictionMap& restrictions)
  {
    std::map<OSMId,FileOffset> wayIdOffsetMap;
    std::map<OSMId,Id>         nodeIdMap;

    //
    // Just read the way ids
    //

    if (!ReadTurnRestrictionIds(parameter,
                                progress,
                                wayIdOffsetMap,
                                nodeIdMap)) {
      return false;
    }

    //
    // Now map way ids to file offsets
    //

    if (!ResolveWayIds(parameter,
                       progress,
                       wayIdOffsetMap)) {
      return false;
    }

    //
    // Now map node ids to file offsets
    //

    if (!ResolveNodeIds(parameter,
                       progress,
                       nodeIdMap)) {
      return false;
    }

    //
    // Finally read restrictions again and replace way ids with way offsets
    //

    if (!ReadTurnRestrictionData(parameter,
                                 progress,
                                 nodeIdMap,
                                 wayIdOffsetMap,
                                 restrictions)) {
      return false;
    }

    return true;
  }

  bool RouteDataGenerator::CanTurn(const std::vector<TurnRestrictionData>& restrictions,
                                   FileOffset from,
                                   FileOffset to) const
  {
    bool defaultReturn=true;

    if (restrictions.empty()) {
      return true;
    }

    for (const auto& restriction : restrictions) {
      if (restriction.fromWayOffset==from) {
        if (restriction.type==TurnRestrictionData::Allow) {
          if (restriction.toWayOffset==to) {
            return true;
          }

          // If there are allow restrictions,everything else is forbidden
          defaultReturn=false;
        }
        else if (restriction.type==TurnRestrictionData::Forbit) {
          if (restriction.toWayOffset==to) {
            return false;
          }

          // If there are forbid restrictions, everything else is allowed
          defaultReturn=true;
        }
      }
    }

    // Now lets hope nobody is mixing allow and forbid!
    return defaultReturn;
  }

  bool RouteDataGenerator::ReadIntersections(const ImportParameter& parameter,
                                             Progress& progress,
                                             const TypeConfig& typeConfig,
                                             NodeUseMap& nodeUseMap)
  {
    FileScanner scanner;
    uint32_t    dataCount=0;

    progress.Info("Scanning ways");

    if (!scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                      "ways.dat"),
                      FileScanner::Sequential,
                      parameter.GetWayDataMemoryMaped())) {
      progress.Error("Cannot open '"+scanner.GetFilename()+"'");

      return false;
    }

    if (!scanner.Read(dataCount)) {
      progress.Error("Error while reading number of data entries in file");
      return false;
    }

    for (uint32_t d=1; d<=dataCount; d++) {
      progress.SetProgress(d,dataCount);

      Way way;

      if (!way.Read(typeConfig,
                    scanner)) {
        progress.Error(std::string("Error while reading data entry ")+
                       NumberToString(d)+" of "+
                       NumberToString(dataCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      if (way.GetType()->GetIgnore()) {
        continue;
      }

      if (!GetAccess(way).CanRoute()) {
        continue;
      }

      std::set<Id> nodeIds;

      for (const auto& id : way.ids) {
        if (id==0) {
          continue;
        }

        if (nodeIds.find(id)==nodeIds.end()) {
          nodeUseMap.SetNodeUsed(id);

          nodeIds.insert(id);
        }
      }
    }

    if (!scanner.Close()) {
      progress.Error("Cannot close file 'ways.dat'");
      return false;
    }

    progress.Info("Scanning areas");

    if (!scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                      "areas.dat"),
                      FileScanner::Sequential,
                      parameter.GetWayDataMemoryMaped())) {
      progress.Error("Cannot open '"+scanner.GetFilename()+"'");

      return false;
    }

    if (!scanner.Read(dataCount)) {
      progress.Error("Error while reading number of data entries in file");
      return false;
    }

    for (uint32_t d=1; d<=dataCount; d++) {
      progress.SetProgress(d,dataCount);

      Area area;

      if (!area.Read(typeConfig,
                     scanner)) {
        progress.Error(std::string("Error while reading data entry ")+
                       NumberToString(d)+" of "+
                       NumberToString(dataCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      if (area.GetType()->GetIgnore()) {
        continue;
      }

      if (!area.GetType()->CanRoute()) {
        continue;
      }

      // We currently route only on simple areas, multipolygon relations
      if (!area.IsSimple()) {
        continue;
      }

      std::set<Id> nodeIds;

      for (const auto& id : area.rings.front().ids) {
        if (id==0) {
          continue;
        }

        if (nodeIds.find(id)==nodeIds.end()) {
          nodeUseMap.SetNodeUsed(id);

          nodeIds.insert(id);
        }
      }
    }

    if (!scanner.Close()) {
      progress.Error("Cannot close file 'areas.dat'");
      return false;
    }

    return true;
  }

  bool RouteDataGenerator::ReadObjectsAtIntersections(const ImportParameter& parameter,
                                                      Progress& progress,
                                                      const TypeConfig& typeConfig,
                                                      const NodeUseMap& nodeUseMap,
                                                      NodeIdObjectsMap& nodeObjectsMap)
  {
    FileScanner scanner;
    uint32_t    dataCount=0;

    progress.Info("Scanning ways");

    if (!scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                      "ways.dat"),
                      FileScanner::Sequential,
                      parameter.GetWayDataMemoryMaped())) {
      progress.Error("Cannot open '"+scanner.GetFilename()+"'");

      return false;
    }

    if (!scanner.Read(dataCount)) {
      progress.Error("Error while reading number of data entries in file");
      return false;
    }

    for (uint32_t d=1; d<=dataCount; d++) {
      progress.SetProgress(d,dataCount);

      Way        way;
      FileOffset fileOffset;

      if (!scanner.GetPos(fileOffset)) {
        progress.Error(std::string("Error while reading file offset for data entry ")+
                       NumberToString(d)+" of "+
                       NumberToString(dataCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      if (!way.Read(typeConfig,
                    scanner)) {
        progress.Error(std::string("Error while reading data entry ")+
                       NumberToString(d)+" of "+
                       NumberToString(dataCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      if (way.GetType()->GetIgnore()) {
        continue;
      }

      if (!GetAccess(way).CanRoute()) {
        continue;
      }

      std::set<Id> nodeIds;

      for (auto id : way.ids) {
        if (id==0) {
          continue;
        }

        if (nodeIds.find(id)==nodeIds.end()) {
          if (nodeUseMap.IsNodeUsedAtLeastTwice(id)) {
            nodeObjectsMap[id].push_back(ObjectFileRef(fileOffset,refWay));
          }

          nodeIds.insert(id);
        }
      }
    }

    if (!scanner.Close()) {
      progress.Error("Cannot close file 'ways.dat'");
      return false;
    }

    progress.Info("Scanning areas");

    if (!scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                      "areas.dat"),
                      FileScanner::Sequential,
                      parameter.GetWayDataMemoryMaped())) {
      progress.Error("Cannot open '"+scanner.GetFilename()+"'");

      return false;
    }

    if (!scanner.Read(dataCount)) {
      progress.Error("Error while reading number of data entries in file");
      return false;
    }

    for (uint32_t d=1; d<=dataCount; d++) {
      progress.SetProgress(d,dataCount);

      Area       area;
      FileOffset fileOffset;

      if (!scanner.GetPos(fileOffset)) {
        progress.Error(std::string("Error while reading file offset for data entry ")+
                       NumberToString(d)+" of "+
                       NumberToString(dataCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      if (!area.Read(typeConfig,
                     scanner)) {
        progress.Error(std::string("Error while reading data entry ")+
                       NumberToString(d)+" of "+
                       NumberToString(dataCount)+
                       " in file '"+
                       scanner.GetFilename()+"'");
        return false;
      }

      if (area.GetType()->GetIgnore()) {
        continue;
      }

      if (!(area.GetType()->CanRoute())) {
        continue;
      }

      // We currently route only on simple areas, multipolygon relations
      if (!area.IsSimple()) {
        continue;
      }

      std::set<Id> nodeIds;

      for (auto id : area.rings.front().ids) {
        if (id==0) {
          continue;
        }

        if (nodeIds.find(id)==nodeIds.end()) {
          if (nodeUseMap.IsNodeUsedAtLeastTwice(id)) {
            nodeObjectsMap[id].push_back(ObjectFileRef(fileOffset,refArea));
          }

          nodeIds.insert(id);
        }
      }
    }

    if (!scanner.Close()) {
      progress.Error("Cannot close file 'areas.dat'");
      return false;
    }

    return true;
  }

  bool RouteDataGenerator::WriteIntersections(const ImportParameter& parameter,
                                              Progress& progress,
                                              NodeIdObjectsMap& nodeIdObjectsMap)
  {
    FileWriter writer;

    if (!writer.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                     RoutingService::FILENAME_INTERSECTIONS_DAT))) {
      progress.Error("Cannot create '"+writer.GetFilename()+"'");
      return false;
    }

    writer.Write((uint32_t)nodeIdObjectsMap.size());

    for (const auto& junction : nodeIdObjectsMap) {

      writer.WriteNumber(junction.first);
      writer.WriteNumber((uint32_t)junction.second.size());

      ObjectFileRefStreamWriter objectFileRefWriter(writer);

      for (const auto& object : junction.second) {
        objectFileRefWriter.Write(object);
      }
    }

    return writer.Close();
  }

  bool RouteDataGenerator::LoadWays(const TypeConfig& typeConfig,
                                    Progress& progress,
                                    FileScanner& scanner,
                                    const std::set<FileOffset>& fileOffsets,
                                    std::unordered_map<FileOffset,WayRef>& waysMap)
  {
    if (fileOffsets.empty()) {
      return true;
    }

    progress.Info("Loading " +NumberToString(fileOffsets.size())+" ways");

    FileOffset oldPos;

    if (!scanner.GetPos(oldPos)) {
      progress.Error("Error while loading current file position");
      return false;
    }

    for (auto offset : fileOffsets) {
      if (!scanner.SetPos(offset)) {
        progress.Error("Error while moving to way at offset " + NumberToString(offset));
        return false;
      }

      WayRef way=std::make_shared<Way>();

      way->Read(typeConfig,
                scanner);

      if (scanner.HasError()) {
        progress.Error("Error while loading way at offset " + NumberToString(offset));
        return false;
      }

      waysMap[offset]=way;
    }

    if (!scanner.SetPos(oldPos)) {
      progress.Error("Error while resetting current file position");
      return false;
    }

    return true;
  }

  bool RouteDataGenerator::LoadAreas(const TypeConfig& typeConfig,
                                     Progress& progress,
                                     FileScanner& scanner,
                                     const std::set<FileOffset>& fileOffsets,
                                     std::unordered_map<FileOffset,AreaRef>& areasMap)
  {
    if (fileOffsets.empty()) {
      return true;
    }

    progress.Info("Loading " +NumberToString(fileOffsets.size())+" areas");

    FileOffset oldPos;

    if (!scanner.GetPos(oldPos)) {
      progress.Error("Error while loading current file position");
      return false;
    }

    for (auto offset : fileOffsets) {
      if (!scanner.SetPos(offset)) {
        progress.Error("Error while moving to area at offset " + NumberToString(offset));
        return false;
      }

      AreaRef area=std::make_shared<Area>();

      area->Read(typeConfig,
                 scanner);

      if (scanner.HasError()) {
        progress.Error("Error while loading area at offset " + NumberToString(offset));
        return false;
      }

      areasMap[offset]=area;
    }

    if (!scanner.SetPos(oldPos)) {
      progress.Error("Error while resetting current file position");
      return false;
    }

    return true;
  }

  bool RouteDataGenerator::GetRouteNodeCoord(Progress& progress,
                                             Id id,
                                             const std::list<ObjectFileRef>& objects,
                                             std::unordered_map<FileOffset,WayRef>& waysMap,
                                             std::unordered_map<FileOffset,AreaRef>& areasMap,
                                             GeoCoord& coord) const
  {
    for (const auto& ref : objects) {
      if (ref.GetType()==refWay) {
        const WayRef& way=waysMap[ref.GetFileOffset()];

        if (!way) {
          progress.Error("Error while loading way at offset "+
                         NumberToString(ref.GetFileOffset()) +
                         " (Internal error?)");
          continue;
        }

        int currentNode=0;

        // Find current route node in area
        while (currentNode<(int)way->nodes.size() &&
              way->ids[currentNode]!=id) {
          currentNode++;
        }

        // Make sure we found it
        assert(currentNode<(int)way->nodes.size());

        coord=way->nodes[currentNode];

        return true;
      }
      else if (ref.GetType()==refArea) {
        const AreaRef& area=areasMap[ref.GetFileOffset()];

        if (!area) {
          progress.Error("Error while loading area at offset "+
                         NumberToString(ref.GetFileOffset()) +
                         " (Internal error?)");
          continue;
        }

        int               currentNode=0;
        const Area::Ring& ring=area->rings.front();

        // Find current route node in area
        while (currentNode<(int)ring.nodes.size() &&
              ring.ids[currentNode]!=id) {
          currentNode++;
        }

        // Make sure we found it
        assert(currentNode<(int)ring.nodes.size());

        coord=ring.nodes[currentNode];

        return true;
      }
    }

    return false;
  }


  /*
  uint8_t RouteDataGenerator::CalculateEncodedBearing(const Way& way,
                                                      size_t currentNode,
                                                      size_t nextNode,
                                                      bool clockwise) const
  {
    size_t currentNodeFollower;
    size_t nextNodePrecursor;

    if (clockwise) {
      currentNodeFollower=currentNode==way.nodes.size()-1 ? 0 : currentNode+1;
      nextNodePrecursor=nextNode>0 ? nextNode-1 :way.nodes.size()-1;
    }
    else {
      currentNodeFollower=currentNode>0 ? currentNode-1 :way.nodes.size()-1;
      nextNodePrecursor=nextNode==way.nodes.size()-1 ? 0 : nextNode+1;
    }

    double initialBearing=GetSphericalBearingInitial(way.nodes[currentNode].GetLon(),
                                                     way.nodes[currentNode].GetLat(),
                                                     way.nodes[currentNodeFollower].GetLon(),
                                                     way.nodes[currentNodeFollower].GetLat());
    double finalBearing=GetSphericalBearingInitial(way.nodes[nextNodePrecursor].GetLon(),
                                                   way.nodes[nextNodePrecursor].GetLat(),
                                                   way.nodes[nextNode].GetLon(),
                                                   way.nodes[nextNode].GetLat());

    // Transform in 0..360 Degree

    initialBearing=fmod(initialBearing*180.0/M_PI+360.0,360.0);
    finalBearing=fmod(initialBearing*180.0/M_PI+360.0,360.0);

    uint8_t bearing=initialBearing/10+100+finalBearing/10;

    return bearing;
  }*/

  void RouteDataGenerator::CalculateAreaPaths(RouteNode& routeNode,
                                              const Area& area,
                                              uint16_t objectVariantIndex,
                                              FileOffset routeNodeOffset,
                                              const NodeIdObjectsMap& nodeObjectsMap,
                                              const NodeIdOffsetMap& nodeIdOffsetMap,
                                              PendingRouteNodeOffsetsMap& pendingOffsetsMap)
  {
    int               currentNode=0;
    double            distance;
    const Area::Ring& ring=area.rings.front();

    // Find current route node in area
    while (currentNode<(int)ring.nodes.size() &&
          ring.ids[currentNode]!=routeNode.id) {
      currentNode++;
    }

    // Make sure we found it
    assert(currentNode<(int)ring.nodes.size());

    // Find next routing node in order

    int nextNode=currentNode+1;

    if (nextNode>=(int)ring.nodes.size()) {
      nextNode=0;
    }

    distance=GetSphericalDistance(ring.nodes[currentNode].GetLon(),
                                  ring.nodes[currentNode].GetLat(),
                                  ring.nodes[nextNode].GetLon(),
                                  ring.nodes[nextNode].GetLat());

    while (nextNode!=currentNode &&
           nodeObjectsMap.find(ring.ids[nextNode])==nodeObjectsMap.end()) {
      int lastNode=nextNode;
      nextNode++;

      if (nextNode>=(int)ring.nodes.size()) {
        nextNode=0;
      }

      if (nextNode!=currentNode) {
        distance+=GetSphericalDistance(ring.nodes[lastNode].GetLon(),
                                       ring.nodes[lastNode].GetLat(),
                                       ring.nodes[nextNode].GetLon(),
                                       ring.nodes[nextNode].GetLat());
      }
    }

    // Found next routing node in order
    if (nextNode!=currentNode &&
        ring.ids[nextNode]!=routeNode.id) {
      RouteNode::Path                 path;
      NodeIdOffsetMap::const_iterator pathNodeOffset=nodeIdOffsetMap.find(ring.ids[nextNode]);

      if (pathNodeOffset!=nodeIdOffsetMap.end()) {
        path.offset=pathNodeOffset->second;
      }
      else {
        PendingOffset pendingOffset;

        pendingOffset.routeNodeOffset=routeNodeOffset;
        pendingOffset.index=routeNode.paths.size();

        pendingOffsetsMap[ring.ids[nextNode]].push_back(pendingOffset);
      }

      path.objectIndex=routeNode.AddObject(ObjectFileRef(area.GetFileOffset(),refArea),
                                           objectVariantIndex);
      //path.bearing=CalculateEncodedBearing(way,currentNode,nextNode,true);
      path.flags=CopyFlags(ring);
      path.distance=distance;

      routeNode.paths.push_back(path);
    }

    // Find next routing node against order

    int prevNode=currentNode-1;

    if (prevNode<0) {
      prevNode=(int)(ring.nodes.size()-1);
    }

    distance=GetSphericalDistance(ring.nodes[currentNode].GetLon(),
                                  ring.nodes[currentNode].GetLat(),
                                  ring.nodes[prevNode].GetLon(),
                                  ring.nodes[prevNode].GetLat());

    while (prevNode!=currentNode &&
        nodeObjectsMap.find(ring.ids[prevNode])==nodeObjectsMap.end()) {
      int lastNode=prevNode;
      prevNode--;

      if (prevNode<0) {
        prevNode=(int)(ring.nodes.size()-1);
      }

      if (prevNode!=currentNode) {
        distance+=GetSphericalDistance(ring.nodes[lastNode].GetLon(),
                                       ring.nodes[lastNode].GetLat(),
                                       ring.nodes[prevNode].GetLon(),
                                       ring.nodes[prevNode].GetLat());
      }
    }

    // Found next routing node against order

    if (prevNode!=currentNode &&
        prevNode!=nextNode &&
        ring.ids[prevNode]!=routeNode.id) {
      RouteNode::Path                 path;
      NodeIdOffsetMap::const_iterator pathNodeOffset=nodeIdOffsetMap.find(ring.ids[prevNode]);

      if (pathNodeOffset!=nodeIdOffsetMap.end()) {
        path.offset=pathNodeOffset->second;
      }
      else {
        PendingOffset pendingOffset;

        pendingOffset.routeNodeOffset=routeNodeOffset;
        pendingOffset.index=routeNode.paths.size();

        pendingOffsetsMap[ring.ids[prevNode]].push_back(pendingOffset);
      }

      path.objectIndex=routeNode.AddObject(ObjectFileRef(area.GetFileOffset(),refArea),
                                           objectVariantIndex);
      //path.bearing=CalculateEncodedBearing(way,currentNode,prevNode,false);
      path.flags=CopyFlags(ring);
      path.distance=distance;

      routeNode.paths.push_back(path);
    }
  }

  void RouteDataGenerator::CalculateCircularWayPaths(RouteNode& routeNode,
                                                     const Way& way,
                                                     uint16_t objectVariantIndex,
                                                     FileOffset routeNodeOffset,
                                                     const NodeIdObjectsMap& nodeObjectsMap,
                                                     const NodeIdOffsetMap& nodeIdOffsetMap,
                                                     PendingRouteNodeOffsetsMap& pendingOffsetsMap)
  {
    int    currentNode=0;
    double distance;

    // Search for current route node

    while (currentNode<(int)way.nodes.size() &&
          way.ids[currentNode]!=routeNode.id) {
      currentNode++;
    }

    assert(currentNode<(int)way.nodes.size());

    // In path direction

    int nextNode=currentNode+1;
    if (GetAccess(way).CanRouteForward()) {

      if (nextNode>=(int)way.nodes.size()) {
        nextNode=0;
      }

      distance=GetSphericalDistance(way.nodes[currentNode].GetLon(),
                                    way.nodes[currentNode].GetLat(),
                                    way.nodes[nextNode].GetLon(),
                                    way.nodes[nextNode].GetLat());

      while (nextNode!=currentNode &&
          nodeObjectsMap.find(way.ids[nextNode])==nodeObjectsMap.end()) {
        int lastNode=nextNode;
        nextNode++;

        if (nextNode>=(int)way.nodes.size()) {
          nextNode=0;
        }

        if (nextNode!=currentNode) {
          distance+=GetSphericalDistance(way.nodes[lastNode].GetLon(),
                                         way.nodes[lastNode].GetLat(),
                                         way.nodes[nextNode].GetLon(),
                                         way.nodes[nextNode].GetLat());
        }
      }

      if (nextNode!=currentNode &&
          way.ids[nextNode]!=routeNode.id) {
        RouteNode::Path                 path;
        NodeIdOffsetMap::const_iterator pathNodeOffset=nodeIdOffsetMap.find(way.ids[nextNode]);

        if (pathNodeOffset!=nodeIdOffsetMap.end()) {
          path.offset=pathNodeOffset->second;
        }
        else {
          PendingOffset pendingOffset;

          pendingOffset.routeNodeOffset=routeNodeOffset;
          pendingOffset.index=routeNode.paths.size();

          pendingOffsetsMap[way.ids[nextNode]].push_back(pendingOffset);
        }

        path.objectIndex=routeNode.AddObject(ObjectFileRef(way.GetFileOffset(),refWay),
                                             objectVariantIndex);
        //path.bearing=CalculateEncodedBearing(way,currentNode,nextNode,true);
        path.flags=CopyFlagsForward(way);
        path.distance=distance;

        routeNode.paths.push_back(path);
      }
    }

    // Against path direction

    if (GetAccess(way).CanRouteBackward()) {
      int prevNode=currentNode-1;

      if (prevNode<0) {
        prevNode=(int)(way.nodes.size()-1);
      }

      distance=GetSphericalDistance(way.nodes[currentNode].GetLon(),
                                    way.nodes[currentNode].GetLat(),
                                    way.nodes[prevNode].GetLon(),
                                    way.nodes[prevNode].GetLat());

      while (prevNode!=currentNode &&
          nodeObjectsMap.find(way.ids[prevNode])==nodeObjectsMap.end()) {
        int lastNode=prevNode;
        prevNode--;

        if (prevNode<0) {
          prevNode=(int)(way.nodes.size()-1);
        }

        if (prevNode!=currentNode) {
          distance+=GetSphericalDistance(way.nodes[lastNode].GetLon(),
                                         way.nodes[lastNode].GetLat(),
                                         way.nodes[prevNode].GetLon(),
                                         way.nodes[prevNode].GetLat());
        }
      }

      if (prevNode!=currentNode &&
          prevNode!=nextNode &&
          way.ids[prevNode]!=routeNode.id) {
        RouteNode::Path                 path;
        NodeIdOffsetMap::const_iterator pathNodeOffset=nodeIdOffsetMap.find(way.ids[prevNode]);

        if (pathNodeOffset!=nodeIdOffsetMap.end()) {
          path.offset=pathNodeOffset->second;
        }
        else {
          PendingOffset pendingOffset;

          pendingOffset.routeNodeOffset=routeNodeOffset;
          pendingOffset.index=routeNode.paths.size();

          pendingOffsetsMap[way.ids[prevNode]].push_back(pendingOffset);
        }

        path.objectIndex=routeNode.AddObject(ObjectFileRef(way.GetFileOffset(),refWay),
                                             objectVariantIndex);
        //path.bearing=CalculateEncodedBearing(way,prevNode,nextNode,false);
        path.flags=CopyFlagsBackward(way);
        path.distance=distance;

        routeNode.paths.push_back(path);
      }
    }
  }

  void RouteDataGenerator::CalculateWayPaths(RouteNode& routeNode,
                                             const Way& way,
                                             uint16_t objectVariantIndex,
                                             FileOffset routeNodeOffset,
                                             const NodeIdObjectsMap& nodeObjectsMap,
                                             const NodeIdOffsetMap& nodeIdOffsetMap,
                                             PendingRouteNodeOffsetsMap& pendingOffsetsMap)
  {
    for (size_t i=0; i<way.nodes.size(); i++) {
      if (way.ids[i]==routeNode.id) {
        // Route backward
        if (GetAccess(way).CanRouteBackward() &&
            i>0) {
          int j=i-1;

          // Search for previous routing node on way
          while (j>=0) {
            if (nodeObjectsMap.find(way.ids[j])!=nodeObjectsMap.end()) {
              break;
            }

            j--;
          }

          if (j>=0 &&
              way.ids[j]!=routeNode.id) {
            RouteNode::Path                 path;
            NodeIdOffsetMap::const_iterator pathNodeOffset=nodeIdOffsetMap.find(way.ids[j]);

            if (pathNodeOffset!=nodeIdOffsetMap.end()) {
              path.offset=pathNodeOffset->second;
            }
            else {
              PendingOffset pendingOffset;

              pendingOffset.routeNodeOffset=routeNodeOffset;
              pendingOffset.index=routeNode.paths.size();

              pendingOffsetsMap[way.ids[j]].push_back(pendingOffset);

              path.offset=0;
            }

            path.objectIndex=routeNode.AddObject(ObjectFileRef(way.GetFileOffset(),refWay),
                                                 objectVariantIndex);
            //path.bearing=CalculateEncodedBearing(way,i,j,false);
            path.flags=CopyFlagsBackward(way);

            path.distance=0.0;
            for (size_t d=j;d<i; d++) {
              path.distance+=GetSphericalDistance(way.nodes[d].GetLon(),
                                                  way.nodes[d].GetLat(),
                                                  way.nodes[d+1].GetLon(),
                                                  way.nodes[d+1].GetLat());
            }

            routeNode.paths.push_back(path);
          }
        }

        // Route forward
        if (GetAccess(way).CanRouteForward() &&
            i+1<way.nodes.size()) {
          size_t j=i+1;

          // Search for next routing node on way
          while (j<way.nodes.size()) {
            if (nodeObjectsMap.find(way.ids[j])!=nodeObjectsMap.end()) {
              break;
            }

            j++;
          }

          if (j<way.nodes.size() &&
              way.ids[j]!=routeNode.id) {
            RouteNode::Path                 path;
            NodeIdOffsetMap::const_iterator pathNodeOffset=nodeIdOffsetMap.find(way.ids[j]);

            if (pathNodeOffset!=nodeIdOffsetMap.end()) {
              path.offset=pathNodeOffset->second;
            }
            else {

              PendingOffset pendingOffset;

              pendingOffset.routeNodeOffset=routeNodeOffset;
              pendingOffset.index=routeNode.paths.size();

              pendingOffsetsMap[way.ids[j]].push_back(pendingOffset);

              path.offset=0;
            }

            path.objectIndex=routeNode.AddObject(ObjectFileRef(way.GetFileOffset(),refWay),
                                                 objectVariantIndex);
            //path.bearing=CalculateEncodedBearing(way,i,j,true);
            path.flags=CopyFlagsForward(way);

            path.distance=0.0;
            for (size_t d=i;d<j; d++) {
              path.distance+=GetSphericalDistance(way.nodes[d].GetLon(),
                                                  way.nodes[d].GetLat(),
                                                  way.nodes[d+1].GetLon(),
                                                  way.nodes[d+1].GetLat());
            }

            routeNode.paths.push_back(path);
          }
        }
      }
    }
  }

  void RouteDataGenerator::FillRoutePathExcludes(RouteNode& routeNode,
                                                 const std::list<ObjectFileRef>& objects,
                                                 const ViaTurnRestrictionMap& restrictions)
  {
    ViaTurnRestrictionMap::const_iterator turnConstraints=restrictions.find(routeNode.GetId());

    if (turnConstraints==restrictions.end()) {
      return;
    }

    for (const auto& source : objects) {

        // TODO: Fix CanTurn
        if (source.GetType()!=refWay) {
          continue;
        }

      for (const auto& dest: objects) {
        // TODO: Fix CanTurn
        if (dest.GetType()!=refWay) {
          continue;
        }

        if (source==dest) {
          continue;
        }

        if (!CanTurn(turnConstraints->second,
                     source.GetFileOffset(),
                     dest.GetFileOffset())) {
          RouteNode::Exclude exclude;

          exclude.source=source;
          exclude.targetIndex=0;

          while (exclude.targetIndex<routeNode.paths.size() &&
              routeNode.objects[routeNode.paths[exclude.targetIndex].objectIndex].object!=dest) {
            exclude.targetIndex++;
          }

          if (exclude.targetIndex<routeNode.paths.size()) {
            routeNode.excludes.push_back(exclude);
          }
        }
      }
    }
  }

  bool RouteDataGenerator::HandlePendingOffsets(Progress& progress,
                                                const NodeIdOffsetMap& routeNodeIdOffsetMap,
                                                PendingRouteNodeOffsetsMap& pendingOffsetsMap,
                                                FileWriter& routeNodeWriter,
                                                const std::vector<NodeIdObjectsMap::const_iterator>& block,
                                                size_t blockCount)
  {
    std::map<FileOffset,RouteNodeRef> routeNodeOffsetMap;
    FileScanner                       routeScanner;
    FileOffset                        currentOffset;

    if (!routeNodeWriter.GetPos(currentOffset)) {
      progress.Error(std::string("Error while reading current file offset in file '")+
                     routeNodeWriter.GetFilename()+"'");
      return false;
    }

    if (!routeScanner.Open(routeNodeWriter.GetFilename(),
                           FileScanner::LowMemRandom,
                           false)) {
      progress.Error("Cannot open '"+routeScanner.GetFilename()+"'");
      return false;
    }

    for (size_t b=0; b<blockCount; b++) {
      PendingRouteNodeOffsetsMap::iterator pendingRouteNodeEntry=pendingOffsetsMap.find(block[b]->first);

      if (pendingRouteNodeEntry==pendingOffsetsMap.end()) {
        continue;
      }

      NodeIdOffsetMap::const_iterator pathNodeOffset=routeNodeIdOffsetMap.find(pendingRouteNodeEntry->first);

      assert(pathNodeOffset!=routeNodeIdOffsetMap.end());

      for (const auto& pendingOffset : pendingRouteNodeEntry->second) {
        std::map<FileOffset,RouteNodeRef>::const_iterator routeNodeIter=routeNodeOffsetMap.find(pendingOffset.routeNodeOffset);
        RouteNodeRef                                      routeNode;

        if (routeNodeIter!=routeNodeOffsetMap.end()) {
          routeNode=routeNodeIter->second;
        }
        else {
          routeNode=std::make_shared<RouteNode>();

          if (!routeScanner.SetPos(pendingOffset.routeNodeOffset)) {
            return false;
          }

          if (!routeNode->Read(routeScanner)) {
            return false;
          }

          routeNodeOffsetMap.insert(std::make_pair(pendingOffset.routeNodeOffset,routeNode));
        }

        assert(pendingOffset.index<routeNode->paths.size());

        routeNode->paths[pendingOffset.index].offset=pathNodeOffset->second;
      }

      pendingOffsetsMap.erase(pendingRouteNodeEntry);
    }

    if (!routeScanner.Close()) {
      return false;
    }

    for (const auto& routeNodeEntry : routeNodeOffsetMap) {
      if (!routeNodeWriter.SetPos(routeNodeEntry.first)) {
        progress.Error(std::string("Error while setting file offset in file '")+
                       routeNodeWriter.GetFilename()+"'");
        return false;
      }

      if (!routeNodeEntry.second->Write(routeNodeWriter)) {
        progress.Error(std::string("Error while writing route node to file '")+
                       routeNodeWriter.GetFilename()+"'");
        return false;
      }
    }

    if (!routeNodeWriter.SetPos(currentOffset)) {
      progress.Error(std::string("Error while setting current file offset in file '")+
                     routeNodeWriter.GetFilename()+"'");
      return false;
    }

    return !routeNodeWriter.HasError();
  }

  bool RouteDataGenerator::WriteObjectVariantData(Progress& progress,
                                                  const std::string& variantFilename,
                                                  const std::map<ObjectVariantData,uint16_t>& routeDataMap)
  {
    FileWriter writer;

    if (!writer.Open(variantFilename)) {
      progress.Error("Cannot create '"+variantFilename+"'");
      return false;
    }

    std::vector<ObjectVariantData> objectVariantData;

    objectVariantData.resize(routeDataMap.size());

    for (const auto& entry : routeDataMap) {
      objectVariantData[entry.second]=entry.first;
    }

    writer.Write((uint32_t)objectVariantData.size());

    for (const auto& entry : objectVariantData) {
      if (!entry.Write(writer)) {
        progress.Error(std::string("Error while writing object variant data to file '")+
                       writer.GetFilename()+"'");
        return false;
      }
    }

    progress.Info(NumberToString(objectVariantData.size()) + " object variant(s)");

    if (!writer.Close()) {
      return false;
    }

    return true;
  }

  bool RouteDataGenerator::WriteRouteGraph(const ImportParameter& parameter,
                                           Progress& progress,
                                           const TypeConfig& typeConfig,
                                           const NodeIdObjectsMap& nodeObjectsMap,
                                           const ViaTurnRestrictionMap& restrictions,
                                           VehicleMask vehicles,
                                           const std::string& dataFilename,
                                           const std::string& variantFilename)
  {
    FileScanner                wayScanner;
    FileScanner                areaScanner;
    FileWriter                 writer;

    uint32_t                   handledRouteNodeCount=0;
    uint32_t                   writtenRouteNodeCount=0;
    uint32_t                   objectCount=0;
    uint32_t                   pathCount=0;
    uint32_t                   excludeCount=0;
    uint32_t                   simpleNodesCount=0;

    NodeIdOffsetMap            routeNodeIdOffsetMap;
    PendingRouteNodeOffsetsMap pendingOffsetsMap;

    std::map<ObjectVariantData,uint16_t> routeDataMap;

    //
    // Writing route nodes
    //

    if (!writer.Open(dataFilename)) {
      progress.Error("Cannot create '"+dataFilename+"'");
      return false;
    }

    writer.Write(writtenRouteNodeCount);

    if (!wayScanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                         "ways.dat"),
                         FileScanner::Sequential,
                         parameter.GetWayDataMemoryMaped())) {
      progress.Error("Cannot open '"+wayScanner.GetFilename()+"'");
      return false;
    }

    if (!areaScanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                          "areas.dat"),
                          FileScanner::Sequential,
                          parameter.GetWayDataMemoryMaped())) {
      progress.Error("Cannot open '"+areaScanner.GetFilename()+"'");
      return false;
    }

    std::vector<NodeIdObjectsMap::const_iterator> block(parameter.GetRouteNodeBlockSize());

    NodeIdObjectsMap::const_iterator node=nodeObjectsMap.begin();
    while (node!=nodeObjectsMap.end()) {

      // Fill the current block of nodes to be processed

      size_t blockCount=0;

      progress.Info("Loading up to " + NumberToString(block.size()) + " route nodes");
      while (blockCount<block.size() &&
             node!=nodeObjectsMap.end()) {
        block[blockCount]=node;

        blockCount++;
        node++;
      }

      progress.Info("Loading intersecting ways and areas");

      // Collect way ids of all ways in current block and load them

      std::set<FileOffset> wayOffsets;
      std::set<FileOffset> areaOffsets;

      for (size_t b=0; b<blockCount; b++) {
        for (const auto& ref : block[b]->second) {
          switch (ref.GetType())
          {
          case refNone:
          case refNode:
            // Should never happen, since nodes are not routable
            assert(false);
            break;
          case refWay:
            wayOffsets.insert(ref.GetFileOffset());
            break;
          case refArea:
            areaOffsets.insert(ref.GetFileOffset());
            break;
          }
        }
      }

      std::unordered_map<FileOffset,WayRef>  waysMap;

      if (!LoadWays(typeConfig,
                    progress,
                    wayScanner,
                    wayOffsets,
                    waysMap)) {
        return false;
      }

      wayOffsets.clear();

      std::unordered_map<FileOffset,AreaRef> areasMap;

      if (!LoadAreas(typeConfig,
                     progress,
                     areaScanner,
                     areaOffsets,
                     areasMap)) {
        return false;
      }

      areaOffsets.clear();

      progress.Info("Storing route nodes");

      for (size_t b=0; b<blockCount; b++) {
        NodeIdObjectsMap::const_iterator node=block[b];
        FileOffset                       routeNodeOffset;

        if (!writer.GetPos(routeNodeOffset)) {
          return false;
        }

        handledRouteNodeCount++;
        progress.SetProgress(handledRouteNodeCount,
                             (uint32_t)nodeObjectsMap.size());

        //
        // Find out if any of the areas/ways at the intersection is routable
        // for us for the given vehicle (we already only loaded those objects
        // that are routable at all).
        // If none of the objects is routable the complete node is not routable and
        // we can safely drop this node from the routing graph.
        //

        if (!IsAnyRoutable(progress,
                           node->second,
                           waysMap,
                           areasMap,
                           vehicles)) {
          continue;
        }

        RouteNode routeNode;

        routeNode.id=node->first;

        if (!GetRouteNodeCoord(progress,
                               node->first,
                               node->second,
                               waysMap,
                               areasMap,
                               routeNode.coord)) {
          continue;
        }

        //
        // Calculate all outgoing paths
        //

        for (const auto& ref : node->second) {
          if (ref.GetType()==refWay) {
            const WayRef& way=waysMap[ref.GetFileOffset()];

            if (!way) {
              progress.Error("Error while loading way at offset "+
                             NumberToString(ref.GetFileOffset()) +
                             " (Internal error?)");
              continue;
            }

            if (!GetAccess(*way).CanRoute(vehicles)) {
              continue;
            }

            uint16_t objectVariantIndex=RegisterOrUseObjectVariantData(routeDataMap,
                                                                       way->GetType(),
                                                                       GetMaxSpeed(*way),
                                                                       GetGrade(*way));

            if (way->IsCircular()) {
              // Circular way routing (similar to current area routing, but respecting isOneway())
              CalculateCircularWayPaths(routeNode,
                                        *way,
                                        objectVariantIndex,
                                        routeNodeOffset,
                                        nodeObjectsMap,
                                        routeNodeIdOffsetMap,
                                        pendingOffsetsMap);
            }
            else {
              // Normal way routing
              CalculateWayPaths(routeNode,
                                *way,
                                objectVariantIndex,
                                routeNodeOffset,
                                nodeObjectsMap,
                                routeNodeIdOffsetMap,
                                pendingOffsetsMap);
            }
          }
          else if (ref.GetType()==refArea) {
            const AreaRef& area=areasMap[ref.GetFileOffset()];

            if (!area) {
              progress.Error("Error while loading area at offset "+
                             NumberToString(ref.GetFileOffset()) +
                             " (Internal error?)");
              continue;
            }

            if (!area->GetType()->CanRoute()) {
              continue;
            }

            uint16_t objectVariantIndex=RegisterOrUseObjectVariantData(routeDataMap,
                                                                       area->GetType(),
                                                                       0,
                                                                       1);

            routeNode.AddObject(ref,
                                objectVariantIndex);

            CalculateAreaPaths(routeNode,
                               *area,
                               objectVariantIndex,
                               routeNodeOffset,
                               nodeObjectsMap,
                               routeNodeIdOffsetMap,
                               pendingOffsetsMap);
          }
        }

        FillRoutePathExcludes(routeNode,
                              node->second,
                              restrictions);

        routeNodeIdOffsetMap.insert(std::make_pair(node->first,routeNodeOffset));

        if (routeNode.paths.size()==1) {
          simpleNodesCount++;
        }

        objectCount+=routeNode.objects.size();
        pathCount+=routeNode.paths.size();
        excludeCount+=routeNode.excludes.size();

        if (!routeNode.Write(writer)) {
          progress.Error(std::string("Error while writing route node to file '")+
                         writer.GetFilename()+"'");
          return false;
        }

        writtenRouteNodeCount++;
      }

      writer.Flush();

      //
      // A route node stores the fileOffset of all other route nodes (destinations) it can route to.
      // However if the destination route node is not yet stored, we do not have a file offset yet. Route nodes
      // we do not have a file offset for yet, are stored in the pendingOffsetsMap map.
      // So for every blocked store we are looking if any node in the block is in the pendingOffsetsMap, reload
      // the requesting route node, store the new offsets and write the route node back.

      if (!HandlePendingOffsets(progress,
                                routeNodeIdOffsetMap,
                                pendingOffsetsMap,
                                writer,
                                block,
                                blockCount)) {
        return false;
      }
    }

    assert(pendingOffsetsMap.empty());

    if (!wayScanner.Close()) {
      progress.Error("Cannot close file '"+wayScanner.GetFilename()+"'");
      return false;
    }

    if (!areaScanner.Close()) {
      progress.Error("Cannot close file '"+areaScanner.GetFilename()+"'");
      return false;
    }

    writer.SetPos(0);
    writer.Write(writtenRouteNodeCount);

    progress.Info(NumberToString(writtenRouteNodeCount) + " route node(s) written");
    progress.Info(NumberToString(simpleNodesCount)+ " route node(s) are simple and only have 1 path");
    progress.Info(NumberToString(objectCount)+ " object(s)");
    progress.Info(NumberToString(pathCount) + " path(s)");
    progress.Info(NumberToString(excludeCount) + " exclude(s)");

    if (!writer.Close()) {
      return false;
    }

    if (!WriteObjectVariantData(progress,
                                variantFilename,
                                routeDataMap)) {
      return false;
    }

    return true;
  }

  bool RouteDataGenerator::Import(const TypeConfigRef& typeConfig,
                                  const ImportParameter& parameter,
                                  Progress& progress)
  {
    // List of restrictions for a way
    ViaTurnRestrictionMap              restrictions;

    NodeUseMap                         nodeUseMap;
    NodeIdObjectsMap                   nodeObjectsMap;
    AccessRestrictedFeatureValueReader accessRestrictedReader(*typeConfig);
    AccessFeatureValueReader           accessReader(*typeConfig);
    MaxSpeedFeatureValueReader         maxSpeedReader(*typeConfig);
    GradeFeatureValueReader            gradeReader(*typeConfig);

    this->accessRestrictedReader=&accessRestrictedReader;
    this->accessReader=&accessReader;
    this->maxSpeedReader=&maxSpeedReader;
    this->gradeReader=&gradeReader;

    //
    // Handling of restriction relations
    //

    progress.SetAction("Scanning for restriction relations");

    if (!ReadTurnRestrictions(parameter,
                              progress,
                              restrictions)) {
      return false;
    }

    progress.Info(std::string("Restrictions for ") + NumberToString(restrictions.size()) + " objects loaded");

    //
    // Building a map of nodes and the number of ways that contain this way
    //

    progress.SetAction("Scanning for intersections");

    if (!ReadIntersections(parameter,
                           progress,
                           *typeConfig,
                           nodeUseMap)) {
      return false;
    }

    //
    // Building a map of endpoint ids and list of objects (ObjectFileRef for ways and areas) having this point as junction point
    //

    progress.SetAction("Collecting objects at intersections");

    if (!ReadObjectsAtIntersections(parameter,
                                    progress,
                                    *typeConfig,
                                    nodeUseMap,
                                    nodeObjectsMap)) {
      return false;
    }

    // We now have the nodeObjectsMap, we do not need this information anymore
    nodeUseMap.Clear();

    progress.Info(NumberToString(nodeObjectsMap.size())+ " route nodes collected");

    progress.SetAction("Postprocessing intersections");


    // We sort objects by increasing file offset, for more efficient storage
    // in route node
    for (auto& entry : nodeObjectsMap) {
      entry.second.sort(ObjectFileRefByFileOffsetComparator());
    }

    progress.SetAction(std::string("Writing intersection file '")+RoutingService::FILENAME_INTERSECTIONS_DAT+"'");

    if (!WriteIntersections(parameter,
                            progress,
                            nodeObjectsMap)) {
      return false;
    }

    for (const auto& router : parameter.GetRouter()) {
      std::string dataFilename=AppendFileToDir(parameter.GetDestinationDirectory(),
                                               router.GetDataFilename());
      std::string variantFilename=AppendFileToDir(parameter.GetDestinationDirectory(),
                                                  router.GetVariantFilename());
      std::string indexFilename=AppendFileToDir(parameter.GetDestinationDirectory(),
                                                router.GetIndexFilename());

      progress.SetAction(std::string("Writing route graph '")+dataFilename+"'");

      WriteRouteGraph(parameter,
                      progress,
                      *typeConfig,
                      nodeObjectsMap,
                      restrictions,
                      router.GetVehicleMask(),
                      dataFilename,
                      variantFilename);

      NumericIndexGenerator<Id,RouteNode> indexGenerator(std::string("Generating '")+indexFilename+"'",
                                                         dataFilename,
                                                         indexFilename);

      if (!indexGenerator.Import(typeConfig,
                                 parameter,
                                 progress)) {
        return false;
      }
    }

    // Cleaning up...

    nodeObjectsMap.clear();
    restrictions.clear();

    return true;
  }
}

