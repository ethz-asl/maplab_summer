#include "rovioli/datasource-rostopic.h"

#include <string>

#include <aslam/common/time.h>
#include <boost/bind.hpp>
#include <maplab-common/accessors.h>
#include <vio-common/rostopic-settings.h>

#include "rovioli/ros-helpers.h"

DECLARE_bool(rovioli_zero_initial_timestamps);

namespace rovioli {

DataSourceRostopic::DataSourceRostopic(
    const vio_common::RosTopicSettings& ros_topics)
    : shutdown_requested_(false),
      ros_topics_(ros_topics),
      image_transport_(node_handle_) {}

DataSourceRostopic::~DataSourceRostopic() {}

void DataSourceRostopic::startStreaming() {
  registerSubscribers(ros_topics_);
}

void DataSourceRostopic::shutdown() {
  shutdown_requested_ = true;
}

void DataSourceRostopic::registerSubscribers(
    const vio_common::RosTopicSettings& ros_topics) {
  // Camera subscriber.
  const size_t num_cameras = ros_topics.camera_topic_cam_index_map.size();
  sub_images_.reserve(num_cameras);

  for (const std::pair<std::string, size_t>& topic_camidx :
       ros_topics.camera_topic_cam_index_map) {
    boost::function<void(const sensor_msgs::ImageConstPtr&)> image_callback =
        boost::bind(
            &DataSourceRostopic::imageCallback, this, _1, topic_camidx.second);

    constexpr size_t kRosSubscriberQueueSizeImage = 20u;
    image_transport::Subscriber image_sub = image_transport_.subscribe(
        topic_camidx.first, kRosSubscriberQueueSizeImage, image_callback);
    sub_images_.push_back(image_sub);
  }

  // IMU subscriber.
  constexpr size_t kRosSubscriberQueueSizeImu = 1000u;
  boost::function<void(const sensor_msgs::ImuConstPtr&)> imu_callback =
      boost::bind(&DataSourceRostopic::imuMeasurementCallback, this, _1);
  sub_imu_ = node_handle_.subscribe(
      ros_topics.imu_topic, kRosSubscriberQueueSizeImu, imu_callback);

  // Odom subscriber.
  if (!ros_topics_.absolute_pose_topic.empty()) {
    constexpr size_t kRosSubscriberQueueSizeOdom = 1000u;
    boost::function<void(const nav_msgs::OdometryConstPtr&)> odometry_callback =
        boost::bind(&DataSourceRostopic::odometryMeasurementCallback, this, _1);
    sub_odometry_ = node_handle_.subscribe(
        ros_topics.absolute_pose_topic, kRosSubscriberQueueSizeOdom,
        odometry_callback);
  }
}

void DataSourceRostopic::imageCallback(
    const sensor_msgs::ImageConstPtr& image_message, size_t camera_idx) {
  if (shutdown_requested_) {
    return;
  }

  vio::ImageMeasurement::Ptr image_measurement =
      convertRosImageToMaplabImage(image_message, camera_idx);

  // Apply the IMU to camera time shift.
  if (FLAGS_rovioli_imu_to_camera_time_offset_ns != 0) {
    image_measurement->timestamp += FLAGS_rovioli_imu_to_camera_time_offset_ns;
  }

  // Shift timestamps to start at 0.
  if (!FLAGS_rovioli_zero_initial_timestamps ||
      shiftByFirstTimestamp(&(image_measurement->timestamp))) {
    invokeImageCallbacks(image_measurement);
  }
}

void DataSourceRostopic::imuMeasurementCallback(
    const sensor_msgs::ImuConstPtr& msg) {
  if (shutdown_requested_) {
    return;
  }

  vio::ImuMeasurement::Ptr imu_measurement = convertRosImuToMaplabImu(msg);

  // Shift timestamps to start at 0.
  if (!FLAGS_rovioli_zero_initial_timestamps ||
      shiftByFirstTimestamp(&(imu_measurement->timestamp))) {
    invokeImuCallbacks(imu_measurement);
  }
}

void DataSourceRostopic::odometryMeasurementCallback(
    const nav_msgs::OdometryConstPtr& msg) {
  if (shutdown_requested_) {
    return;
  }

  vio::OdometryMeasurement::Ptr odometry_measurement =
      convertRosOdometryToMaplabOdometry(msg);

  // Shift timestamps to start at 0.
  if (!FLAGS_rovioli_zero_initial_timestamps ||
      shiftByFirstTimestamp(&(odometry_measurement->timestamp))) {
    invokeOdometryCallbacks(odometry_measurement);
  }
}

}  // namespace rovioli
