#!/bin/bash

SCRIPT_PATH=${0%/*}
pushd $SCRIPT_PATH

. env.sh

# ssh -T -i id root@$REMOTE_IP << EOL
# export DISPLAY="$REMOTE_DISPLAY"
# $REMOTE_FOLDER/build/main
# EOL

# https://superuser.com/questions/1141887/ssh-here-document-does-ctrlc-get-to-remote-side
# https://stackoverflow.com/questions/7114990/pseudo-terminal-will-not-be-allocated-because-stdin-is-not-a-terminal
ssh -tt -i id root@$REMOTE_IP << EOL
cleanup() { echo "Done. Logging out"; sleep 2; logout; }
export DISPLAY="$REMOTE_DISPLAY"; $REMOTE_FOLDER/build/main &
executePID=\$!; trap "kill \$executePID; cleanup" INT HUP EXIT TERM; wait \$executePID; cleanup
EOL

popd