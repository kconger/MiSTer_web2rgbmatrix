#!/bin/sh
#
# web2rgbmatrix service
#

. /media/fat/web2rgbmatrix/web2rgbmatrix.conf

# Debug function
dbug() {
  if [ "${DEBUG}" = "true" ]; then
    if [ ! -e ${DEBUGFILE} ]; then                                              # log file not (!) exists (-e) create it
      echo "---------- web2rgbmatrix Debuglog ----------" > ${DEBUGFILE}
    fi 
    echo "${1}" >> ${DEBUGFILE}                                                 # output debug text
  fi
}

# Send Core GIF image over the web
senddata() {
  if [ "${SD_INSTALLED}" = "true" ]; then
    dbug "Requesting matrix SD Card File"
    curl http://${HOSTNAME}/localplay?file=${1}.gif
  else
    dbug "Trying to send file to matrix"
    if [ -r ${GIF_PATH}/${1}.gif ]; then                          # proceed if file exists and is readable (-r)
      curl -F file=@${GIF_PATH}/${1}.gif http://${HOSTNAME}/play  # transfer CORENAME.gif
    else                                                                         # CORENAME.gif file not found
      echo "File ${GIF_PATH}/${1}.gif not found!"
      dbug "File ${GIF_PATH}/${1}.gif not found!"
      curl -F file=@${GIF_PATH}/MENU.gif http://${HOSTNAME}/play  # transfer CORENAME.gif
    fi
  fi     
}

while true; do                                                                # main loop
  if [ -r ${CORENAMEFILE} ]; then                                             # proceed if file exists and is readable (-r)
    NEWCORE=$(cat ${CORENAMEFILE})                                            # get CORENAME
    echo "Read CORENAME: -${NEWCORE}-"
    dbug "Read CORENAME: -${NEWCORE}-"
    if [ "${NEWCORE}" != "${OLDCORE}" ]; then                                 # proceed only if Core has changed
      echo "Send -${NEWCORE} GIF - to http://${HOSTNAME}/play ."
      dbug "Send -${NEWCORE} GIF - to http://${HOSTNAME}/play ."
      senddata "${NEWCORE}"                                                   # The "Magic"
      OLDCORE="${NEWCORE}"                                                    # update oldcore variable
    fi                                                                        # end if core check
    inotifywait -e modify "${CORENAMEFILE}"                                   # wait here for next change of corename
  else                                                                        # CORENAME file not found
   echo "File ${CORENAMEFILE} not found!"
   dbug "File ${CORENAMEFILE} not found!"
  fi                                                                          # end if /tmp/CORENAME check
done
# ** End Main **
