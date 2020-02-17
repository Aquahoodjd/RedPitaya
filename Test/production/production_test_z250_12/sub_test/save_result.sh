#!/bin/bash
source ./sub_test/common_func.sh

CombineLogVar

###############################################################################
# LOGGING TEST TO SD CARD
###############################################################################
echo
echo "------------Logging test result to SD card------------------------------"
echo
# SAVE RESULTS INTO THE SD-CARD
# Remount the SD card with R&W rigths
mount -o remount,rw $G_SD_CARD_PATH

# Log information to the log file
echo "      Test data logging on the SD card was successful"
echo $LOG_VAR >> "$G_SD_CARD_PATH/$G_LOG_FILENAME"
RPLight7
# Remount the SD card with Read only rigths
mount -o remount,ro $G_SD_CARD_PATH
echo
echo "------------------------------------------------------------------------"
echo


###############################################################################
# LOGGING TEST TO LOCAL PC
###############################################################################
# Create the mounting point folder and mount the device
N_PING_PKG=5
RES=$(ping "$G_LOCAL_SERVER_IP" -c "$N_PING_PKG" | grep 'transmitted' | awk '{print $4}' ) > /dev/null

if [[ $G_SAVE_TO_PC == 1 ]]
then
if [[ "$RES" == "$N_PING_PKG" ]]
then
    echo "-------------Logging test statistics to PRODUCTION PC-------------------"
    echo
    #echo 'sometext' | ssh zumy@192.168.178.123 "cat >> /home/zumy/Desktop/Test_LOGS/manuf_test.log"
    echo $LOG_VAR | sshpass -p "$G_LOCAL_SERVER_PASS" ssh -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null $G_LOCAL_USER@$G_LOCAL_SERVER_IP "cat >> $G_LOCAL_SERVER_DIR/$G_LOG_FILENAME"
    if [[ $? = 255 ]] 
    then
      echo -n "      Test data logging on the local PC "
      print_fail
    else
      echo -n "      Test data logging on the local PC "
      print_ok
    fi
    else
    echo -n "      Not possible to log test statistics to local PC "
    print_fail
fi
echo
echo
echo "------------------------------------------------------------------------"
echo
echo
fi
