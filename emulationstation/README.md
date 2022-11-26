# RetroPie Install and Setup

Manual Install
-------

**Install Ddependencies**
```
sudo apt-get install inotify-tools
```

**Install Emulationstation Scripts**
```
sudo cp -rpf scripts /home/pi/.emulationstation/
sudo chown -R pi:pi /home/pi/.emulationstation/scripts
```

**Install Service Files**
```
sudo cp -rpf etc/web2rgbmatrix /etc/
sudo cp -rpf usr/local/bin/* /usr/local/bin/
sudo cp -rpf etc/init.d/web2rgbmatrix /etc/init.d/
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
sudo service web2rgbmatrix start
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
sudo dpkg -i web2rgbmatrix*.deb
```

**Modify Configuration**

Modify your /etc/web2rgbmatrix/web2rgbmatrix-user.ini to include your rgbmatrix IP or hostname. RetroPie does not support MDNS resolution by default.

```
HOSTNAME="192.168.1.100"
```

**Restart Service**
```
sudo service web2rgbmatrix restart
```

RetroPie Test
-------

Highlighting a system in Emulationstation should display the installed GIF or text of the rom name on the matrix.


Update
-------
```
sudo update_web2rgbmatrix.sh
```
