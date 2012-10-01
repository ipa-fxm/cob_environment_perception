/*!
 *****************************************************************
 * \file
 *
 * \note
 *   Copyright (c) 2012 \n
 *   Fraunhofer Institute for Manufacturing Engineering
 *   and Automation (IPA) \n\n
 *
 *****************************************************************
 *
 * \note
 *  Project name: care-o-bot
 * \note
 *  ROS stack name: cob_environment_perception_intern
 * \note
 *  ROS package name: cob_3d_mapping_point_map
 *
 * \author
 *  Author: Georg Arbeiter, email:georg.arbeiter@ipa.fhg.de
 * \author
 *  Supervised by: Georg Arbeiter, email:georg.arbeiter@ipa.fhg.de
 *
 * \date Date of creation: 01/2011
 *
 * \brief
 * Description:
 *
 * ToDo:
 *
 *****************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer. \n
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution. \n
 *     - Neither the name of the Fraunhofer Institute for Manufacturing
 *       Engineering and Automation (IPA) nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission. \n
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License LGPL for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License LGPL along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************/


//##################
//#### includes ####

// standard includes
//--
#include <sstream>
#include <fstream>


// ROS includes
#include <ros/ros.h>
#include <ros/console.h>
//#include <actionlib/server/simple_action_server.h>
#include <tf/transform_listener.h>
#include <tf_conversions/tf_eigen.h>
#include <pluginlib/class_list_macros.h>
#include <dynamic_reconfigure/server.h>
#include <cob_3d_mapping_point_map/point_map_nodeletConfig.h>

// PCL includes
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl_ros/transforms.h>
#include <pcl_ros/point_cloud.h>
#include <pcl_ros/pcl_nodelet.h>
//#include <pcl/filters/extract_indices.h>
//#include <visualization_msgs/Marker.h>

// ROS message includes
#include <cob_3d_mapping_msgs/SetReferenceMap.h>
#include <cob_3d_mapping_msgs/GetPointMap.h>
#include <cob_srvs/Trigger.h>


//####################
//#### node class ####
class PointMapNodelet : public pcl_ros::PCLNodelet//, protected Reconfigurable_Node<cob_3d_mapping_point_map::point_map_nodeletConfig>
{
  typedef pcl::PointXYZRGB Point;

public:
  // Constructor
  PointMapNodelet()
  : ctr_(0),
    is_running_(false),
    map_frame_id_("/map")
    {

    map_.header.frame_id = map_frame_id_;
    }


  // Destructor
  ~PointMapNodelet()
  {
    // void
  }

  /**
   * @brief Callback for dynamic reconfigure server
   *
   * Callback for dynamic reconfigure server
   *
   * @return nothing
   */
  void
  dynReconfCallback(cob_3d_mapping_point_map::point_map_nodeletConfig &config, uint32_t level)
  {
    file_path_ = config.file_path;
    save_ = config.save;
    voxel_leafsize_ = config.voxel_leafsize;
    map_frame_id_ = config.map_frame_id;
    map_.header.frame_id = map_frame_id_;
  }


  /**
   * @brief initializes parameters
   *
   * initializes parameters
   *
   * @return nothing
   */
  void
  onInit()
  {
    PCLNodelet::onInit();
    n_ = getNodeHandle();

    config_server_ = boost::shared_ptr<dynamic_reconfigure::Server<cob_3d_mapping_point_map::point_map_nodeletConfig> >(new dynamic_reconfigure::Server<cob_3d_mapping_point_map::point_map_nodeletConfig>(getPrivateNodeHandle()));
    config_server_->setCallback(boost::bind(&PointMapNodelet::dynReconfCallback, this, _1, _2));

    point_cloud_sub_ = n_.subscribe("point_cloud2", 1, &PointMapNodelet::updateCallback, this);
    map_pub_ = n_.advertise<pcl::PointCloud<Point> >("map",1);
    clear_map_server_ = n_.advertiseService("clear_map", &PointMapNodelet::clearMap, this);
    get_map_server_ = n_.advertiseService("get_map", &PointMapNodelet::getMap, this);

    //n_.param("aggregate_point_map/file_path" ,file_path_ ,std::string("~/pcl_daten/table/icp/map_"));
    //n_.param("aggregate_point_map/save_",save_ , false);
    //n_.param("aggregate_point_map/voxel_leafsize" ,voxel_leafsize_, 0.03);
  }

  /**
   * @brief callback for point cloud subroutine
   *
   * callback for keyframe subroutine which loads in the first step
   * the unexact transformation from the laser sensors and calibrates the
   * input cloud from the 3d camera. This already transformed data will be
   * used to build a 3d map either aligned to the first frame or to an
   * existing map. Additionally debug output to *.pcd files are possible.
   *
   * @param req  not used
   * @param res  not used
   *
   * @return nothing
   */
  void
  updateCallback(const pcl::PointCloud<Point>::Ptr& pc)
  {
    ROS_DEBUG("PointCloudSubCallback");
    if (pc->size() < 1)
    {
      ROS_WARN("[point_map] Incoming point cloud is empty, skipping map update.");
      return;
    }
    tf::StampedTransform trf_map;
    try
    {
      std::stringstream ss2;
      tf_listener_.waitForTransform(map_frame_id_, pc->header.frame_id, pc->header.stamp, ros::Duration(2.0));
      tf_listener_.lookupTransform(map_frame_id_, pc->header.frame_id, pc->header.stamp/*ros::Time(0)*/, trf_map);
    }
    catch (tf::TransformException ex)
    {
      ROS_ERROR("[aggregate_point_map] : %s",ex.what());
      return;
    }
    Eigen::Affine3d af;
    tf::TransformTFToEigen(trf_map, af);
    Eigen::Matrix4f trf = af.matrix().cast<float>();
    pcl::transformPointCloud(*pc, *pc, trf);
    pc->header.frame_id = map_frame_id_;

    updateMap(pc);
    map_pub_.publish(map_);

    ROS_DEBUG("[point_map] Updated map has %d points", map_.size());

    if(save_)
    {
      std::stringstream ss;
      ss << file_path_ << "/map_" << ctr_ << ".pcd";
      pcl::io::savePCDFileASCII (ss.str(), map_);
    }
    ctr_++;
    return;
  }

  void
  updateMap(const pcl::PointCloud<Point>::Ptr& pc)
  {
    map_ += *pc;
    downsampleMap();
  }


  /**
   * @brief action callback
   *
   * default action callback to start or stop node
   *
   * @param goal settings
   *
   * @return nothing
   */
  /*void
  actionCallback(const cob_3d_mapping_msgs::TriggerMappingGoalConstPtr &goal)
  {
    //boost::mutex::scoped_lock l(m_mutex_actionCallback);

    cob_3d_mapping_msgs::TriggerMappingResult result;
    if(goal->start && !is_running_)
    {
      ROS_INFO("Starting mapping...");
      point_cloud_sub_ = n_.subscribe("point_cloud2", 1, &PointMapNodelet::updateCallback, this);
      //point_cloud_sub_.subscribe(n_, "point_cloud2", 1);
      //transform_sub_.subscribe(n_, "transform_reg", 1);
      is_running_ = true;
    }
    else if(!goal->start && is_running_)
    {
      ROS_INFO("Stopping mapping...");
      point_cloud_sub_.shutdown();//unsubscribe();
      //transform_sub_.unsubscribe();
      //first_ = true;
      is_running_ = false;
    }
    as_->setSucceeded(result);
  }*/

  /**
   * @brief clears map
   *
   * deletes 3d map of the environment
   *
   * @param req not needed
   * @param res not needed
   *
   * @return nothing
   */
  bool
  clearMap(cob_srvs::Trigger::Request &req,
           cob_srvs::Trigger::Response &res)
  {
    //TODO: add mutex
    ROS_INFO("Clearing point map...");
    map_.clear();
    Point p;
    map_.points.push_back(p);
    map_pub_.publish(map_);
    return true;
  }

  /**
   * @brief service callback for GetPointMap service
   *
   * Fills the service response of the GetPointMap service with the current point map
   *
   * @param req request to send map
   * @param res the current point map
   *
   * @return nothing
   */
  bool
  getMap(cob_3d_mapping_msgs::GetPointMap::Request &req,
         cob_3d_mapping_msgs::GetPointMap::Response &res)
  {
    pcl::toROSMsg(map_, res.map);
    return true;
  }

  /**
   * @brief sets reference map
   *
   * sets the 3d map representing a static environment,
   * overrides any existing map
   *
   * @param req containing reference map
   * @param res not needed
   *
   * @return nothing
   */
  bool
  setReferenceMap(cob_3d_mapping_msgs::SetReferenceMap::Request &req,
                  cob_3d_mapping_msgs::SetReferenceMap::Response &res)
  {
    pcl::fromROSMsg(req.map, map_);
    downsampleMap();
    //ROS_WARN("not needed");
    return true;
  }


  /**
   * @brief downsamples the map
   *
   * downsamples the map using the voxel_leafsize parameters to voxelize
   *
   * @return nothing
   */
  void
  downsampleMap()
  {
    pcl::VoxelGrid<Point> vox_filter;
    vox_filter.setInputCloud(map_.makeShared());
    vox_filter.setLeafSize(voxel_leafsize_,voxel_leafsize_,voxel_leafsize_);
    vox_filter.filter(map_);
  }


  ros::NodeHandle n_;


protected:
  ros::Subscriber point_cloud_sub_;	//subscriber for input pc
  ros::Publisher map_pub_;		//publisher for map
  ros::ServiceServer clear_map_server_;
  ros::ServiceServer get_map_server_;

  boost::shared_ptr<dynamic_reconfigure::Server<cob_3d_mapping_point_map::point_map_nodeletConfig> > config_server_;

  tf::TransformListener tf_listener_;

  unsigned int ctr_;
  bool is_running_;

  // Parameters from launch file or dynamic config
  std::string file_path_;
  bool save_;
  std::string map_frame_id_;
  double voxel_leafsize_;

  pcl::PointCloud<Point> map_;

};

PLUGINLIB_DECLARE_CLASS(cob_3d_mapping_point_map, PointMapNodelet, PointMapNodelet, nodelet::Nodelet)

