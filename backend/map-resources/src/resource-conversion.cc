#include "map-resources/resource-conversion.h"

#include <aslam/cameras/camera-factory.h>
#include <aslam/cameras/camera-pinhole.h>
#include <aslam/cameras/camera-unified-projection.h>
#include <aslam/cameras/camera.h>
#include <aslam/cameras/distortion.h>
#include <glog/logging.h>
#include <maplab-common/pose_types.h>
#include <opencv2/core.hpp>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>
#include <voxblox/core/color.h>
#include <voxblox/core/common.h>

namespace backend {

// Field definitions for PointCloud2.
static const std::string kPointCloud2IntensityV1 = "intensity";
static const std::string kPointCloud2IntensityV2 = "intensities";
static const std::string kPointCloud2IntensityV3 = "i";
static const std::string kPointCloud2PointX = "x";
static const std::string kPointCloud2PointY = "y";
static const std::string kPointCloud2PointZ = "z";
static const std::string kPointCloud2ColorRGBA = "rgba";
static const std::string kPointCloud2ColorR = "r";
static const std::string kPointCloud2ColorG = "g";
static const std::string kPointCloud2ColorB = "b";
static const std::string kPointCloud2ColorA = "a";

inline std::string getScalarFieldName(
    const sensor_msgs::PointCloud2& point_cloud) {
  for (const sensor_msgs::PointField& field : point_cloud.fields) {
    if (field.name == kPointCloud2IntensityV1 ||
        field.name == kPointCloud2IntensityV2 ||
        field.name == kPointCloud2IntensityV3) {
      return field.name;
    }
  }
  return "";
}

bool convertDepthMapToPointCloud(
    const cv::Mat& depth_map, const aslam::Camera& camera,
    resources::PointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);

  cv::Mat image(1, 1, CV_8UC1);
  return convertDepthMapToPointCloud(depth_map, image, camera, point_cloud);
}

bool convertDepthMapToPointCloud(
    const cv::Mat& depth_map, const aslam::Camera& camera,
    voxblox::Pointcloud* point_cloud) {
  CHECK_NOTNULL(point_cloud)->clear();

  cv::Mat image(1, 1, CV_8UC1);
  return convertDepthMapToPointCloud(depth_map, image, camera, point_cloud);
}

bool convertDepthMapWithImageToPointCloud(
    const cv::Mat& depth_map, const cv::Mat& image, const aslam::Camera& camera,
    resources::PointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);

  return convertDepthMapToPointCloud(depth_map, image, camera, point_cloud);
}

bool convertDepthMapWithImageToPointCloud(
    const cv::Mat& depth_map, const cv::Mat& image, const aslam::Camera& camera,
    voxblox::Pointcloud* points_C, voxblox::Colors* colors) {
  CHECK_NOTNULL(points_C)->clear();
  CHECK_NOTNULL(colors);
  resources::VoxbloxColorPointCloud voxblox_point_cloud;
  voxblox_point_cloud.points_C = points_C;
  voxblox_point_cloud.colors = colors;
  return convertDepthMapToPointCloud(
      depth_map, image, camera, &voxblox_point_cloud);
}

template <>
void addPointToPointCloud(
    const Eigen::Vector3d& point_C, const size_t index,
    voxblox::Pointcloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  CHECK_GT(point_cloud->size(), index);

  voxblox::Point& point = (*point_cloud)[index];
  point = point_C.cast<voxblox::FloatingPoint>();
}

template <>
void addPointToPointCloud(
    const Eigen::Vector3d& point_C, const size_t index,
    resources::VoxbloxColorPointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  CHECK_NOTNULL(point_cloud->points_C);
  CHECK_GT(point_cloud->points_C->size(), index);

  voxblox::Point& point = (*point_cloud->points_C)[index];
  point = point_C.cast<voxblox::FloatingPoint>();
}

template <>
void addPointToPointCloud(
    const Eigen::Vector3d& point_C, const size_t index,
    resources::PointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  const size_t start_index = 3u * index;
  CHECK_LT(start_index + 2u, point_cloud->xyz.size());

  point_cloud->xyz[start_index] = static_cast<float>(point_C.x());
  point_cloud->xyz[start_index + 1u] = static_cast<float>(point_C.y());
  point_cloud->xyz[start_index + 2u] = static_cast<float>(point_C.z());
}

template <>
void addPointToPointCloud(
    const Eigen::Vector3d& point_C, const size_t index,
    sensor_msgs::PointCloud2* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  sensor_msgs::PointCloud2Iterator<float> it_x(
      *point_cloud, kPointCloud2PointX);
  sensor_msgs::PointCloud2Iterator<float> it_y(
      *point_cloud, kPointCloud2PointY);
  sensor_msgs::PointCloud2Iterator<float> it_z(
      *point_cloud, kPointCloud2PointZ);

  it_x += index;
  it_y += index;
  it_z += index;

  *it_x = static_cast<float>(point_C.x());
  *it_y = static_cast<float>(point_C.y());
  *it_z = static_cast<float>(point_C.z());
}

template <>
void addPointToPointCloud(
    const Eigen::Vector3d& point_C, const size_t index,
    pcl::PointCloud<pcl::PointXYZI>* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  CHECK_LT(index, point_cloud->points.size());
  point_cloud->points[index].x = static_cast<float>(point_C(0));
  point_cloud->points[index].y = static_cast<float>(point_C(1));
  point_cloud->points[index].z = static_cast<float>(point_C(2));
}

template <>
void getPointFromPointCloud(
    const voxblox::Pointcloud& point_cloud, const size_t index,
    Eigen::Vector3d* point_C) {
  CHECK_NOTNULL(point_C);
  Eigen::Vector3d& point_C_out = *point_C;
  DCHECK_GT(point_cloud.size(), index);
  const voxblox::Point& point_C_in = point_cloud[index];
  point_C_out = point_C_in.cast<double>();
}

template <>
void getPointFromPointCloud(
    const resources::VoxbloxColorPointCloud& point_cloud, const size_t index,
    Eigen::Vector3d* point_C) {
  CHECK_NOTNULL(point_C);
  CHECK_NOTNULL(point_cloud.points_C);

  DCHECK_GT(point_cloud.points_C->size(), index);
  *point_C = (*point_cloud.points_C)[index].cast<double>();
}

template <>
void getPointFromPointCloud(
    const resources::PointCloud& point_cloud, const size_t index,
    Eigen::Vector3d* point_C) {
  CHECK_NOTNULL(point_C);

  Eigen::Vector3d& point_C_out = *point_C;

  const size_t real_index = index * 3u;
  DCHECK_GT(point_cloud.xyz.size(), real_index + 2u);
  point_C_out[0] = static_cast<double>(point_cloud.xyz[real_index]);
  point_C_out[1] = static_cast<double>(point_cloud.xyz[real_index + 1u]);
  point_C_out[2] = static_cast<double>(point_cloud.xyz[real_index + 2u]);
}

template <>
void getPointFromPointCloud(
    const sensor_msgs::PointCloud2& point_cloud, const size_t index,
    Eigen::Vector3d* point_C) {
  CHECK_NOTNULL(point_C);

  sensor_msgs::PointCloud2ConstIterator<float> it_x(
      point_cloud, kPointCloud2PointX);
  sensor_msgs::PointCloud2ConstIterator<float> it_y(
      point_cloud, kPointCloud2PointY);
  sensor_msgs::PointCloud2ConstIterator<float> it_z(
      point_cloud, kPointCloud2PointZ);

  point_C->x() = static_cast<double>(*(it_x + index));
  point_C->y() = static_cast<double>(*(it_y + index));
  point_C->z() = static_cast<double>(*(it_z + index));
}

template <>
void addColorToPointCloud(
    const resources::RgbaColor& color, const size_t index,
    resources::PointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  const size_t start_index = 3u * index;

  CHECK_LT(start_index + 2, point_cloud->colors.size());
  point_cloud->colors[start_index] = color[0];
  point_cloud->colors[start_index + 1] = color[1];
  point_cloud->colors[start_index + 2] = color[2];
}

template <>
void addColorToPointCloud(
    const resources::RgbaColor& /*color*/, const size_t /*index*/,
    voxblox::Pointcloud* /*point_cloud*/) {
  LOG(FATAL) << "This is a vector of 3D points only. Colors can't be added.";
}

template <>
void addColorToPointCloud(
    const resources::RgbaColor& color, const size_t index,
    resources::VoxbloxColorPointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  CHECK_NOTNULL(point_cloud->colors);
  CHECK_GT(point_cloud->colors->size(), index);

  voxblox::Color& voxblox_color = (*point_cloud->colors)[index];
  voxblox_color.r = color[0];
  voxblox_color.g = color[1];
  voxblox_color.b = color[2];
  voxblox_color.a = 255u;
}

template <>
void addColorToPointCloud(
    const resources::RgbaColor& /*color*/, const size_t /*index*/,
    pcl::PointCloud<pcl::PointXYZI>* /*point_cloud*/) {
  LOG(FATAL) << "This is a point-cloud with x, y, z and intensity. "
             << "Colors can't be added.";
}

template <>
void addColorToPointCloud(
    const resources::RgbaColor& color, const size_t index,
    sensor_msgs::PointCloud2* point_cloud) {
  CHECK_NOTNULL(point_cloud);

  sensor_msgs::PointCloud2Iterator<uint8_t> it_r(
      *point_cloud, kPointCloud2ColorR);
  sensor_msgs::PointCloud2Iterator<uint8_t> it_g(
      *point_cloud, kPointCloud2ColorG);
  sensor_msgs::PointCloud2Iterator<uint8_t> it_b(
      *point_cloud, kPointCloud2ColorB);
  sensor_msgs::PointCloud2Iterator<uint8_t> it_a(
      *point_cloud, kPointCloud2ColorA);

  it_r += index;
  it_g += index;
  it_b += index;
  it_a += index;

  *it_r = color[0];
  *it_g = color[1];
  *it_b = color[2];
  *it_a = color[3];
}

template <>
void getColorFromPointCloud(
    const resources::VoxbloxColorPointCloud& point_cloud, const size_t index,
    resources::RgbaColor* color) {
  CHECK_NOTNULL(color);
  CHECK_NOTNULL(point_cloud.points_C);

  DCHECK_GT(point_cloud.colors->size(), index);
  const voxblox::Color& color_in = (*point_cloud.colors)[index];
  resources::RgbaColor& color_out = *color;

  color_out[0] = color_in.r;
  color_out[1] = color_in.g;
  color_out[2] = color_in.b;
  color_out[3] = color_in.a;
}

template <>
void getColorFromPointCloud(
    const sensor_msgs::PointCloud2& point_cloud, const size_t index,
    resources::RgbaColor* color) {
  CHECK_NOTNULL(color);
  resources::RgbaColor& color_out = *color;
  if (hasColorInformation(point_cloud)) {
    sensor_msgs::PointCloud2ConstIterator<uint8_t> it_r(
        point_cloud, kPointCloud2ColorR);
    sensor_msgs::PointCloud2ConstIterator<uint8_t> it_g(
        point_cloud, kPointCloud2ColorG);
    sensor_msgs::PointCloud2ConstIterator<uint8_t> it_b(
        point_cloud, kPointCloud2ColorB);
    sensor_msgs::PointCloud2ConstIterator<uint8_t> it_a(
        point_cloud, kPointCloud2ColorA);

    color_out[0] = *(it_r + index);
    color_out[1] = *(it_g + index);
    color_out[2] = *(it_b + index);
    color_out[3] = *(it_a + index);

  } else if (hasScalarInformation(point_cloud)) {
    const std::string field_name = getScalarFieldName(point_cloud);
    CHECK(!field_name.empty());

    sensor_msgs::PointCloud2ConstIterator<float> it_intensity(
        point_cloud, field_name);

    const double scalar = static_cast<double>(*(it_intensity + index));
    const voxblox::Color color = voxblox::grayColorMap(scalar);
    color_out[0] = color.r;
    color_out[1] = color.g;
    color_out[2] = color.b;
    color_out[3] = color.a;
  } else {
    LOG(FATAL) << "Cannot get color from pcl point cloud type if there are "
                  "no colors or scalars!";
  }
}

template <>
void getColorFromPointCloud(
    const resources::PointCloud& point_cloud, const size_t index,
    resources::RgbaColor* color) {
  CHECK_NOTNULL(color);
  resources::RgbaColor& color_out = *color;

  if (hasColorInformation(point_cloud)) {
    const size_t real_index = index * 3u;

    DCHECK_GT(point_cloud.colors.size(), real_index + 2u);

    color_out[0] = point_cloud.colors[real_index];
    color_out[1] = point_cloud.colors[real_index + 1u];
    color_out[2] = point_cloud.colors[real_index + 2u];
    color_out[3] = 255;
  } else if (hasScalarInformation(point_cloud)) {
    const voxblox::Color color =
        voxblox::grayColorMap(point_cloud.scalars[index]);
    color_out[0] = color.r;
    color_out[1] = color.g;
    color_out[2] = color.b;
    color_out[3] = color.a;
  } else {
    LOG(FATAL) << "Cannot get color from maplab point cloud type if there are "
                  "no colors or scalars!";
  }
}

template <>
void addScalarToPointCloud(
    const float scalar, const size_t index,
    resources::PointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  CHECK_LT(index, point_cloud->scalars.size());
  point_cloud->scalars[index] = scalar;
}

template <>
void addScalarToPointCloud(
    const float /*scalar*/, const size_t /*index*/,
    resources::VoxbloxColorPointCloud* /*point_cloud*/) {
  LOG(FATAL) << "The voxblox point cloud type does not support scalars!";
}

template <>
void addScalarToPointCloud(
    const float scalar, const size_t index,
    sensor_msgs::PointCloud2* point_cloud) {
  CHECK_NOTNULL(point_cloud);

  sensor_msgs::PointCloud2Iterator<float> it_intensity(
      *point_cloud, kPointCloud2IntensityV1);

  it_intensity += index;

  *it_intensity = scalar;
}

template <>
void addScalarToPointCloud(
    const float scalar, const size_t index,
    pcl::PointCloud<pcl::PointXYZI>* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  CHECK_LT(index, point_cloud->points.size());
  point_cloud->points[index].intensity = scalar;
}

template <>
void getScalarFromPointCloud(
    const resources::PointCloud& point_cloud, const size_t index,
    float* scalar) {
  CHECK_NOTNULL(scalar);

  DCHECK_GT(point_cloud.scalars.size(), index);
  *scalar = point_cloud.scalars[index];
}

template <>
void getScalarFromPointCloud(
    const sensor_msgs::PointCloud2& point_cloud, const size_t index,
    float* scalar) {
  CHECK_NOTNULL(scalar);

  const std::string field_name = getScalarFieldName(point_cloud);
  CHECK(!field_name.empty());

  sensor_msgs::PointCloud2ConstIterator<float> it_intensity(
      point_cloud, field_name);

  *scalar = *(it_intensity + index);
}

template <>
void resizePointCloud(
    const size_t size, const bool /*has_color*/, const bool /*has_normals*/,
    const bool /*has_scalar*/, voxblox::Pointcloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  point_cloud->resize(size);
}

template <>
void resizePointCloud(
    const size_t size, const bool has_color, const bool /*has_normals*/,
    const bool /*has_scalar*/, resources::VoxbloxColorPointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  CHECK_NOTNULL(point_cloud->colors)->clear();
  CHECK_NOTNULL(point_cloud->points_C)->clear();

  point_cloud->points_C->resize(size);
  if (has_color) {
    point_cloud->colors->resize(size);
  }
}

template <>
void resizePointCloud(
    const size_t size, const bool has_color, const bool has_normals,
    const bool has_scalar, resources::PointCloud* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  point_cloud->resize(size, has_normals, has_color, has_scalar);
}

template <>
void resizePointCloud(
    const size_t num_points, const bool has_color, const bool /*has_normals*/,
    const bool has_scalar, sensor_msgs::PointCloud2* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  assert(sizeof(float) == 4u);
  CHECK_GT(num_points, 0u);

  point_cloud->height = 1u;
  point_cloud->width = num_points;
  point_cloud->is_dense = false;
  point_cloud->is_bigendian = false;

  // Set fields.
  point_cloud->fields.clear();
  int offset = 0;

  offset = addPointField(
      *point_cloud, kPointCloud2PointX, 1, sensor_msgs::PointField::FLOAT32,
      offset);
  offset = addPointField(
      *point_cloud, kPointCloud2PointY, 1, sensor_msgs::PointField::FLOAT32,
      offset);
  offset = addPointField(
      *point_cloud, kPointCloud2PointZ, 1, sensor_msgs::PointField::FLOAT32,
      offset);

  // The offset includes 1x4bytes for padding, to get a better memory
  // alignment.
  offset += sizeOfPointField(sensor_msgs::PointField::FLOAT32);

  if (has_color) {
    offset = addPointField(
        *point_cloud, kPointCloud2ColorRGBA, 1,
        sensor_msgs::PointField::FLOAT32, offset);

    // The offset includes 2x 4bytes for padding, to get a better memory
    // alignment.
    offset += 3 * sizeOfPointField(sensor_msgs::PointField::FLOAT32);
  }
  if (has_scalar) {
    offset = addPointField(
        *point_cloud, kPointCloud2IntensityV1, 1,
        sensor_msgs::PointField::FLOAT32, offset);

    // The offset includes 2x 4bytes for padding, to get a better memory
    // alignment.
    offset += 3 * sizeOfPointField(sensor_msgs::PointField::FLOAT32);
  }

  point_cloud->point_step = offset;
  point_cloud->row_step = point_cloud->width * point_cloud->point_step;
  point_cloud->data.resize(point_cloud->height * point_cloud->row_step);

  CHECK_EQ(hasScalarInformation(*point_cloud), has_scalar);
  CHECK_EQ(hasColorInformation(*point_cloud), has_color);
}

template <>
void resizePointCloud(
    const size_t num_points, const bool /*has_color*/,
    const bool /*has_normals*/, const bool has_scalar,
    pcl::PointCloud<pcl::PointXYZI>* point_cloud) {
  CHECK_NOTNULL(point_cloud);
  CHECK_GT(num_points, 0u);
  CHECK(has_scalar) << "A scalar is required for the point-cloud with x, y, z, "
                    << "and intensity.";
  point_cloud->points.resize(num_points);
}

void createCameraWithoutDistortion(
    const aslam::Camera& camera,
    aslam::Camera::Ptr* camera_without_distortion) {
  CHECK_NOTNULL(camera_without_distortion);

  switch (camera.getType()) {
    case aslam::Camera::Type::kPinhole:
      *camera_without_distortion = aslam::createCamera<aslam::PinholeCamera>(
          camera.getParameters(), camera.imageWidth(), camera.imageHeight());
      break;
    case aslam::Camera::Type::kUnifiedProjection:
      *camera_without_distortion =
          aslam::createCamera<aslam::UnifiedProjectionCamera>(
              camera.getParameters(), camera.imageWidth(),
              camera.imageHeight());
      break;
    default:
      LOG(FATAL) << "Unknown camera type: " << camera.getType();
  }
}

template <>
size_t getPointCloudSize(const voxblox::Pointcloud& point_cloud) {
  return point_cloud.size();
}

template <>
size_t getPointCloudSize(const resources::VoxbloxColorPointCloud& point_cloud) {
  const size_t num_points = point_cloud.points_C->size();
  if (point_cloud.colors != nullptr) {
    const size_t num_colors = point_cloud.colors->size();
    if (num_colors > 0u) {
      CHECK_EQ(num_points, num_colors);
    }
  }
  return num_points;
}

template <>
size_t getPointCloudSize(const resources::PointCloud& point_cloud) {
  return point_cloud.size();
}

template <>
size_t getPointCloudSize(const sensor_msgs::PointCloud2& point_cloud) {
  return point_cloud.data.size() / point_cloud.point_step;
}

template <>
bool hasColorInformation(const voxblox::Pointcloud& /*point_cloud*/) {
  return false;
}

template <>
bool hasColorInformation(const resources::VoxbloxColorPointCloud& point_cloud) {
  return point_cloud.colors->size() == point_cloud.points_C->size();
}

template <>
bool hasColorInformation(const resources::PointCloud& point_cloud) {
  return point_cloud.xyz.size() == point_cloud.colors.size();
}

template <>
bool hasColorInformation(const sensor_msgs::PointCloud2& point_cloud) {
  for (const sensor_msgs::PointField& field : point_cloud.fields) {
    if (field.name == kPointCloud2ColorRGBA) {
      return true;
    }
  }
  return false;
}

template <>
bool hasScalarInformation(const sensor_msgs::PointCloud2& point_cloud) {
  return !getScalarFieldName(point_cloud).empty();
}

template <>
bool hasScalarInformation(const resources::PointCloud& point_cloud) {
  return !point_cloud.scalars.empty();
}

}  // namespace backend
