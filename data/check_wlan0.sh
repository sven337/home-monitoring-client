#!/bin/bash

unload_modules()
{
	rmmod rt2800usb
	rmmod rt2x00usb
}

cycle_port_power()
{
	echo '1-1.2' > /sys/bus/usb/drivers/usb/unbind
	sleep 2
	echo '1-1.2' > /sys/bus/usb/drivers/usb/bind
}

load_modules()
{
	modprobe rt2800usb
}

reconnect()
{
	netctl-auto disable-all
#	unload_modules
#	cycle_port_power
#	load_modules
	netctl-auto enable-all
}

ADDR=$(ip addr show dev wlan0)
if [ $? -ne 0 ]; then
	# Interface has disappeared
	reconnect
	echo `date` "Reconnecting Wifi because it seems to have gone down..." >> /root/wlan0_log
fi



