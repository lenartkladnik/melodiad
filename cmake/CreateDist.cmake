set(temp_path "${TEMP_DIST_DIR}/${PLATFORM}")
set(bin_ext "")
if(PLATFORM STREQUAL "windows")
  set(bin_ext ".exe")
endif()

file(MAKE_DIRECTORY "${temp_path}")

get_filename_component(bin_name "${BIN_PATH}" NAME)
file(COPY "${BIN_PATH}" DESTINATION "${temp_path}")
file(RENAME "${temp_path}/${bin_name}" "${temp_path}/${PROJECT_NAME}${bin_ext}")

set(do_archive FALSE)
if(ARCHIVE_DIST AND NOT IS_DEBUG)
  set(do_archive TRUE)
endif()

if(do_archive)
  message(STATUS "Creating archive for ${PLATFORM} (${VERSION})")

  file(MAKE_DIRECTORY "${OUT_DIR}")

  set(archive_stem "${OUT_DIR}/${PROJECT_NAME}-${PLATFORM}-${VERSION}")
  if(PLATFORM STREQUAL "linux")
    set(archive_path "${archive_stem}.tar.gz")
    set(tar_format "gztar")
  else()
    set(archive_path "${archive_stem}.zip")
    set(tar_format "zip")
  endif()

  if(EXISTS "${archive_path}")
    file(REMOVE "${archive_path}")
  endif()

  if(COPY_BIN)
    file(RENAME "${temp_path}/${PROJECT_NAME}${bin_ext}" "${archive_stem}${bin_ext}")
  endif()

  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar cf "${archive_path}" --format=${tar_format} .
    WORKING_DIRECTORY "${temp_path}"
    RESULT_VARIABLE tar_result
  )
  if(NOT tar_result EQUAL 0)
    message(FATAL_ERROR "Failed to create archive ${archive_path}")
  endif()
else()
  message(STATUS "Creating destination directory for ${PLATFORM} (${VERSION})")

  set(dir_path "${OUT_DIR}/${VERSION}")
  file(MAKE_DIRECTORY "${dir_path}")

  file(GLOB temp_items RELATIVE "${temp_path}" "${temp_path}/*")
  foreach(item ${temp_items})
    if(NOT item STREQUAL ".music_data")
      if(EXISTS "${dir_path}/${item}")
          file(REMOVE_RECURSE "${dir_path}/${item}")
      endif()
      file(RENAME "${temp_path}/${item}" "${dir_path}/${item}")
    elseif(NOT EXISTS "${dir_path}/${item}")
      file(RENAME "${temp_path}/${item}" "${dir_path}/${item}")
    endif()
  endforeach()
endif()

file(REMOVE_RECURSE "${TEMP_DIST_DIR}")

