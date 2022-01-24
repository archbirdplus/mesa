#!/bin/sh
TRACEDIFF="${TRACEDIFF:-$(dirname "$0")/pytracediff.py}"
"$TRACEDIFF" --ignore-junk --width="$(tput cols)" "$@" | less -R
