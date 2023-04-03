#!/bin/sh

COMMAND='luna-send -n 1 luna://org.webosbrew.vncserver.service/start {"autostart": true}'

timeout 5 $COMMAND
# On pre-webOS 7, busybox uses non-standard timeout arguments. We need to retry for that.
if [ $? -eq 127 ]; then
    timeout -t 5 $COMMAND
fi
