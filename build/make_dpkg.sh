#!/bin/bash
# Creates a Debian package
DATE=$(date +"%Y%m%d")
mkdir -p web2rgbmatrix_$DATE-1_all/home/pi/.emulationstation/
cp -rpf ../emulationstation/scripts web2rgbmatrix_$DATE-1_all/home/pi/.emulationstation/
cp -rpf ../emulationstation/etc web2rgbmatrix_$DATE-1_all/
cp -rpf ../emulationstation/usr web2rgbmatrix_$DATE-1_all/
cp -rpf ../emulationstation/DEBIAN web2rgbmatrix_$DATE-1_all/
sed -i "s/VERSION/$DATE/g" web2rgbmatrix_$DATE-1_all/DEBIAN/control
dpkg-deb --build --root-owner-group web2rgbmatrix_$DATE-1_all
