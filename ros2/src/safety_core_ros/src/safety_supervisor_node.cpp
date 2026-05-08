#include "safety_core_ros/safety_supervisor_node.hpp"

#include <chrono>

using namespace std::chrono_literals;
using safety_core::safety::SafetyZone;
using safety_core::sm::Mode;
using std::placeholders::_1;

namespace safety_core_ros
{

    SafetySupervisorNode::SafetySupervisorNode(const rclcpp::NodeOptions& options)
        : rclcpp::Node("safety_supervisor_node", options)
    {
        declare_params();

        const double localization_timeout_s = get_parameter("localization_timeout_s").as_double();
        localization_timeout_ns_            = static_cast<std::uint64_t>(localization_timeout_s * 1e9);

        // Optimized QoS settings
        rclcpp::QoS reliable_qos(10);
        reliable_qos.durability_volatile(); // Volatile durability for faster publishing

        rclcpp::SensorDataQoS sensor_qos;
        sensor_qos.keep_last(5);  // Reduced history depth for lower latency
        sensor_qos.best_effort(); // Use best-effort for sensor data (faster)

        // Order matters: build adapters first, then publishers (used by transport),
        // then state machine / supervisor (which use clock + transport).
        clock_          = std::make_unique<RosClock>(get_clock());
        health_monitor_ = std::make_unique<RosHealthMonitor>(get_logger());

        diag_pub_ = create_publisher<safety_core_msgs::msg::DiagnosticEvent>("safety/diagnostics", reliable_qos);
        diagnostic_transport_ = std::make_shared<RosDiagnosticTransport>(diag_pub_);

        machine_ =
            std::make_unique<safety_core::sm::ModeStateMachine>(health_monitor_.get(), diagnostic_transport_.get());
        machine_->set_clock(clock_.get());

        supervisor_ = std::make_unique<safety_core::safety::SafetySupervisor>(
            machine_.get(), diagnostic_transport_.get(), clock_.get());

        // Idle by default; explicit Idle->Moving requested by external
        // command (e.g. Nav2 starts a goal).
        (void)machine_->transition_to(Mode::Idle);

        state_pub_     = create_publisher<safety_core_msgs::msg::SafetyState>("safety/state", reliable_qos);
        safe_stop_pub_ = create_publisher<std_msgs::msg::Bool>("safety/safe_stop", reliable_qos);

        envelope_sub_ = create_subscription<safety_core_msgs::msg::EnvelopeStatus>(
            "safety/envelope_status", sensor_qos, std::bind(&SafetySupervisorNode::on_envelope, this, _1));
        odom_sub_ = create_subscription<nav_msgs::msg::Odometry>("odom", sensor_qos,
                                                                 std::bind(&SafetySupervisorNode::on_odom, this, _1));

        timer_ = create_wall_timer(50ms, std::bind(&SafetySupervisorNode::timer_tick, this));

        RCLCPP_INFO(get_logger(), "safety_supervisor_node up | localization_timeout=%.3fs", localization_timeout_s);
    }

    void SafetySupervisorNode::declare_params()
    {
        declare_parameter<double>("localization_timeout_s", 0.5);
        declare_parameter<bool>("auto_recover_from_obstacle", true);
    }

    std::uint64_t SafetySupervisorNode::now_ns() const noexcept
    {
        return static_cast<std::uint64_t>(get_clock()->now().nanoseconds());
    }

    void SafetySupervisorNode::on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg)
    {
        const std::uint64_t stamp_ns = static_cast<std::uint64_t>(msg->header.stamp.sec) * 1'000'000'000ULL +
                                       static_cast<std::uint64_t>(msg->header.stamp.nanosec);
        last_odom_time_ns_ = stamp_ns;
    }

    void SafetySupervisorNode::on_envelope(const safety_core_msgs::msg::EnvelopeStatus::ConstSharedPtr msg)
    {
        latest_zone_ = static_cast<SafetyZone>(msg->zone.zone);

        const Mode current_mode = machine_->mode();
        const bool auto_recover = get_parameter("auto_recover_from_obstacle").as_bool();

        switch (latest_zone_)
        {
        case SafetyZone::Clear:
        case SafetyZone::Warning:
            // Recover from obstacle hold if we're paused and the path is now
            // clear enough.
            if (current_mode == Mode::AvoidingObstacle && auto_recover)
            {
                (void)machine_->transition_to(Mode::Moving);
                safe_stop_requested_ = false;
            }
            break;

        case SafetyZone::Protective:
            if (current_mode == Mode::Moving)
            {
                (void)machine_->request_obstacle_hold();
            }
            break;

        case SafetyZone::Emergency:
            // Latch a fault; downstream bridge will execute jerk-limited stop.
            // Fault code 0xE001 == "envelope emergency zone breached".
            (void)machine_->latch_fault(0xE001U);
            (void)machine_->transition_to(Mode::SafeStop);
            safe_stop_requested_ = true;
            break;
        }
    }

    void SafetySupervisorNode::timer_tick()
    {
        // Cache current time and mode to avoid repeated calls
        const std::uint64_t current_time_ns = now_ns();
        const Mode current_mode             = machine_->mode();

        // Localization staleness check.
        if (last_odom_time_ns_.has_value())
        {
            const std::uint64_t age_ns = current_time_ns - last_odom_time_ns_.value();
            (void)supervisor_->observe_localization_age(age_ns, localization_timeout_ns_);

            // Only check for localization lost if not already in SafeStop or LocalizationLost
            if (age_ns > localization_timeout_ns_ && current_mode != Mode::LocalizationLost &&
                current_mode != Mode::SafeStop)
            {
                (void)machine_->report_localization_lost();
            }
        }

        (void)supervisor_->observe_clock_sample(current_time_ns);

        publish_state(current_mode);
    }

    safety_core_msgs::msg::SafetyState SafetySupervisorNode::build_state_msg(Mode current_mode) const
    {
        safety_core_msgs::msg::SafetyState msg;
        msg.header.stamp        = get_clock()->now();
        msg.header.frame_id     = "base_link";
        msg.mode                = static_cast<std::uint8_t>(current_mode);
        msg.fault_latched       = machine_->fault_latched();
        msg.fault_code          = 0U; // Library does not currently expose code; leave 0.
        msg.zone.zone           = static_cast<std::uint8_t>(latest_zone_);
        msg.safe_stop_requested = safe_stop_requested_ || (current_mode == Mode::SafeStop);
        return msg;
    }

    void SafetySupervisorNode::publish_state(Mode current_mode)
    {
        state_pub_->publish(build_state_msg(current_mode));

        std_msgs::msg::Bool stop;
        stop.data = safe_stop_requested_ || (current_mode == Mode::SafeStop);
        safe_stop_pub_->publish(std::move(stop));
    }

} // namespace safety_core_ros
