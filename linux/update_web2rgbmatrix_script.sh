#!/bin/bash
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# You can download the latest version of this script from:
# https://github.com/kconger/MiSTer_web2rgbmatrix

! [ -e /media/fat/web2rgbmatrix/web2rgbmatrix-user.ini ] && touch /media/fat/web2rgbmatrix/web2rgbmatrix-user.ini
. /media/fat/web2rgbmatrix/web2rgbmatrix-system.ini
. /media/fat/web2rgbmatrix/web2rgbmatrix-user.ini

# Check for and create web2rgbmatrix script folder
[[ -d ${WEB2RGBMATRIX_PATH} ]] && cd ${WEB2RGBMATRIX_PATH} || mkdir ${WEB2RGBMATRIX_PATH}

# Check and remount root writable if neccessary
if [ $(/bin/mount | head -n1 | grep -c "(ro,") = 1 ]; then
  /bin/mount -o remount,rw /
  MOUNTRO="true"
fi

if [ ! -e /media/fat/linux/user-startup.sh ] && [ -e /etc/init.d/S99user ]; then
  if [ -e /media/fat/linux/_user-startup.sh ]; then
    echo "Copying /media/fat/linux/_user-startup.sh to /media/fat/linux/user-startup.sh"
    cp /media/fat/linux/_user-startup.sh /media/fat/linux/user-startup.sh
  else
    echo "Building /media/fat/linux/user-startup.sh"
    echo -e "#!/bin/sh\n" > /media/fat/linux/user-startup.sh
    echo -e 'echo "***" $1 "***"' >> /media/fat/linux/user-startup.sh
  fi
fi
if [ $(grep -c "web2rgbmatrix" /media/fat/linux/user-startup.sh) = "0" ]; then
  echo -e "Add web2rgbmatrix to /media/fat/linux/user-startup.sh\n"
  echo -e "\n# Startup web2rgbmatrix" >> /media/fat/linux/user-startup.sh}
  echo -e "[[ -e ${INITSCRIPT} ]] && ${INITSCRIPT} \$1" >> /media/fat/linux/user-startup.sh
fi

echo -e "${fgreen}web2rgbmatrix update script"
echo -e "----------------------${freset}"
echo -e "${fgreen}Checking for available web2rgbmatrix updates...${freset}"


# init script
wget ${NODEBUG} "${REPOSITORY_URL}${REPO_BRANCH}/linux/web2rgbmatrix/S60web2rgbmatrix" -O /tmp/S60web2rgbmatrix
if  ! [ -f ${INITSCRIPT} ]; then
  if  [ -f ${INITDISABLED} ]; then
    echo -e "${fyellow}Found disabled init script, skipping Install${freset}"
  else
    echo -e "${fyellow}Installing init script ${fmagenta}S60web2rgbmatrix${freset}"
    mv -f /tmp/S60web2rgbmatrix ${INITSCRIPT}
    chmod +x ${INITSCRIPT}
  fi
elif ! cmp -s /tmp/S60web2rgbmatrix ${INITSCRIPT}; then
  if [ "${SCRIPT_UPDATE}" = "yes" ]; then
    echo -e "${fyellow}Updating init script ${fmagenta}S60web2rgbmatrix${freset}"
    mv -f /tmp/S60web2rgbmatrix ${INITSCRIPT}
    chmod +x ${INITSCRIPT}
  else
    echo -e "${fblink}Skipping${fyellow} available init script update because of the ${fcyan}SCRIPT_UPDATE${fyellow} INI-Option${freset}"
  fi
fi
[[ -f /tmp/S60web2rgbmatrix ]] && rm /tmp/S60web2rgbmatrix


# daemon
wget ${NODEBUG} "${REPOSITORY_URL}${REPO_BRANCH}/linux/web2rgbmatrix/${DAEMONNAME}" -O /tmp/${DAEMONNAME}
if  ! [ -f ${DAEMONSCRIPT} ]; then
  echo -e "${fyellow}Installing daemon script ${fmagenta}web2rgbmatrix${freset}"
  mv -f /tmp/${DAEMONNAME} ${DAEMONSCRIPT}
  chmod +x ${DAEMONSCRIPT}
elif ! cmp -s /tmp/${DAEMONNAME} ${DAEMONSCRIPT}; then
  if [ "${SCRIPT_UPDATE}" = "yes" ]; then
    echo -e "${fyellow}Updating daemon script ${fmagenta}web2rgbmatrix${freset}"
    mv -f /tmp/${DAEMONNAME} ${DAEMONSCRIPT}
    chmod +x ${DAEMONSCRIPT}
  else
    echo -e "${fblink}Skipping${fyellow} available daemon script update because of the ${fcyan}SCRIPT_UPDATE${fyellow} INI-Option${freset}"
  fi
fi
[[ -f /tmp/${DAEMONNAME} ]] && rm /tmp/${DAEMONNAME}

# GIFs
# Check for and create web2rgbmatrix gifs folder
[[ -d ${GIF_PATH} ]] && cd ${GIF_PATH} || mkdir ${GIF_PATH}
cd ${GIF_PATH}
if [ "${GIF_UPDATE}" = "yes" ]; then
  wget ${NODEBUG} -O - https://github.com/h3llb3nt/marquee_gifs/archive/master.tar.gz | tar xz --strip=2 "marquee_gifs-master/128x32"
else
  wget ${NODEBUG} -O - https://github.com/h3llb3nt/marquee_gifs/archive/master.tar.gz | tar xz --skip-old-files --strip=2 "marquee_gifs-master/128x32"
fi
if ! [ "${HOSTNAME}" = "rgbmatrix.local" ]; then
  if [ "${SD_INSTALLED}" = "true" ] && [ "${GIF_UPDATE}" = "yes" ]; then
    cd ${GIF_PATH}/../
    find gifs -type f -exec curl -u rgbmatrix:password --ftp-create-dirs -T {} ftp://${HOSTNAME}/{} \;
  else
    echo -e "${fblink}Skipping${fyellow} GIF SD Card update because of the ${fcyan}GIF_UPDATE${fyellow} INI-Option${freset}"
  fi
else
  echo -e "${fblink}Skipping${fyellow} GIF SD Card update because ${fcyan}HOSTNAME${fyellow} is not set in INI-Option${freset}"
fi

# Update ESP32-Trinity
cd /tmp
if ! [ "${HOSTNAME}" = "rgbmatrix.local" ]; then
  if [ "${TRINITY_UPDATE}" = "yes" ]; then
    LATEST=$(wget -q -O - "${REPOSITORY_URL}${REPO_BRANCH}/releases/LATEST")
    CURRENT=$(wget -q -O - "${HOSTNAME}/version")
    if (( $(echo "$LATEST > $CURRENT" |bc -l) )); then
      wget ${NODEBUG} "${REPOSITORY_URL}${REPO_BRANCH}/releases/trinity-web2rgbmatrix.ino.bin" -O /tmp/trinity-web2rgbmatrix.ino.bin
      if [ -f /tmp/trinity-web2rgbmatrix.ino.bin ]; then
        curl -F 'file=@trinity-web2rgbmatrix.ino.bin' http://${HOSTNAME}/update
      fi
    else
      echo -e "${fblink}Skipping${fyellow} ESP32-Trinity update because already at latest version: ${fcyan}${LATEST}${freset}"
    fi
  else
    echo -e "${fblink}Skipping${fyellow} ESP32-Trinity update because of the ${fcyan}TRINITY_UPDATE${fyellow} INI-Option${freset}"
  fi
else
  echo -e "${fblink}Skipping${fyellow} ESP32-Trinity update because ${fcyan}HOSTNAME${fyellow} is not set in INI-Option${freset}"
fi

# Check and remount root non-writable if neccessary
[ "${MOUNTRO}" = "true" ] && /bin/mount -o remount,ro /

if [ $(pidof ${DAEMONNAME}) ]; then
  echo -e "${fgreen}Restarting init script\n${freset}"
  ${INITSCRIPT} restart
else
  echo -e "${fgreen}Starting init script\n${freset}"
  ${INITSCRIPT} start
fi

[ -z "${SSH_TTY}" ] && echo -e "${fgreen}Press any key to continue\n${freset}"
