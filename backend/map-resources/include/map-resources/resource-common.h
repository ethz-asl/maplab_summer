#ifndef MAP_RESOURCES_RESOURCE_COMMON_H_
#define MAP_RESOURCES_RESOURCE_COMMON_H_

#include <array>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <maplab-common/unique-id.h>
#include <opencv2/core.hpp>
#include <resources-common/point-cloud.h>
#include <voxblox/core/esdf_map.h>
#include <voxblox/core/occupancy_map.h>
#include <voxblox/core/tsdf_map.h>

#include "map-resources/resource-typedefs.h"

namespace backend {
UNIQUE_ID_DEFINE_ID(ResourceId);
}  // namespace backend

UNIQUE_ID_DEFINE_ID_HASH(backend::ResourceId);

namespace backend {

// NOTE: [ADD_RESOURCE_TYPE] Add enum. When adding a new ResourceType, add it at
// the bottom, right above kCount to ensure backward compatibility.
enum class ResourceType : int {
  kRawImage = 0,
  kUndistortedImage = 1,
  kRectifiedImage = 2,
  kImageForDepthMap = 3,
  kRawColorImage = 4,
  kUndistortedColorImage = 5,
  kRectifiedColorImage = 6,
  kColorImageForDepthMap = 7,
  kRawDepthMap = 8,
  kOptimizedDepthMap = 9,
  kDisparityMap = 10,
  kText = 11,
  kPmvsReconstructionPath = 12,
  kTsdfGridPath = 13,
  kEsdfGridPath = 14,
  kOccupancyGridPath = 15,
  kPointCloudXYZ = 16,
  kPointCloudXYZRGBN = 17,
  kVoxbloxTsdfMap = 18,
  kVoxbloxEsdfMap = 19,
  kVoxbloxOccupancyMap = 20,
  kPointCloudXYZI = 21,
  kObjectInstanceBoundingBoxes = 22,
  kObjectInstanceMasks = 23,
  kCount
};

static constexpr size_t kNumResourceTypes =
    static_cast<size_t>(ResourceType::kCount);

// NOTE: [ADD_RESOURCE_TYPE] Add name.
const std::array<std::string, kNumResourceTypes> ResourceTypeNames = {
    {/*kRawImage*/ "raw_images",
     /*kUndistortedImage*/ "undistorted_images",
     /*kRectifiedImage*/ "rectified images",
     /*kImageForDepthMap*/ "depth_map_images",
     /*kRawColorImage*/ "raw_color_images",
     /*kUndistortedColorImage*/ "undistorted_color_images",
     /*kRectifiedColorImage*/ "rectified_color_images",
     /*kColorImageForDepthMap*/ "depth_map_color_image",
     /*kRawDepthMap*/ "raw_depth_maps",
     /*kOptimizedDepthMap*/ "optimized_depth_maps",
     /*kDisparityMap*/ "disparity_maps",
     /*kText*/ "text",
     /*kPmvsReconstructionPath*/ "pmvs_reconstruction_paths",
     /*kTsdfGridPath*/ "tsdf_grid_paths",
     /*kEsdfGridPath*/ "esdf_grid_paths",
     /*kOccupancyGridPath*/ "occupancy_grid_paths",
     /*kPointCloudXYZ*/ "point_cloud",
     /*kPointCloudXYZRGBN*/ "color_point_cloud",
     /*kVoxbloxTsdfMap*/ "voxblox_tsdf_map",
     /*kVoxbloxEsdfMap*/ "voxblox_esdf_map",
     /*kVoxbloxOccupancyMap*/ "voxblox_occupancy_map",
     /*kPointCloudXYZI*/ "point_cloud_w_intensity",
     /*kObjectInstanceBoundingBoxes*/ "object_instance_bounding_boxes",
     /*kObjectInstanceMasks*/ "object_instance_masks"}};

// NOTE: [ADD_RESOURCE_TYPE] Add suffix.
const std::array<std::string, kNumResourceTypes> ResourceTypeFileSuffix = {
    {/*kRawImage*/ ".pgm",
     /*kUndistortedImage*/ ".pgm",
     /*kRectifiedImage*/ ".pgm",
     /*kImageForDepthMap*/ ".pgm",
     /*kRawColorImage*/ ".ppm",
     /*kUndistortedColorImage*/ ".ppm",
     /*kRectifiedColorImage*/ ".ppm",
     /*kColorImageForDepthMap*/ ".ppm",
     /*kRawDepthMap*/ ".pgm",
     /*kOptimizedDepthMap*/ ".pgm",
     /*kDisparityMap*/ ".pgm",
     /*kText*/ ".txt",
     /*kPmvsReconstructionPath*/ ".txt",
     /*kTsdfGridPath*/ ".txt",
     /*kEsdfGridPath*/ ".txt",
     /*kOccupancyGridPath*/ ".txt",
     /*kPointCloudXYZ*/ ".ply",
     /*kPointCloudXYZRGBN*/ ".ply",
     /*kVoxbloxTsdfMap*/ ".tsdf.voxblox",
     /*kVoxbloxEsdfMap*/ ".esdf.voxblox",
     /*kVoxbloxOccupancyMap*/ ".occupancy.voxblox",
     /*kPointCloudXYZI*/ ".ply",
     /*kObjectInstanceBoundingBoxes*/ ".obj_instance_bboxes.proto",
     /*kObjectInstanceMasks*/ ".obj_instance_mask.ppm"}};

struct ResourceTypeHash {
  template <typename T>
  std::size_t operator()(T t) const {
    return static_cast<std::size_t>(t);
  }
};

typedef std::unordered_map<ResourceType, ResourceIdSet, ResourceTypeHash>
    ResourceTypeToIdsMap;

// NOTE: [ADD_RESOURCE_DATA_TYPE] Declare and implement.
template <typename DataType>
bool isSameResource(const DataType& resource_A, const DataType& resource_B);

template <>
bool isSameResource(const cv::Mat& resource_A, const cv::Mat& resource_B);
template <>
bool isSameResource(
    const std::string& resource_A, const std::string& resource_B);

template <>
bool isSameResource(
    const voxblox::TsdfMap& resource_A, const voxblox::TsdfMap& resource_B);

template <>
bool isSameResource(
    const voxblox::EsdfMap& resource_A, const voxblox::EsdfMap& resource_B);

template <>
bool isSameResource(
    const voxblox::OccupancyMap& resource_A,
    const voxblox::OccupancyMap& resource_B);

template <>
bool isSameResource(
    const resources::PointCloud& point_cloud_A,
    const resources::PointCloud& point_cloud_B);

template <>
bool isSameResource(
    const resources::ObjectInstanceBoundingBoxes& bboxes_A,
    const resources::ObjectInstanceBoundingBoxes& bboxes_B);

}  // namespace backend

#endif  // MAP_RESOURCES_RESOURCE_COMMON_H_
