//
// Created by Gregor Lämmel on 30/03/16.
//

#ifndef JPSCORE_GEOMETRYREADER_H
#define JPSCORE_GEOMETRYREADER_H

class Building;

class GeometryReader {
public:
     GeometryReader(){};
     virtual ~GeometryReader(){};
     virtual void LoadBuilding(Building* building) = 0;

     virtual bool LoadTrafficInfo(Building* building) = 0;
};

#endif //JPSCORE_GEOMETRYREADER_H
