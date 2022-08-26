#!/bin/bash

set -e

rm -rf record
tar xzvf $1
pushd ../build
cmake --build .
popd
../build/Debug/main 2 $2 $3 $4 $5 $6 $7