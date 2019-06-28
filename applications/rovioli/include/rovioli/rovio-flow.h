#ifndef ROVIOLI_ROVIO_FLOW_H_
#define ROVIOLI_ROVIO_FLOW_H_

#include <functional>
#include <memory>
#include <vector>

#include <maplab-common/bidirectional-map.h>
#include <message-flow/message-flow.h>
#include <sensors/imu.h>
#include <sensors/relative-6dof-pose.h>
#include <vio-common/vio-types.h>

#include "rovioli/rovio-estimate.h"
#include "rovioli/rovio-factory.h"
#include "rovioli/rovio-health-monitor.h"
#include "rovioli/rovio-maplab-timetranslation.h"

namespace rovioli {
class RovioLocalizationHandler;

class RovioFlow {
 public:
  explicit RovioFlow(
      const aslam::NCamera& camera_calibration,
      const vi_map::ImuSigmas& imu_sigmas,
      const vi_map::Relative6DoFPose& wheel_sensor);
  ~RovioFlow();

  void includeWheelOdometry(aslam::Transformation T_I_O);

  void attachToMessageFlow(message_flow::MessageFlow* flow);

  void processAndPublishRovioUpdate(const rovio::RovioState& state);

 private:
  std::unique_ptr<rovio::RovioInterface> rovio_interface_;
  std::function<void(const RovioEstimate::ConstPtr&)> publish_rovio_estimates_;

  RovioMaplabTimeTranslation time_translation_;
  RovioHealthMonitor health_monitor_;

  // A camera without a mapping is not being used for motion tracking in ROVIO.
  common::BidirectionalMap<size_t, size_t> maplab_to_rovio_cam_indices_mapping_;

  bool use_wheel_odometry_;
  aslam::Transformation T_I_O_;

  std::unique_ptr<RovioLocalizationHandler> localization_handler_;
};
}  // namespace rovioli
#endif  // ROVIOLI_ROVIO_FLOW_H_
