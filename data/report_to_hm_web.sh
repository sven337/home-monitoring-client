#!/bin/bash

source ~/.hms_api_key
URL=http://192.168.1.6:5000/update

while read line; do
#temperature/27.0
#gas/pulse/374
#electricity/800,8372.22

	curl -s "$URL/$line"
done
