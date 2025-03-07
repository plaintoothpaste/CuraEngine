//Copyright (c) 2022 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#ifndef UTILS_POLYLINE_STITCHER_H
#define UTILS_POLYLINE_STITCHER_H

#include "polygon.h"
#include "SparsePointGrid.h"
#include "PolygonsPointIndex.h"

#include "SymmetricPair.h"
#include <unordered_set>
#include <cassert>

namespace cura
{

/*!
 * Class for stitching polylines into longer polylines or into polygons
 */
template<typename Paths, typename Path, typename Junction>
class PolylineStitcher
{
public:
    /*!
     * Stitch together the separate \p lines into \p result_lines and if they can be closed into \p result_polygons.
     * Only introduce new segments shorter than \p max_stitch_distance, and larger then \p snap_distance
     * but always try to take the shortest connection possible.
     * 
     * Only stitch polylines into closed polygons if they are larger than 3 * \p max_stitch_distance,
     * in order to prevent small segments to accidentally get closed into a polygon.
     * 
     * \warning Tiny polylines (smaller than 3 * max_stitch_distance) will not be closed into polygons. 
     * 
     * \note Resulting polylines and polygons are added onto the existing containers, so you can directly output onto a polygons container with existing polygons in it.
     * However, you shouldn't call this function with the same parameter in \p lines as \p result_lines, because that would duplicate (some of) the polylines.
     */
    static void stitch(const Paths& lines, Paths& result_lines, Paths& result_polygons, coord_t max_stitch_distance = MM2INT(0.1), coord_t snap_distance = 10)
    {
        if (lines.empty())
        {
            return;
        }

        SparsePointGrid<PathsPointIndex<Paths>, PathsPointIndexLocator<Paths>> grid(max_stitch_distance, lines.size() * 2);
        
        // populate grid
        for (size_t line_idx = 0; line_idx < lines.size(); line_idx++)
        {
            const auto line = lines[line_idx];
            grid.insert(PathsPointIndex<Paths>(&lines, line_idx, 0));
            grid.insert(PathsPointIndex<Paths>(&lines, line_idx, line.size() - 1));
        }
        
        std::vector<bool> processed(lines.size(), false);
        
        for (size_t line_idx = 0; line_idx < lines.size(); line_idx++)
        {
            if (processed[line_idx])
            {
                continue;
            }
            processed[line_idx] = true;
            const auto line = lines[line_idx];
            
            Path chain = line;
            bool closest_is_closing_polygon = false;
            for (bool go_in_reverse_direction : { false, true }) // first go in the unreversed direction, to try to prevent the chain.reverse() operation.
            { // NOTE: Implementation only works for this order; we currently only re-reverse the chain when it's closed.
                if (go_in_reverse_direction)
                { // try extending chain in the other direction
                    chain.reverse();
                }
                
                while (true)
                {
                    Point from = make_point(chain.back());
                    
                    PathsPointIndex<Paths> closest;
                    coord_t closest_distance = std::numeric_limits<coord_t>::max();
                    grid.processNearby(from, max_stitch_distance, 
                        std::function<bool (const PathsPointIndex<Paths>&)> (
                            [from, &chain, &closest, &closest_is_closing_polygon, &closest_distance, &processed, go_in_reverse_direction, max_stitch_distance, snap_distance]
                            (const PathsPointIndex<Paths>& nearby)->bool
                        {
                            bool is_closing_segment = false;
                            coord_t dist = vSize(nearby.p() - from);
                            if (dist > max_stitch_distance)
                            {
                                return true; // keep looking
                            }
                            if (nearby.p() == make_point(chain.front()))
                            {
                                if (chain.polylineLength() + dist < 3 * max_stitch_distance // prevent closing of small poly, cause it might be able to continue making a larger polyline
                                    || chain.size() <= 2) // don't make 2 vert polygons
                                {
                                    return true; // look for a better next line
                                }
                                is_closing_segment = true;
                                dist += 10; // prefer continuing polyline over closing a polygon; avoids closed zigzags from being printed separately
                                // continue to see if closing segment is also the closest 
                                // there might be a segment smaller than [max_stitch_distance] which closes the polygon better
                            }
                            else if (processed[nearby.poly_idx])
                            { // it was already moved to output
                                return true; // keep looking for a connection
                            }
                            bool nearby_would_be_reversed = nearby.point_idx != 0;
                            nearby_would_be_reversed = nearby_would_be_reversed != go_in_reverse_direction; // flip nearby_would_be_reversed when searching in the reverse direction
                            if ( ! canReverse(nearby) && nearby_would_be_reversed)
                            { // connecting the segment would reverse the polygon direction
                                return true; // keep looking for a connection
                            }
                            if ( ! canConnect(chain, (*nearby.polygons)[nearby.poly_idx]))
                            {
                                return true; // keep looking for a connection
                            }
                            if (dist < closest_distance)
                            {
                                closest_distance = dist;
                                closest = nearby;
                                closest_is_closing_polygon = is_closing_segment;
                            }
                            if (dist < snap_distance)
                            { // we have found a good enough next line
                                return false; // stop looking for alternatives
                            }
                            return true; // keep processing elements 
                        })
                    );
                    
                    if (!closest.initialized()          // we couldn't find any next line 
                        || closest_is_closing_polygon   // we closed the polygon
                    )
                    {
                        break;
                    }
                    

                    coord_t segment_dist = vSize(make_point(chain.back()) - closest.p());
                    assert(segment_dist <= max_stitch_distance + 10);
                    if (closest.point_idx == 0)
                    {
                        auto start_pos = (*closest.polygons)[closest.poly_idx].begin();
                        if (segment_dist < snap_distance)
                        {
                            ++start_pos;
                        }
                        chain.insert(chain.end(), start_pos, (*closest.polygons)[closest.poly_idx].end());
                    }
                    else
                    {
                        auto start_pos = (*closest.polygons)[closest.poly_idx].rbegin();
                        if (segment_dist < snap_distance)
                        {
                            ++start_pos;
                        }
                        chain.insert(chain.end(), (*closest.polygons)[closest.poly_idx].rbegin(), (*closest.polygons)[closest.poly_idx].rend());
                    }
                    assert( ! processed[closest.poly_idx]);
                    processed[closest.poly_idx] = true;
                }

                if (closest_is_closing_polygon)
                {
                    if (go_in_reverse_direction)
                    { // re-reverse chain to retain original direction
                        // NOTE: not sure if this code could ever be reached, since if a polygon can be closed that should be already possible in the forward direction
                        chain.reverse();
                    }

                    break; // don't consider reverse direction
                }
            }
            if (closest_is_closing_polygon)
            {
                result_polygons.emplace_back(chain);
            }
            else
            {
                PathsPointIndex<Paths> ppi_here(&lines, line_idx, 0);
                if ( ! canReverse(ppi_here))
                { // Since closest_is_closing_polygon is false we went through the second iterations of the for-loop, where go_in_reverse_direction is true
                    // the polyline isn't allowed to be reversed, so we re-reverse it.
                    chain.reverse();
                }
                result_lines.emplace_back(chain);
            }
        }
    }
    
    /*!
     * Whether a polyline is allowed to be reversed. (Not true for wall polylines which are not odd)
     */
    static bool canReverse(const PathsPointIndex<Paths>& polyline);

    /*!
     * Whether two paths are allowed to be connected.
     * (Not true for an odd and an even wall.)
     */
    static bool canConnect(const Path& a, const Path& b);
};

}//namespace cura
#endif//UTILS_POLYLINE_STITCHER_H

