#!/bin/bash
set -e
cd src
inotifywait -mrq -e attrib,close_write,move,create,delete --format %w ~ | ./uniqueue -e /bin/ls
cd ..
