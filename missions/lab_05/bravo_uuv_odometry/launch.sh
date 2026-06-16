#!/bin/bash -e
#----------------------------------------------------------
#  Script: launch.sh
#  Author: Evangelos Siatis
#  LAB 05 - Assignment 8 (BONUS): The Bravo UUV Odometry Mission
#----------------------------------------------------------
#  Περναει το depth_thresh (--depth) και την αποστασι επιστροφης
#  (--dist) ως παραμετρους, μεσω nsplug, στα meta_ templates ->
#  παραγει targ_bravo.moos / targ_bravo.bhv και τα τρεχει.
#----------------------------------------------------------
#  Part 1: Προεπιλογες global μεταβλητων
#----------------------------------------------------------
TIME_WARP=1
JUST_MAKE="no"

DEPTH_THRESH=25     # [BONUS] κατωφλι βαθους (m) -> pOdometry depth_thresh
TRIP_DIST=200       # [BONUS] αποστασι σε βαθος (m) πριν την επιστροφη

#----------------------------------------------------------
#  Part 2: Διαχειριση command-line arguments
#----------------------------------------------------------
for ARGI; do
    if [ "${ARGI}" = "--help" -o "${ARGI}" = "-h" ] ; then
	echo "launch.sh [SWITCHES] [time_warp]                           "
	echo "  --help, -h           Show this help message              "
	echo "  --just_make, -j      Just create targ files, no launch   "
	echo "  --depth=<meters>     Set pOdometry depth_thresh (def: 25) "
	echo "  --dist=<meters>      Set return trip distance   (def: 200)"
	exit 0;
    elif [ "${ARGI}" = "--just_make" -o "${ARGI}" = "-j" ] ; then
	JUST_MAKE="yes"
    elif [ "${ARGI:0:8}" = "--depth=" ] ; then
        DEPTH_THRESH="${ARGI#--depth=}"
    elif [ "${ARGI:0:7}" = "--dist=" ] ; then
        TRIP_DIST="${ARGI#--dist=}"
    elif [ "${ARGI//[^0-9]/}" = "$ARGI" -a "$TIME_WARP" = 1 ]; then
        TIME_WARP=$ARGI
    else
        echo "launch.sh Bad arg:" $ARGI " Exiting with code: 1"
        exit 1
    fi
done

#----------------------------------------------------------
#  Part 3: Δημιουργια των targ_ αρχειων μεσω nsplug
#----------------------------------------------------------
echo "Assembling targ files: WARP=$TIME_WARP DEPTH_THRESH=$DEPTH_THRESH TRIP_DIST=$TRIP_DIST"

nsplug meta_bravo.moos targ_bravo.moos -f \
       WARP=$TIME_WARP  DEPTH_THRESH=$DEPTH_THRESH

nsplug meta_bravo.bhv  targ_bravo.bhv  -f \
       TRIP_DIST=$TRIP_DIST

if [ ${JUST_MAKE} = "yes" ] ; then
    echo "Files assembled; nothing launched; exiting per request."
    exit 0
fi

#----------------------------------------------------------
#  Part 4: Εκκινηση της αποστολης
#----------------------------------------------------------
echo "Launching bravo MOOS Community with WARP:" $TIME_WARP
pAntler targ_bravo.moos --MOOSTimeWarp=$TIME_WARP >& /dev/null &

uMAC targ_bravo.moos

kill -- -$$
