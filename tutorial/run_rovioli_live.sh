#!/usr/bin/env bash
NCAMERA=$1
IMU=$2
WHEELS=$3
MAP_LOC=$4
MAP_OUTPUT=$5
REST=$@

rosrun rovioli rovioli \
  --alsologtostderr=1 \
  --v=1 \
  --ncamera_calibration=$NCAMERA  \
  --imu_parameters_maplab=$IMU \
  --wheel_parameters=$WHEELS \
  --publish_debug_markers \
  --datasource_type="rostopic" \
  --map_builder_save_image_as_resources=false \
  --optimize_map_to_localization_map=false \
  --save_map_folder=$MAP_OUTPUT \
  --overwrite_existing_map=true \
  --use_wheel_odometry=true \
  --vio_localization_map_folder=$MAP_LOC \
  $REST
