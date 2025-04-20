#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "MINIZIP::minizip-ng" for configuration "Release"
set_property(TARGET MINIZIP::minizip-ng APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MINIZIP::minizip-ng PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libminizip-ng.a"
  )

list(APPEND _cmake_import_check_targets MINIZIP::minizip-ng )
list(APPEND _cmake_import_check_files_for_MINIZIP::minizip-ng "${_IMPORT_PREFIX}/lib/libminizip-ng.a" )

# Import target "MINIZIP::libzstd_static" for configuration "Release"
set_property(TARGET MINIZIP::libzstd_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MINIZIP::libzstd_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libzstd.a"
  )

list(APPEND _cmake_import_check_targets MINIZIP::libzstd_static )
list(APPEND _cmake_import_check_files_for_MINIZIP::libzstd_static "${_IMPORT_PREFIX}/lib/libzstd.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
