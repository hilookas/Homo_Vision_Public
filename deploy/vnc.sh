#!/bin/bash

SCRIPT_PATH=${0%/*}
pushd $SCRIPT_PATH

. env.sh

ssh -T -i id root@$REMOTE_IP << 'EOL'
kill $(/usr/bin/pgrep tightvnc)
vncserver
EOL

"C:\Program Files\TightVNC\tvnviewer.exe" $REMOTE_IP:1 -password=$REMOTE_VNC_PASSWORD &

popd