#!/bin/sh
export DYLD_LIBRARY_PATH=`pwd`/../..:$DYLD_LIBRARY_PATH
./test_vm
