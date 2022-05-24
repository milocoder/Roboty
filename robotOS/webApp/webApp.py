# robotOS v1.0
# file: webApp.py
# last update: 05-05-2022

# imports
import os
import time
import json
from gpiozero import CPUTemperature
from gevent.pywsgi import WSGIServer
from flask import Flask, Response, render_template, send_file, request
from gevent import monkey
monkey.patch_all()

# global variables
start_time = time.time()
cpu = CPUTemperature()
battery_status = 100
camera_angle = 0
camera_offset = 0
program_status = "standby"

# App config.
app = Flask(__name__)

# main function
def main():
    write_data_to_config("send_event", "NOTHING")
    http_server = WSGIServer(("0.0.0.0", 80), app)
    http_server.serve_forever()

# main html
@app.route("/", methods=['GET'])
def home():
    return render_template('main.html')

# info html
@app.route("/info", methods=['GET'])
def info():
    robot_name = get_data_from_file("/home/pi/robotOS/data/config.data", "robot_name" , "none")
    templateData = {
        'robot_name': robot_name
    }
    return render_template('info.html', **templateData)

# camera html
@app.route("/camera", methods=['GET'])
def camera():
    top_color = get_data_from_file("/home/pi/robotOS/data/config.data", "top_color" , "none")
    bottom_color = get_data_from_file("/home/pi/robotOS/data/config.data", "bottom_color" , "none")
    templateData = {
        'top_color': top_color,
        'bottom_color': bottom_color
    }
    return render_template('camera.html', **templateData)

# motors html
@app.route("/motors", methods=['GET'])
def motors():
    motor_max_speed = get_data_from_file("/home/pi/robotOS/data/config.data", "motor_max_speed", "none")
    motor_balance = get_data_from_file("/home/pi/robotOS/data/config.data", "motor_balance", "none")
    templateData = {
        'motor_max_speed': motor_max_speed,
        'motor_balance': motor_balance
    }
    return render_template('motors.html', **templateData)

# server send event
@app.route("/data")
def listen():

    def respond_to_client():
        global start_time, battery_status, camera_angle, camera_offset, program_status
        while True:
            time_now = time.time()
            runtime = int(time_now - start_time)
            runtime_m, runtime_s = divmod(runtime, 60)
            cpu_temp = get_cpu_temp()
            battery_status = int(get_data_from_file(
                "/home/pi/robotOS/data/live.data", "battery_status", f"{battery_status}"))
            camera_angle = int(get_data_from_file(
                "/home/pi/robotOS/data/live.data", "camera_angle", f"{camera_angle}"))
            camera_offset = int(get_data_from_file(
                "/home/pi/robotOS/data/live.data", "camera_offset", f"{camera_offset}"))
            program_status = get_data_from_file(
                "/home/pi/robotOS/data/live.data", "program_status", f"{program_status}")
            _data = json.dumps({"battery_status": battery_status,
                                "camera_angle": camera_angle,
                                "camera_offset": camera_offset,
                                "program_status":program_status,
                                "runtime_m": runtime_m,
                                "runtime_s": runtime_s,
                                "cpu_temp": cpu_temp})
            yield f"id: 1\ndata: {_data}\nevent: data\n\n"
            time.sleep(0.5)
    return Response(respond_to_client(), mimetype='text/event-stream')

# poweroff
@app.route('/poweroff')
def poweroff():
    os.system("sudo poweroff")
    return "ok"

# send event
@app.route('/sendevent')
def send_event():
    event = request.args.get('event', default="NOTHING", type=str)
    write_data_to_config("send_event", event)
    return "ok"

# save the info settings
@app.route('/save-info')
def save_info():
    name = request.args.get('name', default="windesheim", type=str)
    write_data_to_config("robot_name", name)
    return "ok"

# save the camera settings
@app.route('/save-camera')
def save_camera():
    top_color = request.args.get('top_color', default=0, type=int)
    bottom_color = request.args.get('bottom_color', default=255, type=int)
    write_data_to_config("top_color", top_color)
    write_data_to_config("bottom_color", bottom_color)
    return "ok"

# get the camera preview image
@app.route('/camera-preview.png')
def get_camera_preview():
    filename = '/home/pi/robotOS/data/images/camera-preview.png'
    return send_file(filename, mimetype='image/png')

# save the motor settings
@app.route('/save-motors')
def save_motors():
    motor_max_speed = request.args.get('motor_max_speed', default=50, type=int)
    motor_balance = request.args.get('motor_balance', default=0, type=int)
    write_data_to_config("motor_max_speed", motor_max_speed)
    write_data_to_config("motor_balance", motor_balance)
    return "ok"

# icon favicon
@app.route('/favicon.ico')
def get_icon():
    filename = '/home/pi/robotOS/webApp/static/images/favicon.ico'
    return send_file(filename, mimetype='image/ico')

# apple app icon 120
@app.route('/apple-touch-icon-120x120-precomposed.png')
def get_apple_icon_120():
    filename = '/home/pi/robotOS/webApp/static/images/apple-touch-icon-120x120-precomposed.png'
    return send_file(filename, mimetype='image/png')

# apple app icon 152
@app.route('/apple-touch-icon-152x152-precomposed.png')
def get_apple_icon_152():
    filename = '/home/pi/robotOS/webApp/static/images/apple-touch-icon-152x152-precomposed.png'
    return send_file(filename, mimetype='image/png')

 # get data from a data file
def get_data_from_file(file_name: str, data_line: str, old_data: str):
    try:
        with open(file_name, "r") as data_file:
            lines = data_file.readlines()
        for line in lines:
            if f"{data_line}=" in line:
                data = line.replace("\n", "")
                data = data.replace(f"{data_line}=", "")
                return data
        print("[ERROR] Reading data error.")
        return old_data
    except:
        print("[ERROR] Reading data error.")
        return old_data

# write data to the config file
def write_data_to_config(config_line: str, new_data):
    with open("/home/pi/robotOS/data/config.data", "r") as config_file:
        lines = config_file.readlines()
    for line in lines:
        if f"{config_line}=" in line:
            line = line.replace("\n", "")
            replace_in_file("/home/pi/robotOS/data/config.data",
                            line, f"{config_line}={new_data}")
            return
    with open("/home/pi/robotOS/data/config.data", "a+") as config_file:
        config_file.write(f"{config_line}={new_data}\n")

# replace data in a text/data file
def replace_in_file(file: str, one: str, two: str):
    with open(file, "r") as reading_file:
        new_file_content = ""
        for line in reading_file:
            stripped_line = line.strip()
            new_line = stripped_line.replace(one, two)
            new_file_content += f"{new_line}\n"
    with open(file, "w") as writing_file:
        writing_file.write(new_file_content)

# get the temperature of the CPU
def get_cpu_temp():
    temp = int(cpu.temperature)
    return temp


# start the main function
if __name__ == "__main__":
    main()
