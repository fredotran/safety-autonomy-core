// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

<<<<<<< feat/demo-improvements
#include <atomic>
#include <fcntl.h>
#include <geometry_msgs/msg/twist.hpp>
#include <iostream>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <termios.h>
#include <thread>
#include <unistd.h>

class TeleopNode : public rclcpp::Node
{
  public:
    TeleopNode() : rclcpp::Node("teleop_node")
    {
        // Publisher for velocity commands
        cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel_nav", 10);

        // Publisher for status messages
        status_pub_ = this->create_publisher<std_msgs::msg::String>("/teleop/status", 10);

        // Parameters
        linear_speed_  = this->declare_parameter("linear_speed", 0.5);
        angular_speed_ = this->declare_parameter("angular_speed", 0.5);

=======
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <std_msgs/msg/string.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

class TeleopNode : public rclcpp::Node {
public:
    TeleopNode() : rclcpp::Node("teleop_node") {
        // Publisher for velocity commands
        cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel_nav", 10);
        
        // Publisher for status messages
        status_pub_ = this->create_publisher<std_msgs::msg::String>("/teleop/status", 10);
        
        // Parameters
        linear_speed_ = this->declare_parameter("linear_speed", 0.5);
        angular_speed_ = this->declare_parameter("angular_speed", 0.5);
        
>>>>>>> main
        RCLCPP_INFO(this->get_logger(), "Teleop Node Started");
        RCLCPP_INFO(this->get_logger(), "Controls:");
        RCLCPP_INFO(this->get_logger(), "  w/s: forward/backward");
        RCLCPP_INFO(this->get_logger(), "  a/d: rotate left/right");
        RCLCPP_INFO(this->get_logger(), "  q: quit");
        RCLCPP_INFO(this->get_logger(), "Linear speed: %.2f m/s", linear_speed_);
        RCLCPP_INFO(this->get_logger(), "Angular speed: %.2f rad/s", angular_speed_);
<<<<<<< feat/demo-improvements

        // Start keyboard input thread
        running_      = true;
        input_thread_ = std::thread(&TeleopNode::keyboardLoop, this);
    }

    ~TeleopNode()
    {
        running_ = false;
        if (input_thread_.joinable())
        {
            input_thread_.join();
        }
    }

  private:
    void keyboardLoop()
    {
=======
        
        // Start keyboard input thread
        running_ = true;
        input_thread_ = std::thread(&TeleopNode::keyboardLoop, this);
    }
    
    ~TeleopNode() {
        running_ = false;
        if (input_thread_.joinable()) {
            input_thread_.join();
        }
    }
    
private:
    void keyboardLoop() {
>>>>>>> main
        // Set terminal to non-canonical mode
        struct termios old_tio, new_tio;
        tcgetattr(STDIN_FILENO, &old_tio);
        new_tio = old_tio;
        new_tio.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
<<<<<<< feat/demo-improvements

        // Set stdin to non-blocking
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

        char c;
        double linear  = 0.0;
        double angular = 0.0;

        while (running_ && rclcpp::ok())
        {
            // Read character
            if (read(STDIN_FILENO, &c, 1) > 0)
            {
                switch (c)
                {
                case 'w':
                    linear = linear_speed_;
                    publishStatus("Moving forward");
                    break;
                case 's':
                    linear = -linear_speed_;
                    publishStatus("Moving backward");
                    break;
                case 'a':
                    angular = angular_speed_;
                    publishStatus("Rotating left");
                    break;
                case 'd':
                    angular = -angular_speed_;
                    publishStatus("Rotating right");
                    break;
                case ' ':
                    linear  = 0.0;
                    angular = 0.0;
                    publishStatus("Stopping");
                    break;
                case 'q':
                    running_ = false;
                    publishStatus("Quitting");
                    break;
                default:
                    break;
                }
            }

            // Publish velocity command
            if (linear != 0.0 || angular != 0.0)
            {
                auto msg      = geometry_msgs::msg::Twist();
                msg.linear.x  = linear;
                msg.angular.z = angular;
                cmd_vel_pub_->publish(msg);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Restore terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    }

    void publishStatus(const std::string& status)
    {
=======
        
        // Set stdin to non-blocking
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        
        char c;
        double linear = 0.0;
        double angular = 0.0;
        
        while (running_ && rclcpp::ok()) {
            // Read character
            if (read(STDIN_FILENO, &c, 1) > 0) {
                switch (c) {
                    case 'w':
                        linear = linear_speed_;
                        publishStatus("Moving forward");
                        break;
                    case 's':
                        linear = -linear_speed_;
                        publishStatus("Moving backward");
                        break;
                    case 'a':
                        angular = angular_speed_;
                        publishStatus("Rotating left");
                        break;
                    case 'd':
                        angular = -angular_speed_;
                        publishStatus("Rotating right");
                        break;
                    case ' ':
                        linear = 0.0;
                        angular = 0.0;
                        publishStatus("Stopping");
                        break;
                    case 'q':
                        running_ = false;
                        publishStatus("Quitting");
                        break;
                    default:
                        break;
                }
            }
            
            // Publish velocity command
            if (linear != 0.0 || angular != 0.0) {
                auto msg = geometry_msgs::msg::Twist();
                msg.linear.x = linear;
                msg.angular.z = angular;
                cmd_vel_pub_->publish(msg);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Restore terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    }
    
    void publishStatus(const std::string& status) {
>>>>>>> main
        auto msg = std_msgs::msg::String();
        msg.data = status;
        status_pub_->publish(msg);
        RCLCPP_INFO(this->get_logger(), "%s", status.c_str());
    }
<<<<<<< feat/demo-improvements

=======
    
>>>>>>> main
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
    std::thread input_thread_;
    std::atomic<bool> running_;
    double linear_speed_;
    double angular_speed_;
};

<<<<<<< feat/demo-improvements
int main(int argc, char** argv)
{
=======
int main(int argc, char** argv) {
>>>>>>> main
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TeleopNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
