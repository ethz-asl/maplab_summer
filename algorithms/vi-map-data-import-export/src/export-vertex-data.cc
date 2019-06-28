#include "vi-map-data-import-export/export-vertex-data.h"

#include <console-common/command-registerer.h>
#include <glog/logging.h>
#include <maplab-common/file-logger.h>
#include <maplab-common/progress-bar.h>
#include <sensors/sensor.h>
#include <vi-map/vi-map.h>

namespace data_import_export {

char convertSensorTypeToFrameIdentifier(const vi_map::SensorType sensor_type) {
  switch (sensor_type) {
    case vi_map::SensorType::kImu:
      return 'I';
      break;
    case vi_map::SensorType::kRelative6DoFPose:
    case vi_map::SensorType::kGpsUtm:
    case vi_map::SensorType::kGpsWgs:
      return 'B';
      break;
    case vi_map::SensorType::kLidar:
      return 'L';
      break;
    default:
      LOG(FATAL) << "Unknown sensor type: " << static_cast<int>(sensor_type);
      break;
  }
  return '\0';
}

int exportPosesVelocitiesAndBiasesToCsv(
    const vi_map::VIMap& map, const vi_map::MissionIdList& mission_ids,
    const aslam::Transformation& T_I_S, const char sensor_frame_identifier,
    const std::string& pose_export_file) {
  pose_graph::VertexIdList vertex_ids;
  for (const vi_map::MissionId& mission_id : mission_ids) {
    CHECK(mission_id.isValid());
    pose_graph::VertexIdList vertex_ids_along_mission_graph;
    map.getAllVertexIdsInMissionAlongGraph(
        mission_id, &vertex_ids_along_mission_graph);
    vertex_ids.insert(
        vertex_ids.end(), vertex_ids_along_mission_graph.begin(),
        vertex_ids_along_mission_graph.end());
  }

  if (pose_export_file.empty()) {
    LOG(ERROR) << "Need to specify file path with the flag -pose_export_file.";
    return common::kUnknownError;
  }

  common::FileLogger csv_file(pose_export_file);
  if (!csv_file.isOpen()) {
    LOG(ERROR) << "Failed to open logger to file: '" << pose_export_file << "'";
    return common::kUnknownError;
  }
  LOG(INFO) << "Exporting poses, velocities and biases to: "
            << pose_export_file;

  const std::string kDelimiter = ", ";
  csv_file.writeDataWithDelimiterAndNewLine(
      kDelimiter, "# timestamp [ns]", "vertex-id", "mission-id",
      "p_G_" + std::to_string(sensor_frame_identifier) + "x [m]",
      "p_G_" + std::to_string(sensor_frame_identifier) + "y [m]",
      "p_G_" + std::to_string(sensor_frame_identifier) + "z [m]",
      "q_G_" + std::to_string(sensor_frame_identifier) + "w",
      "q_G_" + std::to_string(sensor_frame_identifier) + "x",
      "q_G_" + std::to_string(sensor_frame_identifier) + "y",
      "q_G_" + std::to_string(sensor_frame_identifier) + "z",
      "p_M_" + std::to_string(sensor_frame_identifier) + "x [m]",
      "p_M_" + std::to_string(sensor_frame_identifier) + "y [m]",
      "p_M_" + std::to_string(sensor_frame_identifier) + "z [m]",
      "q_M_" + std::to_string(sensor_frame_identifier) + "w",
      "q_M_" + std::to_string(sensor_frame_identifier) + "x",
      "q_M_" + std::to_string(sensor_frame_identifier) + "y",
      "q_M_" + std::to_string(sensor_frame_identifier) + "z", "v_Mx [m/s]",
      "v_My [m/s]", "v_Mz [m/s]", "bgx [rad/s]", "bgy [rad/s]", "bgz [rad/s]",
      "bax [m/s^2]", "bay [m/s^2]", "baz [m/s^2]");

  for (const pose_graph::VertexId& vertex_id : vertex_ids) {
    CHECK(vertex_id.isValid());
    const vi_map::Vertex& vertex = map.getVertex(vertex_id);
    const vi_map::MissionId& mission_id = vertex.getMissionId();
    CHECK(mission_id.isValid());

    const int64_t timestamp_nanoseconds = vertex.getMinTimestampNanoseconds();
    const aslam::Transformation T_G_I = map.getVertex_T_G_I(vertex_id);
    const aslam::Transformation T_G_S = T_G_I * T_I_S;
    const Eigen::Vector3d& p_G_S = T_G_S.getPosition();
    const kindr::minimal::RotationQuaternion& q_G_S = T_G_S.getRotation();
    const aslam::Transformation& T_M_I = vertex.get_T_M_I();
    const aslam::Transformation T_M_S = T_M_I * T_I_S;
    const Eigen::Vector3d& p_M_S = T_M_S.getPosition();
    const kindr::minimal::RotationQuaternion& q_M_S = T_M_S.getRotation();
    const Eigen::Vector3d& v_M = vertex.get_v_M();
    const Eigen::Vector3d& gyro_bias = vertex.getGyroBias();
    const Eigen::Vector3d& acc_bias = vertex.getAccelBias();

    csv_file.writeDataWithDelimiterAndNewLine(
        kDelimiter, timestamp_nanoseconds, vertex_id.hexString(),
        mission_id.hexString(), p_G_S[0], p_G_S[1], p_G_S[2], q_G_S.w(),
        q_G_S.x(), q_G_S.y(), q_G_S.z(), p_M_S[0], p_M_S[1], p_M_S[2],
        q_M_S.w(), q_M_S.x(), q_M_S.y(), q_M_S.z(), v_M[0], v_M[1], v_M[2],
        gyro_bias[0], gyro_bias[1], gyro_bias[2], acc_bias[0], acc_bias[1],
        acc_bias[2]);
  }
  return common::kSuccess;
}

int exportPosesVelocitiesAndBiasesToCsv(
    const vi_map::VIMap& map, const vi_map::MissionIdList& mission_ids,
    const vi_map::SensorId& reference_sensor_id,
    const std::string& pose_export_file) {
  CHECK(reference_sensor_id.isValid());
  const vi_map::SensorManager& sensor_manager = map.getSensorManager();
  const vi_map::SensorType sensor_type =
      sensor_manager.getSensor(reference_sensor_id).getSensorType();
  CHECK(sensor_type != vi_map::SensorType::kInvalidSensor);

  aslam::Transformation T_I_S;
  if (sensor_manager.hasSensorSystem() &&
      sensor_manager.getSensorSystem().getReferenceSensorId() !=
          reference_sensor_id &&
      !sensor_manager.getSensor_T_R_S(reference_sensor_id, &T_I_S)) {
    LOG(ERROR) << "No sensor extrinsics available for sensor with id "
               << reference_sensor_id.hexString() << '.';
  }
  const char sensor_frame_identifier =
      convertSensorTypeToFrameIdentifier(sensor_type);

  return exportPosesVelocitiesAndBiasesToCsv(
      map, mission_ids, T_I_S, sensor_frame_identifier, pose_export_file);
}

}  // namespace data_import_export
