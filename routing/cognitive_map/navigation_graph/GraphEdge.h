/**
 * @file   GraphEdge.h
 * @author David Haensel (d.haensel@fz-juelich.de)
 * @date   January, 2014
 * @brief  Edge of a Graph.
 *
 */

#ifndef GRAPHEDGE_H_
#define GRAPHEDGE_H_


class SubRoom;
class GraphVertex;
class Crossing;

/**
 * @brief Graph Edge.
 *
 */

class GraphEdge {

public:
    /****************************
     * Constructors & Destructors
     ****************************/

    GraphEdge(const GraphVertex * const s, const GraphVertex * const d, const Crossing * const crossing);
    GraphEdge(GraphEdge const & ge);
    virtual ~GraphEdge();

    void CalcApproximateDistance();

    // Getter collection
    const GraphVertex * GetDest() const;
    const GraphVertex * GetSrc() const;
    const Crossing * GetCrossing() const;

    double GetApproximateDistance() const;


private:
    const GraphVertex  * const src;
    const GraphVertex  * const dest;
    const Crossing  * const crossing;

    //WEIGHTS
    double approximate_distance;

};


#endif /* GRAPHEDGE_H_ */
