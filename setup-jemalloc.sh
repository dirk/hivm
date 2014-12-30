#!/bin/bash
cd deps
wget "https://github.com/jemalloc/jemalloc/archive/3.6.0.tar.gz" -O jemalloc-3.6.0.tar.gz
tar xzf jemalloc-3.6.0.tar.gz
cd jemalloc-3.6.0
autoconf
./configure
make
