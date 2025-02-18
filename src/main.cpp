#include "main.h"
#include "lemlib/api.hpp"


/**
* Runs initialization code. This occurs as soon as the program is started.
*
* All other competition modes are blocked by initialize; it is recommended
* to keep execution time for this mode under a few seconds.
*/


// Sensors
pros::Rotation rotation_sensor(14); // rotational sensor on port 14
pros::Optical optical_sensor(20); // optical sensor on port 20
pros::Imu imu(12); // imu on port 12


// Motors
pros::MotorGroup left_motors({-1, -2, -3}, pros::MotorGearset::blue); // left motors on ports 1, 2, 3
pros::MotorGroup right_motors({4, 5, 6}, pros::MotorGearset::blue); // right motors on ports 4, 5, 6
pros::MotorGroup intake({-8, 9}); // intake motors on ports 8 and 9
pros::Motor firstStage(8, pros::MotorGearset::green); // intake first stage on port 8
pros::Motor secondStage(9, pros::MotorGearset::blue); // intake second stage on port 9
pros::Motor wallstake(13, pros::MotorGearset::green); // wallstake motor on port 13


// Pistons
pros::ADIDigitalOut clamp_piston('B', false); // piston on special port B
pros::ADIDigitalOut doinker_piston('D', false); // piston on special port D
pros::ADIDigitalOut tipper_piston('F', false); // piston on special port F


// Variables
bool doinker_state = false; // tracks the state of the doinker piston
bool clamp_state = false; // tracks the state of the clamp pistons
bool tipper_state = false; // tracks the state of the tipper piston
static bool run_color_sort = true; // controls if color sorting should happen or not
bool is_sorting_active = false; // tracks if color sorting is happening
static double wallstakeTargetPos = 0.0;
const int none = 0;
const int red = 1;
const int blue = 2;

// Team color and opposing color
static int teamColor = red;
static int opposingTeamColor = blue;


pros::Controller controller(pros::E_CONTROLLER_MASTER);


// drivetrain settings
lemlib::Drivetrain drivetrain(&left_motors, // left motor group
                             &right_motors, // right motor group
                             13, // 13 inch track width
                             lemlib::Omniwheel::OLD_275, // using old 2.75" omnis
                             450, // drivetrain rpm is 450
                             2 // horizontal drift is 2 (for now)
);


// lateral PID controller
lemlib::ControllerSettings linearController(
                            8.5, // proportional gain (kP)
                            0, // integral gain (kI)
                            8.5, // derivative gain (kD)
                            3, // anti windup
                            1, // small error range, in inches
                            100, // small error range timeout, in milliseconds
                            3, // large error range, in inches
                            500, // large error range timeout, in milliseconds
                            20 // maximum acceleration (slew)
);


// angular PID controller
lemlib::ControllerSettings angularController(
                            8, // proportional gain (kP)
                            0, // integral gain (kI)
                            66, // derivative gain (kD)
                            3, // anti windup
                            1, // small error range, in degrees
                            100, // small error range timeout, in milliseconds
                            3, // large error range, in degrees
                            500, // large error range timeout, in milliseconds
                            0 // maximum acceleration (slew)
);


// sensors for odometry
lemlib::OdomSensors sensors(nullptr, // set to nullptr as we don't have a vertical tracking wheel
                            nullptr, // set to nullptr as we don't have a second vertical tracking wheel
                            nullptr, // set to nullptr as we don't have a horizontal tracking wheel
                            nullptr, // set to nullptr as we don't have a second horizontal tracking wheel
                            &imu // inertial sensor
);


// create the chassis
lemlib::Chassis chassis(drivetrain, linearController, angularController, sensors);


// Get detected color
static int get_opticalColor() {
    double hue = optical_sensor.get_hue();
    if (optical_sensor.get_proximity() < 100) return none;
    if (hue < 10 || hue > 355) return red;
    if (hue > 200 && hue < 240) return blue;
    return none;
}


void initialize() {
    chassis.calibrate(); // calibrate sensors
    rotation_sensor.reset_position(); // rotational sensor calibration
    optical_sensor.set_led_pwm(100);
    wallstake.tare_position();
    // set drive motors to coast mode
    for (int port : {1, 2, 3, 4, 5, 6}) {
        pros::Motor motor(port);
        motor.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
    }
    pros::lcd::initialize(); // initialize brain screen
    // Perform sorting logic
    pros::Task sorting_task([&]() {
        while (true) {
            if (run_color_sort) {
                int color = get_opticalColor();
                if (color == opposingTeamColor) {
                    is_sorting_active = true;
                    pros::delay(100);
                    secondStage.move(-127);
                    pros::delay(100);
                    secondStage.move(127);
                    is_sorting_active = false;
                }
            }
            pros::delay(25);
        }
    });
    // print position to brain screen
    pros::Task screen_task([&]() {
    while (true) {
        // print robot location to the brain screen
        pros::lcd::print(0, "X: %f", chassis.getPose().x); // x
        pros::lcd::print(1, "Y: %f", chassis.getPose().y); // y
        pros::lcd::print(2, "Theta: %f", chassis.getPose().theta); // heading
        // log position telemetry
        lemlib::telemetrySink()->info("Chassis pose: {}", chassis.getPose());
        // delay to save resources
        pros::delay(50);
    }
    });
}


/**
* Runs while the robot is in the disabled state of Field Management System or
* the VEX Competition Switch, following either autonomous or opcontrol. When
* the robot is enabled, this task will exit.
*/
void disabled() {}


/**
* Runs after initialize(), and before autonomous when connected to the Field
* Management System or the VEX Competition Switch. This is intended for
* competition-specific initialization routines, such as an autonomous selector
* on the LCD.
*
* This task will exit when the robot is enabled and autonomous or opcontrol
* starts.
*/
void competition_initialize() {}


/**
* Runs the user autonomous code. This function will be started in its own task
* with the default priority and stack size whenever the robot is enabled via
* the Field Management System or the VEX Competition Switch in the autonomous
* mode. Alternatively, this function may be called in initialize or opcontrol
* for non-competition testing purposes.
*
* If the robot is disabled or communications is lost, the autonomous task
* will be stopped. Re-enabling the robot will restart the task, not re-start it
* from where it left off.
*/


void setClamp(bool clamp_state) {
    clamp_piston.set_value(clamp_state);
}


void setdoinker(bool doinker_state) {
    doinker_piston.set_value(doinker_state);
}


void setTipper(bool tipper_state) {
    tipper_piston.set_value(tipper_state);
}


void setIntake(int power) {
    intake.move(power);
}


void auton_red_negative() {
    chassis.setPose(0, 0, 0);
    chassis.moveToPose(0, -36, 0, 2500, {.forwards = false});
    pros::delay(1500);
    setClamp(true);
    setIntake(127);
    chassis.moveToPose(21, -30, 60, 2000, {.maxSpeed = 90});
    pros::delay(800);
    chassis.moveToPose(20, -48, 200, 1500);
    pros::delay(700);
    chassis.moveToPose(20, -36, 200, 1000, {.forwards = false});
    pros::delay(1000);
    chassis.moveToPose(24, -48, 170, 1500);
    pros::delay(700);
    chassis.moveToPose(-2, -38, -95, 2000, {.maxSpeed = 80});
    pros::delay(2500);
    setIntake(0);
    wallstake.move(127);
    pros::delay(550);
    wallstake.move(0);
}


void auton_blue_negative() {
    
}


void autonomous() {
    auton_red_negative();
    //auton_blue_negative();
}


/**
* Runs the operator control code. This function will be started in its own task
* with the default priority and stack size whenever the robot is enabled via
* the Field Management System or the VEX Competition Switch in the operator
* control mode.
*
* If no competition control is connected, this function will run immediately
* following initialize().
*
* If the robot is disabled or communications is lost, the
* operator control task will be stopped. Re-enabling the robot will restart the
* task, not resume it from where it left off.
*/


void opcontrol() {
    // Target positions for macros
    int intake_position = 324;  // position for intake
    int scoring_position = 110; // position for scoring
    double target_position = 0; // Desired position (set when macro is active)

    // Track whether a macro command is active
    bool macro_active = false;


    // Track whether manual control is active
    bool manual_control_active = false;

    while (true) {
        // Get left y and right y positions
        int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
        int rightY = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_Y);

        chassis.tank(leftY, rightY);

        // Intake control logic
        if (is_sorting_active == false) {
            if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2)) {
                intake.move(127); // Forward
            } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2)) {
                intake.move(-127); // Backward
            } else {
                intake.move(0); // Stop
            }
        }


        // Turn off color sort
        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_L1)) {
            run_color_sort = false;
        }


        // Manual wallstake control logic
        if (!macro_active) {
            manual_control_active = true;
            wallstake.set_brake_mode(pros::E_MOTOR_BRAKE_COAST); // Coast mode during manual control
            if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_DOWN)) {
                wallstake.move(127); // Move wallstake motor forward
            } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_B)) {
                wallstake.move(-127); // Move wallstake motor backward
            } else {
                wallstake.move(0); // Stop
            }
        }

        // Wallstake position macros
        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A)) {
            macro_active = true;
            manual_control_active = false; // Disable manual control
            wallstake.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD); // Enable hold mode
            target_position = intake_position; // Set target position for PID
        }

        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_X)) {
            macro_active = true;
            manual_control_active = false; // Disable manual control
            wallstake.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD); // Enable hold mode
            target_position = scoring_position; // Set target position for PID
        }

        // PID control loop
        if (!manual_control_active) {
            // PID constant
            double kP = 0.85;
        
            // Get current position from the rotational sensor
            float angle_in_centidegrees = rotation_sensor.get_angle();
            float angle_in_degrees = angle_in_centidegrees / 100.0;
            double current_position = angle_in_degrees;

            // PID variables
            double error = current_position - target_position;
            double velocity = kP * error;

            // Apply motor power
            wallstake.move(velocity);
        }

        // Exit macro and re-enable manual control if buttons are pressed
        if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_DOWN) ||
            controller.get_digital(pros::E_CONTROLLER_DIGITAL_B)) {
            macro_active = false; // Disable macro
            manual_control_active = true; // Allow manual control
        }


        // Clamp toggle logic
        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_R1)) {
            clamp_state = !clamp_state;
            clamp_piston.set_value(clamp_state); // Set the clamp piston to the new state
            if (clamp_state == true) {
                controller.clear();
                controller.print(0, 0, "Clamped");
            }
            else {
                controller.clear();
                controller.print(0, 0, "Unclamped");
            }
        }


        // tipper control logic
        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_RIGHT)) {
            tipper_state = !tipper_state;
            tipper_piston.set_value(tipper_state); // Set the tipper piston to the new state
        }


        // doinker toggle logic
        if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_Y)) {
            doinker_state = !doinker_state;
            doinker_piston.set_value(doinker_state); // Set the doinker piston to the new state
        }


        // Delay to save resources
        pros::delay(25);
    }
}