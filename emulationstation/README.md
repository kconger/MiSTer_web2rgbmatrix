# RetroPie Setup Instructions

**Install dependencies**
```
sudo apt-get install inotify-tools
```

**Install Emulationstation Scripts**
```
cp -rpf scripts /home/pi/.emulationstation/
```

**Install Init Script**
```
cp -rpf web2rgbmatrix /etc/init.d/
```

**Enable Service**
```
sudo update-rc.d web2rgbmatrix defaults
```

**Start Service**
```
service web2rgbmatrix start
```