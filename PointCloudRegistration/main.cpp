#include <iostream>
#include <time.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/registration/icp.h>
#include <pcl/registration/icp_nl.h>
#include <pcl/registration/ia_ransac.h>
#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>
#include <pcl/point_types.h>
#include <pcl/features/pfh.h>
#include <pcl/filters/passthrough.h>
#include <pcl/visualization/cloud_viewer.h>
#include <limits>
#include <fstream>
#include <vector>
#include <Eigen/Core>
#include "pcl/point_cloud.h" 
#include "pcl/kdtree/kdtree_flann.h" 
#include "pcl/filters/passthrough.h" 
#include "pcl/filters/voxel_grid.h" 
#include "pcl/features/fpfh.h" 

#include "filters.h"
#include "features.h"
#include "registration.h"
#include "sac_ia.h"
#include "visualization.h"

#include "common.h"

using namespace std;

const double FILTER_LIMIT = 1000.0;
const int MAX_SACIA_ITERATIONS = 1000; // 2000

const float VOXEL_GRID_SIZE = 3;
const double NORMALS_RADIUS = 20;
const double FEATURES_RADIUS = 50; // 50
const double SAC_MAX_CORRESPONDENCE_DIST = 1000; // 2000
const double SAC_MIN_CORRESPONDENCE_DIST = 3;

int main(int argc, char *argv[]){
	string filename1, filename2;
	if (argc == 3){
		filename1 = argv[1];
		filename2 = argv[2];
	} else{
		filename1 = "pcd/a3.pcd";
		filename2 = "pcd/a4.pcd";
	}

	Timer t;
	cout << "Loading clouds...\n";
	cout.flush();

	// open the clouds
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud1(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud2(new pcl::PointCloud<pcl::PointXYZ>);
	
	pcl::io::loadPCDFile(filename1, *cloud1);
	pcl::io::loadPCDFile(filename2, *cloud2);

	long double cloud1ia_cx = 0, cloud1ia_cy = 0, cloud1ia_cz = 0;
	long double cloud2ia_cx = 0, cloud2ia_cy = 0, cloud2ia_cz = 0;
	for (size_t i = 0; i < cloud1->points.size(); ++i){
		cloud1ia_cx += cloud1->points[i].x;
		cloud1ia_cy += cloud1->points[i].y;
		cloud1ia_cz += cloud1->points[i].z;
	}
	cloud1ia_cx /= cloud1->points.size();
	cloud1ia_cy /= cloud1->points.size();
	cloud1ia_cz /= cloud1->points.size();
	cout << "Cloud1 Init Points Count: " << cloud1->points.size() << endl;
	cout << "Cloud1 Init Centroid Position: " << cloud1ia_cx << ", " << cloud1ia_cy << ", " << cloud1ia_cz << endl;

	for (size_t i = 0; i < cloud2->points.size(); ++i){
		cloud2ia_cx += cloud2->points[i].x;
		cloud2ia_cy += cloud2->points[i].y;
		cloud2ia_cz += cloud2->points[i].z;
	}
	cloud2ia_cx /= cloud2->points.size();
	cloud2ia_cy /= cloud2->points.size();
	cloud2ia_cz /= cloud2->points.size();
	cout << "Cloud2 Init Points Count: " << cloud2->points.size() << endl;
	cout << "Cloud2 Init Centroid Position: " << cloud2ia_cx << ", " << cloud2ia_cy << ", " << cloud2ia_cz << endl;

	// downsample the clouds
	t.StartWatchTimer();
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud1ds(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud2ds(new pcl::PointCloud<pcl::PointXYZ>);

	voxelFilter(cloud1, cloud1ds, VOXEL_GRID_SIZE);
	voxelFilter(cloud2, cloud2ds, VOXEL_GRID_SIZE);

	// compute normals
	pcl::PointCloud<pcl::Normal>::Ptr normals1 = getNormals(cloud1ds, NORMALS_RADIUS);
	pcl::PointCloud<pcl::Normal>::Ptr normals2 = getNormals(cloud2ds, NORMALS_RADIUS);

	// compute local features
	pcl::PointCloud<pcl::FPFHSignature33>::Ptr features1 = getFeatures(cloud1ds, normals1, FEATURES_RADIUS);
	pcl::PointCloud<pcl::FPFHSignature33>::Ptr features2 = getFeatures(cloud2ds, normals2, FEATURES_RADIUS);

	// 
	auto sac_ia = align(cloud1ds, cloud2ds, features1, features2, 
		MAX_SACIA_ITERATIONS, SAC_MIN_CORRESPONDENCE_DIST, SAC_MAX_CORRESPONDENCE_DIST);
	t.ReadWatchTimer();

	Eigen::Matrix4f init_transform = sac_ia.getFinalTransformation();
	pcl::transformPointCloud(*cloud2, *cloud2, init_transform);
	pcl::PointCloud<pcl::PointXYZ> final = *cloud1;
	final += *cloud2;

	cout << "IA Time: " << t << "ms" << endl;
	cout << init_transform << endl;
	cout.flush();

	long double cloud1_cx = 0, cloud1_cy = 0, cloud1_cz = 0;
	long double cloud2_cx = 0, cloud2_cy = 0, cloud2_cz = 0;
	for (size_t i = 0; i < cloud1->points.size(); ++i){
		cloud1_cx += cloud1->points[i].x;
		cloud1_cy += cloud1->points[i].y;
		cloud1_cz += cloud1->points[i].z;
	}
	cloud1_cx /= cloud1->points.size();
	cloud1_cy /= cloud1->points.size();
	cloud1_cz /= cloud1->points.size();
	cout << "Cloud1 IA Points Count: " << cloud1->points.size() << endl;
	cout << "Cloud1 IA Centroid Position: " << cloud1_cx << ", " << cloud1_cy << ", " << cloud1_cz << endl;

	for (size_t i = 0; i < cloud2->points.size(); ++i){
		cloud2_cx += cloud2->points[i].x;
		cloud2_cy += cloud2->points[i].y;
		cloud2_cz += cloud2->points[i].z;
	}
	cloud2_cx /= cloud2->points.size();
	cloud2_cy /= cloud2->points.size();
	cloud2_cz /= cloud2->points.size();
	cout << "Cloud2 IA Points Count: " << cloud2->points.size() << endl;
	cout << "Cloud2 IA Centroid Position: " << cloud2_cx << ", " << cloud2_cy << ", " << cloud2_cz << endl;

	pcl::io::savePCDFile("result.pcd", final);
	viewPair(cloud1ds, cloud2ds, cloud1, cloud2);
	//view(final);
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr output = coloredMerge(cloud1, cloud2);
	pcl::io::savePCDFile("output.pcd", *output);

	// copy source data
	pcl::transformPointCloud(*cloud2ds, *cloud2ds, init_transform);

	pcl::PointCloud<pcl::PointXYZ>::Ptr src(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr tgt(new pcl::PointCloud<pcl::PointXYZ>);

	src = cloud1;
	tgt = cloud2;

	// compute normals
	t.StartWatchTimer();
	pcl::PointCloud<pcl::PointNormal>::Ptr points_with_normals_src = getPointNormals(src, 30);
	pcl::PointCloud<pcl::PointNormal>::Ptr points_with_normals_tgt = getPointNormals(tgt, 30);

	// ICP + LM (Non Linear ICP)
	pcl::IterativeClosestPointNonLinear<pcl::PointNormal, pcl::PointNormal> reg;
	Eigen::Matrix4f Ti = icpNonLinear(points_with_normals_src, points_with_normals_tgt, 2, 2, 0.1);
	Eigen::Matrix4f Tiv = Ti.inverse();
	t.ReadWatchTimer();
	cout << Tiv << endl;
	cout << "ICP Time: " << t << "ms" << endl;
	pcl::transformPointCloud(*tgt, *tgt, Tiv);
	viewPair(cloud1, cloud2, src, tgt);
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr icpout = coloredMerge(src, tgt);
	pcl::io::savePCDFile("icpout.pcd", *icpout);

	// caculate the centroid position
	long double src_cx = 0, src_cy = 0, src_cz = 0;
	long double tgt_cx = 0, tgt_cy = 0, tgt_cz = 0;
	for (size_t i = 0; i < src->points.size(); ++i){
		src_cx += src->points[i].x;
		src_cy += src->points[i].y;
		src_cz += src->points[i].z;
	}
	src_cx /= src->points.size();
	src_cy /= src->points.size();
	src_cz /= src->points.size();
	cout << "Src PC Points Count: " << src->points.size() << endl;
	cout << "Src PC Centroid Position: " << src_cx << ", " << src_cy << ", " << src_cz << endl;

	for (size_t i = 0; i < tgt->points.size(); ++i){
		tgt_cx += tgt->points[i].x;
		tgt_cy += tgt->points[i].y;
		tgt_cz += tgt->points[i].z;
	}
	tgt_cx /= tgt->points.size();
	tgt_cy /= tgt->points.size();
	tgt_cz /= tgt->points.size();
	cout << "Tgt PC Points Count: " << tgt->points.size() << endl;
	cout << "Tgt PC Centroid Position: " << tgt_cx << ", " << tgt_cy << ", " << tgt_cz << endl;

	return 0;
}