#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "akasha::akasha" for configuration "Release"
set_property(TARGET akasha::akasha APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(akasha::akasha PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libakasha.a"
  )

list(APPEND _cmake_import_check_targets akasha::akasha )
list(APPEND _cmake_import_check_files_for_akasha::akasha "${_IMPORT_PREFIX}/lib/libakasha.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
