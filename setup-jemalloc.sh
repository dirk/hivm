#!/bin/bash
cd deps
wget "https://github.com/jemalloc/jemalloc/archive/3.6.0.tar.gz" -q -O jemalloc-3.6.0.tar.gz
tar xzf jemalloc-3.6.0.tar.gz
cd jemalloc-3.6.0
autoconf
./configure --quiet --with-jemalloc-prefix=je_
make --quiet
