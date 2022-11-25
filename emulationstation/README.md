# RetroPie Setup Instructions

Manual Install
-------

**Install dependencies**
```
sudo apt-get install inotify-tools
```

**Install Emulationstation Scripts**
```
cp -rpf scripts /home/pi/.emulationstation/
```

**Install Files**
```
cp -rpf etc/web2rgbmatrix /etc/
cp -rpf usr/local/bin/* /usr/local/bin/
cp -rpf etc/init.d/web2rgbmatrix /etc/init.d/
```

**Modify Configuration**

Modify your /etc/web2rgbmatrix/web2rgbmatrix-user.ini to include your rgbmatrix IP or hostname.

**Enable Service**
```
sudo update-rc.d web2rgbmatrix defaults
```

**Start Service**
```
service web2rgbmatrix start
```
