#!/bin/sh

# We want to see if the visibility of symbols is too permissive. If
# the object/library includes more visible symbols (i.e. global
# variables and functions not marked as `static`) than specified, emit
# a warning.
#
# Arguments:
#
# 1. grep string matching allowed symbols
# 2. the .o, .a, or .so file to assess

VISIBLE_SYMBS=`nm -g --defined-only $2 | cut -d ' ' -f 3 | grep --invert-match ".*\.o:$\|^$" | grep --invert-match "$1"`
if [ -z "$VISIBLE_SYMBS" ]; then
    echo "Symbol visibility checker: SUCCESS," $2 "includes only the specified visible symbols"
else
    echo "Symbol visibility checker: WARNING," $2 "defines too many non-'static', visible symbols --" $VISIBLE_SYMBS
fi
