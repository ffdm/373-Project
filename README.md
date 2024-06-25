# Need for Speed
Finn Moore, Dimash Umirbayev

<p align="left">
  <img src="https://github.com/ffdm/need-for-speed/blob/master/assets/cars.jpg" width="400" title="Demo Image">
</p>

## Idea
- Create fun and exciting remote-controlled cars with live video streaming
- Compete in a fast-paced racing game and explore inaccessible locations

## System Architecture
<p align="left">
  <img src="https://github.com/ffdm/need-for-speed/blob/master/assets/architecture.png" width="550" title="Architecture Diagram">
</p>

## Controls
PS2 controller hacked over SPI
<p align="left">
  <img src="https://github.com/ffdm/need-for-speed/blob/master/assets/controller.png" width="350" title="Controller Image">
</p>

- Custom PID module in C
- Speed control (PWM) and direction (H-Bridge) controlled with right joystick
- Camera panning servos controlled with left joystick
- Camera position reset: `X` button, LED flashlight toggle: `O` button
- Start race with `Start` button

## Wireless Transmission
<p align="left">
  <img src="https://github.com/ffdm/need-for-speed/blob/master/assets/video_transmission.png" width="600" title="Video Transmission Diagmam">
</p>

- XBee modules on base station and each car wirelessly transmit via UART protocol
- ESP32-CAM module records, compresses (JPG), and transmits video over UDP

## Finish Line Detection
- The blue finish line is detected using the color connected components mode on PixyCam modules on each Car
- Sends data over XBee indicating which car crossed the finish line
- When this data is received, a UART Interrupt is triggered on the base station stopping the race and displaying winner
