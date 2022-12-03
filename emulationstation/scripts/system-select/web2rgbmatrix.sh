#!/bin/bash
. /etc/web2rgbmatrix/web2rgbmatrix-system.ini
. /etc/web2rgbmatrix/web2rgbmatrix-user.ini

SYSTEM=$1
sed -f <(sed 's/=> //;s# #/#;s#$#/#;s#^#s/#' /etc/web2rgbmatrix/corename.map)<<<"$SYSTEM" > $CORENAMEFILE
