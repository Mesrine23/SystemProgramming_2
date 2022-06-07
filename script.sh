#!/bin/bash
./remoteClient -d server -p 12006 -i 192.168.1.7 > log1 &
./remoteClient -d server -p 12006 -i 192.168.1.7 > log2 &
