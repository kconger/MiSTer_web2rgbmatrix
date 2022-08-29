#!/bin/sh
#
# web2rgbmatrix service
#

. /media/fat/web2rgbmatrix/web2rgbmatrix.conf

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
    HTTP_CODE=$(curl --write-out "%{http_code}" http://${HOSTNAME}/localplay?file=${1}.gif --output /dev/null --silent) # request CORENAME.gif
    case $HTTP_CODE in
      "200")
        echo "Successfully requested ${1}.gif"
        dbug "Successfully requested ${1}.gif"
        ;;
      "404")
        echo "${1}.gif not found on matrix SD, trying MENU.gif"
        dbug "${1}.gif not found on matrix SD, trying MENU.gif"
        HTTP_CODE=$(curl --write-out "%{http_code}" http://${HOSTNAME}/localplay?file=MENU.gif --output /dev/null --silent) # request CORENAME.gif
        case $HTTP_CODE in
          "200")
            echo "Successfully requested MENU.gif"
            dbug "Successfully requested MENU.gif"
            ;;
          "404")
            echo "MENU.gif not found on matrix SD"
            dbug "MENU.gif not found on matrix SD"
            ;;
          *)
            echo "Unknown error requesting MENU.gif on matrix"
            dbug "Unknown error requesting MENU.gif on matrix"
            ;;
        esac
        ;;
      *)
        echo "Unknown error requesting ${1}.gif on matrix"
        dbug "Unknown error requesting ${1}.gif on matrix"
        ;;
    esac
  else
    dbug "Trying to send GIF to matrix"
    if [ -r ${GIF_PATH}/${1}.gif ]; then                                     # proceed if file exists and is readable (-r)
      HTTP_CODE=$(curl --write-out "%{http_code}" -F file=@${GIF_PATH}/${1}.gif http://${HOSTNAME}/play --output /dev/null --silent) # transfer CORENAME.gif
      case $HTTP_CODE in
        "200")
          echo "Successfully copied ${GIF_PATH}/${1}.gif to matrix"
          dbug "Successfully copied ${GIF_PATH}/${1}.gif to matrix"
          ;;
        "500")
          echo "Error copying ${GIF_PATH}/${1}.gif to matrix"
          dbug "Error copying ${GIF_PATH}/${1}.gif to matrix"
          ;;
        *)
          echo "Unknown error copying ${GIF_PATH}/${1}.gif to matrix"
          dbug "Unknown error copying ${GIF_PATH}/${1}.gif to matrix"
          ;;
      esac
    else                                                                     # CORENAME.gif file not found
      echo "File ${GIF_PATH}/${1}.gif not found!"
      dbug "File ${GIF_PATH}/${1}.gif not found!"
      HTTP_CODE=$(curl --write-out "%{http_code}" -F file=@${GIF_PATH}/MENU.gif http://${HOSTNAME}/play --output /dev/null --silent)  # transfer MENU.gif
      case $HTTP_CODE in
        "200")
          echo "Successfully copied ${GIF_PATH}/MENU.gif to matrix"
          dbug "Successfully copied ${GIF_PATH}/MENU.gif to matrix"
          ;;
        "500")
          echo "Error copying ${GIF_PATH}/MENU.gif to matrix"
          dbug "Error copying ${GIF_PATH}/MENU.gif to matrix"
          ;;
        *)
          echo "Unknown error copying ${GIF_PATH}/MENU.gif to matrix"
          dbug "Unknown error copying ${GIF_PATH}/MENU.gif to matrix"
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
    inotifywait -e modify "${CORENAMEFILE}"                                   # wait here for next change of corename
  else                                                                        # CORENAME file not found
   echo "File ${CORENAMEFILE} not found!"
   dbug "File ${CORENAMEFILE} not found!"
  fi                                                                          # end if /tmp/CORENAME check
done
# ** End Main **
