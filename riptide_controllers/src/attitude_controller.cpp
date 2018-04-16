#include "riptide_controllers/attitude_controller.h"

#undef debug
#undef report
#undef progress

float round(float d) {
  return floor(d + 0.5);
}

int main(int argc, char **argv) {
  ros::init(argc, argv, "attitude_controller");
  AttitudeController ac;
  ros::spin();
}

void AttitudeController::UpdateError() {

  sample_duration = current_attitude.header.stamp - sample_start;
  dt = sample_duration.toSec();

  // Roll error
  roll_error = roll_cmd - round(current_attitude.vector.x);
  roll_error_dot = (roll_error - last_error.x) / dt;
  last_error.x = roll_error;

  // Pitch error
  pitch_error = pitch_cmd - round(current_attitude.vector.y);
  pitch_error_dot = (pitch_error - last_error.y) / dt;
  last_error.y = pitch_error;

  // Yaw error
  // Always take shortest path to setpoint
  yaw_error = yaw_cmd - round(current_attitude.vector.z);
  if (yaw_error > 180)
      yaw_error -= 360;
  else if (yaw_error < -180)
      yaw_error += 360;

  yaw_error_dot = (yaw_error - last_error.z) / dt;
  last_error.z = yaw_error;

  accel_cmd.x = roll_controller_pid.computeCommand(roll_error, roll_error_dot, sample_duration);
  accel_cmd.y = pitch_controller_pid.computeCommand(pitch_error, pitch_error_dot, sample_duration);
  accel_cmd.z = yaw_controller_pid.computeCommand(yaw_error, yaw_error_dot, sample_duration);

  error_msg.vector.x = roll_error;
  error_msg.vector.y = pitch_error;
  error_msg.vector.z = yaw_error;
  error_msg.header.stamp = current_attitude.header.stamp;

  error_pub.publish(error_msg);
  cmd_pub.publish(accel_cmd);
  sample_start = current_attitude.header.stamp;
}

AttitudeController::AttitudeController() {
    ros::NodeHandle rcpid("roll_controller");
    ros::NodeHandle ycpid("yaw_controller");
    ros::NodeHandle pcpid("pitch_controller");

    pid_initialized = false;

    cmd_sub = nh.subscribe<geometry_msgs::Vector3>("command/attitude", 1, &AttitudeController::CommandCB, this);
    imu_sub = nh.subscribe<riptide_msgs::Imu>("state/imu", 1, &AttitudeController::ImuCB, this);
    kill_sub = nh.subscribe<riptide_msgs::SwitchState>("state/switches", 1, &AttitudeController::SwitchCB, this);

    roll_controller_pid.init(rcpid, false);
    yaw_controller_pid.init(ycpid, false);
    pitch_controller_pid.init(pcpid, false);

    cmd_pub = nh.advertise<geometry_msgs::Vector3>("command/accel/angular", 1);
    error_pub = nh.advertise<geometry_msgs::Vector3Stamped>("error/attitude", 1);
    sample_start = ros::Time::now();
}

// Subscribe to state/imu
void AttitudeController::ImuCB(const riptide_msgs::Imu::ConstPtr &imu) {
  current_attitude.vector = imu->euler_rpy;
  current_attitude.header = imu->header;
  if (pid_initialized) {
    AttitudeController::UpdateError();
  }
}

//Subscribe to state/switches
void AttitudeController::SwitchCB(const riptide_msgs::SwitchState::ConstPtr &state) {
  if (!state->kill) {
    AttitudeController::ResetController();
  }
}

// Subscribe to command/orientation
// set the MAX_ROLL and MAX_PITCH value in the header
void AttitudeController::CommandCB(const geometry_msgs::Vector3::ConstPtr &cmd) {
  roll_cmd = round(cmd->x);
  pitch_cmd = round(cmd->y);
  yaw_cmd = round(cmd->z);

  // Constrain pitch
  if(roll_cmd > MAX_ROLL)
    roll_cmd = MAX_ROLL;
  else if (roll_cmd < -MAX_ROLL)
    roll_cmd = -MAX_ROLL;

  // constrain roll
  if (pitch_cmd > MAX_PITCH)
      pitch_cmd = MAX_PITCH;
  else if (pitch_cmd < -MAX_PITCH)
    pitch_cmd = -MAX_PITCH;

  if (!pid_initialized)
    pid_initialized = true;

  AttitudeController::UpdateError();
}

void AttitudeController::ResetController() {
  roll_cmd = 0;
  roll_error = 0;
  roll_error_dot = 0;
  roll_controller_pid.reset();

  pitch_cmd = 0;
  pitch_error = 0;
  pitch_error_dot = 0;
  pitch_controller_pid.reset();

  yaw_cmd = 0;
  yaw_error = 0;
  yaw_error_dot = 0;
  yaw_controller_pid.reset();

  current_attitude.vector.x = 0;
  current_attitude.vector.y = 0;
  current_attitude.vector.z = 0;
  last_error.x = 0;
  last_error.y = 0;
  last_error.z = 0;
  sample_start = ros::Time::now();
  sample_duration = ros::Duration(0);
  dt = 0;

  pid_initialized = false;
}
