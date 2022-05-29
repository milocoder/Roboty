/*  robotOS v1.0
 *  file: main.js
 *  last update: 29-05-2022
 *  author: Richard Willems
*/

// shows the overview page
function show_overview() {
  document.getElementById("settings").style.left = "100%";
  document.getElementById("overview_button").style.color = "#3478F6";
  document.getElementById("settings_button").style.color = "#8A8A8D";
}
// shows the settings page
function show_settings() {
  document.getElementById("settings").style.left = "0%";
  document.getElementById("overview_button").style.color = "#8A8A8D";
  document.getElementById("settings_button").style.color = "#3478F6";
}

// changes the top bar when scrolling in overview.
function overview_scroll() {
  top_bar = document.getElementById("top_bar_overview");
  text = document.getElementById("top_bar_text_overview");

  if (document.getElementById("overview_scroll_box").scrollTop > 28) {
    top_bar.className = "top_bar top_bar_state_2";
    text.hidden = false;
  } else {
    top_bar.className = "top_bar top_bar_state_1";
    text.hidden = true;
  }
}

// changes the top bar when scrolling in settings.
function settings_scroll() {
  top_bar = document.getElementById("top_bar_settings");

  if (document.getElementById("settings_scroll_box").scrollTop > 1) {
    top_bar.className = "top_bar top_bar_state_2";
  } else {
    top_bar.className = "top_bar top_bar_state_1";
  }
}

// server send event

var eventSource = new EventSource("/data");

eventSource.addEventListener("message", function (e) {
  console.log(e.data);
}, false)

eventSource.addEventListener("data", function (e) {
  data = JSON.parse(e.data);
  document.getElementById("battery_status").innerHTML = data.battery_status;
  document.getElementById("camera_angle").innerHTML = data.camera_angle;
  document.getElementById("camera_offset").innerHTML = data.camera_offset;
  document.getElementById("camera_fps").innerHTML = data.camera_fps;
  document.getElementById("program_runtime_m").innerHTML = data.runtime_m;
  document.getElementById("program_runtime_s").innerHTML = data.runtime_s;
  document.getElementById("cpu_temp").innerHTML = data.cpu_temp;
  document.getElementById("program_status").innerHTML = data.program_status;
  if (data.battery_status < 20) {
    document.getElementById("battery_icon").innerHTML = "&#xf243;";
    document.getElementById("battery_color").style.color = "#EB4D3D";
  } else if (data.battery_status < 50) {
    document.getElementById("battery_icon").innerHTML = "&#xf242;";
    document.getElementById("battery_color").style.color = "#64DA64";
  } else if (data.battery_status < 85) {
    document.getElementById("battery_icon").innerHTML = "&#xf241;";
    document.getElementById("battery_color").style.color = "#64DA64";
  } else {
    document.getElementById("battery_icon").innerHTML = "&#xf240;";
    document.getElementById("battery_color").style.color = "#64DA64";
  }

  if (data.program_status == "standby" && toggle_wait == 0) {
    document.getElementById("start_stop_switch").checked = false;
    document.getElementById("start_stop_switch").disabled = false;
  } else if (data.program_status == "active" && toggle_wait == 0) {
    document.getElementById("start_stop_switch").checked = true;
    document.getElementById("start_stop_switch").disabled = false;
  } else if (toggle_wait == 0) {
    document.getElementById("start_stop_switch").disabled = true;
  }
}, true)

// poweroff
function poweroff() {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "/poweroff", true);
  xhttp.send();
}

toggle_wait = 0;
// start/stop the main program
function set_program_state(toggle_switch) {
  toggle_wait = 1;
  var xhttp = new XMLHttpRequest();
  if (toggle_switch.checked == true) {
    xhttp.open("GET", "/sendevent?event=START", true);
    xhttp.send();
  } else if (toggle_switch.checked == false) {
    xhttp.open("GET", "/sendevent?event=STOP", true);
    xhttp.send();
  }
  setTimeout(function () {
    toggle_wait = 0;
    xhttp.open("GET", "/sendevent?event=NOTHING", true);
    xhttp.send();
  }, 1000);
}

//button click animation
function button_click(button) {
  button.style.backgroundColor = "#D1D1D5";
  setTimeout(function () {
    button.style.backgroundColor = "";
  }, 400);
}