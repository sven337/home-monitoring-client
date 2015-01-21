#!/bin/sh
# Send notification that the (physical) mailbox was opened

LAST_NOTIFICATION_FILE=/tmp/.last_physical_mailbox_notification_on_day

if [ ! -r $LAST_NOTIFICATION_FILE ]; then
	echo 0 > $LAST_NOTIFICATION_FILE
fi

if [ "`date +%d`" -ne "`cat $LAST_NOTIFICATION_FILE`" ]; then
	(echo -n "Mailbox open at "; date +%H:%M:%S) | mail -s 'You have new mail' root
	date +%d > $LAST_NOTIFICATION_FILE
fi
