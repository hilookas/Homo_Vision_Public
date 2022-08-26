#!/bin/bash

SCRIPT_PATH=${0%/*}
pushd $SCRIPT_PATH

. env.sh

# Please use Git for Windows 2.34.1 to avoid ctrl-c to exit whole ssh connection
# Ref: https://unix.stackexchange.com/questions/689166/stop-command-in-ssh-session-without-exiting-ssh
ssh -t -i id root@$REMOTE_IP "export DISPLAY=\"$REMOTE_DISPLAY\"; bash"