#!/bin/sh

insmod ./snull.ko $*
ifconfig eth0 local0
ifconfig eth1 local1
