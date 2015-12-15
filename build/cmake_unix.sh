#!/bin/bash -e
BUILD_TYPE=${BUILD_TYPE:-"Debug"}
mkdir -p "unix/${BUILD_TYPE}" || true
cd "unix/${BUILD_TYPE}"
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ../../../
make
cd ..
