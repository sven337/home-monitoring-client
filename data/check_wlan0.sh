#!/bin/bash

unload_modules()
{
	netctl stop wlan0-NEUF_01F8
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

restart_connection()
{
	netctl stop wlan0-NEUF_01F8
	netctl start wlan0-NEUF_01F8
}

reconnect()
{
	unload_modules
	cycle_port_power
	load_modules
	restart_connection
}

ADDR=$(ip addr show dev wlan0)
if [ $? -ne 0 ]; then
	# Interface has disappeared
	reconnect
	echo `date` "Reconnecting Wifi because it seems to have gone down..." >> /root/wlan0_log
fi



