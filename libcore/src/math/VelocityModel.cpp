/**
 * \file        VelocityModel.cpp
 * \date        Aug. 07, 2015
 * \version     v0.7
 * \copyright   <2009-2015> Forschungszentrum Jülich GmbH. All rights reserved.
 *
 * \section License
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * \section Description
 * Implementation of first-order model
 * 3. Velocity Model: Tordeux2015
 *
 *
 **/
#include "VelocityModel.h"

#include "direction/walking/DirectionStrategy.h"
#include "general/OpenMP.h"
#include "geometry/SubRoom.h"
#include "geometry/Wall.h"
#include "neighborhood/NeighborhoodSearch.h"
#include "pedestrian/Pedestrian.h"

#include <Logger.h>

double xRight = 26.0;
double xLeft  = 0.0;
double cutoff = 2.0;
VelocityModel::VelocityModel( std::shared_ptr<DirectionManager> dir,
       double aped,
       double Dped,
       double awall,
       double Dwall,
       double esigma,
       double emu)
{
     _direction = dir;
     // Force_rep_PED Parameter
     _aPed = aped;
     _DPed = Dped;
     // Force_rep_WALL Parameter
     _aWall = awall;
     _DWall = Dwall;
    
    _esigma = esigma;
    _emu = emu;
}


VelocityModel::~VelocityModel() {}

bool VelocityModel::Init(Building * building)
{
    _direction->Init(building);

    const std::vector<Pedestrian *> & allPeds = building->GetAllPedestrians();
    size_t peds_size                          = allPeds.size();
    for(unsigned int p = 0; p < peds_size; p++) {
        Pedestrian * ped = allPeds[p];
        double cosPhi, sinPhi;
        //a destination could not be found for that pedestrian
        int ped_is_waiting = 1; // quick and dirty fix
        // we should maybe differentiate between pedestrians who did not find
        // routs because of a bug in the router and these who simplyt just want
        // to wait in waiting areas
        int res = ped->FindRoute();
        if(!ped_is_waiting && res == -1) {
            std::cout << ped->GetID() << " has no route\n";
            LOG_ERROR(
                "VelocityModel::Init() cannot initialise route. ped {:d} is deleted in Room "
                "%d %d.\n",
                ped->GetID(),
                ped->GetRoomID(),
                ped->GetSubRoomID());
            building->DeletePedestrian(ped);
            // TODO KKZ track deleted peds
            p--;
            peds_size--;
            continue;
        }

        // TODO
        // HERE every ped should have a navline already
        //


        Point target = Point(0, 0);
        if(ped->GetExitLine())
            target = ped->GetExitLine()->ShortestPoint(ped->GetPos());
        else {
            std::cout << "Ped " << ped->GetID() << " has no exit line in INIT\n";
        }
        Point d     = target - ped->GetPos();
        double dist = d.Norm();
        if(dist != 0.0) {
            cosPhi = d._x / dist;
            sinPhi = d._y / dist;
        } else {
            LOG_ERROR("allPeds::Init() cannot initialise phi! dist to target is 0");
            return false;
        }

        ped->InitV0(target);

        JEllipse E = ped->GetEllipse();
        E.SetCosPhi(cosPhi);
        E.SetSinPhi(sinPhi);
        ped->SetEllipse(E);
    }
    return true;
}

void VelocityModel::ComputeNextTimeStep(
    double current,
    double deltaT,
    Building * building,
    int periodic)
{
    // collect all pedestrians in the simulation.
    const std::vector<Pedestrian *> & allPeds = building->GetAllPedestrians();
    std::vector<Pedestrian *> pedsToRemove;
    pedsToRemove.reserve(500);
    unsigned long nSize;
    nSize = allPeds.size();

    int nThreads = omp_get_max_threads();

    int partSize;
    partSize = ((int) nSize > nThreads) ? (int) (nSize / nThreads) : (int) nSize;
    if(partSize == (int) nSize)
        nThreads = 1; // not worthy to parallelize


//TODO richtig parallelisieren!
#pragma omp parallel default(shared) num_threads(nThreads)
    {
        std::vector<Point> result_acc = std::vector<Point>();
        result_acc.reserve(nSize);
        std::vector<std::tuple<double,double,int>>spacings = std::vector<std::tuple<double,double,int>>();
        std::vector<std::tuple<double,double,int>> spacings_nonoise = std::vector<std::tuple<double,double,int>>();

        spacings.reserve(nSize);             // larger than needed
        spacings.push_back(std::make_tuple(100,1,-1)); // in case there are no neighbors
        spacings_nonoise.reserve(nSize);             // larger than needed
        spacings_nonoise.push_back(std::make_tuple(100,1,-1)); // in case there are no neighbors
        const int threadID = omp_get_thread_num();

        int start = threadID * partSize;
        int end;
        end = (threadID < nThreads - 1) ? (threadID + 1) * partSize - 1 : (int) (nSize - 1);
        for(int p = start; p <= end; ++p) {
            Pedestrian * ped  = allPeds[p];
            Room * room       = building->GetRoom(ped->GetRoomID());
            SubRoom * subroom = room->GetSubRoom(ped->GetSubRoomID());
            Point repPed      = Point(0, 0);
            std::vector<Pedestrian *> neighbours =
                building->GetNeighborhoodSearch().GetNeighbourhood(ped);

            int size = (int) neighbours.size();
            for(int i = 0; i < size; i++) {
                Pedestrian * ped1 = neighbours[i];
                if(ped1 == nullptr) {
                    std::cout << "Velocity Model debug: " << size << std::endl;
                }
                //if they are in the same subroom
                Point p1 = ped->GetPos();

                Point p2 = ped1->GetPos();
                //subrooms to consider when looking for neighbour for the 3d visibility
                std::vector<SubRoom *> emptyVector;
                emptyVector.push_back(subroom);
                emptyVector.push_back(
                    building->GetRoom(ped1->GetRoomID())->GetSubRoom(ped1->GetSubRoomID()));
                bool isVisible = building->IsVisible(p1, p2, emptyVector, false);
                if(!isVisible)
                    continue;
                if(ped->GetUniqueRoomID() == ped1->GetUniqueRoomID()) {
                    repPed += ForceRepPed(ped, ped1, periodic);
                } else {
                    // or in neighbour subrooms
                    SubRoom * sb2 =
                        building->GetRoom(ped1->GetRoomID())->GetSubRoom(ped1->GetSubRoomID());
                    if(subroom->IsDirectlyConnectedWith(sb2)) {
                        repPed += ForceRepPed(ped, ped1, periodic);
                    }
                }
            } // for i
            //repulsive forces to walls and closed transitions that are not my target
            //Point repWall = ForceRepRoom(allPeds[p], subroom);

            // calculate new direction ei according to (6)
            Point direction = e0(ped, room); //+ repPed + repWall;
            ped->SetDirNn(direction);

            std::random_device rd;
            std::mt19937 eng(rd());
            //std::mt19937 mt(ped->GetBuilding()->GetConfig()->GetSeed());
            //std::uniform_real_distribution<double> dist(0, 1.0);
            //std::cout << "emu = " << _emu << " esigme = " << _esigma << std::endl;
            std::normal_distribution<double> dist(_emu,_esigma); //this could be angle

            double random_x = dist(eng);
            //std::cout << random_x << std::endl;
            //Log->Write("%f", random_x);
            double random_y = dist(eng);
            //std::cout << random_y << std::endl;
            Point noise = Point(random_x,random_y);
            //std::cout <<"nonoise " << direction._x << " " << direction._y << std::endl;
            Point direction_nonoise = direction;

            direction = direction + noise;
            direction = direction.Normalized();
            ped->SetDir(direction);
            //std::cout << "noise "  <<  direction._x << " " << direction._y << std::endl;

            
            for(int i = 0; i < size; i++) {
                Pedestrian * ped1 = neighbours[i];
                // calculate spacing
                // my_pair spacing_winkel = GetSpacing(ped, ped1);
                if(ped->GetUniqueRoomID() == ped1->GetUniqueRoomID()) {
                    spacings.push_back(GetSpacing(ped, ped1, direction, periodic));
                    spacings_nonoise.push_back(GetSpacing(ped,ped1,direction_nonoise,periodic));
                } else {
                    // or in neighbour subrooms
                    SubRoom * sb2 =
                        building->GetRoom(ped1->GetRoomID())->GetSubRoom(ped1->GetSubRoomID());
                    if(subroom->IsDirectlyConnectedWith(sb2)) {
                        spacings.push_back(GetSpacing(ped, ped1, direction, periodic));
                    }
                }
            }
            //TODO get spacing to walls
            //TODO update direction every DT?

            // calculate min spacing
            std::sort(spacings.begin(), spacings.end());
            std::sort(spacings_nonoise.begin(), spacings_nonoise.end());

            double spacing = std::get<0>(spacings[0]);
            double spacing_nonoise = std::get<0>(spacings_nonoise[0]);
            //============================================================
            // TODO: Hack for Head on situations: ped1 x ------> | <------- x ped2
            if(0 && direction.NormSquare() < 0.5) {
                double pi_half = 1.57079663;
                double alpha   = pi_half * exp(-spacing);
                direction      = e0(ped, room).Rotate(cos(alpha), sin(alpha));
                printf(
                    "\nRotate %f, %f, norm = %f alpha = %f, spacing = %f\n",
                    direction._x,
                    direction._y,
                    direction.NormSquare(),
                    alpha,
                    spacing);
                getc(stdin);
            }
            //============================================================
            Point speed = direction.Normalized() * OptimalSpeed(ped, spacing);
            ped->SetSpeedNn(OptimalSpeed(ped, spacing_nonoise));
            //ped->SetAngleNn(std::get<1>(spacings_nonoise[0]));
            ped->SetAngleNn(std::get<1>(spacings_nonoise[0]));
            ped->SetIntID(std::get<2>(spacings_nonoise[0]));
            ped->SetIntIDN(std::get<2>(spacings[0]));
            
            result_acc.push_back(speed);


            spacings.clear(); //clear for ped p
            spacings_nonoise.clear();
            // stuck peds get removed. Warning is thrown. low speed due to jam is omitted.
            if(ped->GetTimeInJam() > ped->GetPatienceTime() &&
               ped->GetGlobalTime() > 10000 + ped->GetPremovementTime() &&
               std::max(ped->GetMeanVelOverRecTime(), ped->GetV().Norm()) < 0.01 &&
               size == 0) // size length of peds neighbour vector
            {
                LOG_WARNING(
                    "ped {:d} with vmean {:f} has been deleted in room {:d}/{:d} after time "
                    "{:f}s (current={:f}",
                    ped->GetID(),
                    ped->GetMeanVelOverRecTime(),
                    ped->GetRoomID(),
                    ped->GetSubRoomID(),
                    ped->GetGlobalTime(),
                    current);
                //TODO KKZ track deleted peds
#pragma omp critical(VelocityModel_ComputeNextTimeStep_pedsToRemove)
                pedsToRemove.push_back(ped);
            }

        } // for p

#pragma omp barrier
        // update
        for(int p = start; p <= end; ++p) {
            Pedestrian * ped = allPeds[p];

            Point v_neu   = result_acc[p - start];
            Point pos_neu = ped->GetPos() + v_neu * deltaT;

            //Jam is based on the current velocity
            if(v_neu.Norm() >= ped->GetV0Norm() * 0.5) {
                ped->ResetTimeInJam();
            } else {
                ped->UpdateTimeInJam();
            }
            //only update the position if the velocity is above a threshold
            if(v_neu.Norm() >= J_EPS_V) {
                ped->SetPhiPed();
            }
            ped->SetPos(pos_neu);
            if(periodic) {
                if(ped->GetPos()._x >= xRight) {
                    ped->SetPos(Point(ped->GetPos()._x - (xRight - xLeft), ped->GetPos()._y));
                    //ped->SetID( ped->GetID() + 1);
                }
            }
            ped->SetV(v_neu);
        }
    } //end parallel

    // remove the pedestrians that have left the building
    for(unsigned int p = 0; p < pedsToRemove.size(); p++) {
        building->DeletePedestrian(pedsToRemove[p]);
    }
    pedsToRemove.clear();
}

Point VelocityModel::e0(Pedestrian * ped, Room * room) const
{
    Point target;

    if(_direction && ped->GetExitLine()) {
        // target is where the ped wants to be after the next timestep
        target = _direction->GetTarget(room, ped);
    } else { //@todo: we need a model for waiting pedestrians
        std::cout << ped->GetID() << " VelocityModel::e0 Ped has no navline.\n";
        // set random destination
        std::mt19937 mt(ped->GetBuilding()->GetConfig()->GetSeed());
        std::uniform_real_distribution<double> dist(0, 1.0);
        double random_x = dist(mt);
        double random_y = dist(mt);
        Point P1        = Point(ped->GetPos()._x - random_x, ped->GetPos()._y - random_y);
        Point P2        = Point(ped->GetPos()._x + random_x, ped->GetPos()._y + random_y);
        const NavLine L = Line(P1, P2);
        ped->SetExitLine((const NavLine *) &L);
        target = P1;
    }
    Point desired_direction;
    const Point pos = ped->GetPos();
    double dist     = 0.0;
    if(ped->GetExitLine())
        dist = ped->GetExitLine()->DistTo(pos);
    // check if the molified version works
    Point lastE0 = ped->GetLastE0();
    ped->SetLastE0(target - pos);

    if((dynamic_cast<DirectionLocalFloorfield *>(_direction->GetDirectionStrategy().get())) ||
       (dynamic_cast<DirectionSubLocalFloorfield *>(_direction->GetDirectionStrategy().get()))) {
        desired_direction = target - pos;
        if(desired_direction.NormSquare() < 0.25 && !ped->IsWaiting()) {
            desired_direction = lastE0;
            ped->SetLastE0(lastE0);
        }
    } else if(dist > J_EPS_GOAL) {
        desired_direction = ped->GetV0(target);
    } else {
        ped->SetSmoothTurning();
        desired_direction = ped->GetV0();
    }
    return desired_direction;
}

double VelocityModel::OptimalSpeed(Pedestrian * ped, double spacing) const
{
    double v0    = ped->GetV0Norm();
    double T     = ped->GetT();
    double l     = 2 * ped->GetEllipse().GetBmax(); //assume peds are circles with const radius
    double speed = (spacing - l) / T;
    speed        = (speed > 0) ? speed : 0;
    speed        = (speed < v0) ? speed : v0;
    //      (1-winkel)*speed;
    //todo use winkel
    return speed;
}

// return spacing and id of the nearest pedestrian
std::tuple<double,double,int>
VelocityModel::GetSpacing(Pedestrian * ped1, Pedestrian * ped2, Point ei, int periodic) const
{
    Point distp12 = ped2->GetPos() - ped1->GetPos(); // inversed sign
    Point dir_nn = ped1->GetDirNn();
    Point dir_nn_j = ped2->GetDirNn();
    Point dir = ped1->GetDir();
    Point dir_j = ped2->GetDir();
    double dir_angle_nn;
    if (dir_nn.NormSquare() > 0. && dir_nn_j.NormSquare() > 0.){
        dir_angle_nn = dir_nn.Normalized().ScalarProduct(dir_nn_j.Normalized());
        /*if (dir_angle_nn == 1){
            LOG_WARNING(
                    "dir x ({:.2f}) dir y ({:.2f}) dirj x ({:.2f}) dirj y ({:.2f}) with pos1 ({:.2f},{:.2f}) and pos2 ({:.2f},{:.2f}) ",
                    dir_nn._x,
                    dir_nn._y,
                    dir_nn_j._x,
                    dir_nn_j._y,
                    ped1->GetPos()._x,
                    ped1->GetPos()._y,
                    ped2->GetPos()._x,
                    ped2->GetPos()._y);
            }*/
        }
    
    else{
        dir_angle_nn = -3.;
    }
    //double dir_angle = dir.Normalized().ScalarProduct(dir_j.Normalized());
    if(periodic) {
        double x   = ped1->GetPos()._x;
        double x_j = ped2->GetPos()._x;
        
        if((xRight - x) + (x_j - xLeft) <= cutoff) {
            distp12._x = distp12._x + xRight - xLeft;
        }
    }
    double Distance = distp12.Norm();
    double l        = 2 * ped1->GetEllipse().GetBmax();
    Point ep12;
    if(Distance >= J_EPS) {
        ep12 = distp12.Normalized();
    } else {
        LOG_WARNING(
            "VelocityModel::GetSPacing() ep12 can not be calculated! Pedestrians are to close to "
            "each other ({:f})",
            Distance);
        my_pair(FLT_MAX, ped2->GetID());
        exit(EXIT_FAILURE); //TODO
    }

    double condition1 = ei.ScalarProduct(ep12); // < e_i , e_ij > should be positive
    double condition2 =
        ei.Rotate(0, 1).ScalarProduct(ep12); // theta = pi/2. condition2 should <= than l/Distance
    condition2 = (condition2 > 0) ? condition2 : -condition2; // abs

    if((condition1 >= 0) && (condition2 <= l / Distance)){
        // return a pair <dist, condition1>. Then take the smallest dist. In case of equality the biggest condition1
        if (abs(dir_angle_nn) > 1.)
            return std::make_tuple(distp12.Norm(), -5.,ped2->GetID());
        else
            return std::make_tuple(distp12.Norm(), dir_angle_nn, ped2->GetID());
    }
    else
        return std::make_tuple(FLT_MAX, -2., -3);
        
}
Point VelocityModel::ForceRepPed(Pedestrian * ped1, Pedestrian * ped2, int periodic) const
{
    Point F_rep(0.0, 0.0);
    // x- and y-coordinate of the distance between p1 and p2
    Point distp12 = ped2->GetPos() - ped1->GetPos();

    if(periodic) {
        double x   = ped1->GetPos()._x;
        double x_j = ped2->GetPos()._x;
        if((xRight - x) + (x_j - xLeft) <= cutoff) {
            distp12._x = distp12._x + xRight - xLeft;
        }
    }

    double Distance = distp12.Norm();
    Point ep12; // x- and y-coordinate of the normalized vector between p1 and p2
    double R_ij;
    double l = 2 * ped1->GetEllipse().GetBmax();

    if(Distance >= J_EPS) {
        ep12 = distp12.Normalized();
    } else {
        LOG_ERROR(
            "VelocityModel::forcePedPed() ep12 can not be calculated! Pedestrians are too near to "
            "each other (dist={:f}). Adjust <a> value in force_ped to counter this. Affected "
            "pedestrians ped1 {:d} at ({:f},{:f}) and ped2 {:d} at ({:f}, {:f})",
            Distance,
            ped1->GetID(),
            ped1->GetPos()._x,
            ped1->GetPos()._y,
            ped2->GetID(),
            ped2->GetPos()._x,
            ped2->GetPos()._y);
        exit(EXIT_FAILURE); //TODO: quick and dirty fix for issue #158
                            // (sometimes sources create peds on the same location)
    }
    Point ei = ped1->GetV().Normalized();
    if(ped1->GetV().NormSquare() < 0.01) {
        ei = ped1->GetV0().Normalized();
    }
    double condition1 = ei.ScalarProduct(ep12);            // < e_i , e_ij > should be positive
    condition1        = (condition1 > 0) ? condition1 : 0; // abs

    R_ij  = -_aPed * exp((l - Distance) / _DPed);
    F_rep = ep12 * R_ij;

    return F_rep;
} //END Velocity:ForceRepPed()

Point VelocityModel::ForceRepRoom(Pedestrian * ped, SubRoom * subroom) const
{
    Point f(0., 0.);
    const Point & centroid = subroom->GetCentroid();
    bool inside            = subroom->IsInSubRoom(centroid);
    //first the walls
    for(const auto & wall : subroom->GetAllWalls()) {
        f += ForceRepWall(ped, wall, centroid, inside);
    }

    //then the obstacles
    for(const auto & obst : subroom->GetAllObstacles()) {
        if(obst->Contains(ped->GetPos())) {
            LOG_ERROR(
                "Agent {:d} is trapped in obstacle in room/subroom {:d}/{:d}",
                ped->GetID(),
                subroom->GetRoomID(),
                subroom->GetSubRoomID());
            exit(EXIT_FAILURE);
        } else
            for(const auto & wall : obst->GetAllWalls()) {
                f += ForceRepWall(ped, wall, centroid, inside);
            }
    }

    // and finally the closed doors
    for(const auto & trans : subroom->GetAllTransitions()) {
        if(!trans->IsOpen()) {
            f += ForceRepWall(ped, *(static_cast<Line *>(trans)), centroid, inside);
        }
    }

    return f;
}

Point VelocityModel::ForceRepWall(
    Pedestrian * ped,
    const Line & w,
    const Point & centroid,
    bool inside) const
{
    Point F_wrep = Point(0.0, 0.0);
    Point pt     = w.ShortestPoint(ped->GetPos());

    Point dist       = pt - ped->GetPos(); // x- and y-coordinate of the distance between ped and p
    const double EPS = 0.000;              // molified see Koester2013
    double Distance  = dist.Norm() + EPS;  // distance between the centre of ped and point p
    Point e_iw; // x- and y-coordinate of the normalized vector between ped and pt
    double l = ped->GetEllipse().GetBmax();
    double R_iw;
    double min_distance_to_wall = 0.001; // 10 cm

    if(Distance > min_distance_to_wall) {
        e_iw = dist / Distance;
    } else {
        LOG_WARNING(
            "Velocity: forceRepWall() ped {:d} [{:f}, {:f}] is too near to the wall [{:f}, "
            "{:f}]-[{:f}, {:f}] (dist={:f})",
            ped->GetID(),
            ped->GetPos()._y,
            ped->GetPos()._y,
            w.GetPoint1()._x,
            w.GetPoint1()._y,
            w.GetPoint2()._x,
            w.GetPoint2()._y,
            Distance);
        Point new_dist = centroid - ped->GetPos();
        new_dist       = new_dist / new_dist.Norm();
        e_iw           = (inside ? new_dist : new_dist * -1);
    }
    //-------------------------

    const Point & pos = ped->GetPos();
    double distGoal   = 0.0;
    if(ped->GetExitLine())
        distGoal = ped->GetExitLine()->DistToSquare(pos);

    if(distGoal < J_EPS_GOAL * J_EPS_GOAL)
        return F_wrep;
    //-------------------------
    R_iw   = -_aWall * exp((l - Distance) / _DWall);
    F_wrep = e_iw * R_iw;

    return F_wrep;
}

std::string VelocityModel::GetDescription()
{
    std::string rueck;
    char tmp[1024];

    sprintf(tmp, "\t\ta: \t\tPed: %f \tWall: %f\n", _aPed, _aWall);
    rueck.append(tmp);
    sprintf(tmp, "\t\tD: \t\tPed: %f \tWall: %f\n", _DPed, _DWall);
    rueck.append(tmp);
    return rueck;
}

double VelocityModel::GetaPed() const
{
    return _aPed;
}

double VelocityModel::GetDPed() const
{
    return _DPed;
}


double VelocityModel::GetaWall() const
{
    return _aWall;
}

double VelocityModel::GetDWall() const
{
    return _DWall;
}

