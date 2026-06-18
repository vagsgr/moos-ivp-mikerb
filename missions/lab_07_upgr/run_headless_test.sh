#!/bin/bash
# Headless validation for lab_07_upgr (MCTS + decentralized claiming).
# Launches shoreside(no-gui)+henry+gilda, deploys, runs, then kills gently
# (SIGTERM first so pLogger flushes) and leaves alogs for analysis.
cd "$(dirname "$0")"
LOG=/tmp/lab07u_test.log
: > $LOG
echo "[`date +%T`] start" >> $LOG

PROCS="pAntler MOOSDB pHelmIvP pGenPathMCTS pPointAssign uSimMarineV22 \
pNodeReporter pMarinePIDV22 uProcessWatch pRealm pLogger uTimerScript \
pShare pHostInfo uFldNodeBroker uFldShoreBroker pMarineViewer uMAC"

killall_gentle() {
  for p in $PROCS; do pkill -x "$p" 2>/dev/null; done   # SIGTERM (flush logs)
  sleep 3
  for p in $PROCS; do pkill -9 -x "$p" 2>/dev/null; done # SIGKILL stragglers
}

# clean slate
killall_gentle
rm -rf LOG_* MOOSLog_* 2>/dev/null

# no-GUI shoreside (rogue clicks / DISPLAY issues -> validate without viewer)
sed '/Run = pMarineViewer/d' targ_shoreside.moos > targ_shoreside_nogui.moos

echo "[`date +%T`] launching communities" >> $LOG
pAntler targ_shoreside_nogui.moos >& /tmp/sh.out &
sleep 2
pAntler targ_henry.moos >& /tmp/henry.out &
pAntler targ_gilda.moos >& /tmp/gilda.out &

# let communities connect, handshake (NODE_REPORTs), uTimerScript unpause, points flow
sleep 18
echo "[`date +%T`] poking DEPLOY" >> $LOG
uPokeDB targ_shoreside_nogui.moos DEPLOY_ALL=true MOOS_MANUAL_OVERRIDE_ALL=false >> $LOG 2>&1

# run the mission (warp 8 -> ~90s real = ~720s sim, enough for tours + claims)
sleep 90
echo "[`date +%T`] stopping" >> $LOG
killall_gentle
echo "[`date +%T`] done" >> $LOG
