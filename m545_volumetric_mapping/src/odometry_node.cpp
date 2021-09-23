/*
 * mapping_node.cpp
 *
 *  Created on: Sep 1, 2021
 *      Author: jelavice
 */
#include <open3d/Open3D.h>
#include <open3d/pipelines/registration/Registration.h>
#include "m545_volumetric_mapping/Parameters.hpp"

#include "open3d_conversions/open3d_conversions.h"
#include <ros/ros.h>

#include <sensor_msgs/PointCloud2.h>
#include <eigen_conversions/eigen_msg.h>
#include <geometry_msgs/TransformStamped.h>
#include <tf2_ros/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>

open3d::geometry::PointCloud cloud;
open3d::geometry::PointCloud cloudPrev;
ros::NodeHandlePtr nh;
bool isNewCloudReceived = false;
ros::Time timestamp;
std::shared_ptr<tf2_ros::TransformBroadcaster> tfBroadcaster;
Eigen::Matrix4d curentTransformation = Eigen::Matrix4d::Identity();
namespace registration = open3d::pipelines::registration;

void publishCloud(const open3d::geometry::PointCloud &cloud, const std::string &frame_id, ros::Publisher &pub) {
	sensor_msgs::PointCloud2 msg;
	open3d_conversions::open3dToRos(cloud, msg, frame_id);
	msg.header.stamp = timestamp;
	pub.publish(msg);
}

void cloudCallback(const sensor_msgs::PointCloud2ConstPtr &msg) {
	cloud.Clear();
	open3d_conversions::rosToOpen3d(msg, cloud, true);
	timestamp = msg->header.stamp;
	isNewCloudReceived = true;
}

void estimateNormals(int numNearestNeighbours, open3d::geometry::PointCloud *pcl) {
	open3d::geometry::KDTreeSearchParamKNN param(numNearestNeighbours);
	pcl->EstimateNormals(param);
}

geometry_msgs::Pose getPose(const Eigen::MatrixXd &T) {
	geometry_msgs::Pose pose;

	// Fill pose
	Eigen::Affine3d eigenTr;
	eigenTr.matrix() = T;
	tf::poseEigenToMsg(eigenTr, pose);

	return pose;
}

geometry_msgs::TransformStamped toRos(const Eigen::Matrix4d &Mat, const ros::Time &time, const std::string &frame,
		const std::string &childFrame) {

	geometry_msgs::TransformStamped transformStamped;
	transformStamped.header.stamp = time;
	transformStamped.header.frame_id = frame;
	transformStamped.child_frame_id = childFrame;
	const auto pose = getPose(Mat);
	transformStamped.transform.translation.x = pose.position.x;
	transformStamped.transform.translation.y = pose.position.y;
	transformStamped.transform.translation.z = pose.position.z;
	transformStamped.transform.rotation = pose.orientation;
	return transformStamped;
}

std::shared_ptr<registration::TransformationEstimation> icpObjectiveFactory(
		const m545_mapping::IcpOdometryParameters &p) {

	switch (p.icpObjective_) {
	case m545_mapping::IcpObjective::PointToPoint: {
		auto obj = std::make_shared<registration::TransformationEstimationPointToPoint>(false);
		return obj;
	}

	case m545_mapping::IcpObjective::PointToPlane: {
		auto obj = std::make_shared<registration::TransformationEstimationPointToPlane>();
		return obj;
	}

	default:
		throw std::runtime_error("Unknown icp objective");
	}

}

int main(int argc, char **argv) {
	ros::init(argc, argv, "m545_mapping_node");
	nh.reset(new ros::NodeHandle("~"));
	tfBroadcaster.reset(new tf2_ros::TransformBroadcaster());
	const std::string cloudTopic = nh->param<std::string>("cloud_topic", "");
	ros::Subscriber cloudSub = nh->subscribe(cloudTopic, 1, &cloudCallback);

	ros::Publisher refPub = nh->advertise<sensor_msgs::PointCloud2>("reference", 1, true);
	ros::Publisher targetPub = nh->advertise<sensor_msgs::PointCloud2>("target", 1, true);
	ros::Publisher registeredPub = nh->advertise<sensor_msgs::PointCloud2>("registered", 1, true);

	const std::string paramFile = nh->param<std::string>("parameter_file_path", "");
	std::cout << "loading params from: " << paramFile << "\n";
	m545_mapping::IcpOdometryParameters params;
	m545_mapping::loadParameters(paramFile, &params);
	auto icoObjective = icpObjectiveFactory(params);

	ros::Rate r(100.0);
	while (ros::ok()) {

		if (isNewCloudReceived) {
			isNewCloudReceived = false;

			if (cloudPrev.IsEmpty()) {
				cloudPrev = cloud;
				continue;
			}
			const auto startTime = std::chrono::steady_clock::now();
			const Eigen::Matrix4d init = Eigen::Matrix4d::Identity();
			auto criteria = open3d::pipelines::registration::ICPConvergenceCriteria();
			criteria.max_iteration_ = params.maxNumIter_;
			if(params.icpObjective_ == m545_mapping::IcpObjective::PointToPlane){
				estimateNormals(params.kNNnormalEstimation_, &cloud);
			}
			auto result = open3d::pipelines::registration::RegistrationICP(cloudPrev, cloud,
					params.maxCorrespondenceDistance_, init, *icoObjective, criteria);
			const auto endTime = std::chrono::steady_clock::now();
			const double nMsec = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()
					/ 1e3;

			std::cout << "Scan matching finished \n";
			std::cout << "Time elapsed: " << nMsec << " msec \n";
			std::cout << "Fitness: " << result.fitness_ << "\n";
			std::cout << "RMSE: " << result.inlier_rmse_ << "\n";
			std::cout << "Transform: " << result.transformation_ << "\n";
			std::cout << "\n \n";
			if (result.fitness_ > 1e-2) {
				curentTransformation *= result.transformation_.inverse();
			}
			geometry_msgs::TransformStamped transformStamped = toRos(curentTransformation, timestamp, "odom",
					"range_sensor");
			tfBroadcaster->sendTransform(transformStamped);

			auto registeredCloud = cloudPrev;
			registeredCloud.Transform(result.transformation_);

			publishCloud(cloudPrev, "odom", refPub);
			publishCloud(cloud, "odom", targetPub);
			publishCloud(registeredCloud, "odom", registeredPub);

			// source is cloud
			// target is cloudPrev
			cloudPrev.Clear();
			cloudPrev = cloud;
		}

		ros::spinOnce();
		r.sleep();
	}

	return 0;
}
