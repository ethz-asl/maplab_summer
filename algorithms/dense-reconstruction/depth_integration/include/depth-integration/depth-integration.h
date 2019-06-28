#ifndef DEPTH_INTEGRATION_DEPTH_INTEGRATION_H_
#define DEPTH_INTEGRATION_DEPTH_INTEGRATION_H_

#include <functional>
#include <unordered_set>

#include <vi-map/vi-map.h>
#include <voxblox/core/common.h>

namespace depth_integration {

// NOTE: This interface is currently still depending on voxblox, but only for
// the types.
typedef voxblox::Transformation Transformation;
typedef voxblox::Pointcloud Pointcloud;
typedef voxblox::Colors Colors;
typedef voxblox::FloatingPoint FloatingPoint;

typedef std::function<void(
    const Transformation&, const Pointcloud&, const Colors&)>
    IntegrationFunction;

// Calls the integration function with a point cloud consisting of all good
// quality landmarks from a VI map. The VI map landmarks need to be
// retriangulated beforehand.
void integrateAllLandmarks(
    const vi_map::VIMap& vi_map, IntegrationFunction integration_function);

static std::unordered_set<backend::ResourceType, backend::ResourceTypeHash>
    kSupportedDepthInputTypes{
        backend::ResourceType::kRawDepthMap,
        backend::ResourceType::kOptimizedDepthMap,
        backend::ResourceType::kPointCloudXYZ,
        backend::ResourceType::kPointCloudXYZI,
        backend::ResourceType::kPointCloudXYZRGBN,
    };

// Calls the integration function for all depth frame resources from the
// selected missions using the integration function. For depth maps you can use
// the 'use_distorted_camera' parameter to determine if it should be reprojected
// with or without the distortion of the camera.
bool integrateAllFrameDepthResourcesOfType(
    const vi_map::MissionIdList& mission_ids,
    const backend::ResourceType& input_resource_type,
    const bool use_undistorted_camera_for_depth_maps,
    const vi_map::VIMap& vi_map, IntegrationFunction integration_function);

// Calls the integration function for all optional depth resources from the
// selected missions using the integration function. For depth maps you can use
// the 'use_distorted_camera' parameter to determine if it should be reprojected
// with or without the distortion of the camera. Depending on whether this
// function is called with vi_map::SensorId or aslam::CameraId, it will either
// integrate all optional depth resources associated to a Sensor or a Camera.
template <typename SensorOrCameraId>
bool integrateAllOptionalSensorDepthResourcesOfType(
    const vi_map::MissionIdList& mission_ids,
    const backend::ResourceType& input_resource_type,
    const bool use_undistorted_camera_for_depth_maps,
    const vi_map::VIMap& vi_map, IntegrationFunction integration_function);

// Calls the integration function for all depth (optional and frame) resources.
bool integrateAllDepthResourcesOfType(
    const vi_map::MissionIdList& mission_ids,
    const backend::ResourceType& input_resource_type,
    const bool use_undistorted_camera_for_depth_maps,
    const vi_map::VIMap& vi_map, IntegrationFunction integration_function);

// Calls the integration function for a 3D point cloud.
void integratePointCloud(
    const pose::Transformation& T_G_C, const Pointcloud& points_C,
    IntegrationFunction integration_function);
void integratePointCloud(
    const pose::Transformation& T_G_C, const resources::PointCloud& points_C,
    IntegrationFunction integration_function);
void integrateColorPointCloud(
    const pose::Transformation& T_G_C, const Pointcloud& points_C,
    const Colors& colors, IntegrationFunction integration_function);

// Calls the integration function for a point cloud obtained from a depth map.
void integrateDepthMap(
    const pose::Transformation& T_G_C, const cv::Mat& depth_map,
    const aslam::Camera& camera, IntegrationFunction integration_function);

// Calls the integration function for a point cloud obtained from a depth map
// with a (color) image.
void integrateDepthMap(
    const pose::Transformation& T_G_C, const cv::Mat& depth_map,
    const cv::Mat& image, const aslam::Camera& camera,
    IntegrationFunction integration_function);

}  // namespace depth_integration

#include "depth-integration/depth-integration-inl.h"

#endif  // DEPTH_INTEGRATION_DEPTH_INTEGRATION_H_
