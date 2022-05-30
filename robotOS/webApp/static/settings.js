/*  robotOS v1.0
 *  file: settings.js
 *  last update: 29-05-2022
 *  author: Richard Willems
*/

// changes the top bar when scrolling.
function settings_scroll() {
  top_bar = document.getElementById("top_bar");
  if (document.getElementById("scroll_box").scrollTop > 1) {
    top_bar.className = "top_bar top_bar_state_2";
  } else {
    top_bar.className = "top_bar top_bar_state_1";
  }
}

//button click animation
function button_click(button) {
  button.style.backgroundColor = "#D1D1D5";
  setTimeout(function () {
    button.style.backgroundColor = "";
  }, 400);
}