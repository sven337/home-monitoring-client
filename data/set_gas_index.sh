#!/bin/bash

source ~/.hms_api_key
URL=http://192.168.1.6:5000/update

echo -n "Enter index: "
read index
curl -s "$URL/gas/set_index/$index?api_key=$HMS_APIKEY&admin_key=$HMS_ADMINKEY" 


