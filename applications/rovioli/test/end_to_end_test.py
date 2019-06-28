#!/usr/bin/env python

import os

import numpy as np

# Force matplotlib to not use any Xwindows backend. Needs to be imported before
# any other matplotlib import (time_alignment and end_to_end_test import
# matplotlib).
import matplotlib
matplotlib.use('Agg')

from end_to_end_common.dataset_loader import load_dataset
from end_to_end_common.create_plots import plot_position_error
import end_to_end_common.download_helpers
import end_to_end_common.end_to_end_test
import end_to_end_common.bash_utils

# If set to false, Rovioli isn't run in a given case. A previous CSV file with
# the estimated trajectory has to exist in that case.
# VIO = Rovioli without localization (i.e., rovio)
# VIL = Rovioli with localization
RUN_ESTIMATOR_VIO = True
RUN_ESTIMATOR_VIL = True

VIO_MAX_POSITION_RMSE_M = 0.08
VIL_MAX_POSITION_RMSE_M = 0.05
VIO_MAX_ORIENTATION_RMSE_RAD = 0.07
VIL_MAX_ORIENTATION_RMSE_RAD = VIO_MAX_ORIENTATION_RMSE_RAD


def test_rovioli_end_to_end():
    config_dir = os.environ["ROVIO_CONFIG_DIR"]

    rosbag_local_path = "dataset.bag"
    download_url = \
            "http://robotics.ethz.ch/~asl-datasets/maplab/test_data/end_to_end_tests/V1_01_short.bag" # pylint: disable=line-too-long
    end_to_end_common.download_helpers.download_dataset(
        download_url, rosbag_local_path)
    ground_truth_data_path = "end_to_end_test/V1_01_easy_ground_truth.csv"

    end_to_end_test = end_to_end_common.end_to_end_test.EndToEndTest()

    vio_max_allowed_errors = end_to_end_common.end_to_end_test.TestErrorStruct(
        VIO_MAX_POSITION_RMSE_M, VIO_MAX_POSITION_RMSE_M,
        VIO_MAX_ORIENTATION_RMSE_RAD, VIO_MAX_ORIENTATION_RMSE_RAD)
    vil_max_allowed_errors = end_to_end_common.end_to_end_test.TestErrorStruct(
        VIL_MAX_POSITION_RMSE_M, VIL_MAX_POSITION_RMSE_M,
        VIL_MAX_ORIENTATION_RMSE_RAD, VIL_MAX_ORIENTATION_RMSE_RAD)

    # Run estimator VIO only mode.
    estimator_vio_csv_path = "rovioli_estimated_poses_vio.csv"

    if RUN_ESTIMATOR_VIO:
        end_to_end_common.bash_utils.run(
            "rosrun rovioli rovioli "
            "    --alsologtostderr"
            "    --ncamera_calibration=\"%s\""
            "    --imu_parameters_maplab=\"%s\""
            "    --datasource_type=rosbag"
            "    --datasource_rosbag=\"%s\""
            "    --export_estimated_poses_to_csv=\"%s\""
            "    --rovioli_zero_initial_timestamps=false"
            "    --rovio_enable_frame_visualization=false" %
            (config_dir + "/ncamera-euroc.yaml",
             config_dir + "/imu-adis16488.yaml", rosbag_local_path,
             estimator_vio_csv_path))

    # Compare estimator csv - ground truth data.
    estimator_data_vio_unaligned_G_I = load_dataset(
        estimator_vio_csv_path, input_format="rovioli")
    ground_truth_data_unaligned_W_M = load_dataset(
        ground_truth_data_path, input_format="euroc_ground_truth")
    vio_errors = end_to_end_test.calculate_errors_of_datasets(
        "VIO",
        estimator_data_vio_unaligned_G_I,
        ground_truth_data_unaligned_W_M,
        estimate_scale=False,
        max_errors_list=[vio_max_allowed_errors])

    # Run estimator VIL mode.
    estimator_vil_csv_path = "rovioli_estimated_poses_vil.csv"
    localization_reference_map = "V1_01_summary_map"
    if not os.path.isdir(localization_reference_map):
        os.mkdir(localization_reference_map)
    localization_reference_map_download_path = \
            "http://robotics.ethz.ch/~asl-datasets/maplab/test_data/end_to_end_tests/V1_01_summary_map" # pylint: disable=line-too-long
    end_to_end_common.download_helpers.download_dataset(
        localization_reference_map_download_path,
        os.path.join(localization_reference_map, "localization_summary_map"))

    if RUN_ESTIMATOR_VIL:
        end_to_end_common.bash_utils.run(
            "rosrun rovioli rovioli"
            "    --alsologtostderr"
            "    --ncamera_calibration=\"%s\""
            "    --imu_parameters_maplab=\"%s\""
            "    --datasource_type=rosbag"
            "    --datasource_rosbag=\"%s\""
            "    --export_estimated_poses_to_csv=\"%s\""
            "    --vio_localization_map_folder=\"%s\""
            "    --rovioli_zero_initial_timestamps=false"
            "    --rovio_enable_frame_visualization=false" %
            (config_dir + "/ncamera-euroc.yaml",
             config_dir + "/imu-adis16488.yaml", rosbag_local_path,
             estimator_vil_csv_path, localization_reference_map))

    # Ensure that VIL position errors are smaller than the VIO position errors.
    # For the orientation, we don't want to enforce such a requirement as the
    # differences are usually smaller.
    vio_position_errors = end_to_end_common.end_to_end_test.TestErrorStruct(
        vio_errors.position_mean_m, vio_errors.position_rmse_m,
        VIO_MAX_ORIENTATION_RMSE_RAD, VIO_MAX_ORIENTATION_RMSE_RAD)

    # Get localizations for VIL case.
    vil_localizations = np.genfromtxt(
        estimator_vil_csv_path, delimiter=",", skip_header=1)
    vil_localizations = vil_localizations[:, [0, 15]]
    vil_localizations = vil_localizations[vil_localizations[:, 1] == 1, :]

    estimator_data_vil_unaligned_G_I = load_dataset(
        estimator_vil_csv_path, input_format="rovioli")
    vil_errors = end_to_end_test.calculate_errors_of_datasets(
        "VIL",
        estimator_data_vil_unaligned_G_I,
        ground_truth_data_unaligned_W_M,
        estimate_scale=False,
        max_errors_list=[vil_max_allowed_errors, vio_position_errors],
        localization_state_list=vil_localizations)

    # Get VIWLS errors.
    viwls_csv_path = "end_to_end_test/V1_01_easy_short_viwls_vertices.csv"
    vil_position_errors = end_to_end_common.end_to_end_test.TestErrorStruct(
        vil_errors.position_mean_m, vil_errors.position_rmse_m,
        VIL_MAX_ORIENTATION_RMSE_RAD, VIL_MAX_ORIENTATION_RMSE_RAD)
    estimator_data_viwls_unaligned_G_I = load_dataset(viwls_csv_path)
    end_to_end_test.calculate_errors_of_datasets(
        "VIWLS",
        estimator_data_viwls_unaligned_G_I,
        ground_truth_data_unaligned_W_M,
        estimate_scale=False,
        max_errors_list=[
            vio_max_allowed_errors, vio_position_errors, vil_position_errors
        ])

    end_to_end_test.print_errors()
    end_to_end_test.check_errors()

    plot_position_error(end_to_end_test.test_results)


if __name__ == '__main__':
    test_rovioli_end_to_end()
