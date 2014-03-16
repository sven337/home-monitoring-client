#!/bin/bash

rmmod w1_therm && rmmod w1_gpio
modprobe w1_gpio && modprobe w1_therm strong_pullup=1
sed -n 2p /sys/bus/w1/devices/28-000004dccc84/w1_slave  | cut -f2 -d=
