/* robotOS v1.0
 * file: main.cpp
 * last update: 29-05-2022
 * author: Richard Willems
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

#include <ctime>

using namespace cv;
using namespace std;

// pin definitions
#define PWM_forward_right 1 // GPIO 18
#define PWM_forward_left 23 // GPIO 13
#define BT_emergency_stop 0 //GPIO 17

// global variables
#define screen_width 160
#define screen_height 120
int camera_angle = 0;
int camera_offset = 0;
clock_t camera_fps = 0;
int battery_status = 100;
int motor_balance = 0;
int motor_max_speed = 1024;
float current_pwm = 0;
int bottom_color = 0;
int top_color = 0;
int motor_start_time = 0;
string old_send_event = "STOP";
bool save_previews = false;
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
	TESTINGMOTORLEFT,
	EMERGENCY_STOP
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
void motor_left(float pwm_left);
void motor_right(float pwm_right);
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
	wiringPiISR(BT_emergency_stop, INT_EDGE_RISING, emergency_stop);
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
			}
			else if (EVENT == TESTMOTORRIGHT)
			{
				EVENT = NOTHING;
				STATUS = TESTINGMOTORRIGHT;
			}
			else if (EVENT == TESTMOTORLEFT)
			{
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
			if (test_motor(RIGHT) == true)
			{
				EVENT = NOTHING;
				STATUS = STANDBY;
			}
			break;
		case TESTINGMOTORLEFT:
			if (test_motor(LEFT) == true)
			{
				EVENT = NOTHING;
				STATUS = STANDBY;
			}
			break;
		case EMERGENCY_STOP:
			printf("[INFO] Emergency stop button pushed.\n");
			current_pwm = 0;
			motor_left(0);
			motor_right(0);
			EVENT = NOTHING;
			STATUS = STANDBY;
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
	Point2f pts[4];
	int contour = 0;
	clock_t current_ticks, delta_ticks;

	while (true)
	{
		current_ticks = clock();
		// read frame
		cap.read(frame);
		// find the line by checking in a color range in the whole frame.
		inRange(frame, Scalar(bottom_color, bottom_color, bottom_color), Scalar(top_color, top_color, top_color), whiteLine);
		// make the line small en big again to remove noise.
		erode(whiteLine, whiteLine, Mat(), Point(-1, -1), 2, 1, 1);
		dilate(whiteLine, whiteLine, Mat(), Point(-1, -1), 2, 1, 1);
		// find the contours of the line.
		Mat contourOut = whiteLine.clone();
		findContours(contourOut, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
		// check if there is a line.
		if (contours.size() > 0)
		{
			if (contours.size() == 1)
			{
				contour = 0;
			}
			else
			{
				int lowest_point = screen_height;
				for (int i = 0; i < contours.size(); i++)
				{
					RotatedRect blackbox = minAreaRect(contours[i]);
					blackbox.points(pts);
					Point coordinate = pts[3];
					int contour_point = screen_height - coordinate.y;
					if (contour_point < lowest_point)
					{
						contour = i;
						lowest_point = contour_point;
					}
				}
			}
			// get the angle and corners of the line.
			RotatedRect blackbox = minAreaRect(contours[contour]);
			//  draw in the contours.
			drawContours(frame, contours, -1, Scalar(0, 0, 255), 3);

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

			// putText(frame, "angle:" + to_string(camera_angle), Point(10, 40), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
			// putText(frame, "offset:" + to_string(camera_offset), Point(10, 80), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
			// putText(frame, "fps:" + to_string(fps), Point(10, 120), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
			//  draw a line in the middel of the line.
			line(frame, Point(centerline, 200.0 / 320 * screen_height), Point(centerline, 250.0 / 320 * screen_height), Scalar(255, 0, 0), 3);
			// draw a line in the middel of the screen.
			line(frame, Point((screen_width / 2), 0), Point((screen_width / 2), screen_height), Scalar(255, 255, 255), 1);
		}
		// save previews of the frame and line for the webapp
		if (save_previews == true)
		{
			imwrite("/home/pi/robotOS/data/images/camera-preview.png", frame);
			imwrite("/home/pi/robotOS/data/images/line-preview.png", whiteLine);
			save_previews = false;
		}

		// show the frame / line
		imshow("Live", frame);
		imshow("Black/White", whiteLine);
		waitKey(1);
		// fps counter
		delta_ticks = clock() - current_ticks;
		if (delta_ticks > 0)
			camera_fps = CLOCKS_PER_SEC / delta_ticks;
	}
}

/* motor functions */
// main drive function
void drive()
{
	int steering_offset = 100.0 / (screen_width / 2 + 90) * (camera_offset + camera_angle);
	printf("%d\n", steering_offset);
	int pwm_right = current_pwm;
	int pwm_left = current_pwm;
	if (steering_offset > 0)
	{
		pwm_right = current_pwm / 100 * (100 - steering_offset);
	}
	else if (steering_offset < 0)
	{
		pwm_left = current_pwm / 100 * (100 + steering_offset);
	}
	motor_left(pwm_left);
	motor_right(pwm_right);
}

// set the left motor speed
void motor_left(float pwm_left){
	if(motor_balance < 0){
		pwm_left = pwm_left / 100 * (100 + motor_balance);
	}
	pwmWrite(PWM_forward_left, pwm_left);
}

// set the right motor speed
void motor_right(float pwm_right){
	if(motor_balance > 0){
		pwm_right = pwm_right / 100 * (100 - motor_balance);
	}
	pwmWrite(PWM_forward_right, pwm_right);
}

// ramp up the motor speed from 0% to 100%
bool motors_start()
{
	for (current_pwm = 0; current_pwm < motor_max_speed; ++current_pwm)
	{
		motor_left(current_pwm);
		motor_right(current_pwm);
		delay(motor_start_time);
	}
	return true;
}

// ramp down the motor speed from 100% to 0%
bool motors_stop()
{
	for (current_pwm = motor_max_speed; current_pwm >= 0; --current_pwm)
	{
		motor_left(current_pwm);
		motor_right(current_pwm);
		delay(motor_start_time);
	}
	return true;
}

// test the motor side and direction
bool test_motor(direction motor)
{
	for (current_pwm = 0; current_pwm < 512; ++current_pwm)
	{
		if (motor == LEFT)
		{
			motor_left(current_pwm);
		}
		else if (motor == RIGHT)
		{
			motor_right(current_pwm);
		}
		delay(motor_start_time);
	}
	delay(1000);
	for (current_pwm = 512; current_pwm >= 0; --current_pwm)
	{
		if (motor == LEFT)
		{
			motor_left(current_pwm);
		}
		else if (motor == RIGHT)
		{
			motor_right(current_pwm);
		}
		delay(motor_start_time);
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
		program_status = "test motor rechts";
	}
	else if (STATUS == TESTINGMOTORLEFT)
	{
		program_status = "test motor links";
	}
	ofstream data_file("/home/pi/robotOS/data/live.data");
	data_file << "camera_angle=" << camera_angle
			  << "\ncamera_offset=" << camera_offset
			  << "\ncamera_fps=" << camera_fps
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
	motor_start_time = stoi(get_config_data("motor_start_time", to_string(motor_start_time)));
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
		else if (event == "SAVEPREVIEWS")
		{
			save_previews = true;
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
// Emergency stop button
void emergency_stop(){
	STATUS = EMERGENCY_STOP;
}