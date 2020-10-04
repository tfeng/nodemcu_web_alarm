#!/bin/sh

killall web-alarm-server 2> /dev/null
while pgrep web-alarm-server > /dev/null; do
  sleep 1;
done
