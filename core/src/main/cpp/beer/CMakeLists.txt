# beer/sandhook native code is built via ndk-build (core/src/main/cpp/beer/Android.mk)
# and statically linked into core's libbba.so. This file is kept only as a
# pointer for anyone looking for the old standalone shared-library build.
cmake_minimum_required(VERSION 3.4.1)
message(FATAL_ERROR "beer is statically linked into libbba; use core/src/main/cpp/Android.mk")
