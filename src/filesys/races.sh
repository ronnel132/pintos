#!/bin/bash
n=$1

while [ $n -ge 0 ]
do
    
    pintos -v -k -j $n  --filesys-size=2 -p tests/filesys/base/syn-write -a syn-write -p tests/filesys/base/child-syn-wrt -a child-syn-wrt -- -q  -f run syn-write
    let n=$(($n-1))
done
