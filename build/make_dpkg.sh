#!/bin/sh

mkdir web2rgbmatrix_$(date +"%Y%m%d")-1_all
cp -rpf ../emulationstation/service/* web2rgbmatrix_$(date +"%Y%m%d")-1_all/
sed -i 's/VERSION/$(date +"%Y%m%d")/g' web2rgbmatrix_$(date +"%Y%m%d")-1_all/DEBIAN/control
dpkg-deb --build --root-owner-group web2rgbmatrix_20221125-1_all
