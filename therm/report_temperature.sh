#!/bin/bash
 
file="/sys/bus/w1/devices/28-00000*/w1_slave"
function find(){
 [[ "$1" =~ "$2" ]] && true || false
}
 
#read device
CRC=false
while read curline; do
		#check crc
		if( find "$curline" "crc" && find "$curline" "YES" ) then
				CRC=true
		fi
		if($CRC && find "$curline" "t=") then
			TEMP=`echo $curline|cut -d'=' -f 2`
#Divide by 1000 and report
			echo temperature/$(echo $TEMP | head -c 2).$(echo $TEMP | tail -c +3) | /root/home-monitoring-client/data/report_to_hm_web.sh >/dev/null
		fi
done <$file
