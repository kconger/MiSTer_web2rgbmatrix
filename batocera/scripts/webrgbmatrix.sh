#!/bin/bash
# This is an example file of how gameStart and gameStop events can be used for web2rgbmatrix.

# these values will be passed to the script

# gameStop mame libretro mame /userdata/roms/mame/bublbobl.zip
 
# It's good practice to set all constants before anything else.
logfile=/tmp/scriptlog.txt
hostname=webrgbmatrix.local
event=$1
emulator=$2
gamename=$5

echo "" >> $logfile

# write all parameters to logfile.
echo "$@" >> $logfile

echo "$emulator" >> $logfile

# Case selection for first parameter parsed, our event.
case $event in
    gameStart)
	case $emulator in
	    mame)
                # Commands in here will be executed on the start of any game.
                echo "START" >> $logfile
                echo "$gamename" >> $logfile

                filename=$(basename "$gamename")

	            echo "$filename" >> $logfile

	            giffile=${filename%%.*}
	
                echo "$giffile" >> $logfile

	            curl http://$hostname/localplay?file=$giffile
            ;;
	    snes)
                # Commands in here will be executed on the start of any game.
                echo "START" >> $logfile
                echo "snes" >> $logfile

                curl http://$hostname/localplay?file=SNES
	    ;;
	    c64)
                # Commands in here will be executed on the start of any game.
                echo "START" >> $logfile
                echo "c64" >> $logfile

                curl http://$hostname/localplay?file=C64
            ;;
	    amiga500)
                # Commands in here will be executed on the start of any game.
                echo "START" >> $logfile
                echo "amiga500" >> $logfile

                curl http://$hostname/localplay?file=Minimig
            ;;
	    n64)
                # Commands in here will be executed on the start of any game.
                echo "START" >> $logfile
                echo "n64" >> $logfile

                curl http://$hostname/localplay?file=n64
            ;;
        esac
	;;
    gameStop)
        # Commands here will be executed on the stop of every game.
        echo "END" >> $logfile
        # Show batocera gif on the matrix
        curl http://$hostname/localplay?file=batocera

    ;;
esac
