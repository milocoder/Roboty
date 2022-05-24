/* robotOS v1.0
 * file: main.cpp
 * last update: 05-05-2022
 */

// includes
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <fstream>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <thread>

using namespace cv;
using namespace std;

// pin definitions
#define PWM_forward_right 23 // GPIO 13
#define PWM_forward_left 1 // GPIO 18
#define BT_emergency_stop 7 // GPIO 4

// global variables
#define screen_width 640
#define screen_height 360
int camera_angle = 0;
int camera_offset = 0;
int battery_status = 100;
int motor_balance = 0;
int motor_max_speed = 50;
double pct_pwm = 0;
int bottom_color = 0;
int top_color = 0;
string old_send_event = "STOP";
bool save_line_preview = false;
enum events
{
	NOTHING,
	STOP,
	START,
	TESTMOTORRIGHT,
	TESTMOTORLEFT
};
enum states
{
	STANDBY,
	STARTING,
	STOPING,
	STEP1,
	SETP2,
	TESTINGMOTORRIGHT,
	TESTINGMOTORLEFT
};
enum direction
{
	LEFT,
	RIGHT
};
events EVENT = NOTHING;
states STATUS = STANDBY;

// function declarations
void write_live_data();
string get_config_data(string config_line, string old_data);
void line_detection();
void set_motor_speed(int pct_pwm_left, int pct_pwm_right);
bool motors_start();
bool motors_stop();
void drive();
bool test_motor(direction motor);
void load_config_data();
void ticker(int time);
void emergency_stop();

/* main function */
int main()
{
	wiringPiSetup();
	wiringPiISR(BT_emergency_stop, INT_FALLING_EDGE, emergency_stop)
	pinMode(PWM_forward_right, PWM_OUTPUT);
	pinMode(PWM_forward_left, PWM_OUTPUT);
	pwmWrite(PWM_forward_right, 0);
	pwmWrite(PWM_forward_left, 0);
	load_config_data();
	thread line_detection_thread(line_detection);
	thread ticker_tread(ticker, 500);
	// mainloop
	int i = 0;
	while (true)
	{
		switch (STATUS)
		{
		case STANDBY:
			if (EVENT == START)
			{
				EVENT = NOTHING;
				STATUS = STARTING;
				printf("[INFO] Starting program.\n");
			} else if(EVENT == TESTMOTORRIGHT){
				EVENT = NOTHING;
				STATUS = TESTINGMOTORRIGHT;
			} else if(EVENT == TESTMOTORLEFT){
				EVENT = NOTHING;
				STATUS = TESTINGMOTORLEFT;
			}
			break;
		case STARTING:
			if (motors_start() == true)
			{
				EVENT = NOTHING;
				STATUS = STEP1;
			}
			break;
		case STEP1:
			drive();
			if (EVENT == STOP)
			{
				EVENT = NOTHING;
				STATUS = STOPING;
			}
			break;
		case STOPING:
			if (motors_stop() == true)
			{
				EVENT = NOTHING;
				STATUS = STANDBY;
				printf("[INFO] Stoping program.\n");
			}
			break;
		case TESTINGMOTORRIGHT:
			if(test_motor(RIGHT) == true){
				EVENT = NOTHING;
				STATUS = STANDBY;
			}
			break;
		case TESTINGMOTORLEFT:
			if(test_motor(LEFT) == true){
				EVENT = NOTHING;
				STATUS = STANDBY;
			}
			break;
		default:
			break;
		}
	}
}

/* vision functions */

// vision / line detection
void line_detection()
{
	// initializing the camera
	VideoCapture cap(0);
	cap.set(CAP_PROP_FRAME_WIDTH, screen_width);
	cap.set(CAP_PROP_FRAME_HEIGHT, screen_height);
	// variables
	Mat frame, whiteLine;
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	while (true)
	{
		// read frame
		cap.read(frame);
		// find the line by checking in a color range in the whole frame.
		inRange(frame, Scalar(bottom_color, bottom_color, bottom_color), Scalar(top_color, top_color, top_color), whiteLine);
		// make the line small en big again to remove noise.
		erode(whiteLine, whiteLine, Mat(), Point(-1, -1), 20, 1, 1);
		dilate(whiteLine, whiteLine, Mat(), Point(-1, -1), 20, 1, 1);
		// find the contours of the line.
		Mat contourOut = whiteLine.clone();
		findContours(contourOut, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
		// check if there is a line.
		if (contours.size() > 0)
		{
			// get the angle and corners of the line.
			RotatedRect blackbox = minAreaRect(contours[0]);
			camera_angle = blackbox.angle;
			if (camera_angle > 45)
			{
				camera_angle = -90 + camera_angle;
			}
			if (blackbox.size.width > blackbox.size.height && camera_angle > 0)
			{
				camera_angle = (90 - camera_angle) * -1;
			}
			if (blackbox.size.width < blackbox.size.height && camera_angle < 0)
			{
				camera_angle = 90 + camera_angle;
			}
			// get the offset form the center of the screen.
			float centerline = blackbox.center.x;
			camera_offset = centerline - (screen_width / 2);

			putText(whiteLine, "angle:" + to_string(camera_angle), Point(10, 40), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
			putText(whiteLine, "offset:" + to_string(camera_offset), Point(10, 80), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
			// draw a line in the middel of the line.
			line(whiteLine, Point(centerline, 200), Point(centerline, 250), Scalar(255, 255, 255), 3);
			// draw a line in the middel of the screen.
			line(whiteLine, Point((screen_width / 2), 0), Point((screen_width / 2), screen_height), Scalar(255, 255, 255), 1);
		}
		// show the frame
		// imshow("Live", frame);
		// imwrite("/home/pi/test.png",frame);
		if(save_line_preview == true){
			imwrite("/home/pi/robotOS/data/images/camera-preview.png",whiteLine);
			save_line_preview = false;
		}
		//imshow("Black/White", whiteLine);
		waitKey(1);
	}
}

/* motor functions */

void emergency_stop()
{
	EVENT = NOTHING;
	STATUS = STOPING;
	printf("[info] Emergency stop!\n");
	delay(50);
	pct_pwm = 0;
	set_motor_speed(0, 0);
}

void drive()
{
	int steering_offset = 100.0 / ((screen_width / 2) + 90) * (camera_offset + camera_angle);
	int pwm_right = pct_pwm;
	int pwm_left = pct_pwm;
	if (steering_offset > 0)
	{
		if (steering_offset >= motor_max_speed)
		{
			pwm_right = 0;
		} else {
			pwm_right = pct_pwm - steering_offset;
		}
		pwm_left = pct_pwm;
	}
	else if (steering_offset < 0)
	{
		if (steering_offset <= motor_max_speed)
		{
			pwm_left = 0;
		} else {
			pwm_left = pct_pwm + steering_offset;
		}
		pwm_right = pct_pwm;
	}
	set_motor_speed(pwm_left, pwm_right);
}

// set the motor speed in percentages
void set_motor_speed(int pct_pwm_left, int pct_pwm_right)
{
	int pwm_right = 1024.0 / 100 * pct_pwm_right;
	int pwm_left = 1024.0 / 100 * pct_pwm_left;
	if (motor_balance > 0)
	{
		pwm_right = double(1024 / 100 * pct_pwm_right) / 100 * (100 + motor_balance);
		pwm_left = 1024.0 / 100 * pct_pwm_left;
	}
	else if (motor_balance < 0)
	{
		pwm_right = 1024.0 / 100 * pct_pwm_right;
		pwm_left = double(1024 / 100 * pct_pwm_left) / 100 * (100 - motor_balance);
	}
	pwmWrite(PWM_forward_right, pwm_right);
	pwmWrite(PWM_forward_left, pwm_left);
}

// ramp up the motor speed from 0% to 100%
bool motors_start()
{
	for (pct_pwm = 0; pct_pwm < motor_max_speed; ++pct_pwm)
	{
		set_motor_speed(pct_pwm, pct_pwm);
		delay(25);
	}
	return true;
}

// ramp down the motor speed from 100% to 0%
bool motors_stop()
{
	for (pct_pwm = motor_max_speed; pct_pwm >= 0; --pct_pwm)
	{
		set_motor_speed(pct_pwm, pct_pwm);
		delay(25);
	}
	return true;
}

// test the motor side and direction
bool test_motor(direction motor)
{
	for (pct_pwm = 0; pct_pwm < 50; ++pct_pwm)
	{
		if (motor == LEFT)
		{
			set_motor_speed(pct_pwm, 0);
		}
		else if (motor == RIGHT)
		{
			set_motor_speed(0, pct_pwm);
		}
		delay(25);
	}
	delay(1000);
	for (pct_pwm = 50; pct_pwm >= 0; --pct_pwm)
	{
		if (motor == LEFT)
		{
			set_motor_speed(pct_pwm, 0);
		}
		else if (motor == RIGHT)
		{
			set_motor_speed(0, pct_pwm);
		}
		delay(25);
	}
	return true;
}

/* data functions */

// get data from the config file
string get_config_data(string config_line, string old_data)
{
	string line;
	config_line = config_line + "=";
	ifstream config_file("/home/pi/robotOS/data/config.data");
	while (getline(config_file, line))
	{
		if (line.find(config_line) != string::npos)
		{
			config_file.close();
			line.replace(line.find(config_line), config_line.length(), "");
			return line;
		}
	}
	config_file.close();
	printf("[ERROR] Reading data error.\n");
	return old_data;
}

// write live data to the live data file
void write_live_data()
{
	string program_status = "standby";
	if (STATUS == STANDBY)
	{
		program_status = "standby";
	}
	else if (STATUS == STARTING)
	{
		program_status = "starten";
	}
	else if (STATUS == STEP1)
	{
		program_status = "active";
	}
	else if (STATUS == STOPING)
	{
		program_status = "stoppen";
	}
	else if (STATUS == TESTINGMOTORRIGHT)
	{
		program_status = "motor test rechts";
	}
	else if (STATUS == TESTINGMOTORLEFT)
	{
		program_status = "motor test links";
	}
	ofstream data_file("/home/pi/robotOS/data/live.data");
	data_file << "camera_angle=" << camera_angle
			  << "\ncamera_offset=" << camera_offset
			  << "\nbattery_status=" << battery_status
			  << "\nprogram_status=" << program_status
			  << "\n";
	data_file.close();
}

// read data from the config file
void load_config_data()
{
	bottom_color = stoi(get_config_data("bottom_color", to_string(bottom_color)));
	top_color = stoi(get_config_data("top_color", to_string(top_color)));
	motor_balance = stoi(get_config_data("motor_balance", to_string(motor_balance)));
	motor_max_speed = stoi(get_config_data("motor_max_speed", to_string(motor_max_speed)));
	string event = get_config_data("send_event", old_send_event);
	if (old_send_event != event)
	{
		old_send_event = event;
		if (event == "START")
		{
			EVENT = START;
		}
		else if (event == "STOP")
		{
			EVENT = STOP;
		}
		else if (event == "TESTMOTORRIGHT")
		{
			EVENT = TESTMOTORRIGHT;
		}
		else if (event == "TESTMOTORLEFT")
		{
			EVENT = TESTMOTORLEFT;
		}
		else if (event == "SAVELINEPREVIEW")
		{
			save_line_preview = true;
		}
	}
}

// Ticker
void ticker(int time)
{
	while (true)
	{
		delay(time);
		write_live_data();
		load_config_data();
	}
}