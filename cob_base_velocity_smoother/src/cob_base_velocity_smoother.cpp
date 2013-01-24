/****************************************************************
 *
 * Copyright (c) 2012
 *
 * Fraunhofer Institute for Manufacturing Engineering  
 * and Automation (IPA)
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Project name: care-o-bot
 * ROS stack name: cob_driver
 * ROS package name: cob_base_velocity_smoother
 *  							
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *  		
 * Author: Florian Mirus, email:Florian.Mirus@ipa.fhg.de
 *
 * Date of creation: May 2012
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *  	 notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *  	 notice, this list of conditions and the following disclaimer in the
 *  	 documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Fraunhofer Institute for Manufacturing 
 *  	 Engineering and Automation (IPA) nor the names of its
 *  	 contributors may be used to endorse or promote products derived from
 *  	 this software without specific prior written permission.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as 
 * published by the Free Software Foundation, either version 3 of the 
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License LGPL for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public 
 * License LGPL along with this program. 
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************/

#include <cob_base_velocity_smoother.h>

/****************************************************************
 * the ros navigation doesn't run very smoothly because acceleration is too high
 * --> cob has strong base motors and therefore reacts with shaking behavior 
 * (PR2 has much more mechanical damping)
 * solution: the additional node cob_base_velocity_smoother smooths the velocities
 * comming from ROS-navigation or teleoperation, by calculating the mean values of a certain number 
 * of past messages and limiting the acceleration under a given threshold. 
 * cob_base_velocity_smoother subsribes (input) and publishes (output) geometry_msgs::Twist.
 ****************************************************************/

// constructor
cob_base_velocity_smoother::cob_base_velocity_smoother()
{

  // create node handles
  nh_ = ros::NodeHandle();
  pnh_ = ros::NodeHandle("~");

  // publisher
  pub_ = nh_.advertise<geometry_msgs::Twist>("output", 1);

  // subscriber
  geometry_msgs_sub_ = nh_.subscribe<geometry_msgs::Twist>("input", 1, boost::bind(&cob_base_velocity_smoother::geometryCallback, this, _1));

  //get parameters from parameter server if possible or write default values to variables
  if( !pnh_.hasParam("circular_buffer_capacity") )
  {
     ROS_WARN("No parameter circular_buffer_capacity on parameter server. Using default [12]");
  }
  pnh_.param("circular_buffer_capacity", buffer_capacity_, 12);

  if( !pnh_.hasParam("maximal_time_delay") )
  {
    ROS_WARN("No parameter maximal_time_delay on parameter server. Using default [4 in s]");
  }
  pnh_.param("maximal_time_delay", store_delay_, 4.0);
  
  if( !pnh_.hasParam("thresh_max_acc") )
  {
    ROS_WARN("No parameter thresh_max_ac on parameter server. Using default [0.3 in m/s]");
  }
  pnh_.param("thresh_max_acc", thresh_, 0.3);
  

  //set a geometry message containing zero-values
  zero_values_.linear.x=0;
  zero_values_.linear.y=0;
  zero_values_.linear.z=0;

  zero_values_.angular.x=0;
  zero_values_.angular.y=0;
  zero_values_.angular.z=0;

  //initialize circular buffers
  cb_.set_capacity(buffer_capacity_);
  cb_out_.set_capacity(buffer_capacity_);
  cb_time_.set_capacity(buffer_capacity_);
	
  //set actual ros::Time
  ros::Time now=ros::Time::now();

  //fill circular buffer with zero values
  while(cb_.full() == false){

    cb_.push_front(zero_values_);
    cb_time_.push_front(now);

  }	
};

cob_base_velocity_smoother::~cob_base_velocity_smoother(){}

//returns true if all messages in cb are out of date in consideration of store_delay
bool cob_base_velocity_smoother::circBuffOutOfDate(ros::Time now)
{
  bool result=true;

  long unsigned int count=0;

  while( (count < cb_.size()) && (result == true) ){
		
    double delay=(now.toSec() - cb_time_[count].toSec());

    if(delay < store_delay_){
      result = false;
    }
    count++;
  }

  return result;

};

//returns true if the input msg cmd_vel equals zero_values_, false otherwise
bool cob_base_velocity_smoother::IsZeroMsg(geometry_msgs::Twist cmd_vel)
{
  bool result = true;
  if( (cmd_vel.linear.x) != 0 || (cmd_vel.linear.y != 0) || (cmd_vel.angular.z != 0) ){
    result = false;
  }

  return result;
};

int cob_base_velocity_smoother::signum(double var)
{
  if(var < 0){
    return -1;
  }
  else{
    return 1;
  }
};

//functions to calculate the mean values for linear/x
double cob_base_velocity_smoother::meanValueX()
{
  double result = 0;
  long unsigned int size = cb_.size();

  //calculate sum
  for(long unsigned int i=0; i<size; i++){

    result = result + cb_[i].linear.x;

  }
  result = result / size;
	
  if(size > 1){

    double help_result = 0;
    double max = cb_[0].linear.x;
    long unsigned int max_ind = 0;
    for(long unsigned int i=0; i<size; i++){

      if(abs(result-cb_[i].linear.x) > abs(result-max)){
        max = cb_[i].linear.x;
        max_ind = i;
      }

    }

    //calculate sum
    for(long unsigned int i=0; i<size; i++){
		
      if(i != max_ind){
        help_result = help_result + cb_[i].linear.x;
      }
    }
    result = help_result / (size - 1);
  }

  return result;
	
};

//functions to calculate the mean values for linear/y
double cob_base_velocity_smoother::meanValueY()
{
  double result = 0;
  long unsigned int size = cb_.size();

  //calculate sum
  for(long unsigned int i=0; i<size; i++){

    result = result + cb_[i].linear.y;

  }
  result = result / size;

  if(size > 1){

    double help_result = 0;
    double max = cb_[0].linear.y;
    long unsigned int max_ind = 0;
    for(long unsigned int i=0; i<size; i++){

      if(abs(result-cb_[i].linear.y) > abs(result-max)){
        max = cb_[i].linear.y;
        max_ind = i;
      }

    }

    //calculate sum
    for(long unsigned int i=0; i<size; i++){
		
      if(i != max_ind){
        help_result = help_result + cb_[i].linear.y;
      }
    }
    result = help_result / (size - 1);
  }

  return result;
	
};

//functions to calculate the mean values for angular/z
double cob_base_velocity_smoother::meanValueZ()
{
  double result = 0;
  long unsigned int size = cb_.size();

  //calculate sum
  for(long unsigned int i=0; i<size; i++){

  result = result + cb_[i].angular.z;

  }
  result = result / size;
	
  if(size > 1){
		
    double help_result = 0;
    double max = cb_[0].angular.z;
    long unsigned int max_ind = 0;
    for(long unsigned int i=0; i<size; i++){

      if(abs(result-cb_[i].angular.z) > abs(result-max)){
        max = cb_[i].angular.z;
        max_ind = i;
      }

    }

    //calculate sum
    for(long unsigned int i=0; i<size; i++){

      if(i != max_ind){
        help_result = help_result + cb_[i].angular.z;
      }
    }
    result = help_result / (size - 1);
  }

  return result;
	
};

//function that updates the circular buffer after receiving a new geometry message
void cob_base_velocity_smoother::reviseCircBuff(ros::Time now, geometry_msgs::Twist cmd_vel)
{
  if(this->circBuffOutOfDate(now) == true){
	
    //clear buffers
    cb_.clear();
    cb_time_.clear();

    //fill circular buffer with zero_values_ and time buffer with actual time-stamp
    while(cb_.full() == false){

      cb_.push_front(zero_values_);
      cb_time_.push_front(now);

    }

    //add new command velocity message to circular buffer
    cb_.push_front(cmd_vel);
    //add new timestamp for subscribed command velocity message
    cb_time_.push_front(now);

  }
  else{
    double delay=(now.toSec() - cb_time_.back().toSec());

    while( delay >= store_delay_ ){
      //remove out-dated messages
      cb_.pop_back();
      cb_time_.pop_back();

      delay=(now.toSec() - cb_time_.back().toSec());
    }
    //if the circular buffer is empty now, refill with zero values
    if(cb_.empty() == true){
      while(cb_.full() == false){

        cb_.push_front(zero_values_);
        cb_time_.push_front(now);

      }
    }
    if(this->IsZeroMsg(cmd_vel)){

      long unsigned int size = floor( cb_.size() / 3 );

      //to stop the robot faster, fill the circular buffer with more than one, in fact floor (cb_.size() / 3 ), zero messages
      for(long unsigned int i=0; i< size; i++){

        //add new command velocity message to circular buffer
        cb_.push_front(cmd_vel);
        //add new timestamp for subscribed command velocity message
        cb_time_.push_front(now);
      }

    }
    else{

      //add new command velocity message to circular buffer
      cb_.push_front(cmd_vel);
      //add new timestamp for subscribed command velocity message
      cb_time_.push_front(now);

    }
  }
};

//function to limit the acceleration under the given threshhold thresh
void cob_base_velocity_smoother::limitAcceleration(ros::Time now, geometry_msgs::Twist& result){

  //limit the acceleration under thresh
  // only if cob_base_velocity_smoother has published a message yet
	
  double deltaTime = 0;	

  if(cb_time_.size() > 1){
    deltaTime = now.toSec() - cb_time_[2].toSec();
  }

  if( cb_out_.size() > 0){

    if(deltaTime > 0){
      //set delta velocity and acceleration values
      double deltaX = result.linear.x - cb_out_.front().linear.x;
      double accX = deltaX / deltaTime;

      double deltaY = result.linear.y - cb_out_.front().linear.y;
      double accY = deltaY / deltaTime;
		
      double deltaZ = result.angular.z - cb_out_.front().angular.z;
      double accZ = deltaZ / deltaTime;

      if( abs(accX) > thresh_){

        result.linear.x = cb_out_.front().linear.x + ( this->signum(accX) * thresh_ * deltaTime );

      }
      if( abs(accY) > thresh_){

        result.linear.y = cb_out_.front().linear.y + ( this->signum(accY) * thresh_ * deltaTime );

      }
      if( abs(accZ) > thresh_){

        result.angular.z = cb_out_.front().angular.z + ( this->signum(accZ) * thresh_ * deltaTime );

      }
    }
  }
};


//function for the actual computation
//calls the reviseCircBuff and the meanValue-functions and limits the acceleration under thresh
//returns the resulting geometry message to be published to the base_controller
geometry_msgs::Twist cob_base_velocity_smoother::setOutput(ros::Time now, geometry_msgs::Twist cmd_vel)
{
  geometry_msgs::Twist result = zero_values_;

  //update the circular buffers
  this->reviseCircBuff(now, cmd_vel);

  //calculate the mean values for each direction
  result.linear.x = meanValueX();
  result.linear.y = meanValueY();
  result.angular.z = meanValueZ();

  //limit acceleration
  this->limitAcceleration(now, result);

  //insert the result-message into the circular-buffer storing the output
  cb_out_.push_front(result);

  return result;

}

//callback function to subsribe to the geometry messages cmd_vel and publish to base_controller/command
void cob_base_velocity_smoother::geometryCallback(const geometry_msgs::Twist::ConstPtr &cmd_vel)
{

  geometry_msgs::Twist cmd_vel_local = *cmd_vel;

  //set actual ros::Time
  ros::Time now = ros::Time::now();

  //generate Output messages
  geometry_msgs::Twist result = this->setOutput(now, cmd_vel_local);

  //publish result
  pub_.publish(result);

};

int main(int argc, char **argv)
{
  // initialize ros and specifiy node name
  ros::init(argc, argv, "cob_base_velocity_smoother");
  // create Node Class
  cob_base_velocity_smoother my_velocity_smoother;

  ros::spin();

  return 0;
}

