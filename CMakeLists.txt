cmake_minimum_required(VERSION 3.1)

project(Hivm)

# find_path(GLIB_INCLUDE_DIR glib.h PATH_SUFFIXES glib-2.0)
find_package(PkgConfig)

# GLib dependency for all sources
pkg_check_modules(GLIB glib-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

# pkg_check_modules(JEMALLOC jemalloc REQUIRED)
find_library(JEAMLLOC_STATIC_LIB libjemalloc.a)
link_libraries(${JEAMLLOC_STATIC_LIB})

set(LLVM_CONFIG  "llvm-config")
set(LLVM_MODULES core analysis mcjit native)
# Deal with LLVM on Homebrew
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  execute_process(COMMAND brew --prefix llvm OUTPUT_VARIABLE LLVM_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
  # message(STATUS "LLVM_PATH: ${LLVM_PATH}")
  set(LLVM_CONFIG        "${LLVM_PATH}/bin/${LLVM_CONFIG}")
  # set(CMAKE_LIBRARY_PATH "${LLVM_PATH}/lib")
endif()

# execute_process(COMMAND ${LLVM_CONFIG} --includedir           OUTPUT_VARIABLE LLVM_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --cflags                   OUTPUT_VARIABLE LLVM_CFLAGS  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --libfiles ${LLVM_MODULES} OUTPUT_VARIABLE LLVM_LIBS    OUTPUT_STRIP_TRAILING_WHITESPACE)
# Cleanup some of the LLVM output
separate_arguments(LLVM_LIBS)
separate_arguments(LLVM_CFLAGS)

# message(STATUS "CMAKE_LIBRARY_PATH: ${CMAKE_LIBRARY_PATH}")
# message(STATUS "LLVM_LIBS:           ${LLVM_LIBS}")
# message(STATUS "JEAMLLOC_STATIC_LIB: ${JEAMLLOC_STATIC_LIB}")

# Compiling JIT library separately
add_library(jit-compiler OBJECT src/jit-compiler)
# Flags for compiling with LLVM's APIs
target_compile_options(jit-compiler PRIVATE ${LLVM_CFLAGS})

# Link the JIT compiler with the LLVM libraries
add_custom_target(
  jit-compiler-llvm
  COMMAND ld src/jit-compiler.o ${LLVM_LIBS} -r -o src/jit-compiler-llvm.o
  DEPENDS jit-compiler
  SOURCES src/jit-compiler.c
)

# Primary Hivm library
add_library(common
  src/vm src/object src/symbol src/frame src/chunk
  src/generator src/bootstrap src/exception src/gc1
  src/jit-tracer
)

file(GLOB SOURCES src/*.c)

# Manually archive everything together
add_custom_target(
  hivm ALL
  COMMAND ar rcs libhivm.a libcommon.a src/jit-compiler-llvm.o
  SOURCES ${SOURCES}
)

# Make sure the compiler gets built before the rest so that it will get linked
add_dependencies(hivm common jit-compiler-llvm)
# target_link_libraries(hivm src/jit-compiler-llvm.o)
