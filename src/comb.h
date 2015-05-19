/** Copyright (C) 2013 David Braam - Released under terms of the AGPLv3 License */
#ifndef COMB_H
#define COMB_H

#include "utils/polygon.h"

namespace cura 
{
struct CombPath : public  std::vector<Point> //!< A single path either inside or outise the parts
{
    unsigned int part_idx;
};
struct CombPaths : public  std::vector<CombPath> {}; //!< A list of paths alternating between inside a part and outside a part

/*!
 * Class for generating a combing move action from point a to point b and avoiding collision with other parts when moving through air.
 * 
 * The general implementation is by rotating everything such that the the line segment from a to b is aligned with the x-axis.
 * We call the line on which a and b lie the 'scanline'.
 */
class LinePolygonsCrossings
{
private:

    Polygons boundary;
    Point startPoint;
    Point endPoint;
    
    struct Crossing
    {
        int64_t x; //!< x coordinate of crossings between the polygon and the scanline.
        unsigned int point_idx; 
        Crossing(int64_t x, unsigned int point_idx)
        : x(x), point_idx(point_idx)
        {
        }
    };
    
    struct PolyCrossings
    {
        unsigned int poly_idx; 
        Crossing min;
        Crossing max;
        PolyCrossings(unsigned int poly_idx) 
        : poly_idx(poly_idx)
        , min(INT64_MAX, NO_INDEX), max(INT64_MIN, NO_INDEX) 
        { 
        }
    };
    
    struct PartCrossings : public std::vector<PolyCrossings>
    {
        //unsigned int part_idx;
    };
    PointMatrix transformation_matrix; //!< The transformation which rotates everything such that the scanline is aligned with the x-axis.
    Point transformed_startPoint; //!< The startPoint (see Comb::calc) as transformed by Comb::transformation_matrix
    Point transformed_endPoint; //!< The endPoint (see Comb::calc) as transformed by Comb::transformation_matrix


    
    PolyCrossings getNextPolygonAlongScanline(int64_t x);
    
    void getBasicCombingPath(PolyCrossings crossings, std::vector<Point>& pointList);
    
    
public:
    
    bool lineSegmentCollidesWithBoundary();
    /*!
     * Calculate Comb::minX, Comb::maxX, Comb::minIdx, Comb::maxIdx, Comb::minIdx_global and Comb::maxIdx_global.
     */
    void calcScanlineCrossings();
    
    
    void getCombingPath(Polygons& offsettedBoundary, CombPath& combPath);
    
// private:
    void getBasicCombingPathPart(Point endPoint, CombPath& combPath);
    static bool optimizePath(Polygons& offsettedBoundary, Point startPoint, std::vector<Point>& pointList, CombPath& combPoints);
    
public:
    
    PartCrossings crossings;
    unsigned int min_crossing_idx;
    unsigned int max_crossing_idx;
    
    LinePolygonsCrossings(Polygons& boundary, Point& start, Point& end)
    : boundary(boundary), startPoint(start), endPoint(end)
    {
    }
};

class Comb 
{
    Polygons boundary;
    Polygons boundary_inside;
    const int64_t max_moveInside_distance2 = MM2INT(5.0)*MM2INT(5.0);
   /*
    Polygons boundary_outside;
        
    void calcScanlineCrossings(PolygonsPart& part, unsigned int part_idx, PartCrossings& partCrossings);

    void getBasicCombingPathPart(PartCrossings crossings_inside, PolygonsPart part, Point endPoint, CombPath& combPath);

    void getBasicCombingPath(PolygonsPart part, PolyCrossings crossings, std::vector<Point>& pointList);*/


    /*!
     * Check if we are crossing any boundaries, and pre-calculate some values.
     * 
     * Sets Comb::transformation_matrix, Comb::transformed_startPoint and Comb::transformed_endPoint
     */
//     bool lineSegmentCollidesWithAnyInsideBoundary(Point startPoint, Point endPoint);
//     
//     bool lineSegmentCollidesWithInsideBoundary(Point startPoint, Point endPoint, PolygonsPart part);
// 
//     bool lineSegmentCollidesWithAnyOutsideBoundary(Point startPoint, Point endPoint);

    /*!
     * Calculate Comb::minX, Comb::maxX, Comb::minIdx, Comb::maxIdx, Comb::minIdx_global and Comb::maxIdx_global.
     */
//     void calcScanlineCrossings();
//     
//     
//     void calcScanlineCrossings(std::vector<PolygonsPart>& boundary);
    
    /*!
     * Find the first polygon cutting the scanline after \p x.
     * 
     * Note that this function only looks at the first segment cutting the scanline (see Comb::minX)!
     * It doesn't return the next polygon which crosses the scanline, but the first polygon crossing the scanline for the first time.
     * 
     * \param x The point on the scanline from where to look.
     * \return The index into the Comb::boundary of the first next polygon. Or NO_INDEX if there's none left.
     */
//     Crossing getNextPolygonAlongScanline(std::vector<PolyCrossings>& crossings, int64_t x);
    
    /*!
     * Get a point at an inset of 0.2mm of a given point in a polygon of the boudary.
     * 
     * \param polygon_idx The index of the polygon in the boundary.
     * \param point_idx The index of the point in the polygon.
     * \return A point at the given distance inward from the point on the boundary polygon.
     */
//     Point getBoundaryPointWithOffset(unsigned int polygon_idx, unsigned int point_idx, int64_t offset);
    
public:
    Comb(Polygons& _boundary);
    ~Comb();
    
    
    //! Utility function for `boundary.inside(p)`.
    bool inside(const Point p) { return boundary.inside(p); }
    
    /*!
     * Moves the point \p from inside the comb boundary or leaves the point as-is, when the comb boundary is not within 3 mm distance.
     * 
     * \param from The point to move.
     * \param distance The distance by which to offset the point from the boundary.
     * \return Whether we succeeded in moving inside the comb boundary
     */
    unsigned int moveInside_(Point& from, int distance = 100);

    /*!
     * Calculate the comb paths (if any) - one for each polygon combed alternated with travel paths
     * 
     * \param startPoint Where to start moving from
     * \param endPoint Where to move to
     * \param combPoints Output parameter: The points along the combing path, excluding the \p startPoint (?) and \p endPoint
     * \return Whether combing has succeeded; otherwise a retraction is needed.
     */    
    bool calc(Point startPoint, Point endPoint, CombPaths& combPaths);
    
private:
    
    /*!
     * Optimize the \p pointList: skip each point we could already reach by not crossing a boundary. This smooths out the path and makes it skip any unneeded corners.
     * 
     * \param offsettedBoundary The polygons which not to cross
     * \param startPoint The starting point of the comb move.
     * \param endPoint The destination point of the comb move.
     * \param pointList The unoptimized combing path.
     * \param combPoints Output parameter: The points of optimized combing path
     * \return 
     */
//     static bool optimizePath(Polygons& offsettedBoundary, Point startPoint, std::vector<Point>& pointList, CombPath& combPoints);
    
    /*! 
     * Get the basic combing path, without shortcuts. The path goes straight toward the \p endPoint and follows the boundary when it hits it, until it passes the scanline again.
     * 
     * Walk trough the crossings, for every boundary we cross, find the initial cross point and the exit point. Then add all the points in between
     * to the pointList and continue with the next boundary we will cross, until there are no more boundaries to cross.
     * This gives a path from the start to finish curved around the holes that it encounters.
     * 
     * \param endPoint The endPoint toward which to comb.
     * \param pointList Output parameter: the points along the combing path.
     */
//     void getBasicCombingPath(Point endPoint, std::vector<Point>& pointList);

    /*! 
     * Get the basic combing paths, without shortcuts - one for each polygon combed. The path goes straight toward the \p endPoint and follows the boundary when it hits it, until it passes the scanline again.
     * 
     * Walk trough the crossings, for every boundary we cross, find the initial cross point and the exit point. Then add all the points in between
     * to the pointList and continue with the next boundary we will cross, until there are no more boundaries to cross.
     * This gives a path from the start to finish curved around the holes that it encounters.
     * 
     * \param endPoint The endPoint toward which to comb.
     * \param combPaths Output parameter: the paths of points along which to comb for each boundary polygon.
     */
//     void getBasicCombingPaths(Point endPoint, std::vector<std::vector<Point>>& combPaths);
    
    /*! 
     * Get the basic combing path along a single boundary polygon, without shortcuts. The path goes straight toward the endPoint and follows the boundary when it hits it, until it passes the scanline again.
     * 
     * Walk trough the crossings, for every boundary we cross, find the initial cross point and the exit point. Then add all the points in between
     * to the pointList and continue with the next boundary we will cross, until there are no more boundaries to cross.
     * This gives a path from the start to finish curved around the holes that it encounters.
     * 
     * \param poly_idx The index of the boundary polygon along which to comb
     * \param pointList Output parameter: the points along the combing path.
     */    
//     void getBasicCombingPath(unsigned int poly_idx, std::vector<Point>& pointList);


    /*!
     * Optimize the \p basicCombPaths: skip each point we could already reach by not crossing a boundary. This smooths out the path and makes it skip any unneeded corners.
     * 
     * \param startPoint The starting point of the comb move.
     * \param endPoint The destination point of the comb move.
     * \param pointList The unoptimized combing path.
     * \param combPoints Output parameter: The points of optimized combing path
     * \return 
     */
//     bool optimizePaths(Point startPoint, std::vector<std::vector<Point>>& basicCombPaths, std::vector<std::vector<Point>>& combPaths);

};

}//namespace cura

#endif//COMB_H
