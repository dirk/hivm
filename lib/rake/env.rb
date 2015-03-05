
# Setting up global constants
CC       = ENV['CC'].nil?  ? 'clang'   : ENV['CC']
# cpp    = ENV['CPP'].nil? ? 'clang++' : ENV['CPP']
AR       = 'ar'
LD       = 'ld'
IS_LINUX = `uname -s`.strip == 'Linux'
VERSION  = `cat ./VERSION`.strip

# $ldflags = "-lpthread -L. #{include_env 'LDFLAGS'}".strip
warnings = '-Wall -Wextra -Wconversion'
cflags   = "-g -fPIC #{warnings} -std=c99 -I. #{include_env 'CFLAGS'}".strip

# Expose cflags and Lua for `cflags_for`
CFLAGS = cflags
LUA    = 'lua5.1'

# Settings for LLVM
llvm_config = 'llvm-config'
if `uname -s`.strip == 'Darwin'
  llvm_config = "#{`brew --prefix llvm`.strip}/bin/llvm-config"
end
LLVM_CONFIG  = llvm_config
LLVM_MODULES = 'core analysis mcjit native'
LLVM_LIBDIR  = `#{llvm_config} --libdir`.strip
