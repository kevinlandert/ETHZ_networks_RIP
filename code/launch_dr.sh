#!/bin/sh

# Author: Roger Liao
# Contact: rogliao@cs.stanford.edu

args=`getopt s:p: $*`
set -- $args

server=""
port=""

for i; do
    case "$i" in
        -s ) server="$2"
            shift; shift;;
        -p ) port="$2"
            shift; shift;;
        -- ) shift; break;;
    esac
done

if [ $? != 0 -o $# -lt 1 ]; then
   echo "Usage: ./launch_dr.sh [-s SERVER] [-p PORT] NUM"
   exit
fi

passFlags=""
if [ "$server" != "" ]; then
    passFlags="${passFlags} -s ${server}"
fi
if [ "$port" != "" ]; then
    passFlags="${passFlags} -p ${port}"
fi

for i in $(seq 1 $1); do
   touch dr${i}.log
   echo "******** Starting new dr instance ********" >> dr${i}.log
   ./dr -v dr${i} $passFlags 2>&1 >> dr${i}.log &
done
