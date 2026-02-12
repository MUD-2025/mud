#!/bin/sh
cat syslog | iconv --from-code=koi8-r | grep -E -w 'reconnected|вошел|quit the game|New player' | grep -v nusage
