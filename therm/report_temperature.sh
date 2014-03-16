#!/bin/bash

FILE=/sys/bus/w1/devices/28-000004dccc84/w1_slave

if [ ! -r $FILE ]; then
	exit 1
fi

TEMP=$(sed -n 2p $FILE  | cut -f2 -d=)
if [ $TEMP -gt 0 ]; then
#Divide by 1000 and report
	echo temperature/$(echo $TEMP | head -c 2).$(echo $TEMP | tail -c +3) | /root/home-monitoring-client/data/report_to_hm_web.sh >/dev/null
fi
