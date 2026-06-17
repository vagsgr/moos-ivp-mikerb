#!/bin/bash
#---------------------------------------------------------------
# FILE: launch.sh   (Lab 06 - Exercise 2: Alpha Bravo pShare)
# Ksekinaei TREIS koinotites: shoreside + alpha + bravo.
# Xrisi:  ./launch.sh [TIME_WARP] [--nogui]   (px ./launch.sh 10)
#---------------------------------------------------------------
TIME_WARP=1
GUI="yes"

for ARG in "$@" ; do
    if [ "${ARG}" = "--help" -o "${ARG}" = "-h" ] ; then
        echo "launch.sh [TIME_WARP] [--nogui] [--help/-h]"
        exit 0
    elif [ "${ARG}" = "--nogui" ] ; then
        GUI="no"
    elif [ "${ARG//[^0-9]/}" = "$ARG" -a "$ARG" != "" ] ; then
        TIME_WARP=$ARG
    else
        echo "launch.sh: agnwsto orisma [$ARG]"; exit 1
    fi
done

# [ΣΗΜ] gia headless test, vgazoume to pMarineViewer apo to shoreside
SHORE_MOOS=shoreside.moos
if [ "${GUI}" = "no" ] ; then
    sed '/Run = pMarineViewer/d' shoreside.moos > shoreside_nogui.moos
    SHORE_MOOS=shoreside_nogui.moos
fi

echo "Ksekiname shoreside community (warp=${TIME_WARP})..."
pAntler ${SHORE_MOOS} --MOOSTimeWarp=${TIME_WARP} >& /dev/null &
sleep 1
echo "Ksekiname alpha (vehicle) community (warp=${TIME_WARP})..."
pAntler alpha.moos --MOOSTimeWarp=${TIME_WARP} >& /dev/null &
sleep 1
echo "Ksekiname bravo (vehicle) community (warp=${TIME_WARP})..."
pAntler bravo.moos --MOOSTimeWarp=${TIME_WARP} >& /dev/null &

echo "Ola ksekinisan. Pata CTRL-C edw gia na termatiseis."
trap "echo; echo 'Termatismos...'; for p in pAntler MOOSDB pShare pMarineViewer pHelmIvP pMarinePIDV22 uSimMarineV22 pNodeReporter uProcessWatch pLogger pRealm; do pkill -x \$p; done; exit 0" SIGINT
while true; do sleep 1; done
