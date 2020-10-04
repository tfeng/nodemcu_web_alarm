#!/bin/sh

dir=$(dirname "$0")
"$dir/shutdown.sh"
(cd "$dir/../build" && nohup ./web-alarm-server &)
