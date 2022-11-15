# Install and Setup

ESP32-Trinity Setup
-------

Flash the Arduino sketch to the ESP32-Trinity using Arduino IDE. If using another ESP32 board you may need to adjust pin assignments in the ino file before flashing.

rgbmatrix SD Card Setup
-------

Format an SD card as FAT.  Copy the "static" and "animated" folders from the "128x32" folder of the [Marquee GIF Repo](https://github.com/h3llb3nt/marquee_gifs) to root of your SD card.  The update_web2rgbmatrix script has an option to install and update the GIFs on the rgbmatrix as well. Its slow and not recommended at this time.

Example SD card folder structure:

```
.
├── animated
│   ├── 0
│   ├── 1
│   │   └── 1944.gif
│   ├── 2
│   ├── 3
│   ├── 4
│   ├── 5
│   ├── 6
│   ├── 7
│   ├── 8
│   ├── 9
│   ├── A
│   ├── B
│   ├── C
│   ├── D
│   ├── E
│   ├── F
│   ├── G
│   ├── H
│   ├── I
│   ├── J
│   ├── K
│   ├── L
│   ├── M
│   ├── N
│   ├── O
│   ├── P
│   ├── Q
│   ├── R
│   ├── S
│   ├── T
│   ├── U
│   ├── V
│   ├── W
│   ├── X
│   ├── Y
│   └── Z
└── static
    ├── 0
    ├── 1
    │   └── 1944.gif
    ├── 2
    ├── 3
    ├── 4
    ├── 5
    ├── 6
    ├── 7
    ├── 8
    ├── 9
    ├── A
    ├── B
    ├── C
    ├── D
    ├── E
    ├── F
    ├── G
    ├── H
    ├── I
    ├── J
    ├── K
    ├── L
    ├── M
    ├── N
    ├── O
    ├── P
    ├── Q
    ├── R
    ├── S
    ├── T
    ├── U
    ├── V
    ├── W
    ├── X
    ├── Y
    └── Z
```

rgbmatrix Setup
-------

Initially, the rgbmatrix starts up in AP mode with an SSID of "rgbmatrix" and the password "password".  Once connected to its SSID go to [http://rgbmatrix.local/](http://rgbmatrix.local/) in your web browser and configure the wifi client to connect to your Wifi infrastructure.

You will want to create a DHCP reservation for your rgbmatrix so that its IP does not change. Take note of your rgbmatrix IP, you will need it to configure the Linux service later.

rgbmatrix Test
-------

Test your rgbmatrix by browsing to [http://rgbmatrix.local/localplay?file=MENU](http://rgbmatrix.local/localplay?file=MENU), the rgbmatrix should display the MiSTer marquee from the rgbmatrix SD card.

MiSTer Setup
-------
**Wireless Connection**

Copy linux/update_web2rgbmatrix.sh to your MiSTer Scripts folder: /media/fat/Scripts/

Run "update_web2rgbmatrix.sh" on your MiSTer.  This can be done from the console, ssh, or TV.

Modify your "/media/fat/web2rgbmatrix/web2rgbmatrix-user.ini" to include your rgbmatrix IP address or hostname. MiSTer does not support MDNS resolution.

```HOSTNAME="192.168.1.100"```

Optionally there are some other user reconfigurable options for the MiSTer service at the top of "/media/fat/web2rgbmatrix/web2rgbmatrix-system.ini". Copy any setting you would like to overwrite in your "/media/fat/web2rgbmatrix/web2rgbmatrix-user.ini" file.

**Serial Connection**

See [MiSTer_tty2x](https://github.com/venice1200/MiSTer_tty2x) project for software and setup instructions.

MiSTer Test
-------

Reboot your MiSTer and test by changing cores. 

OTA Updates
-------
To build an OTA update file, use the "Sketch"-->"Export Compiled Binary" menu option in the Arduino IDE.  The resulting "web2rgbmatrix.ino.bin" file will be under the build folder within the arduino project folder.

![matrix_ota_file_build](images/matrix-ota-file-build.jpg "matrix_ota_file_build")
