systemctl stop serial-getty@ttyAMA0.service
./ti_cat | egrep '^(PAPP|BASE)' -a --line-buffered | ./cksum | ../data/report_to_hm_web.sh
