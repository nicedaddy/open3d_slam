/*
 * Odometry.cpp
 *
 *  Created on: Oct 15, 2021
 *      Author: jelavice
 */
#include "open3d_slam/Odometry.hpp"
#include "open3d_slam/frames.hpp"
#include "open3d_slam/helpers.hpp"
#include "open3d_slam/time.hpp"
#include "open3d_slam/output.hpp"

#include <iostream>

namespace o3d_slam {


void LidarOdometryTools::setParameters(const OdometryToolsParameters &p) {
	params_ = p;
	icpConvergenceCriteria_.max_iteration_ = p.scanMatcher_.maxNumIter_;
	icpObjective_ = icpObjectiveFactory(p.scanMatcher_.icpObjective_);
//	cropper_ = std::make_shared<MaxRadiusCroppingVolume>(params_.scanProcessing_.croppingRadius_);
//	cropper_ = std::make_shared<CylinderCroppingVolume>(params_.scanProcessing_.croppingRadius_,-3.0,3.0);
	const auto &par = p.scanProcessing_.cropper_;
	cropper_ = croppingVolumeFactory(par);
}

LidarOdometry::LidarOdometry() {
	scanToScanOdomTools_.icpObjective_ = icpObjectiveFactory(IcpObjective::PointToPlane);
	scanToScanOdomTools_.cropper_ = std::make_shared<CroppingVolume>();
	mapInitializingOdomTools_.icpObjective_ = icpObjectiveFactory(IcpObjective::PointToPlane);
	mapInitializingOdomTools_.cropper_ = std::make_shared<CroppingVolume>();
}

bool LidarOdometry::addRangeScan(const open3d::geometry::PointCloud &cloud, const Time &timestamp) {
	if (cloudPrev_.IsEmpty()) {
		cloudPrev_ = cloud;
		odomToRangeSensorBuffer_.push(timestamp, odomToRangeSensorCumulative_);
		lastMeasurementTimestamp_ = timestamp;
		return true;
	}

	if (timestamp < lastMeasurementTimestamp_) {
			std::cerr << "\n\n !!!!! LIDAR ODOMETRY WARNING: Measurements came out of order!!!! \n\n";
			return false;
	}

	const o3d_slam::Timer timer;
	LidarOdometryTools &tools = isMapInitializing_ ? mapInitializingOdomTools_ : scanToScanOdomTools_;
	auto croppedCloud = tools.cropper_->crop(cloud);

	o3d_slam::voxelize(tools.params_.scanProcessing_.voxelSize_, croppedCloud.get());
	auto downSampledCloud = croppedCloud->RandomDownSample(tools.params_.scanProcessing_.downSamplingRatio_);
	
	if (tools.params_.scanMatcher_.icpObjective_ == o3d_slam::IcpObjective::PointToPlane) {
		o3d_slam::estimateNormals(tools.params_.scanMatcher_.kNNnormalEstimation_, downSampledCloud.get());
		downSampledCloud->NormalizeNormals();
	}

	auto result = open3d::pipelines::registration::RegistrationICP(
		cloudPrev_, *downSampledCloud, tools.params_.scanMatcher_.maxCorrespondenceDistance_,
		tools.icpTransform_, *tools.icpObjective_, tools.icpConvergenceCriteria_);

	//todo magic
	const bool isOdomOkay = result.fitness_ > tools.params_.minAcceptableFitness_;
	if (!isOdomOkay) {
		  std::cout << "Odometry failed!!!!! \n";
			std::cout << "Size of the odom buffer: " << odomToRangeSensorBuffer_.size() << std::endl;
			std::cout << "Scan matching time elapsed: " << timer.elapsedMsec() << " msec \n";
			std::cout << "Fitness: " << result.fitness_ << "\n";
			std::cout << "RMSE: " << result.inlier_rmse_ << "\n";
			std::cout << "Transform: \n" << asString(Transform(result.transformation_)) << "\n";
			std::cout << "target size: " << cloud.points_.size() << std::endl;
			std::cout << "reference size: " << cloudPrev_.points_.size() << std::endl;
			std::cout << "\n \n";
		if (!downSampledCloud->IsEmpty()){
			cloudPrev_ = std::move(*downSampledCloud);
		}
		return isOdomOkay;
	}

	if (isMapInitializing_)
	{
		isMapInitializing_ = false;
	}
	odomToRangeSensorCumulative_.matrix() *= result.transformation_.inverse();
	cloudPrev_ = std::move(*downSampledCloud);
	odomToRangeSensorBuffer_.push(timestamp, odomToRangeSensorCumulative_);
	lastMeasurementTimestamp_ = timestamp;
	return isOdomOkay;
}
const Transform LidarOdometry::getOdomToRangeSensor(const Time &t) const {
	return getTransform(t, odomToRangeSensorBuffer_);
}

const open3d::geometry::PointCloud& LidarOdometry::getPreProcessedCloud() const {
	return cloudPrev_;
}

const TransformInterpolationBuffer& LidarOdometry::getBuffer() const {
	return odomToRangeSensorBuffer_;
}

bool  LidarOdometry::hasProcessedMeasurements() const{
	return !odomToRangeSensorBuffer_.empty();
}

void LidarOdometry::setParameters(const OdometryParameters& p) {
	isMapInitializing_ = p.isMapInitializing_;
	scanToScanOdomTools_.setParameters(p.scanToScanToolsParams_);
	if (isMapInitializing_) {
		mapInitializingOdomTools_.setParameters(p.mapInitializingToolsParams_);
	}
}

void LidarOdometry::setInitialTransform(const Eigen::Matrix4d &initialTransform) {
	mapInitializingOdomTools_.icpTransform_ = initialTransform;
}

} // namespace o3d_slam
