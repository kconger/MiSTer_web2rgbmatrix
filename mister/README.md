# MiSTer Install and Setup

MiSTer Setup
-------
**Wireless Connection**

Copy linux/update_web2rgbmatrix.sh to your MiSTer Scripts folder: /media/fat/Scripts/

Run "update_web2rgbmatrix.sh" on your MiSTer.  This can be done from the console, ssh, or TV.

Modify your "/media/fat/web2rgbmatrix/web2rgbmatrix-user.ini" to include your rgbmatrix IP address or hostname. MiSTer does not support MDNS resolution.

```
HOSTNAME="192.168.1.100"
```

Optionally there are some other user reconfigurable options for the MiSTer service at the top of "/media/fat/web2rgbmatrix/web2rgbmatrix-system.ini". Copy any setting you would like to overwrite in your "/media/fat/web2rgbmatrix/web2rgbmatrix-user.ini" file.

**Serial Connection**

See [MiSTer_tty2x](https://github.com/venice1200/MiSTer_tty2x) project for software and setup instructions.

MiSTer Test
-------

Reboot your MiSTer and test by changing cores. 

Update
-------
```
update_web2rgbmatrix.sh
```

Extras
-------

[MGL Files](mgl/)
