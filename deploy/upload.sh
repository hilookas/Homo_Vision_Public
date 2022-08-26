#!/bin/bash

SCRIPT_PATH=${0%/*}
pushd $SCRIPT_PATH

. env.sh

rm -f deploy.tgz
tar -C .. --exclude "deploy" --exclude "build" --exclude ".git" --exclude "data" -czvf deploy.tgz .
scp -i id deploy.tgz root@$REMOTE_IP:
rm deploy.tgz

ssh -T -i id root@$REMOTE_IP << EOL
mv deploy.tgz $REMOTE_FOLDER
pushd $REMOTE_FOLDER
tar xzvf deploy.tgz
rm deploy.tgz
mkdir -p build
pushd build
cmake --build . -- -j 4
EOL

popd