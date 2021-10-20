/*
 * helpers.cpp
 *
 *  Created on: Sep 26, 2021
 *      Author: jelavice
 */

#include "m545_volumetric_mapping/helpers.hpp"
#include "m545_volumetric_mapping/output.hpp"
#include "m545_volumetric_mapping/math.hpp"
#include "m545_volumetric_mapping/time.hpp"
#include "m545_volumetric_mapping/Voxel.hpp"

#include <open3d/Open3D.h>
#include <open3d/pipelines/registration/Registration.h>
#include <open3d/utility/Eigen.h>
#include "open3d/geometry/KDTreeFlann.h"

#ifdef M545_VOLUMETRIC_MAPPING_OPENMP_FOUND
#include <omp.h>
#endif

namespace m545_mapping {

namespace {
namespace registration = open3d::pipelines::registration;

class AccumulatedPoint {
public:
	AccumulatedPoint() :
			num_of_points_(0), point_(0.0, 0.0, 0.0), normal_(0.0, 0.0, 0.0), color_(0.0, 0.0, 0.0) {
	}

public:
	void AddPoint(const open3d::geometry::PointCloud &cloud, int index) {
		point_ += cloud.points_[index];
		if (cloud.HasNormals()) {
			if (!std::isnan(cloud.normals_[index](0)) && !std::isnan(cloud.normals_[index](1))
					&& !std::isnan(cloud.normals_[index](2))) {
				normal_ += cloud.normals_[index];
			}
		}
		if (cloud.HasColors()) {
			color_ += cloud.colors_[index];
		}
		num_of_points_++;
	}

	Eigen::Vector3d GetAveragePoint() const {
		return point_ / double(num_of_points_);
	}

	Eigen::Vector3d GetAverageNormal() const {
		// Call NormalizeNormals() afterwards if necessary
		return normal_ / double(num_of_points_);
	}

	Eigen::Vector3d GetAverageColor() const {
		return color_ / double(num_of_points_);
	}

public:
	int num_of_points_;
	Eigen::Vector3d point_;
	Eigen::Vector3d normal_;
	Eigen::Vector3d color_;
};

class point_cubic_id {
public:
	size_t point_id;
	int cubic_id;
};

} //namespace

void cropPointcloud(const open3d::geometry::AxisAlignedBoundingBox &bbox, open3d::geometry::PointCloud *pcl) {
	auto croppedCloud = pcl->Crop(bbox);
	*pcl = *croppedCloud;
}

std::string asString(const Eigen::Isometry3d &T) {
	const double kRadToDeg = 180.0 / M_PI;
	const auto &t = T.translation();
	const auto &q = Eigen::Quaterniond(T.rotation());
	const std::string trans = string_format("t:[%f, %f, %f]", t.x(), t.y(), t.z());
	const std::string rot = string_format("q:[%f, %f, %f, %f]", q.x(), q.y(), q.z(), q.w());
	const auto rpy = toRPY(q) * kRadToDeg;
	const std::string rpyString = string_format("rpy (deg):[%f, %f, %f]", rpy.x(), rpy.y(), rpy.z());
	return trans + " ; " + rot + " ; " + rpyString;

}

void estimateNormals(int numNearestNeighbours, open3d::geometry::PointCloud *pcl) {
	open3d::geometry::KDTreeSearchParamKNN param(numNearestNeighbours);
	pcl->EstimateNormals(param);
}

std::shared_ptr<registration::TransformationEstimation> icpObjectiveFactory(const m545_mapping::IcpObjective &obj) {

	switch (obj) {
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

open3d::geometry::AxisAlignedBoundingBox boundingBoxAroundPosition(const Eigen::Vector3d &low,
		const Eigen::Vector3d &high, const Eigen::Vector3d &origin /*= Eigen::Vector3d::Zero()*/) {
	open3d::geometry::AxisAlignedBoundingBox bbox;
	bbox.min_bound_ = origin + low;
	bbox.max_bound_ = origin + high;
	return bbox;
}

void randomDownSample(double downSamplingRatio, open3d::geometry::PointCloud *pcl) {
	if (downSamplingRatio >= 1.0) {
		return;
	}
	auto downSampled = pcl->RandomDownSample(downSamplingRatio);
	*pcl = *downSampled;
}
void voxelize(double voxelSize, open3d::geometry::PointCloud *pcl) {
	if (voxelSize <= 0) {
		return;
	}
	auto voxelized = pcl->VoxelDownSample(voxelSize);
	*pcl = *voxelized;
}

bool isInside(const open3d::geometry::AxisAlignedBoundingBox &bbox, const Eigen::Vector3d &p) {
	return p.x() <= bbox.max_bound_.x() && p.y() <= bbox.max_bound_.y() && p.z() <= bbox.max_bound_.z()
			&& p.x() >= bbox.min_bound_.x() && p.y() >= bbox.min_bound_.y() && p.z() >= bbox.min_bound_.z();
}

std::shared_ptr<open3d::geometry::PointCloud> voxelizeAroundPosition(double voxel_size,
		const open3d::geometry::AxisAlignedBoundingBox &bbox, const open3d::geometry::PointCloud &cloud) {
	using namespace open3d::geometry;
	auto output = std::make_shared<PointCloud>();
	if (voxel_size <= 0.0) {
		*output = cloud;
		return output;
//		throw std::runtime_error("[VoxelDownSample] voxel_size <= 0.");
	}

	const Eigen::Vector3d voxelSize = Eigen::Vector3d(voxel_size, voxel_size, voxel_size);
	const auto voxelBounds = computeVoxelBounds(cloud, voxelSize);
	const Eigen::Vector3d voxelMinBound = voxelBounds.first;
	const Eigen::Vector3d voxelMaxBound = voxelBounds.second;
	if (voxel_size * std::numeric_limits<int>::max() < (voxelMaxBound - voxelMinBound).maxCoeff()) {
		throw std::runtime_error("[VoxelDownSample] voxel_size is too small.");
	}
	std::unordered_map<Eigen::Vector3i, AccumulatedPoint, open3d::utility::hash_eigen<Eigen::Vector3i>> voxelindex_to_accpoint;

	const bool has_normals = cloud.HasNormals();
	const bool has_colors = cloud.HasColors();
	output->points_.reserve(cloud.points_.size());
	if (has_colors) {
		output->colors_.reserve(cloud.points_.size());
	}
	if (has_normals) {
		output->normals_.reserve(cloud.points_.size());
	}
	voxelindex_to_accpoint.reserve(cloud.points_.size());

	Eigen::Vector3d ref_coord;
	Eigen::Vector3i voxel_index;
	for (int i = 0; i < (int) cloud.points_.size(); i++) {
		if (isInside(bbox, cloud.points_[i])) {
			const Eigen::Vector3i voxelIdx = getVoxelIdx(cloud.points_[i], voxelSize, voxelMinBound, voxelMaxBound);
			voxelindex_to_accpoint[voxelIdx].AddPoint(cloud, i);
		} else {
			output->points_.push_back(cloud.points_[i]);
			if (has_normals) {
				output->normals_.push_back(cloud.normals_[i]);
			}
			if (has_colors) {
				output->colors_.push_back(cloud.colors_[i]);
			}
		}
	}

	for (auto accpoint : voxelindex_to_accpoint) {
		output->points_.push_back(accpoint.second.GetAveragePoint());
		if (has_normals) {
			output->normals_.push_back(accpoint.second.GetAverageNormal());
		}
		if (has_colors) {
			output->colors_.push_back(accpoint.second.GetAverageColor());
		}
	}

	return output;
}

std::pair<std::vector<double>, std::vector<size_t>> computePointCloudDistance(
		const open3d::geometry::PointCloud &reference, const open3d::geometry::PointCloud &cloud,
		const std::vector<size_t> &idsInReference) {
	std::vector<double> distances(idsInReference.size());
	std::vector<int> indices(idsInReference.size());
	open3d::geometry::KDTreeFlann kdtree;
	kdtree.SetGeometry(cloud); // fast cca 1 ms

#pragma omp parallel for schedule(static)
	for (int i = 0; i < (int) idsInReference.size(); i++) {
		const size_t idx = idsInReference[i];
		const int knn = 1;
		std::vector<int> ids(knn);
		std::vector<double> dists(knn);
//			if (kdtree.SearchHybrid(reference.points_[idx], 2.0, knn, ids, dists) != 0) {
		if (kdtree.SearchKNN(reference.points_[idx], knn, ids, dists) != 0) {
			distances[i] = std::sqrt(dists[0]);
			indices[i] = idx;
		} else {
			distances[i] = -1.0;
			indices[i] = -1;
//				std::cout << "could not find a nearest neighbour \n";
		}
	} // end for

	// remove distances/ids for which no neighbor was found
	std::vector<double> distsRet;
	distsRet.reserve(distances.size());
	std::copy_if(distances.begin(), distances.end(), std::back_inserter(distsRet), [](double x) {
		return x >= 0;
	});
	std::vector<size_t> idsRet;
	idsRet.reserve(indices.size());
	std::copy_if(indices.begin(), indices.end(), std::back_inserter(idsRet), [](int x) {
		return x >= 0;
	});
	return {distsRet,idsRet};
}

void removeByIds(const std::vector<size_t> &ids, open3d::geometry::PointCloud *cloud) {

	if (ids.empty()) {
		return;
	}
	auto trimmedCloud = cloud->SelectByIndex(ids, true);
	*cloud = *trimmedCloud;

}

} /* namespace m545_mapping */

