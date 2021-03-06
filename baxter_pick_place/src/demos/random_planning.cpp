/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, Open Source Robotics Foundation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Open Source Robotics Foundation
 *     nor the names of its contributors may be
 *     used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/*
  Author: Dave Coleman
  Desc:   Randomly moves the arms around
*/

#include <ros/ros.h>

// MoveIt!
#include <moveit/move_group_interface/move_group.h>

// Baxter Utilities
#include <baxter_control/baxter_utilities.h>
#include <baxter_pick_place/custom_environment2.h>

// Visualization
#include <moveit_visual_tools/visual_tools.h> // simple tool for showing graspsp

#include <std_msgs/Float64.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Empty.h>
#include <std_msgs/Bool.h>
#include <baxter_core_msgs/HeadPanCommand.h>

// Baxter specific properties
#include <moveit_simple_grasps/grasp_data.h>
#include <baxter_pick_place/custom_environment2.h>

namespace baxter_pick_place
{

static const std::string PLANNING_GROUP_NAME = "left_arm";

class RandomPlanning
{
public:

  // class for publishing stuff to rviz
  moveit_visual_tools::VisualToolsPtr visual_tools_;

  // our interface with MoveIt
  boost::scoped_ptr<move_group_interface::MoveGroup> group_;

  // baxter helper
  baxter_control::BaxterUtilities baxter_util_;

  ros::Publisher gripper_position_topic_;
  ros::Publisher gripper_release_topic_;
  ros::Publisher head_nod_topic_;
  ros::Publisher head_turn_topic_;

  // robot-specific data for generating grasps
  moveit_simple_grasps::GraspData grasp_data_;

  // allow head to move?
  bool allow_head_movements_;

  RandomPlanning()
    : allow_head_movements_(false)
  {
    ros::NodeHandle nh;

    // ---------------------------------------------------------------------------------------------
    // Load grasp generator
    if (!grasp_data_.loadRobotGraspData(nh, "right"))
      ros::shutdown();

    // ---------------------------------------------------------------------------------------------
    // Load the Robot Viz Tools for publishing to rviz
    visual_tools_.reset(new moveit_visual_tools::VisualTools( grasp_data_.base_link_));
    visual_tools_->setFloorToBaseHeight(-0.9);
    visual_tools_->loadEEMarker(grasp_data_.ee_group_, PLANNING_GROUP_NAME);

    // ---------------------------------------------------------------------------------------------
    // Create MoveGroup
    group_.reset(new move_group_interface::MoveGroup(PLANNING_GROUP_NAME));
    group_->setPlanningTime(30.0);

    // --------------------------------------------------------------------------------------------------------
    // Add objects to scene
    //createEnvironment(visual_tools_);

    // --------------------------------------------------------------------------------------------------------
    // Create publishers for stuff
    ROS_DEBUG_STREAM_NAMED("random_planning","Starting close publisher");
    gripper_position_topic_ = nh.advertise<std_msgs::Float32>("/robot/limb/right/accessory/gripper/command_grip",10);

    ROS_DEBUG_STREAM_NAMED("random_planning","Starting open publisher");
    gripper_release_topic_ = nh.advertise<std_msgs::Empty>("/robot/limb/right/accessory/gripper/command_release",10);

    if ( allow_head_movements_ )
    {
      ROS_DEBUG_STREAM_NAMED("random_planning","Starting head nod publisher");
      head_nod_topic_ = nh.advertise<std_msgs::Bool>("/robot/head/command_head_nod",10);
    }

    if ( allow_head_movements_ )
    {
      ROS_DEBUG_STREAM_NAMED("random_planning","Starting turn head publisher");
      head_turn_topic_ = nh.advertise<baxter_core_msgs::HeadPanCommand>("/robot/head/command_head_pan",10);
    }

    // Create the walls and tables
    createEnvironment(visual_tools_);

    // Wait for everything to be ready
    ros::Duration(1.0).sleep();

    // Enable baxter
    if( !baxter_util_.enableBaxter() )
      return;

    // Do it.
    startRoutine();

    // Shutdown
    baxter_util_.disableBaxter();
  }

  double fRand(double fMin, double fMax)
  {
    double f = (double)rand() / RAND_MAX;
    return fMin + f * (fMax - fMin);
  }

  void startRoutine()
  {

    std_msgs::Empty empty_msg;
    std_msgs::Float32 gripper_command; // for closing gripper
    gripper_command.data = 0;
    std_msgs::Bool true_command;
    true_command.data = true;
    baxter_core_msgs::HeadPanCommand head_command;
    head_command.speed = 60;

    // ---------------------------------------------------------------------------------------------
    // Start the demo
    while(ros::ok())
    {
      // First look around
      if ( allow_head_movements_ )
      {
        head_command.target = fRand(-1,-0.1);
        head_turn_topic_.publish(head_command);

        ros::Duration(0.5).sleep();
        head_command.target = fRand(0.1,1.0);
        head_turn_topic_.publish(head_command);

        ros::Duration(0.5).sleep();
        head_command.target = 0.0;
        head_turn_topic_.publish(head_command);
      }

      do
      {
        ROS_INFO_STREAM_NAMED("random_planning","Planning to random target...");
        group_->setRandomTarget();
        if ( allow_head_movements_ )
        {
          head_nod_topic_.publish(true_command);
        }
      } while(!group_->move() && ros::ok());

      /*
      // Open grippers
      gripper_release_topic_.publish(empty_msg);

      ros::Duration(0.25).sleep();

      // Close grippers
      gripper_position_topic_.publish(gripper_command);

      ros::Duration(0.25).sleep();
      */
    }

  }

}; // end class

} // nodehandle

int main(int argc, char **argv)
{
  ros::init (argc, argv, "baxter_random");
  ros::AsyncSpinner spinner(1);
  spinner.start();

  // Start the pick place node
  baxter_pick_place::RandomPlanning();

  ros::shutdown();

  return 0;
}
