# RetroPie Install and Setup

Manual Install
-------

**Install Ddependencies**
```
sudo apt-get install inotify-tools
```

**Install Emulationstation Scripts**
```
cp -rpf scripts /home/pi/.emulationstation/
```

**Install Service Files**
```
cp -rpf etc/web2rgbmatrix /etc/
cp -rpf usr/local/bin/* /usr/local/bin/
cp -rpf etc/init.d/web2rgbmatrix /etc/init.d/
```

**Modify Configuration**

Modify your /etc/web2rgbmatrix/web2rgbmatrix-user.ini to include your rgbmatrix IP or hostname. RetroPie does not support MDNS resolution by default.

```
HOSTNAME="192.168.1.100"
```

**Enable Service**
```
sudo update-rc.d web2rgbmatrix defaults
```

**Start Service**
```
service web2rgbmatrix start
```

dpkg Install
-------

**Build dpkg**
```
cd build
./make_dpkg.sh
```

**Install dpkg**
```
dpkg -i web2rgbmatrix*.deb
```

**Modify Configuration**

Modify your /etc/web2rgbmatrix/web2rgbmatrix-user.ini to include your rgbmatrix IP or hostname. RetroPie does not support MDNS resolution by default.

```
HOSTNAME="192.168.1.100"
```

**Restart Service**
```
service web2rgbmatrix restart
```

RetroPie Test
-------

Highlighting a system in Emulationstation should display the installed GIF or text of the rom name on the matrix.


Update
-------
```
sudo update_web2rgbmatrix_script.sh
```
