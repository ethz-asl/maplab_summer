#ifndef ROVIOLI_DATASOURCE_ROSTOPIC_H_
#define ROVIOLI_DATASOURCE_ROSTOPIC_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <aslam/common/time.h>
#include <vio-common/rostopic-settings.h>
#include <vio-common/vio-types.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <nav_msgs/Odometry.h>
#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/image_encodings.h>
#pragma GCC diagnostic pop

#include "rovioli/datasource.h"

DECLARE_int64(rovioli_imu_to_camera_time_offset_ns);

namespace rovioli {

class DataSourceRostopic : public DataSource {
 public:
  explicit DataSourceRostopic(const vio_common::RosTopicSettings& settings);
  virtual ~DataSourceRostopic();

  virtual void startStreaming();
  virtual void shutdown();

  virtual bool allDataStreamed() const {
    // Workers streaming live data never run out of data.
    return !ros::ok();
  }

  virtual std::string getDatasetName() const {
    return "live-rostopic";
  }

 private:
  void registerSubscribers(const vio_common::RosTopicSettings& ros_topics);
  void imageCallback(const sensor_msgs::ImageConstPtr& msg, size_t camera_idx);
  void imuMeasurementCallback(const sensor_msgs::ImuConstPtr& msg);
  void odometryMeasurementCallback(const nav_msgs::OdometryConstPtr& msg);

  std::atomic<bool> shutdown_requested_;
  const vio_common::RosTopicSettings ros_topics_;
  ros::NodeHandle node_handle_;
  image_transport::ImageTransport image_transport_;
  std::vector<image_transport::Subscriber> sub_images_;
  ros::Subscriber sub_imu_;
  ros::Subscriber sub_odometry_;
};

}  // namespace rovioli

#endif  // ROVIOLI_DATASOURCE_ROSTOPIC_H_
