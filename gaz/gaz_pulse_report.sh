./receive_gaz_pulses | tee /dev/tty | grep --line-buffered '^gas' | ~/home-monitoring-client/data/report_to_hm_web.sh
