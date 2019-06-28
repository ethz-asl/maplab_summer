#include <ceres/ceres.h>
#include <ceres/gradient_checker.h>
#include <vector>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include <aslam/cameras/camera-pinhole.h>
#include <aslam/cameras/distortion-fisheye.h>
#include <maplab-common/pose_types.h>
#include <maplab-common/test/testing-entrypoint.h>
#include <maplab-common/test/testing-predicates.h>

#include <ceres-error-terms/anchored-inverse-depth-error-term.h>
#include <ceres-error-terms/anchored-inverse-depth-helpers.h>
#include <ceres-error-terms/parameterization/unit3-param.h>
#include <ceres-error-terms/visual-error-term.h>

using ceres_error_terms::VisualReprojectionError;

class PosegraphErrorTerms : public ::testing::Test {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

 protected:
  typedef aslam::FisheyeDistortion DistortionType;
  typedef aslam::PinholeCamera CameraType;

  virtual void SetUp() {
    zero_position_.setZero();
    unit_quaternion_.setIdentity();

    distortion_param_ = 0.0;
    fu_ = 1;
    fv_ = 1;
    res_u_ = 640;
    res_v_ = 480;
    cu_ = res_u_ / 2.0;
    cv_ = res_v_ / 2.0;

    pixel_sigma_ = 0.7;

    // Set unit rotation and zero translation to dummy pose objects.
    dummy_7d_0_ << 0, 0, 0, 1, 0, 0, 0;
    dummy_7d_1_ << 0, 0, 0, 1, 0, 0, 0;
  }

  void constructCamera() {
    Eigen::VectorXd distortion_parameters(1);
    distortion_parameters << distortion_param_;
    aslam::Distortion::UniquePtr distortion(
        new DistortionType(distortion_parameters));

    Eigen::VectorXd intrinsics(4);
    intrinsics << fu_, fv_, cu_, cv_;

    camera_.reset(new CameraType(intrinsics, res_u_, res_v_, distortion));
  }

  void solveProblem();
  void addResidual(
      const Eigen::Vector2d& measurement, double pixel_sigma,
      double* anchor_position, double* A_u_AL, double* inverse_depth_A,
      double* q_M_I, double* M_p_M_I, double* camera_to_imu_orientation,
      double* camera_to_imu_position);

  ceres::Problem problem_;
  ceres::Solver::Summary summary_;
  ceres::Solver::Options options_;

  std::shared_ptr<CameraType> camera_;

  Eigen::Vector3d zero_position_;
  Eigen::Quaterniond unit_quaternion_;

  double distortion_param_;
  double fu_, fv_;
  double cu_, cv_;
  double res_u_, res_v_;
  double pixel_sigma_;

  // Ordering is [orientation position] -> [xyzw xyz].
  Eigen::Matrix<double, 7, 1> dummy_7d_0_;
  Eigen::Matrix<double, 7, 1> dummy_7d_1_;
};

void PosegraphErrorTerms::addResidual(
    const Eigen::Vector2d& measurement, double pixel_sigma,
    double* anchor_position, double* A_u_AL, double* inverse_depth_A,
    double* q_M_I, double* M_p_M_I, double* camera_to_imu_orientation,
    double* camera_to_imu_position) {
  ceres::CostFunction* cost_function =
      new ceres_error_terms::AIDReprojectionError<
          CameraType, DistortionType,
          ceres_error_terms::visual::VisualErrorType::kLocalMission>(
          measurement, pixel_sigma, camera_.get());

  problem_.AddResidualBlock(
      cost_function, NULL, anchor_position, A_u_AL, inverse_depth_A, q_M_I,
      M_p_M_I, camera_to_imu_orientation, camera_to_imu_position,
      camera_->getParametersMutable(),
      camera_->getDistortionMutable()->getParametersMutable());

  ceres::LocalParameterization* quaternion_parameterization =
      new ceres_error_terms::EigenQuaternionParameterization;

  ceres::LocalParameterization* unit3_parameterization =
      new ceres_error_terms::Unit3Parameterization;

  if (problem_.GetParameterization(q_M_I) == NULL) {
    problem_.SetParameterization(q_M_I, quaternion_parameterization);
  }
  if (problem_.GetParameterization(A_u_AL) == NULL) {
    problem_.SetParameterization(A_u_AL, unit3_parameterization);
  }

  if (problem_.GetParameterization(camera_to_imu_orientation) == NULL) {
    problem_.SetParameterization(
        camera_to_imu_orientation, quaternion_parameterization);
  }
}

void PosegraphErrorTerms::solveProblem() {
  options_.linear_solver_type = ceres::DENSE_SCHUR;
  options_.minimizer_progress_to_stdout = true;
  options_.max_num_iterations = 1e4;
  options_.parameter_tolerance = 1e-50;
  options_.gradient_tolerance = 1e-50;
  options_.function_tolerance = 1e-50;
  ceres::Solve(options_, &problem_, &summary_);

  LOG(INFO) << summary_.BriefReport();
}

// This test verifies if starting from a ground-truth inital value will make
// the optimizer immediately stop with zero cost.
TEST_F(PosegraphErrorTerms, VisualErrorTermOnePointOneCamera) {
  Eigen::Vector2d keypoint(0, 0);

  Eigen::Vector3d landmark_base_position(0, 0, -1);
  Eigen::Quaterniond landmark_base_orientation(1, 0, 0, 0);
  Eigen::Vector3d imu_position(0, 0, -1);
  Eigen::Quaterniond imu_orientation(1, 0, 0, 0);
  Eigen::Vector3d landmark_position(0, 0, 1);
  Eigen::Quaterniond q_CI(1, 0, 0, 0);

  Eigen::Quaterniond A_u_AL;
  double A_inverse_depth;
  ceres_error_terms::aid_helpers::Euclidean2AID(
      landmark_position, landmark_base_position, &A_u_AL, &A_inverse_depth);

  Eigen::Vector4d intrinsics;
  intrinsics << 1, 1, 0, 0;
  constructCamera();
  camera_->setParameters(intrinsics);

  addResidual(
      keypoint, pixel_sigma_, landmark_base_position.data(),
      A_u_AL.coeffs().data(), &A_inverse_depth, imu_orientation.coeffs().data(),
      imu_position.data(), q_CI.coeffs().data(), zero_position_.data());
  solveProblem();

  EXPECT_EQ(summary_.final_cost, 0.0);
  EXPECT_EQ(summary_.iterations.size(), 1u);
  EXPECT_NEAR_EIGEN(landmark_base_position, Eigen::Vector3d(0, 0, -1), 1e-15);
  EXPECT_NEAR_EIGEN(imu_position, Eigen::Vector3d(0, 0, -1), 1e-15);
  EXPECT_NEAR_EIGEN(landmark_position, Eigen::Vector3d(0, 0, 1), 1e-15);
}

// The test verifies if a simple problem where the position of a landmark
// gets optimized if seen by 2 cameras.
TEST_F(PosegraphErrorTerms, VisualErrorTermOnePointTwoCamerasNoisy) {
  Eigen::Vector3d anchor_point(0, 0, -2);

  Eigen::Vector3d landmark_position(0.2, -0.1, 0.05);
  // Eigen::Vector3d landmark_position(0, 0, 0);

  Eigen::Vector4d intrinsics;
  intrinsics << 1, 1, cu_, cv_;
  constructCamera();
  camera_->setParameters(intrinsics);

  Eigen::Vector2d camera0_keypoint(cu_, cv_);
  Eigen::Vector3d camera0_position(0, 0, -2);
  Eigen::Quaterniond camera0_orientation(1, 0, 0, 0);

  Eigen::Vector2d camera1_keypoint(cu_, cv_);
  Eigen::Vector3d camera1_position(2, 0, 0);
  Eigen::Quaterniond camera1_orientation(-sqrt(2) / 2, 0, sqrt(2) / 2, 0);

  Eigen::Quaterniond A_u_AL;
  double A_inverse_depth;
  ceres_error_terms::aid_helpers::Euclidean2AID(
      landmark_position, anchor_point, &A_u_AL, &A_inverse_depth);

  addResidual(
      camera0_keypoint, pixel_sigma_, anchor_point.data(),
      A_u_AL.coeffs().data(), &A_inverse_depth,
      camera0_orientation.coeffs().data(), camera0_position.data(),
      unit_quaternion_.coeffs().data(), zero_position_.data());

  addResidual(
      camera1_keypoint, pixel_sigma_, anchor_point.data(),
      A_u_AL.coeffs().data(), &A_inverse_depth,
      camera1_orientation.coeffs().data(), camera1_position.data(),
      unit_quaternion_.coeffs().data(), zero_position_.data());

  problem_.SetParameterBlockConstant(camera0_position.data());
  problem_.SetParameterBlockConstant(camera1_position.data());
  problem_.SetParameterBlockConstant(camera0_orientation.coeffs().data());
  problem_.SetParameterBlockConstant(camera1_orientation.coeffs().data());
  problem_.SetParameterBlockConstant(anchor_point.data());
  problem_.SetParameterBlockConstant(camera_->getParametersMutable());
  problem_.SetParameterBlockConstant(
      camera_->getDistortionMutable()->getParametersMutable());
  problem_.SetParameterBlockConstant(unit_quaternion_.coeffs().data());
  problem_.SetParameterBlockConstant(zero_position_.data());

  solveProblem();

  ceres_error_terms::aid_helpers::AID2Euclidean(
      anchor_point, A_u_AL, A_inverse_depth, &landmark_position);

  EXPECT_LT(summary_.final_cost, 1e-15);
  EXPECT_ZERO_EIGEN(landmark_position, 1e-10);
}

// The test verifies if a simple problem where the position of the anchor
// gets optimized if seen by 2 cameras.
TEST_F(PosegraphErrorTerms, VisualErrorTermOnePointTwoCamerasNoisyAnchor) {
  Eigen::Vector3d anchor_point_gt(0, 0, -2);
  Eigen::Vector3d anchor_point(0.1, 0.3, -2.5);

  Eigen::Vector3d landmark_position(0, 0, 0);

  Eigen::Vector4d intrinsics;
  intrinsics << 1, 1, cu_, cv_;
  constructCamera();
  camera_->setParameters(intrinsics);

  Eigen::Vector2d camera0_keypoint(cu_, cv_);
  Eigen::Vector3d camera0_position(0, 0, -2);
  Eigen::Quaterniond camera0_orientation(1, 0, 0, 0);

  Eigen::Vector2d camera1_keypoint(cu_, cv_);
  Eigen::Vector3d camera1_position(2, 0, 0);
  Eigen::Quaterniond camera1_orientation(-sqrt(2) / 2, 0, sqrt(2) / 2, 0);

  Eigen::Quaterniond A_u_AL;
  double A_inverse_depth;
  ceres_error_terms::aid_helpers::Euclidean2AID(
      landmark_position, anchor_point_gt, &A_u_AL, &A_inverse_depth);

  addResidual(
      camera0_keypoint, pixel_sigma_, anchor_point.data(),
      A_u_AL.coeffs().data(), &A_inverse_depth,
      camera0_orientation.coeffs().data(), camera0_position.data(),
      unit_quaternion_.coeffs().data(), zero_position_.data());

  addResidual(
      camera1_keypoint, pixel_sigma_, anchor_point.data(),
      A_u_AL.coeffs().data(), &A_inverse_depth,
      camera1_orientation.coeffs().data(), camera1_position.data(),
      unit_quaternion_.coeffs().data(), zero_position_.data());

  problem_.SetParameterBlockConstant(camera0_position.data());
  problem_.SetParameterBlockConstant(camera1_position.data());
  problem_.SetParameterBlockConstant(camera0_orientation.coeffs().data());
  problem_.SetParameterBlockConstant(camera1_orientation.coeffs().data());
  problem_.SetParameterBlockConstant(A_u_AL.coeffs().data());
  problem_.SetParameterBlockConstant(&A_inverse_depth);
  problem_.SetParameterBlockConstant(camera_->getParametersMutable());
  problem_.SetParameterBlockConstant(
      camera_->getDistortionMutable()->getParametersMutable());
  problem_.SetParameterBlockConstant(unit_quaternion_.coeffs().data());
  problem_.SetParameterBlockConstant(zero_position_.data());

  solveProblem();

  EXPECT_LT(summary_.final_cost, 1e-15);
  EXPECT_NEAR_EIGEN(anchor_point, anchor_point_gt, 1e-5);
}

// This test verifies if pinhole camera intrinsic parameters will be
// properly optimized to the ground-truth values.
TEST_F(PosegraphErrorTerms, VisualErrorTermIntrinsicsOptimization) {
  Eigen::Vector3d landmark_position(0, 0, 0);

  constructCamera();
  Eigen::Map<Eigen::Vector4d> intrinsics(camera_->getParametersMutable());
  fu_ = 100;
  fv_ = 100;
  intrinsics << fu_ + 18, fv_ - 90, cu_ - 10, cv_ + 13;

  Eigen::Vector2d camera0_keypoint(cu_ + 100, cv_ - 50);
  Eigen::Vector3d camera0_position(-1, 0.5, -1);
  Eigen::Quaterniond camera0_orientation(1, 0, 0, 0);

  Eigen::Vector2d camera1_keypoint(cu_ - 120, cv_ + 30);
  Eigen::Vector3d camera1_position(1, -0.3, 1.2);
  Eigen::Quaterniond camera1_orientation(-sqrt(2) / 2, 0, sqrt(2) / 2, 0);

  Eigen::Vector3d anchor_point =
      camera0_position;  // Set anchor to first Camera.

  Eigen::Quaterniond A_u_AL;
  double A_inverse_depth;
  ceres_error_terms::aid_helpers::Euclidean2AID(
      landmark_position, anchor_point, &A_u_AL, &A_inverse_depth);

  addResidual(
      camera0_keypoint, pixel_sigma_, anchor_point.data(),
      A_u_AL.coeffs().data(), &A_inverse_depth,
      camera0_orientation.coeffs().data(), camera0_position.data(),
      unit_quaternion_.coeffs().data(), zero_position_.data());

  addResidual(
      camera1_keypoint, pixel_sigma_, anchor_point.data(),
      A_u_AL.coeffs().data(), &A_inverse_depth,
      camera1_orientation.coeffs().data(), camera1_position.data(),
      unit_quaternion_.coeffs().data(), zero_position_.data());

  problem_.SetParameterBlockConstant(camera0_orientation.coeffs().data());
  problem_.SetParameterBlockConstant(camera0_position.data());
  problem_.SetParameterBlockConstant(camera1_orientation.coeffs().data());
  problem_.SetParameterBlockConstant(camera1_position.data());
  problem_.SetParameterBlockConstant(anchor_point.data());
  problem_.SetParameterBlockConstant(A_u_AL.coeffs().data());
  problem_.SetParameterBlockConstant(&A_inverse_depth);
  problem_.SetParameterBlockConstant(
      camera_->getDistortionMutable()->getParametersMutable());
  problem_.SetParameterBlockConstant(unit_quaternion_.coeffs().data());
  problem_.SetParameterBlockConstant(zero_position_.data());

  solveProblem();

  EXPECT_LT(summary_.final_cost, 1e-15);
  EXPECT_NEAR_EIGEN(intrinsics, Eigen::Vector4d(fu_, fv_, cu_, cv_), 1e-5);
}

// This test verifies if distortion parameter of fisheye distortion model
// will
// be properly optimized to the ground-truth value. The reference projected
// keypoint coordinates were generated using Matlab script.
TEST_F(PosegraphErrorTerms, VisualErrorTermNonZeroDistortionOptimization) {
  Eigen::Vector3d landmark_position(0, 0, 0);

  distortion_param_ = 0.95;
  constructCamera();
  Eigen::Map<Eigen::Vector4d> intrinsics(camera_->getParametersMutable());
  intrinsics << 100, 100, cu_, cv_;

  // values generated  in Matlab using distortion param w = 1.0
  Eigen::Vector2d camera0_keypoint(399.139364932153, 200.430317533923);
  Eigen::Vector3d camera0_position(-1, 0.5, -1);
  Eigen::Quaterniond camera0_orientation(1, 0, 0, 0);

  Eigen::Vector3d anchor_point =
      camera0_position;  // Set anchor to first Camera.

  Eigen::Quaterniond A_u_AL;
  double A_inverse_depth;
  ceres_error_terms::aid_helpers::Euclidean2AID(
      landmark_position, anchor_point, &A_u_AL, &A_inverse_depth);

  addResidual(
      camera0_keypoint, pixel_sigma_, anchor_point.data(),
      A_u_AL.coeffs().data(), &A_inverse_depth,
      camera0_orientation.coeffs().data(), camera0_position.data(),
      unit_quaternion_.coeffs().data(), zero_position_.data());

  problem_.SetParameterBlockConstant(camera0_orientation.coeffs().data());
  problem_.SetParameterBlockConstant(camera0_position.data());
  problem_.SetParameterBlockConstant(anchor_point.data());
  problem_.SetParameterBlockConstant(A_u_AL.coeffs().data());
  problem_.SetParameterBlockConstant(&A_inverse_depth);
  problem_.SetParameterBlockConstant(camera_->getParametersMutable());
  problem_.SetParameterBlockConstant(unit_quaternion_.coeffs().data());
  problem_.SetParameterBlockConstant(zero_position_.data());

  solveProblem();

  EXPECT_LT(summary_.final_cost, 1e-15);
  EXPECT_NEAR(
      *(camera_->getDistortionMutable()->getParametersMutable()), 1.0, 1e-5);
}

// TODO(burrimi): Fix missing unit tests.

//// This test verifies if IMU pose will get properly optimized starting
//// from noisy initial values.
// TEST_F(PosegraphErrorTerms, VisualErrorTermImuPoseOptimization) {
//  Eigen::Vector3d landmark_base_position(0, 0, 0);
//  Eigen::Quaterniond landmark_base_orientation(1, 0, 0, 0);
//  Eigen::Matrix<double, 7, 1> landmark_base_pose;
//  landmark_base_pose << landmark_base_orientation.coeffs(),
//      landmark_base_position;
//
//  const double landmark_offset = 0.5;
//  Eigen::Matrix3d landmark_positions;
//  landmark_positions.col(0) = Eigen::Vector3d(-landmark_offset, 0, 0);
//  landmark_positions.col(1) = Eigen::Vector3d(landmark_offset, 0, 0);
//  landmark_positions.col(2) = Eigen::Vector3d(0, landmark_offset, 0);
//
//  constructCamera();
//  Eigen::Map<Eigen::Vector4d> intrinsics(camera_->getParametersMutable());
//  const double focal_length = 100;
//  intrinsics << focal_length, focal_length, cu_, cv_;
//
//  Eigen::Matrix<double, 2, 3> camera_keypoints;
//  camera_keypoints.col(0) =
//      Eigen::Vector2d(-focal_length * landmark_offset + cu_, cv_);
//  camera_keypoints.col(1) =
//      Eigen::Vector2d(focal_length * landmark_offset + cu_, cv_);
//  camera_keypoints.col(2) =
//      Eigen::Vector2d(cu_, focal_length * landmark_offset + cv_);
//
//  Eigen::Vector3d camera0_position(0.2, 2.1, -1.5);
//  // ~5deg in each axis
//  Eigen::Quaterniond camera0_orientation(
//      0.9978051316080664, 0.024225143749034013, 0.04470367401201076,
//      0.04242220263937102);
//
//
//  for (unsigned int i = 0; i < camera_keypoints.cols(); ++i) {
//    addResidual(camera_keypoints.col(i), pixel_sigma_,
//                landmark_base_pose.data(), camera0_pose.data(),
//                unit_quaternion_.coeffs().data(), zero_position_.data(),
//                landmark_positions.col(i).data());
//
//    problem_.SetParameterBlockConstant(landmark_positions.col(i).data());
//  }
//
//  problem_.SetParameterBlockConstant(landmark_base_pose.data());
//  problem_.SetParameterBlockConstant(camera_->getParametersMutable());
//  problem_.SetParameterBlockConstant(
//      camera_->getDistortionMutable()->getParametersMutable());
//  problem_.SetParameterBlockConstant(unit_quaternion_.coeffs().data());
//  problem_.SetParameterBlockConstant(zero_position_.data());
//
//  solveProblem();
//
//  EXPECT_LT(summary_.final_cost, 1e-15);
//  EXPECT_NEAR_EIGEN(camera0_position, Eigen::Vector3d(0, 0, -1), 1e-4);
//  EXPECT_ZERO_EIGEN(camera0_orientation.vec(), 1e-5);
// }

// // This test verifies if noisy initial Camera to IMU transformation will get
// // properly optimized to ground-truth values. Note that reference rotation is
// // non-zero.
// TEST_F(PosegraphErrorTerms, VisualErrorTermCamToImuPoseOptimization) {
//  Eigen::Vector3d landmark_base_position(0, 0, 0);
//  Eigen::Quaterniond landmark_base_orientation(1, 0, 0, 0);
//  Eigen::Matrix<double, 7, 1> landmark_base_pose;
//  landmark_base_pose << landmark_base_orientation.coeffs(),
//      landmark_base_position;
//
//  const double landmark_offset = 0.5;
//  Eigen::Matrix3d landmark_positions;
//  landmark_positions.col(0) = Eigen::Vector3d(0, 0, -landmark_offset);
//  landmark_positions.col(1) = Eigen::Vector3d(0, 0, landmark_offset);
//  landmark_positions.col(2) = Eigen::Vector3d(0, landmark_offset, 0);
//
//  constructCamera();
//  Eigen::Map<Eigen::Vector4d> intrinsics(camera_->getParametersMutable());
//  const double focal_length = 100;
//  intrinsics << focal_length, focal_length, cu_, cv_;
//
//  Eigen::Matrix<double, 2, 3> camera_keypoints;
//  camera_keypoints.col(0) =
//      Eigen::Vector2d(-focal_length * landmark_offset + cu_, cv_);
//  camera_keypoints.col(1) =
//      Eigen::Vector2d(focal_length * landmark_offset + cu_, cv_);
//  camera_keypoints.col(2) =
//      Eigen::Vector2d(cu_, focal_length * landmark_offset + cv_);
//
//  Eigen::Quaterniond cam_to_imu_rot(0.6882920059910812, 0.013627671275108364,
//                                    -0.7236496655350155, -0.0489853308190429);
//  Eigen::Vector3d cam_to_imu_pos(0.2, 0.1, -0.3);
//
//  Eigen::Vector3d camera0_position(1, 0, 0);
//  Eigen::Quaterniond camera0_orientation(1, 0, 0, 0);
//  Eigen::Matrix<double, 7, 1> camera0_pose;
//  camera0_pose << camera0_orientation.coeffs(), camera0_position;
//
//  for (unsigned int i = 0; i < camera_keypoints.cols(); ++i) {
//    addResidual(camera_keypoints.col(i), pixel_sigma_,
//                landmark_base_pose.data(), camera0_pose.data(),
//                cam_to_imu_rot.coeffs().data(), cam_to_imu_pos.data(),
//                landmark_positions.col(i).data());
//
//    problem_.SetParameterBlockConstant(landmark_positions.col(i).data());
//  }
//
//  problem_.SetParameterBlockConstant(camera0_pose.data());
//  problem_.SetParameterBlockConstant(landmark_base_pose.data());
//  problem_.SetParameterBlockConstant(camera_->getParametersMutable());
//  problem_.SetParameterBlockConstant(
//      camera_->getDistortionMutable()->getParametersMutable());
//  problem_.SetParameterBlockConstant(camera0_pose.data());
//
//  solveProblem();
//
//  EXPECT_LT(summary_.final_cost, 1e-15);
//  EXPECT_ZERO_EIGEN(cam_to_imu_pos, 1e-10);
//  EXPECT_NEAR_KINDR_QUATERNION(
//      pose::Quaternion(cam_to_imu_rot),
//      pose::Quaternion(sqrt(2) / 2, 0, -sqrt(2) / 2, 0), 1e-15);
// }
//
// // This test verifies if a non-zero landmark base pose, including noisy
// // initial rotation, will get optimized to the ground-truth values. The test
// // can be used to check if the coordinate transformations are right,
// // if Jacobians w.r.t. landmark base pose are right and finally if quaternion
// // Parameterization is properly formulated.
// TEST_F(PosegraphErrorTerms, VisualErrorTermNonzeroLandmarkBasePose) {
//  Eigen::Vector3d landmark_base_position(0, 0, 0.5);
//  Eigen::Quaterniond landmark_base_orientation(
//      0.6882920059910812, 0.013627671275108364, -0.7236496655350155,
//      -0.0489853308190429);
//  landmark_base_orientation.normalize();
//  Eigen::Matrix<double, 7, 1> landmark_base_pose;
//  landmark_base_pose << landmark_base_orientation.coeffs(),
//      landmark_base_position;
//
//  Eigen::Vector3d landmark0_position(99.5, 1, 1);
//  Eigen::Vector3d landmark1_position(0, 1, 1);
//  Eigen::Vector3d landmark2_position(4.5, 0, 2);
//
//  constructCamera();
//  Eigen::Map<Eigen::Vector4d> intrinsics(camera_->getParametersMutable());
//  intrinsics << 100, 100, cu_, cv_;
//
//  Eigen::Vector2d camera0_keypoint0(cu_, cv_);
//  Eigen::Vector2d camera0_keypoint1(cu_, cv_);
//  Eigen::Vector2d camera0_keypoint2(cu_ - 20, cv_ - 20);
//  Eigen::Vector3d camera0_position(-1, 1, 0);
//  Eigen::Quaterniond camera0_orientation(1, 0, 0, 0);
//  Eigen::Matrix<double, 7, 1> camera0_pose;
//  camera0_pose << camera0_orientation.coeffs(), camera0_position;
//
//  Eigen::Vector2d camera1_keypoint0(cu_, cv_);
//  Eigen::Vector3d camera1_position(-20, 1, 100);
//  Eigen::Quaterniond camera1_orientation(sqrt(2) / 2, 0, sqrt(2) / 2, 0);
//  Eigen::Matrix<double, 7, 1> camera1_pose;
//  camera1_pose << camera1_orientation.coeffs(), camera1_position;
//
//  addResidual(camera0_keypoint0, pixel_sigma_, landmark_base_pose.data(),
//              camera0_pose.data(), unit_quaternion_.coeffs().data(),
//              zero_position_.data(), landmark0_position.data());
//
//  addResidual(camera0_keypoint1, pixel_sigma_, landmark_base_pose.data(),
//              camera0_pose.data(), unit_quaternion_.coeffs().data(),
//              zero_position_.data(), landmark1_position.data());
//
//  addResidual(camera0_keypoint2, pixel_sigma_, landmark_base_pose.data(),
//              camera0_pose.data(), unit_quaternion_.coeffs().data(),
//              zero_position_.data(), landmark2_position.data());
//
//  addResidual(camera1_keypoint0, pixel_sigma_, landmark_base_pose.data(),
//              camera1_pose.data(), unit_quaternion_.coeffs().data(),
//              zero_position_.data(), landmark0_position.data());
//
//  problem_.SetParameterBlockConstant(camera0_pose.data());
//  problem_.SetParameterBlockConstant(camera1_pose.data());
//  problem_.SetParameterBlockConstant(landmark0_position.data());
//  problem_.SetParameterBlockConstant(landmark1_position.data());
//  problem_.SetParameterBlockConstant(landmark2_position.data());
//  problem_.SetParameterBlockConstant(camera_->getParametersMutable());
//  problem_.SetParameterBlockConstant(
//      camera_->getDistortionMutable()->getParametersMutable());
//  problem_.SetParameterBlockConstant(unit_quaternion_.coeffs().data());
//  problem_.SetParameterBlockConstant(zero_position_.data());
//
//  solveProblem();
//
//  EXPECT_LT(summary_.final_cost, 1e-15);
//  EXPECT_NEAR_KINDR_QUATERNION(
//      pose::Quaternion(landmark_base_pose.head(4).data()),
//      pose::Quaternion(sqrt(2) / 2, 0, -sqrt(2) / 2, 0), 1e-15);
//  EXPECT_NEAR(landmark_base_pose.head(4).norm(), 1.0, 1e-15);
// }
//
//
// // This test verifies if IMU pose will get properly optimized starting
// // from noisy initial values.
// TEST_F(PosegraphErrorTerms, VisualErrorTermImuPoseOptimization) {
//  Eigen::Vector3d landmark_base_position(0, 0, 0);
//  Eigen::Quaterniond landmark_base_orientation(1, 0, 0, 0);
//  Eigen::Matrix<double, 7, 1> landmark_base_pose;
//  landmark_base_pose << landmark_base_orientation.coeffs(),
//      landmark_base_position;
//
//  const double landmark_offset = 0.5;
//  Eigen::Matrix3d landmark_positions;
//  landmark_positions.col(0) = Eigen::Vector3d(-landmark_offset, 0, 0);
//  landmark_positions.col(1) = Eigen::Vector3d(landmark_offset, 0, 0);
//  landmark_positions.col(2) = Eigen::Vector3d(0, landmark_offset, 0);
//
//  constructCamera();
//  Eigen::Map<Eigen::Vector4d> intrinsics(camera_->getParametersMutable());
//  const double focal_length = 100;
//  intrinsics << focal_length, focal_length, cu_, cv_;
//
//  Eigen::Matrix<double, 2, 3> camera_keypoints;
//  camera_keypoints.col(0) =
//      Eigen::Vector2d(-focal_length * landmark_offset + cu_, cv_);
//  camera_keypoints.col(1) =
//      Eigen::Vector2d(focal_length * landmark_offset + cu_, cv_);
//  camera_keypoints.col(2) =
//      Eigen::Vector2d(cu_, focal_length * landmark_offset + cv_);
//
//  Eigen::Vector3d camera0_position(0.2, 2.1, -1.5);
//  // ~5deg in each axis
//  Eigen::Quaterniond camera0_orientation(
//      0.9978051316080664, 0.024225143749034013, 0.04470367401201076,
//      0.04242220263937102);
//  Eigen::Matrix<double, 7, 1> camera0_pose;
//  camera0_pose << camera0_orientation.coeffs(), camera0_position;
//
//  for (unsigned int i = 0; i < camera_keypoints.cols(); ++i) {
//    addResidual(camera_keypoints.col(i), pixel_sigma_,
//                landmark_base_pose.data(), camera0_pose.data(),
//                unit_quaternion_.coeffs().data(), zero_position_.data(),
//                landmark_positions.col(i).data());
//
//    problem_.SetParameterBlockConstant(landmark_positions.col(i).data());
//  }
//
//  problem_.SetParameterBlockConstant(landmark_base_pose.data());
//  problem_.SetParameterBlockConstant(camera_->getParametersMutable());
//  problem_.SetParameterBlockConstant(
//      camera_->getDistortionMutable()->getParametersMutable());
//  problem_.SetParameterBlockConstant(unit_quaternion_.coeffs().data());
//  problem_.SetParameterBlockConstant(zero_position_.data());
//
//  solveProblem();
//
//  EXPECT_LT(summary_.final_cost, 1e-15);
//  EXPECT_NEAR_EIGEN(camera0_pose.tail(3), Eigen::Vector3d(0, 0, -1), 1e-4);
//  EXPECT_ZERO_EIGEN(camera0_pose.head(3), 1e-5);
//  EXPECT_NEAR(camera0_pose.head(4).norm(), 1.0, 1e-10);
// }

// TODO(burrimi): enable this test, once ceres is updated to 1.12.
// TEST_F(PosegraphErrorTerms, TestJacobian) {
//  // Unpack parameter blocks.
//  Eigen::Vector3d M_p_M_A(1, 2, 3);
//  Eigen::Quaterniond A_u_A_L(
//      -0.9978051316080664, 0.024225143749034013, 0.04470367401201076,
//      0.04242220263937102);
//  // ceres_error_terms::Unit3::GetFromVector(Eigen::Vector3d(1, -2, 3),
//  // &A_u_A_L);
//  double inverse_depth_A = 3;
//
//  Eigen::Quaterniond q_M_I(
//      -0.9978051316080664, 0.024225143749034013, 0.04470367401201076,
//      0.04242220263937102);
//  Eigen::Vector3d M_p_M_I(-1.2, -2.5, 1.2);
//
//  Eigen::Quaterniond q_C_I(
//      0.9978051316080664, 0.024225143749034013, 0.04470367401201076,
//      0.04242220263937102);
//  Eigen::Vector3d C_p_C_I(0.2, 0.3, 0.4);
//
//  const double landmark_offset = 0.5;
//  Eigen::Vector3d landmark_position(-landmark_offset, 0, 0);
//
//  constructCamera();
//  Eigen::Map<Eigen::Vector4d> intrinsics(camera_->getParametersMutable());
//  const double focal_length = 100;
//  intrinsics << focal_length, focal_length, cu_, cv_;
//
//  Eigen::Vector2d camera_keypoint(-focal_length * landmark_offset + cu_, cv_);
//
//  const ceres::CostFunction* my_cost_function =
//      new ceres_error_terms::AIDReprojectionError<
//          CameraType, DistortionType,
//          ceres_error_terms::visual::VisualErrorType::kLocalMission>(
//          camera_keypoint, pixel_sigma_, camera_.get());
//
//  ceres::LocalParameterization* quaternion_parameterization =
//      new ceres_error_terms::EigenQuaternionParameterization;
//
//  ceres::LocalParameterization* unit3_parameterization =
//      new ceres_error_terms::Unit3Parameterization;
//
//  ceres::NumericDiffOptions numeric_diff_options;
//  numeric_diff_options.ridders_relative_initial_step_size = 1e-3;
//
//  std::vector<const ceres::LocalParameterization*> local_parameterizations;
//  local_parameterizations.push_back(NULL);
//  local_parameterizations.push_back(unit3_parameterization);
//  local_parameterizations.push_back(NULL);
//
//  local_parameterizations.push_back(quaternion_parameterization);
//  local_parameterizations.push_back(NULL);
//
//  local_parameterizations.push_back(quaternion_parameterization);
//  local_parameterizations.push_back(NULL);
//
//  local_parameterizations.push_back(NULL);
//  local_parameterizations.push_back(NULL);
//
//  std::vector<double*> parameter_blocks;
//  parameter_blocks.push_back(M_p_M_A.data());
//  parameter_blocks.push_back(A_u_A_L.coeffs().data());
//  parameter_blocks.push_back(&inverse_depth_A);
//  parameter_blocks.push_back(q_M_I.coeffs().data());
//  parameter_blocks.push_back(M_p_M_I.data());
//  parameter_blocks.push_back(q_C_I.coeffs().data());
//  parameter_blocks.push_back(C_p_C_I.data());
//  parameter_blocks.push_back(camera_->getParametersMutable());
//  parameter_blocks.push_back(
//      camera_->getDistortionMutable()->getParametersMutable());
//
//  ceres::GradientChecker gradient_checker(
//      my_cost_function, &local_parameterizations, numeric_diff_options);
//  ceres::GradientChecker::ProbeResults results;
//
//  if (!gradient_checker.Probe(parameter_blocks.data(), 1e-7, &results)) {
//    std::cout << "An error has occurred:\n" << results.error_log;
//  }
//  EXPECT_LT(results.maximum_relative_error, 1e-7);
// }

MAPLAB_UNITTEST_ENTRYPOINT
