# RetroPie Setup Instructions

Manual Install
-------

**Install Emulationstation Scripts**
```
cp -rpf scripts /home/pi/.emulationstation/
```

**Install Files**
```
cp -rpf etc/web2rgbmatrix /etc/
cp -rpf usr/local/bin/* /usr/local/bin/
```

**Modify Configuration**
Edit /etc/web2rgbmatrix/web2rgbmatrix-user.ini to include rgbmatrix IP or Hostname
