/*
 * Parameters.cpp
 *
 *  Created on: Sep 23, 2021
 *      Author: jelavice
 */

#include "m545_volumetric_mapping/Parameters.hpp"

namespace m545_mapping {

void loadParameters(const std::string &filename, PlaceRecognitionParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("PlaceRecognitionParams::loadParameters loading failed");
	}
	loadParameters(basenode["place_recognition"], p);
}
void loadParameters(const YAML::Node &node, PlaceRecognitionParameters *p){


//  feature_map_normal_estimation_radius: 1.0
//  feature_voxel_size: 0.5
//  feature_radius: 2.5
//  feature_knn: 100
//  normal_knn: 10
//  ransac_num_iter: 1000000
//  ransac_probability: 0.99
//  ransac_model_size: 3
//  ransac_max_correspondence_dist: 0.75
//  ransac_correspondence_checker_distance: 0.75
//  ransac_correspondence_checker_edge_lenght: 0.5
//  ransac_min_corresondence_set_size: 25
//  max_icp_correspondence_distance: 0.3
//  min_icp_refinement_fitness: 0.7

}

void loadParameters(const std::string &filename, GlobalOptimizationParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("GlobalOptimizationParameters::loadParameters loading failed");
	}
	loadParameters(basenode["global_optimization"], p);
}
void loadParameters(const YAML::Node &node, GlobalOptimizationParameters *p){
	p->edgePruneThreshold_ = node["edge_prune_threshold"].as<double>();
	p->loopClosurePreference_ = node["loop_closure_preference"].as<double>();
	p->maxCorrespondenceDistance_ = node["max_correspondence_distance"].as<double>();
	p->referenceNode_ = node["reference_node"].as<int>();
}


void loadParameters(const std::string &filename, VisualizationParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("VisualizationParameters::loadParameters loading failed");
	}
	loadParameters(basenode["visualization"], p);
}
void loadParameters(const YAML::Node &node, VisualizationParameters *p){
	p->assembledMapVoxelSize_ = node["assembled_map_voxel_size"].as<double>();
	p->submapVoxelSize_ = node["submaps_voxel_size"].as<double>();
	p->visualizeEveryNmsec_ = node["visualize_every_n_msec"].as<double>();
}


void loadParameters(const std::string &filename, IcpParameters *p) {
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("IcpParameters::loadParameters loading failed");
	}
	loadParameters(basenode["odometry"], p);
}

void loadParameters(const YAML::Node &n, IcpParameters *p) {
	p->icpObjective_ = IcpObjectiveNames.at(n["icp_objective"].as<std::string>());
	p->kNNnormalEstimation_ = n["knn_normal_estimation"].as<int>();
	p->maxCorrespondenceDistance_ = n["max_correspondence_dist"].as<double>();
	p->maxNumIter_ = n["max_n_iter"].as<int>();
}

void loadParameters(const std::string &filename, OdometryParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("Odometry::loadParameters loading failed");
	}
	loadParameters(basenode["odometry"], p);
}
void loadParameters(const YAML::Node &node, OdometryParameters *p){
	loadParameters(node["scan_matching"], &(p->scanMatcher_) );
	loadParameters(node["scan_processing"], &(p->scanProcessing_) );
}

void loadParameters(const std::string &filename, ScanProcessingParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("ScanProcessingParameters::loadParameters loading failed");
	}
	loadParameters(basenode["scan_processing"], p);
}
void loadParameters(const YAML::Node &node, ScanProcessingParameters *p){
	p->voxelSize_ = node["voxel_size"].as<double>();
	p->downSamplingRatio_ = node["downsampling_ratio"].as<double>();
	loadParameters(node["scan_cropping"], &(p->cropper_));
}

void loadParameters(const std::string &filename, ScanCroppingParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("ScanCroppingParameters::loadParameters loading failed");
	}
	loadParameters(basenode["scan_cropping"], p);
}
void loadParameters(const YAML::Node &node, ScanCroppingParameters *p){
	p->croppingRadius_ = node["cropping_radius"].as<double>();
	p->croppingMinZ_ = node["min_z"].as<double>();
	p->croppingMaxZ_ = node["max_z"].as<double>();
	p->cropperName_ = node["cropper_type"].as<std::string>();
}

void loadParameters(const std::string &filename, SubmapParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("SubmapParameters::loadParameters loading failed");
	}
	loadParameters(basenode["submaps"], p);
}
void loadParameters(const YAML::Node &node, SubmapParameters *p){
	p->radius_ = node["size"].as<double>();
	p->minNumRangeData_ = node["min_num_range_data"].as<int>();
}

void loadParameters(const std::string &filename, MapBuilderParameters *p) {
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("MapBuilderParameters::loadParameters loading failed");
	}
	loadParameters(basenode["map_builder"], p);
}
void loadParameters(const YAML::Node &node, MapBuilderParameters *p) {
	p->mapVoxelSize_ = node["map_voxel_size"].as<double>();
	loadParameters(node["space_carving"], &(p->carving_));
	loadParameters(node["scan_cropping"], &(p->cropper_));
}


void loadParameters(const std::string &filename, MapperParameters *p) {
	YAML::Node basenode = YAML::LoadFile(filename);

	if (basenode.IsNull()) {
		throw std::runtime_error("MapperParameters::loadParameters loading failed");
	}

	loadParameters(basenode["mapping"], p);
}
void loadParameters(const YAML::Node &node, MapperParameters *p) {
	p->isBuildDenseMap_ = node["is_build_dense_map"].as<bool>();
	p->minMovementBetweenMappingSteps_ = node["min_movement_between_mapping_steps"].as<double>();
	p->minRefinementFitness_ = node["scan_to_map_refinement"]["min_refinement_fitness"].as<double>();
	p->numScansOverlap_ = node["submaps_num_scan_overlap"].as<int>();
	loadParameters(node["scan_to_map_refinement"]["scan_matching"],&(p->scanMatcher_));
	loadParameters(node["scan_to_map_refinement"]["scan_processing"],&(p->scanProcessing_));
	if(p->isBuildDenseMap_){
		loadParameters(node["dense_map_builder"], &(p->denseMapBuilder_));
	}
	loadParameters(node["map_builder"], &(p->mapBuilder_));
	loadParameters(node["submaps"], &(p->submaps_));
	loadParameters(node["global_optimization"], &(p->globalOptimization_));
}

void loadParameters(const std::string &filename, LocalMapParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("Local map::loadParameters loading failed");
	}
	loadParameters(basenode["local_map"], p);
}
void loadParameters(const YAML::Node &n, LocalMapParameters *p){
	p->voxelSize_ = n["voxel_size"].as<double>();
	p->croppingRadius_ = n["cropping_radius"].as<double>();
}

void loadParameters(const YAML::Node &n, MesherParameters *p) {

	p->strategy_ = mesherStrategyNames.at(n["strategy"].as<std::string>());
	p->knnNormalEstimation_ = n["knn_normal_estimation"].as<int>();
	p->voxelSize_=n["voxel_size"].as<double>();
	p->alphaShapeAlpha_ = n["alpha_shape_alpha"].as<double>();
	p->poissonDepth_ = n["poisson_depth"].as<int>();
	p->poissonMinDensity_ = n["poisson_min_density"].as<double>();
	p->poissonScale_ = n["poisson_scale"].as<double>();
	p->ballPivotRadii_ = n["ball_pivot_radii"].as<std::vector<double>>();
	p->isComputeMesh_ = n["is_compute_mesh"].as<bool>();

}

void loadParameters(const std::string &filename, MesherParameters *p) {
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("MesherParameters::loadParameters loading failed");
	}
	loadParameters(basenode["mesher"], p);
}

void loadParameters(const YAML::Node &n, SpaceCarvingParameters *p){
	p->voxelSize_ = n["voxel_size"].as<double>();
	p->maxRaytracingLength_ = n["max_raytracing_length"].as<double>();
	p->truncationDistance_ = n["truncation_distance"].as<double>();
	p->carveSpaceEveryNsec_ = n["carve_space_every_n_sec"].as<double>();
	p->minDotProductWithNormal_ = n["min_dot_product_with_normal"].as<double>();
}
void loadParameters(const std::string &filename, SpaceCarvingParameters *p){
	YAML::Node basenode = YAML::LoadFile(filename);
	if (basenode.IsNull()) {
		throw std::runtime_error("SpaceCarving::loadParameters loading failed");
	}
	loadParameters(basenode["space_carving"], p);
}

} // namespace m545_mapping

