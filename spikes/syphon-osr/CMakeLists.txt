# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
#
# Throwaway macOS Syphon OSR feasibility spike (Step 4 of 4).
#
# Adapted from the bundled cefsimple sample's CMakeLists.txt (the concrete file
# generated into the CEF binary distribution under tests/cefsimple/). This spike
# is MACOS-ONLY: drop this whole directory into an unpacked CEF macOS Standard
# Distribution as <cef-distro>/tests/syphon-osr/ and add
# `add_subdirectory(tests/syphon-osr)` to the distro's top-level CMakeLists.txt
# (next to the existing `add_subdirectory(tests/cefsimple)`). The CEF CMake
# macros (SET_EXECUTABLE_TARGET_PROPERTIES, COPY_MAC_FRAMEWORK,
# COPY_MAC_RESOURCES, SET_CEF_TARGET_OUT_DIR, the CEF_HELPER_APP_SUFFIXES list,
# CEF_STANDARD_LIBS, libcef_dll_wrapper, etc.) are in scope because they are
# defined by cmake/cef_variables.cmake / cmake/cef_macros.cmake, which the
# distro's top-level CMakeLists.txt includes before any add_subdirectory().

# This spike is macOS-only. Refuse to configure on any other platform.
if(NOT OS_MAC)
  message(FATAL_ERROR "syphon-osr spike is macOS-only")
endif()

#
# Source files.
#

# syphon-osr browser-process + shared sources.
set(SYPHON_OSR_SRCS
  main_mac.mm
  syphon_osr_app.cc
  syphon_osr_app.h
  syphon_osr_handler.cc
  syphon_osr_handler.h
  syphon_bridge.mm
  syphon_bridge.h
  )

# syphon-osr helper (sub-process) sources.
set(SYPHON_OSR_HELPER_SRCS
  process_helper_mac.cc
  )

# syphon-osr bundle resources. The spike ships no .icns/.nib resources, so this
# is intentionally empty (COPY_MAC_RESOURCES below is a no-op).
# VERIFY: if you add a MainMenu.xib / .icns, list it here under mac/.
set(SYPHON_OSR_RESOURCES_SRCS
  )

#
# Shared configuration.
#

# Target executable names. The app bundle is "Syphon OSR Spike.app".
set(CEF_TARGET "syphon_osr")
set(CEF_HELPER_TARGET "syphon_osr_Helper")
set(CEF_HELPER_OUTPUT_NAME "Syphon OSR Spike Helper")

# Determine the target output directory.
SET_CEF_TARGET_OUT_DIR()

#
# Syphon + Metal stack (spike-specific, not in cefsimple).
#

# Path to the directory that contains a built Syphon.framework. Build it from
# https://github.com/Syphon/Syphon-Framework (Xcode) and pass the output dir:
#   cmake ... -DSYPHON_FRAMEWORK_DIR=/path/to/Syphon-Framework/build/Release
set(SYPHON_FRAMEWORK_DIR "" CACHE PATH "Directory containing Syphon.framework")
if(NOT SYPHON_FRAMEWORK_DIR)
  message(FATAL_ERROR
    "SYPHON_FRAMEWORK_DIR is not set. Build Syphon.framework from "
    "Syphon-Framework and pass -DSYPHON_FRAMEWORK_DIR=/path/to/dir "
    "(the directory that *contains* Syphon.framework).")
endif()
if(NOT EXISTS "${SYPHON_FRAMEWORK_DIR}/Syphon.framework")
  message(FATAL_ERROR
    "Syphon.framework not found under SYPHON_FRAMEWORK_DIR="
    "${SYPHON_FRAMEWORK_DIR}")
endif()

#
# Mac OS X configuration.
#

# Output path for the main app bundle.
set(CEF_APP "${CEF_TARGET_OUT_DIR}/${CEF_TARGET}.app")

# Variables referenced from the main Info.plist file.
set(EXECUTABLE_NAME "${CEF_TARGET}")
set(PRODUCT_NAME "${CEF_TARGET}")

# Main app bundle target.
add_executable(${CEF_TARGET} MACOSX_BUNDLE ${SYPHON_OSR_RESOURCES_SRCS} ${SYPHON_OSR_SRCS})
SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
target_link_libraries(${CEF_TARGET} libcef_dll_wrapper ${CEF_STANDARD_LIBS})
set_target_properties(${CEF_TARGET} PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist
  )

# Compile only the Obj-C++ Syphon bridge with ARC. cefsimple's own .mm sources
# may not be ARC-clean, so scope ARC to this one file rather than the whole
# target.
set_source_files_properties(syphon_bridge.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

# Link the Metal/IOSurface stack and Syphon into the MAIN app only (the helper
# sub-processes never touch Metal or Syphon).
target_link_libraries(${CEF_TARGET}
  "-framework Metal"
  "-framework IOSurface"
  "-framework CoreVideo"
  "-framework Foundation"
  "-framework Cocoa"
  )

# Syphon.framework: add its parent dir to the framework search path (compile +
# link), then link the framework.
target_include_directories(${CEF_TARGET} PRIVATE "${SYPHON_FRAMEWORK_DIR}")
target_compile_options(${CEF_TARGET} PRIVATE "-F${SYPHON_FRAMEWORK_DIR}")
target_link_libraries(${CEF_TARGET}
  "-F${SYPHON_FRAMEWORK_DIR}"
  "-framework Syphon"
  )

# Copy the CEF framework into the app bundle's Contents/Frameworks directory and
# create the necessary symlinks.
COPY_MAC_FRAMEWORK("${CEF_TARGET}" "${CEF_BINARY_DIR}" "${CEF_APP}")

# VERIFY: Embed Syphon.framework into the app bundle so dyld can load it at
# runtime (Syphon's install_name is typically @rpath/Syphon.framework/...).
# This mirrors how COPY_MAC_FRAMEWORK embeds the CEF framework. Exact rpath /
# code-signing handling may need adjustment on a real Mac:
#   - The main executable likely needs an @executable_path/../Frameworks rpath
#     (SET_EXECUTABLE_TARGET_PROPERTIES sets one for the CEF framework; confirm
#     it also covers Syphon, otherwise add via INSTALL_RPATH / -rpath).
#   - On Apple Silicon / hardened runtime, embedded frameworks must be (re)signed
#     after copy. For a throwaway local spike, an ad-hoc sign is enough:
#       codesign --force --sign - "<app>/Contents/Frameworks/Syphon.framework"
#     (left as a manual step / VERIFY rather than baked in, since the signing
#     identity is environment-specific).
add_custom_command(
  TARGET ${CEF_TARGET}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${SYPHON_FRAMEWORK_DIR}/Syphon.framework"
          "${CEF_APP}/Contents/Frameworks/Syphon.framework"
  # VERIFY: ad-hoc re-sign the embedded Syphon framework for the hardened
  # runtime. Harmless if codesign is unavailable (|| true keeps the build green).
  COMMAND codesign --force --sign - "${CEF_APP}/Contents/Frameworks/Syphon.framework" || true
  VERBATIM
  )

# Create the multiple Helper app bundle targets. CEF_HELPER_APP_SUFFIXES is
# provided by cmake/cef_variables.cmake and expands to the per-process bundles:
#   ""  " (GPU)"  " (Plugin)"  " (Renderer)"  " (Alerts)".
foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
  # Convert to a list and extract the suffix values.
  string(REPLACE ":" ";" _suffix_list ${_suffix_list})
  list(GET _suffix_list 0 _name_suffix)
  list(GET _suffix_list 1 _target_suffix)
  list(GET _suffix_list 2 _plist_suffix)

  # Define Helper target and output names.
  set(_helper_target "${CEF_HELPER_TARGET}${_target_suffix}")
  set(_helper_output_name "${CEF_HELPER_OUTPUT_NAME}${_name_suffix}")

  # Create Helper-specific variants of the helper-Info.plist file. Do this
  # manually because the configure_file command (which is executed as part of
  # MACOSX_BUNDLE_INFO_PLIST) uses global env variables and would insert the
  # wrong values with multiple targets.
  set(_helper_info_plist "${CMAKE_CURRENT_BINARY_DIR}/helper-Info${_target_suffix}.plist")
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mac/helper-Info.plist" _plist_contents)
  string(REPLACE "\${EXECUTABLE_NAME}" "${_helper_output_name}" _plist_contents ${_plist_contents})
  string(REPLACE "\${PRODUCT_NAME}" "${_helper_output_name}" _plist_contents ${_plist_contents})
  string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_plist_suffix}" _plist_contents ${_plist_contents})
  file(WRITE ${_helper_info_plist} ${_plist_contents})

  # Create Helper executable target.
  add_executable(${_helper_target} MACOSX_BUNDLE ${SYPHON_OSR_HELPER_SRCS})
  SET_EXECUTABLE_TARGET_PROPERTIES(${_helper_target})
  add_dependencies(${_helper_target} libcef_dll_wrapper)
  target_link_libraries(${_helper_target} libcef_dll_wrapper ${CEF_STANDARD_LIBS})
  set_target_properties(${_helper_target} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${_helper_info_plist}
    OUTPUT_NAME ${_helper_output_name}
    )

  # Add the Helper as a dependency of the main executable target.
  add_dependencies(${CEF_TARGET} "${_helper_target}")

  # Copy the Helper app bundle into the Frameworks directory.
  add_custom_command(
    TARGET ${CEF_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_TARGET_OUT_DIR}/${_helper_output_name}.app"
            "${CEF_APP}/Contents/Frameworks/${_helper_output_name}.app"
    VERBATIM
    )
endforeach()

# Manually process and copy over resource files.
# The Xcode generator can support this via the set_target_properties RESOURCE
# directive but that doesn't properly handle nested resource directories.
# Remove these prefixes from input file paths.
set(PREFIXES "mac/")
COPY_MAC_RESOURCES("${SYPHON_OSR_RESOURCES_SRCS}" "${PREFIXES}" "${CEF_TARGET}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CEF_APP}")
