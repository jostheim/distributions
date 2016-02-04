#!/bin/bash

BUILD=examples/cmake/build
ROOT=$PWD

mkdir -p $BUILD
cd $BUILD

export CMAKE_PREFIX_PATH=$VIRTUAL_ENV
cmake .. && make 
DYLD_LIBRARY_PATH=/Users/jostheim/virtualenvs/edison/lib/ ./foo
