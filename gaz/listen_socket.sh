mkfifo /tmp/rf24_out
nc -klvp 45888 0</tmp/rf24_out | ./receive_gaz_pulses 1>/tmp/rf24_out
