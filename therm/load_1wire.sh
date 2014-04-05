#!/bin/bash

modprobe w1-gpio
modprobe w1-therm
sed -n 2p /sys/bus/w1/devices/28-0000*/w1_slave  | cut -f2 -d=
