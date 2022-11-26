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

! [ -e /etc/web2rgbmatrix/web2rgbmatrix-user.ini ] && touch /etc/web2rgbmatrix/web2rgbmatrix-user.ini
. /etc/web2rgbmatrix/web2rgbmatrix-system.ini
. /etc/web2rgbmatrix/web2rgbmatrix-user.ini

echo -e "${fgreen}web2rgbmatrix update script"
echo -e "----------------------${freset}"
echo -e "${fgreen}Checking for available web2rgbmatrix updates...${freset}"


# init script
wget ${NODEBUG} "${REPOSITORY_URL}${REPO_BRANCH}/emulationstation/service/etc/init.d/web2rgbmatrix" -O /tmp/web2rgbmatrix
if  ! [ -f ${INITSCRIPT} ]; then
  if  [ -f ${INITDISABLED} ]; then
    echo -e "${fyellow}Found disabled init script, skipping Install${freset}"
  else
    echo -e "${fyellow}Installing init script ${fmagenta}web2rgbmatrix${freset}"
    mv -f /tmp/web2rgbmatrix ${INITSCRIPT}
    chmod +x ${INITSCRIPT}
	sudo update-rc.d web2rgbmatrix defaults
  fi
elif ! cmp -s /tmp/web2rgbmatrix ${INITSCRIPT}; then
  if [ "${SCRIPT_UPDATE}" = "true" ]; then
    echo -e "${fyellow}Updating init script ${fmagenta}web2rgbmatrix${freset}"
    mv -f /tmp/web2rgbmatrix ${INITSCRIPT}
    chmod +x ${INITSCRIPT}
	sudo update-rc.d web2rgbmatrix defaults
  else
    echo -e "${fblink}Skipping${fyellow} available init script update because of the ${fcyan}SCRIPT_UPDATE${fyellow} INI-Option${freset}"
  fi
fi
[[ -f /tmp/web2rgbmatrix ]] && rm /tmp/web2rgbmatrix

# Update daemon
wget ${NODEBUG} "${REPOSITORY_URL}${REPO_BRANCH}/emulationstation/service/usr/local/bin/${DAEMONNAME}" -O /tmp/${DAEMONNAME}
if  ! [ -f ${DAEMONSCRIPT} ]; then
  echo -e "${fyellow}Installing daemon script ${fmagenta}web2rgbmatrix${freset}"
  mv -f /tmp/${DAEMONNAME} ${DAEMONSCRIPT}
  chmod +x ${DAEMONSCRIPT}
elif ! cmp -s /tmp/${DAEMONNAME} ${DAEMONSCRIPT}; then
  if [ "${SCRIPT_UPDATE}" = "true" ]; then
    echo -e "${fyellow}Updating daemon script ${fmagenta}web2rgbmatrix${freset}"
    mv -f /tmp/${DAEMONNAME} ${DAEMONSCRIPT}
    chmod +x ${DAEMONSCRIPT}
  else
    echo -e "${fblink}Skipping${fyellow} available daemon script update because of the ${fcyan}SCRIPT_UPDATE${fyellow} INI-Option${freset}"
  fi
fi
[[ -f /tmp/${DAEMONNAME} ]] && rm /tmp/${DAEMONNAME}

# Update Emulationstation scripts
if [ "${SCRIPT_UPDATE}" = "true" ]; then
  echo -e "${fyellow}Installing Emulationstation event scripts${freset}"
  [[ -d ${ES_CONFIG_PATH}/scripts ]] && cd ${ES_CONFIG_PATH}/scripts || mkdir -p ${ES_CONFIG_PATH}/scripts
  cd ${ES_CONFIG_PATH}/scripts
  wget ${NODEBUG} -O - https://github.com/kconger/MiSTer_web2rgbmatrix/archive/master.tar.gz | tar xz --strip=3 "MiSTer_web2rgbmatrix-master/emulationstation/scripts"
  chown -R pi:pi ${ES_CONFIG_PATH}/scripts
else
  echo -e "${fblink}Skipping${fyellow} possible Emulationstation script update because of the ${fcyan}SCRIPT_UPDATE${fyellow} INI-Option${freset}"
fi

# Update GIFs
if [[ "${SD_UPDATE}" = "true" || "${GIF_UPDATE}" = "true" ]]; then
  echo -e "${fyellow}Installing GIFs${freset}"
  [[ -d ${GIF_PATH} ]] && cd ${GIF_PATH} || mkdir ${GIF_PATH}
  cd ${GIF_PATH}
  if ["${GIF_UPDATE}" = "true"]; then
    wget ${NODEBUG} -O - https://github.com/h3llb3nt/marquee_gifs/archive/main.tar.gz | tar xz --strip=2 "marquee_gifs-main/128x32"
  else
    wget ${NODEBUG} -O - https://github.com/h3llb3nt/marquee_gifs/archive/main.tar.gz | tar xz --skip-old-files --strip=2 "marquee_gifs-main/128x32"
  fi
  chown -R pi:pi ${GIF_PATH}
  if ! [ "${HOSTNAME}" = "rgbmatrix.local" ]; then
    if [[ "${SD_INSTALLED}" = "true" && "${SD_UPDATE}" = "true" ]]; then
      cd ${GIF_PATH}/../
      find gifs -type f -exec curl -u rgbmatrix:password --ftp-create-dirs -T {} ftp://${HOSTNAME}/{} \;
    else
      echo -e "${fblink}Skipping${fyellow} GIF SD Card update because of the ${fcyan}GIF_UPDATE${fyellow} INI-Option${freset}"
    fi
  else
    echo -e "${fblink}Skipping${fyellow} GIF SD Card update because ${fcyan}HOSTNAME${fyellow} is not set in INI-Option${freset}"
  fi
fi

# Update ESP32-Trinity
cd /tmp
if ! [ "${HOSTNAME}" = "rgbmatrix.local" ]; then
  if [ "${TRINITY_UPDATE}" = "true" ]; then
    LATEST=$(wget -q -O - "${REPOSITORY_URL}${REPO_BRANCH}/releases/LATEST")
    CURRENT=$(wget -q -O - "${HOSTNAME}/version")
    if (( $(echo "$LATEST > $CURRENT" |bc -l) )); then
      wget ${NODEBUG} "${REPOSITORY_URL}${REPO_BRANCH}/releases/trinity-web2rgbmatrix.ino.bin" -O /tmp/trinity-web2rgbmatrix.ino.bin
      if [ -f /tmp/trinity-web2rgbmatrix.ino.bin ]; then
		echo -e "${fyellow}Installing ESP32-Trinity update${freset}"
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

if [ $(pidof ${DAEMONNAME}) ]; then
  echo -e "${fgreen}Restarting init script\n${freset}"
  ${INITSCRIPT} restart
else
  echo -e "${fgreen}Starting init script\n${freset}"
  ${INITSCRIPT} start
fi

[ -z "${SSH_TTY}" ] && echo -e "${fgreen}Press any key to continue\n${freset}"
