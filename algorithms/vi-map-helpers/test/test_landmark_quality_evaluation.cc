#include <glog/logging.h>
#include <landmark-triangulation/landmark-triangulation.h>
#include <maplab-common/test/testing-entrypoint.h>
#include <maplab-common/test/testing-predicates.h>
#include <vi-map/vi-map.h>
#include <vi-mapping-test-app/vi-mapping-test-app.h>

#include "vi-map-helpers/test/vi-map-landmark-quality-check.h"
#include "vi-map-helpers/vi-map-landmark-quality-evaluation.h"

DECLARE_double(vi_map_landmark_quality_min_observation_angle_deg);
DECLARE_uint64(vi_map_landmark_quality_min_observers);
DECLARE_double(vi_map_landmark_quality_max_distance_from_closest_observer);
DECLARE_double(vi_map_landmark_quality_min_distance_from_closest_observer);

namespace vi_map_helpers {

class ViMappingTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    test_app_.loadDataset("./test_maps/vi_app_test");
    CHECK_NOTNULL(test_app_.getMapMutable());
  }

  virtual void corruptLandmarks();
  visual_inertial_mapping::VIMappingTestApp test_app_;
};

void ViMappingTest::corruptLandmarks() {
  constexpr double kLandmarkPositionStdDev = 5.0;
  constexpr int kEveryNthToCorrupt = 1;
  test_app_.corruptLandmarkPositions(
      kLandmarkPositionStdDev, kEveryNthToCorrupt);
}

TEST_F(ViMappingTest, TestLandmarkQualityEvaluation) {
  vi_map::VIMap* map = CHECK_NOTNULL(test_app_.getMapMutable());

  vi_map::MissionIdList mission_ids;
  map->getAllMissionIds(&mission_ids);
  resetLandmarkQualityToUnknown(mission_ids, map);

  checkLandmarkQualityInView(*map, 8359, 0, 0);

  FLAGS_vi_map_landmark_quality_min_observation_angle_deg = 5;
  FLAGS_vi_map_landmark_quality_min_observers = 4;
  FLAGS_vi_map_landmark_quality_max_distance_from_closest_observer = 40;
  FLAGS_vi_map_landmark_quality_min_distance_from_closest_observer = 0.05;

  evaluateLandmarkQuality(map);
  checkLandmarkQualityInView(*map, 0, 6107, 2252);
}

TEST_F(ViMappingTest, TestLandmarkQualityMetrics) {
  corruptLandmarks();

  vi_map::VIMap* map = test_app_.getMapMutable();
  vi_map_helpers::checkLandmarkQualityInView(*map, 8359, 0, 0);

  FLAGS_vi_map_landmark_quality_min_observation_angle_deg = 5;
  FLAGS_vi_map_landmark_quality_min_observers = 4;
  FLAGS_vi_map_landmark_quality_max_distance_from_closest_observer = 40;
  FLAGS_vi_map_landmark_quality_min_distance_from_closest_observer = 0.05;

  vi_map::MissionIdList mission_ids;
  test_app_.getMapMutable()->getAllMissionIds(&mission_ids);
  landmark_triangulation::retriangulateLandmarks(mission_ids, map);
  checkLandmarkQualityInView(*map, 0, 6138, 2221);
}

TEST_F(ViMappingTest, TestLandmarkEvaluation) {
  vi_map::VIMap* map = test_app_.getMapMutable();
  checkLandmarkQualityInView(*map, 8359, 0, 0);

  FLAGS_vi_map_landmark_quality_min_observation_angle_deg = 5;
  FLAGS_vi_map_landmark_quality_min_observers = 4;
  FLAGS_vi_map_landmark_quality_max_distance_from_closest_observer = 40;
  FLAGS_vi_map_landmark_quality_min_distance_from_closest_observer = 0.05;

  vi_map::MissionIdList mission_ids;
  test_app_.getMapMutable()->getAllMissionIds(&mission_ids);
  landmark_triangulation::retriangulateLandmarks(mission_ids, map);
  checkLandmarkQualityInView(*map, 0, 6138, 2221);
}

}  // namespace vi_map_helpers

MAPLAB_UNITTEST_ENTRYPOINT
