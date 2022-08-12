[MiSTer_web2rgbmatrix](https://github.com/kconger/MiSTer_web2rgbmatrix) is an hardware and software addon for the [MiSTer FPGA](https://github.com/MiSTer-devel). Heavily inspired by the [tty2rgbmatrix](https://github.com/h3llb3nt/tty2rgbmatrix) and [MiSTer_tty2oled](https://github.com/venice1200/MiSTer_tty2oled) projects.

This module displays the animated GIF logo of current the running core on a RGB Matrix.

Current features
-------
- Plays animated Core GIF when properly formatted GIF(128x32) is uploaded to http://rgbmatrix.local/play
- Turns off display if requesting mister is off for 1 minute
- Wifi AP or Infrastructure Mode
- Web interface to configure wifi and to see current status of Wifi mode, playing GIF and connected client IP
- Serial debug output

Requirements
-------

Hardware
- [ESP32 Trinity](https://esp32trinity.com/)
- (2) 64x32 HUB75 compatible RGB matrix or matrices. [ie.](https://www.aliexpress.com/item/3256801502846969.html)
- 5V Power Supply

Software
- Arduino IDE
- ESP32 Proper board support package
- Library Dependencies: AnimatedGIF, ArduinoJson, ESP32-HUB75-MatrixPanel-I2S, ESP32Ping

Linux/MiSTer service code from the [MiSTer_tty2oled](https://github.com/venice1200/MiSTer_tty2oled) project with minor modifications.

Install
-------

1. **Assemble**

Assemble the hardware

2. **MiSTer Setup**

Copy linux/web2rgbmatrix folder to root of your MiSTer_Data partition.

Copy your GIF files into the web2rgbmatrix/gifs/ directory.  Name the gif you want to use for the core, CoreName.gif.  The [tty2rgbmatrix](https://github.com/h3llb3nt/tty2rgbmatrix) project has a nice set of GIFs you can use.

Add the following to the bottom of MiSTer_Data/linux/user-startup.sh

```
/media/fat/web2rgbmatrix/S60web2rgbmatrix start
```

3. **Hardware Setup**

Flash the Arduino sketch to the board using Arduino IDE


4. **Configure rgbmatrix**

Initially the rgbmatrix starts up in AP mode with a SSID of 'rgbmatrix' and the password 'password'.  Once connected go to http://rgbmatrix.local/ in your web browser and configure the wifi client to connect to your Wifi infrastructure.

If using the rgbmatrix in Wifi infrastructure mode, you'll want to create a DHCP reservation on your DHCP server so that your IP doesn't change. Add this IP to the 'HOSTNAME' variable in the web2rgbmatrix.conf file.
