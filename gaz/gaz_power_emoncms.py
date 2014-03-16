#!/usr/bin/python -u

import urllib.request
import time

DELAY = 300

old_index = 0
def get_gaz_index():
    try:
        response = urllib.request.urlopen('http://alia/emoncms/feed/value.json?id=11&apikey=b169d286cf3390bd4bb8cb9cfca651d5')
        value = response.read()[1:-1]
        return float(value)
    except:
        return float(old_index)
        pass


while True:
    index = get_gaz_index()
#    print("gaz index is ", index)
    if old_index == 0:
        old_index = index
    power = (index - old_index) * 10.82 * 1000 * 3600 / DELAY

    print("appart.GAZ_POWER:%f" % (power))

    old_index = index
    time.sleep(DELAY)    
