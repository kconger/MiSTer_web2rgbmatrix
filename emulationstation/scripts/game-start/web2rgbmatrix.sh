#!/bin/bash
. /etc/web2rgbmatrix/web2rgbmatrix-system.ini
. /etc/web2rgbmatrix/web2rgbmatrix-user.ini

ROM_PATH=$1
ROM_NAME=$2
GAME_NAME=$3

#Only set if a Mame rom
if grep -qi mame <<<"$ROM_PATH"; then
    echo "$ROM_NAME" > $CORENAMEFILE
fi

