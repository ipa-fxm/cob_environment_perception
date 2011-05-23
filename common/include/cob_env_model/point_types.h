/*
 * cpc_point.h
 *
 *  Created on: 26.08.2010
 *      Author: goa
 */

#ifndef IPA_POINT_TYPES_H_
#define IPA_POINT_TYPES_H_

#include <pcl/ros/register_point_struct.h>
#include <pcl/point_types.h>
//#include <ros/ros.h>

using namespace pcl;

struct PointXYZCI;

struct PointXYZA;

#include "cob_env_model/impl/point_types.hpp"

//Point obtained from Swissranger devices, confidence only used for SR2
POINT_CLOUD_REGISTER_POINT_STRUCT(
	PointXYZCI,
  (float, x, x)
  (float, y, y)
  (float, z, z)
  (float, confidence, confidence)
  (float, intensity, intensity));

//Point obtained from Camboard
POINT_CLOUD_REGISTER_POINT_STRUCT(
	PointXYZA,
  (float, x, x)
  (float, y, y)
  (float, z, z)
  (float, amplitude, amplitude));

#endif /* IPA_POINT_TYPES_H_ */
