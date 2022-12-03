#!/bin/bash
. /etc/web2rgbmatrix/web2rgbmatrix-system.ini
. /etc/web2rgbmatrix/web2rgbmatrix-user.ini

SYSTEM=$1
sed `cat /etc/web2rgbmatrix/corename.map | awk '{print "-e s/"$1"/"$3"/"}'`<<<"$SYSTEM" > $CORENAMEFILE
