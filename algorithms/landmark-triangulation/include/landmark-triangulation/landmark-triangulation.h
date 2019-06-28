#ifndef LANDMARK_TRIANGULATION_LANDMARK_TRIANGULATION_H_
#define LANDMARK_TRIANGULATION_LANDMARK_TRIANGULATION_H_

#include <string>

#include <aslam/common/memory.h>
#include <aslam/common/unique-id.h>
#include <vi-map/unique-id.h>
#include <vi-map/vi-map.h>

namespace landmark_triangulation {

void retriangulateLandmarks(vi_map::VIMap* map);
void retriangulateLandmarks(
    const vi_map::MissionIdList& mission_ids, vi_map::VIMap* map);
void retriangulateLandmarksOfMission(
    const vi_map::MissionId& mission_id, vi_map::VIMap* map);
void retriangulateLandmarksOfVertex(
    const pose_graph::VertexId& storing_vertex_id, vi_map::VIMap* map);

}  // namespace landmark_triangulation
#endif  // LANDMARK_TRIANGULATION_LANDMARK_TRIANGULATION_H_
