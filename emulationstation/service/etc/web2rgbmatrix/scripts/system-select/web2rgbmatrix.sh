#!/bin/bash
. /etc/web2rgbmatrix/web2rgbmatrix-system.ini
. /etc/web2rgbmatrix/web2rgbmatrix-user.ini

SYSTEM=$1
echo "$SYSTEM" > $CORENAMEFILE
