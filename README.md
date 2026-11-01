### Emerson / Vertiv R48 controller
This is a controller program designed to set various parameters (online and offline) of the Emerson or Vertiv R48 high performance rectifiers.

The code is entirely based on a [endless-sphere.com forum post](https://endless-sphere.com/sphere/threads/emerson-vertiv-r48-series-can-programming.114785/page-5) and was slightly modified to work on an ESP8266. It provides an API and beautiful web interface from where you can change various R48 parameters in runtime.

<p align="center">
  <img src="images/web-top.png" width="480px" />
</p>

### Hardware

The R48 unit is accessible via the CAN bus. To use this project to control the R48, get one ESP8266 board like a Wemos D1 m1n1 or a Lolin NodeMCU v3 and a MCP2515 board. These are readily available on a known chinese online shop.

<p align="center">
  <img src="images/lolin-nodemcu-v3-board.jpg" width="300px"/> <img src="images/mcp2515-board.jpg" width="300pc" />
</p>

<p align="center">
  <img src="images/d1_mini-board.jpg" width="300px"/> <img src="images/mcp2515-board.jpg" width="300pc" />
</p>

Connect the ESP8266 board to the MCP2515 board using the following pins. You can change them in the code if you want.

| Lolin d1 mini | MCP2515 (pin)|
----------------|---------------
| D7            | SI      (5)  | 
| D6            | SO      (4)  |
| D5            | SCK     (6)  |
| D8            | CS      (3)  | 
| +3.3V         | VCC     (1)  | 
| GND           | GND     (2)  | 
| not connected | INT     (7)  |

Optionally connect the Nokia 3310 LCD board using the following pins

| Lolin D1 mini | LCD 3310 (pin)| 
----------------|----------------
| D7            | SDSI    (6)   |
| D5            | SCLK    (7)   |
| D4            | LED     (8)   | 
| D3            | RES     (4)   | 
| D2            | SCE     (3)   | 
| D1            | D/C     (5)   | 
| +3.3V         | VCC     (1)   |
| GND           | GND     (2)   |

Here's a reference pinout of the Lolin NodeMCU v3 board.
<p align="center">
  <img src="images/lolin-nodemcu-v3.jpg" width="480px" />
</p>

Additionally there's now support to use a Nokia 3310 LCD (red board) as display the output of the unit, as well as the IP address and WiFi status. (**this is entirely optional**)
<p align="center">
  <img src="images/lcd3310-red-board.jpg" width="480px" />
</p>

### Building the project
The simplest way is to install **Platform IO**, which will load the project and take care of all the dependencies.

You can also use the **Arduino IDE** and install these dependencies manually:
* expressif 8266 core 4.2.1
* mcp_can 1.5.1
* ESP32Async/ESPAsyncTCP 2.0.0
* ESP32Async/ESPAsyncWebServer 3.8.1


### References
* The [endless-sphere.com forum post](https://endless-sphere.com/sphere/threads/emerson-vertiv-r48-series-can-programming.114785/page-5)
* ESPHome [Emerson Vertiv R48 Component](https://github.com/leodesigner/esphome-emerson-vertiv-r48/)