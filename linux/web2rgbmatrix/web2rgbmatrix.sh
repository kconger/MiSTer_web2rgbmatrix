#!/bin/sh
#
# web2rgbmatrix service
#

. /media/fat/web2rgbmatrix/web2rgbmatrix-system.ini
. /media/fat/web2rgbmatrix/web2rgbmatrix-user.ini

# Debug function
dbug() {
  if [ "${DEBUG}" = "true" ]; then
    if [ ! -e ${DEBUGFILE} ]; then                                           # log file not (!) exists (-e) create it
      echo "---------- web2rgbmatrix Debuglog ----------" > ${DEBUGFILE}
    fi 
    echo "${1}" >> ${DEBUGFILE}                                              # output debug text
  fi
}

# Send Core GIF image over the web
senddata() {
  if [ "${SD_INSTALLED}" = "true" ]; then
    dbug "Requesting matrix to display GIF from its SD Card File"
    HTTP_CODE=$(curl --write-out "%{http_code}" http://${HOSTNAME}/localplay?file=${1} --output /dev/null --silent) # request CORENAME.gif
    case $HTTP_CODE in
      "200")
        echo "Successfully requested ${1} GIF"
        dbug "Successfully requested ${1} GIF"
        ;;
      "404")
        echo "${1} GIF not found on matrix SD"
        dbug "${1} GIF not found on matrix SD"
        ;;
      *)
        echo "Unknown error requesting ${1} GIF on matrix"
        dbug "Unknown error requesting ${1} GIF on matrix"
        ;;
    esac
  else
    dbug "Trying to send GIF to matrix"
    LTR=$(echo "${1:0:1}" | tr [a-z] [A-Z])
    if [ -r ${GIF_PATH}/${LTR}/${1}.gif ]; then                                     # proceed if file exists and is readable (-r)
      HTTP_CODE=$(curl --write-out "%{http_code}" -F file=@${GIF_PATH}/${LTR}/${1}.gif http://${HOSTNAME}/remoteplay --output /dev/null --silent) # transfer CORENAME.gif
      case $HTTP_CODE in
        "200")
          echo "Successfully copied ${GIF_PATH}/${LTR}/${1}.gif to matrix"
          dbug "Successfully copied ${GIF_PATH}/${LTR}/${1}.gif to matrix"
          ;;
        "500")
          echo "Error copying ${GIF_PATH}/${LTR}/${1}.gif to matrix"
          dbug "Error copying ${GIF_PATH}/${LTR}/${1}.gif to matrix"
          ;;
        *)
          echo "Unknown error copying ${GIF_PATH}/${LTR}/${1}.gif to matrix"
          dbug "Unknown error copying ${GIF_PATH}/${LTR}/${1}.gif to matrix"
          ;;
      esac
    else                                                                     # CORENAME.gif file not found
      echo "File ${GIF_PATH}/${LTR}/${1}.gif not found!"
      dbug "File ${GIF_PATH}/${LTR}/${1}.gif not found!"
      HTTP_CODE=$(curl --write-out "%{http_code}" http://${HOSTNAME}/text?line=${1} --output /dev/null --silent) # request Core Name as text
      case $HTTP_CODE in
        "200")
          echo "Successfully requested text display: ${1}"
          dbug "Successfully requested text display: ${1}"
          ;;
        "500")
          echo "Error requesting text display: ${1}"
          dbug "Error requesting text display: ${1}"
          ;;
        *)
          echo "Unknown error requesting text display: ${1}"
          dbug "Unknown error requesting text display: ${1}"
          ;;
      esac
    fi
  fi     
}

while true; do                                                                # main loop
  if [ -r ${CORENAMEFILE} ]; then                                             # proceed if file exists and is readable (-r)
    NEWCORE=$(cat ${CORENAMEFILE})                                            # get CORENAME
    echo "Read CORENAME: -${NEWCORE}-"
    dbug "Read CORENAME: -${NEWCORE}-"
    if [ "${NEWCORE}" != "${OLDCORE}" ]; then                                 # proceed only if Core has changed
      senddata "${NEWCORE}"                                                   # The "Magic"
      OLDCORE="${NEWCORE}"                                                    # update oldcore variable
    fi                                                                        # end if core check
    if [ "${DEBUG}" = "false" ]; then
      # wait here for next change of corename, -qq for quietness
      inotifywait -qq -e modify "${CORENAMEFILE}"
    fi
    if [ "${DEBUG}" = "true" ]; then
      # but not -qq when debugging
      inotifywait -e modify "${CORENAMEFILE}"
    fi
  else                                                                        # CORENAME file not found
   echo "File ${CORENAMEFILE} not found!"
   dbug "File ${CORENAMEFILE} not found!"
  fi                                                                          # end if /tmp/CORENAME check
done
# ** End Main **
