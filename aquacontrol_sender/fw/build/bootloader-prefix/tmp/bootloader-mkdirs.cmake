# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/zenithtek/esp/esp-idf/components/bootloader/subproject"
  "/home/zenithtek/aquacontrol_fw/aquacontrol_sender/fw/build/bootloader"
  "/home/zenithtek/aquacontrol_fw/aquacontrol_sender/fw/build/bootloader-prefix"
  "/home/zenithtek/aquacontrol_fw/aquacontrol_sender/fw/build/bootloader-prefix/tmp"
  "/home/zenithtek/aquacontrol_fw/aquacontrol_sender/fw/build/bootloader-prefix/src/bootloader-stamp"
  "/home/zenithtek/aquacontrol_fw/aquacontrol_sender/fw/build/bootloader-prefix/src"
  "/home/zenithtek/aquacontrol_fw/aquacontrol_sender/fw/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/zenithtek/aquacontrol_fw/aquacontrol_sender/fw/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/zenithtek/aquacontrol_fw/aquacontrol_sender/fw/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
