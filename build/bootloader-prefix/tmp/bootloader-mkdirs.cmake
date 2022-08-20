# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/kilvia.da.s.carvalho/esp/esp-idf/components/bootloader/subproject"
  "C:/Users/kilvia.da.s.carvalho/hidroponia/build/bootloader"
  "C:/Users/kilvia.da.s.carvalho/hidroponia/build/bootloader-prefix"
  "C:/Users/kilvia.da.s.carvalho/hidroponia/build/bootloader-prefix/tmp"
  "C:/Users/kilvia.da.s.carvalho/hidroponia/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/kilvia.da.s.carvalho/hidroponia/build/bootloader-prefix/src"
  "C:/Users/kilvia.da.s.carvalho/hidroponia/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/kilvia.da.s.carvalho/hidroponia/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
