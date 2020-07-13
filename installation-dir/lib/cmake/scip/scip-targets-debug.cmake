#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "scip" for configuration "Debug"
set_property(TARGET scip APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(scip PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/scip"
  )

list(APPEND _IMPORT_CHECK_TARGETS scip )
list(APPEND _IMPORT_CHECK_FILES_FOR_scip "${_IMPORT_PREFIX}/bin/scip" )

# Import target "libscip" for configuration "Debug"
set_property(TARGET libscip APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libscip PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libscip.so.6.0.2.0"
  IMPORTED_SONAME_DEBUG "libscip.so.6.0"
  )

list(APPEND _IMPORT_CHECK_TARGETS libscip )
list(APPEND _IMPORT_CHECK_FILES_FOR_libscip "${_IMPORT_PREFIX}/lib/libscip.so.6.0.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
