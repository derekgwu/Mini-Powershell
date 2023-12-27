#!/bin/sh

readelf -s $1 | awk '{print $8}'  | grep ".*\..*\|Name\|^$" --invert-match
