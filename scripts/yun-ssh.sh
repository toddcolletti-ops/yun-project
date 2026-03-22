#!/bin/bash
# Quick SSH access to the Arduino Yun
# Usage: ./scripts/yun-ssh.sh [command]
#
# Without arguments, opens an interactive shell.
# With arguments, runs the command and returns output.

if [ $# -eq 0 ]; then
  ssh yun
else
  ssh yun "$@"
fi
