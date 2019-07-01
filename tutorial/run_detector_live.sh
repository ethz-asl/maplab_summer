#!/usr/bin/env bash
CFG=$1
WEIGHTS=$2
NCAMERA=$3
REST=$@

rosrun detector detector \
  --ncamera_calibration=$NCAMERA  \
  --yolo_cfg=$CFG \
  --yolo_weights=$WEIGHTS \
  $REST
