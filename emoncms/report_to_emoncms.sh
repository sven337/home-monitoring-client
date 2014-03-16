#!/bin/bash

source /root/.emoncms_api_key
BASEURL="http://ACDEF/emoncms/input/post.json?apikey="${APIKEY}"&json={"

url=$BASEURL

while read line; do
	url=$BASEURL${line}}
#	echo "$url" 
	curl -silent --request GET $url >/dev/null
done
