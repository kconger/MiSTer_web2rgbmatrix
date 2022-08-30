# web2rgbmatrix
[MiSTer_web2rgbmatrix](https://github.com/kconger/MiSTer_web2rgbmatrix) is an hardware and software addon for the [MiSTer FPGA](https://github.com/MiSTer-devel) that displays an animated or static GIF logo of the current running [MiSTer FPGA](https://github.com/MiSTer-devel) core on an RGB LED Matrix.

![matrix_on_](docs/images/matrix-on.jpg "matrix_on")

Current features
-------
- Display active MiSTer Core logo
- Store GIFs on an SD Card or on the MiSTer, storing on the MiSTer introduces a delay due to the transfer.
- Turns off the display if requesting client is off for 1 minute
- Wifi AP or Infrastructure Mode
- Web interface to configure wifi and to see the current status of Wifi mode, currently loaded GIF, and connected client IP
- Displays network config at boot for 1 minute
- OTA updates
- Serial debug output

Requirements
-------
Hardware
- [ESP32 Trinity](https://esp32trinity.com/) or other ESP32 with WiFi
- (2) 64x32 HUB75 compatible RGB matrix or matrices. [ie.](https://www.aliexpress.com/item/3256801502846969.html)
- SD Card module
- SD Card
- 5V Power Supply

Software
- Arduino IDE
- ESP32 Proper board support package
- Library Dependencies: AnimatedGIF, ArduinoJson, ESP32-HUB75-MatrixPanel-I2S, ESP32Ping

Install
-------
**ESP32 Setup**

Flash the Arduino sketch to the board using Arduino IDE.

**MiSTer Setup**

Copy linux/web2rgbmatrix folder to root of your MiSTer_Data partition.

Copy your GIF files into a "gifs" folder at the root of your matrix SD card.  If not using an SD card in your matrix, copy them somewhere your MiSTer can access.  GIF file names must match the core name exactly, ie. Minimig.gif.  Included are some static core logos.  The [tty2rgbmatrix](https://github.com/h3llb3nt/tty2rgbmatrix) project has a nice set of animated GIFs you can use as well.

Modify your 'web2rgbmatrix.conf'

Add the following to the bottom of MiSTer_Data/linux/user-startup.sh

```
/media/fat/web2rgbmatrix/S60web2rgbmatrix start
```

**Configure rgbmatrix**

Initially, the rgbmatrix starts up in AP mode with an SSID of 'rgbmatrix' and the password 'password'.  Once connected to its SSID go to http://rgbmatrix.local/ in your web browser and configure the wifi client to connect to your Wifi infrastructure.
 
If using the rgbmatrix in Wifi infrastructure mode, you'll want to create a DHCP reservation on your DHCP server so that your IP doesn't change. Add this IP to the 'HOSTNAME' variable in the web2rgbmatrix.conf file.


Credits
-------
Inspired by the [tty2rgbmatrix](https://github.com/h3llb3nt/tty2rgbmatrix) and [MiSTer_tty2oled](https://github.com/venice1200/MiSTer_tty2oled) projects.

Linux/MiSTer service code from the [MiSTer_tty2oled](https://github.com/venice1200/MiSTer_tty2oled) with minor modifications.
