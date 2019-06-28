#include "vi-map-data-import-export-plugin/data-import-export-plugin.h"

#include <console-common/console.h>
#include <csv-export/csv-export.h>
#include <map-manager/map-manager.h>
#include <maplab-common/file-logger.h>
#include <vi-map-data-import-export/export-ncamera-calibration.h>
#include <vi-map-data-import-export/export-vertex-data.h>
#include <vi-map-data-import-export/import-export-gps-data.h>
#include <vi-map/vi-map.h>

DECLARE_string(map_mission);
DECLARE_bool(csv_export_imu_data);
DECLARE_bool(csv_export_tracks_and_keypoints);
DECLARE_bool(csv_export_descriptors);
DECLARE_bool(csv_export_landmarks);
DECLARE_bool(csv_export_observations);

DEFINE_string(csv_export_path, "", "Path to save the map in CSV format into.");
DEFINE_string(
    mission_info_export_path, "", "Export path of the mission info yaml.");

DEFINE_string(
    ncamera_calibration_export_folder, "",
    "Folder to export the ncamera calibration into.");

DEFINE_string(pose_export_file, "", "File to export poses to.");

DEFINE_string(bag_file, "", "Bag file to import data from.");

DEFINE_string(
    gps_topic, "", "The topic name for importing GPS/UTM data from a rosbag.");

DEFINE_string(
    gps_yaml, "",
    "The GPS sensor YAML file containing ID, type and calibration parameters.");

DEFINE_string(
    pose_export_reference_sensor_type, "IMU",
    "Sensor defining in "
    "what coordinate frame to express the vertex poses.");

namespace data_import_export {

DataImportExportPlugin::DataImportExportPlugin(common::Console* console)
    : common::ConsolePluginBase(console) {
  auto csv_export = [this]() -> int {
    std::string selected_map_key;
    if (!getSelectedMapKeyIfSet(&selected_map_key)) {
      return common::kStupidUserError;
    }

    vi_map::VIMapManager map_manager;
    vi_map::VIMapManager::MapReadAccess map =
        map_manager.getMapReadAccess(selected_map_key);
    const std::string& save_path = FLAGS_csv_export_path;
    if (save_path.empty()) {
      LOG(ERROR) << "No path to export the CSV files into has been specified. "
                 << "Please specify using the --csv_export_path flag.";
      return common::kStupidUserError;
    }

    csv_export::exportMapToCsv(*map, save_path);
    return common::kSuccess;
  };
  addCommand(
      {"csv_export"}, csv_export,
      "Exports keyframe, keypoint and track, landmark and IMU data to CSV "
      "files in a folder specified by --csv_export_path. Check the "
      "documentation for information on the CSV format.",
      common::Processing::Sync);
  addCommand(
      {"csv_export_vertices_only"},
      [csv_export]() -> int {
        FLAGS_csv_export_imu_data = false;
        FLAGS_csv_export_tracks_and_keypoints = false;
        FLAGS_csv_export_descriptors = false;
        FLAGS_csv_export_landmarks = false;
        FLAGS_csv_export_observations = false;
        return csv_export();
      },
      "Exports only vertices in a CSV file in a folder specified by "
      "--csv_export_path.",
      common::Processing::Sync);

  addCommand(
      {"export_mission_info"}, [this]() -> int { return exportMissionInfo(); },
      "Exports a yaml that lists map key to mission id associations of all "
      "loaded maps so that data from the CSV exporter can more easily be "
      "linked to a specific mission.",
      common::Processing::Sync);

  addCommand(
      {"export_trajectory_to_csv", "ettc"},
      [this]() -> int { return exportPosesVelocitiesAndBiasesToCsv(); },
      "Export poses, velocities and biases to a CSV file specified with "
      "--pose_export_file.",
      common::Processing::Sync);

  addCommand(
      {"export_ncamera_calibration", "encc"},
      [this]() -> int { return exportNCameraCalibration(); },
      "Exports the ncamera calibration to the folder specified with "
      "--ncamera_calibration_export_folder.",
      common::Processing::Sync);

  addCommand(
      {"import_gps_data_from_rosbag"},
      [this]() -> int { return importGpsDataFromRosbag(); },
      "Imports GPS (UTM, WGS) data from the rosbag specified with --bag_file. "
      "The topic can be specified with --gps_topic and the YAML file with "
      "--gps_yaml.",
      common::Processing::Sync);

  addCommand(
      {"export_gps_utm_data_to_csv"},
      [this]() -> int { return exportGpsUtmToCsv(); },
      "Exports GPS UTM data to a CSV file placed inside the loaded map folder.",
      common::Processing::Sync);

  addCommand(
      {"export_gps_wgs_data_to_csv"},
      [this]() -> int { return exportGpsWgsToCsv(); },
      "Exports GPS WGS data to a CSV file placed inside the loaded map folder.",
      common::Processing::Sync);
}

int DataImportExportPlugin::exportMissionInfo() const {
  const vi_map::VIMapManager map_manager;
  std::unordered_set<std::string> list_of_all_map_keys;
  map_manager.getAllMapKeys(&list_of_all_map_keys);
  if (list_of_all_map_keys.empty()) {
    LOG(ERROR) << "No maps are loaded.";
    return common::kStupidUserError;
  }
  if (FLAGS_mission_info_export_path.empty()) {
    LOG(ERROR) << "No export path has been specified, use "
               << "--mission_info_export_path to specify one.";
    return common::kStupidUserError;
  }

  if (!common::createPathToFile(FLAGS_mission_info_export_path)) {
    LOG(ERROR) << "Couldn't create path to \"" << FLAGS_mission_info_export_path
               << "\".";
    return common::kUnknownError;
  }

  common::FileLogger output_file(FLAGS_mission_info_export_path);
  for (const std::string& map_key : list_of_all_map_keys) {
    const vi_map::VIMapManager::MapReadAccess map =
        map_manager.getMapReadAccess(map_key);
    vi_map::MissionIdList list_of_all_mission_ids_in_map;
    map->getAllMissionIds(&list_of_all_mission_ids_in_map);
    if (!list_of_all_mission_ids_in_map.empty()) {
      output_file << map_key << ":\n";
      for (size_t idx = 0u; idx < list_of_all_mission_ids_in_map.size();
           ++idx) {
        const vi_map::MissionId& mission_id =
            list_of_all_mission_ids_in_map[idx];
        output_file << "  - index: " << idx << '\n';
        output_file << "    id: " << mission_id.hexString() << '\n';
      }
    }
  }

  return common::kSuccess;
}

int DataImportExportPlugin::exportPosesVelocitiesAndBiasesToCsv() const {
  std::string selected_map_key;
  if (!getSelectedMapKeyIfSet(&selected_map_key)) {
    return common::kStupidUserError;
  }

  vi_map::VIMapManager map_manager;
  vi_map::VIMapManager::MapReadAccess map =
      map_manager.getMapReadAccess(selected_map_key);

  vi_map::MissionIdList mission_ids;
  if (FLAGS_map_mission.empty()) {
    map->getAllMissionIds(&mission_ids);
    if (mission_ids.empty()) {
      LOG(ERROR)
          << "There are no missions available in the loaded map. Aborting.";
      return common::kUnknownError;
    }
  } else {
    vi_map::MissionId mission_id;
    map->ensureMissionIdValid(FLAGS_map_mission, &mission_id);
    if (!mission_id.isValid()) {
      LOG(ERROR) << "Mission ID invalid. Specify a valid mission id with "
                    "--map_mission.";
      return common::kUnknownError;
    }
    mission_ids.emplace_back(mission_id);
  }
  CHECK(!mission_ids.empty());

  const std::string kFilename = "vertex_poses_velocities_biases.csv";
  const std::string filepath =
      FLAGS_pose_export_file.empty()
          ? common::concatenateFolderAndFileName(map->getMapFolder(), kFilename)
          : FLAGS_pose_export_file;
  CHECK(!filepath.empty());

  const vi_map::SensorType sensor_type =
      vi_map::stringToSensorType(FLAGS_pose_export_reference_sensor_type);
  if (sensor_type == vi_map::SensorType::kInvalidSensor) {
    return common::kStupidUserError;
  }

  const vi_map::SensorManager& sensor_manager = map->getSensorManager();
  vi_map::SensorIdSet sensor_ids;
  sensor_manager.getAllSensorIdsOfType(sensor_type, &sensor_ids);
  if (sensor_ids.empty()) {
    LOG(ERROR) << "No sensor of type "
               << vi_map::sensorTypeToString(sensor_type) << " available.";
    return common::kStupidUserError;
  }
  if (sensor_ids.size() > 1u) {
    LOG(ERROR) << "More than one sensor of type "
               << vi_map::sensorTypeToString(sensor_type)
               << " available. Don't know "
               << "how to choose.";
    return common::kStupidUserError;
  }
  CHECK_EQ(sensor_ids.size(), 1u);

  data_import_export::exportPosesVelocitiesAndBiasesToCsv(
      *map, mission_ids, *sensor_ids.begin(), filepath);
  return common::kSuccess;
}

int DataImportExportPlugin::exportNCameraCalibration() const {
  std::string selected_map_key;
  if (!getSelectedMapKeyIfSet(&selected_map_key)) {
    return common::kStupidUserError;
  }

  vi_map::VIMapManager map_manager;
  vi_map::VIMapManager::MapReadAccess map =
      map_manager.getMapReadAccess(selected_map_key);

  if (FLAGS_ncamera_calibration_export_folder.empty()) {
    LOG(ERROR) << "Specify a valid export folder with "
               << "--ncamera_calibration_export_folder.";
    return common::kStupidUserError;
  }

  data_import_export::exportNCameraCalibration(
      *map, FLAGS_ncamera_calibration_export_folder);
  return common::kSuccess;
}

int DataImportExportPlugin::importGpsDataFromRosbag() const {
  const std::string& bag_file = FLAGS_bag_file;
  if (bag_file.empty()) {
    LOG(ERROR) << "The specified bag file parameter is empty. "
               << "Please specify a valid bag file with --bag_file.";
    return common::kStupidUserError;
  }

  if (!common::fileExists(bag_file)) {
    LOG(ERROR) << "The specified bag file does not "
               << "exist on the file-system. Please point to an existing bag "
               << "file with --bag_file.";
    return common::kStupidUserError;
  }

  const std::string& gps_topic = FLAGS_gps_topic;
  if (gps_topic.empty()) {
    LOG(ERROR) << "GPS topic is empty. Please specify "
               << "valid GPS topic with --gps_topic.";
    return common::kStupidUserError;
  }

  const std::string& gps_yaml = FLAGS_gps_yaml;
  if (gps_yaml.empty()) {
    LOG(ERROR) << "The specified GPS YAML file parameter is empty. "
               << "Please specify a valid yaml file with --gps_yaml.";
    return common::kStupidUserError;
  }

  if (!common::fileExists(gps_yaml)) {
    LOG(ERROR) << "The specified GPS YAML file does not "
               << "exist on the file-system. Please point to an existing YAML "
               << "file with --gps_yaml.";
    return common::kStupidUserError;
  }

  std::string selected_map_key;
  if (!getSelectedMapKeyIfSet(&selected_map_key)) {
    return common::kStupidUserError;
  }

  vi_map::VIMapManager map_manager;
  vi_map::VIMapManager::MapWriteAccess map =
      map_manager.getMapWriteAccess(selected_map_key);

  vi_map::MissionIdList mission_ids;
  map->getAllMissionIds(&mission_ids);
  if (mission_ids.empty()) {
    LOG(ERROR) << "The loaded map does not contain any missions yet.";
    return common::kUnknownError;
  }

  vi_map::MissionId mission_id;
  CHECK(!mission_id.isValid());
  if (mission_ids.size() == 1u) {
    mission_id = mission_ids.front();
  } else {
    if (FLAGS_map_mission.empty()) {
      LOG(ERROR)
          << "There are more than 1 mission present in the loaded map. "
          << "Please specify the mission to which you want to add the GPS "
          << "data with --map_mission.";
      return common::kStupidUserError;
    }
    map->ensureMissionIdValid(FLAGS_map_mission, &mission_id);
  }
  CHECK(mission_id.isValid());
  data_import_export::importGpsDataFromRosbag(
      bag_file, gps_topic, gps_yaml, mission_id, map.get());

  return common::kSuccess;
}

int DataImportExportPlugin::exportGpsUtmToCsv() const {
  const std::string kCsvFilename = "gps_utm_measurements.csv";

  std::string selected_map_key;
  if (!getSelectedMapKeyIfSet(&selected_map_key)) {
    return common::kStupidUserError;
  }

  vi_map::VIMapManager map_manager;
  vi_map::VIMapManager::MapReadAccess map =
      map_manager.getMapReadAccess(selected_map_key);

  exportGpsDataMatchedToVerticesToCsv<vi_map::GpsUtmMeasurement>(
      *map, kCsvFilename);

  return common::kSuccess;
}

int DataImportExportPlugin::exportGpsWgsToCsv() const {
  const std::string kCsvFilename = "gps_wgs_measurements.csv";

  std::string selected_map_key;
  if (!getSelectedMapKeyIfSet(&selected_map_key)) {
    return common::kStupidUserError;
  }

  vi_map::VIMapManager map_manager;
  vi_map::VIMapManager::MapReadAccess map =
      map_manager.getMapReadAccess(selected_map_key);

  exportGpsDataMatchedToVerticesToCsv<vi_map::GpsWgsMeasurement>(
      *map, kCsvFilename);
  return common::kSuccess;
}

}  // namespace data_import_export

MAPLAB_CREATE_CONSOLE_PLUGIN(data_import_export::DataImportExportPlugin);
