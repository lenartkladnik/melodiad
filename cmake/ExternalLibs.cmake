# name | path (external/lib/{path}) | git (url to git repo) | globs (a list of globs for the git repo) | "stamp" path (if this path exists the lib is considered downloaded)
set(EXTERNAL_LIBS
  "thread-pool||https://github.com/bshoshany/thread-pool|include/BS_thread_pool.hpp|BS_thread_pool.hpp"
  "stb|stb|https://github.com/nothings/stb|stb_image_write.h,stb_image_resize2.h,stb_image.h|stb/*"
  "json|nlohmann|https://github.com/nlohmann/json|single_include/nlohmann/json.hpp|nlohmann/*"
  "cpp-httplib||https://github.com/yhirose/cpp-httplib|httplib.h|httplib.h"
  "atomic_queue|atomic_queue|https://github.com/max0x7ba/atomic_queue|include/atomic_queue/*|atomic_queue/*"
)

set(clone_base "${CMAKE_SOURCE_DIR}/external/tmp-fetch")

function(_melodia_fetch_one entry)
  string(REPLACE "|" ";" fields "${entry}")
  list(GET fields 0 lib_name)
  list(GET fields 1 dest_subdir)
  list(GET fields 2 git_url)
  list(GET fields 3 globs_csv)
  list(GET fields 4 stamp_path)
  string(REPLACE "," ";" glob_list "${globs_csv}")

  set(clone_dir "${clone_base}/${lib_name}")
  set(dest_dir "${CMAKE_SOURCE_DIR}/external/lib/${dest_subdir}")

  file(GLOB _existing "${CMAKE_SOURCE_DIR}/external/lib/${stamp_path}")
  if(_existing)
      message(STATUS "external lib '${lib_name}' already present in ${dest_dir}, skipping")
      return()
  endif()

  if(NOT EXISTS "${clone_dir}/.git")
    message(STATUS "Cloning external lib '${lib_name}' from ${git_url}")
    execute_process(
        COMMAND git clone --depth 1 "${git_url}" "${clone_dir}"
        RESULT_VARIABLE _clone_result
    )
    if(NOT _clone_result EQUAL 0)
        message(FATAL_ERROR "Failed to clone external lib '${lib_name}' from ${git_url}")
    endif()
  endif()

  file(MAKE_DIRECTORY "${dest_dir}")

  foreach(pattern ${glob_list})
    if(pattern MATCHES "^(.*)/\\*$") # Folder wildcard (eg.: include/*)
      set(src_dir "${clone_dir}/${CMAKE_MATCH_1}")
      if(NOT EXISTS "${src_dir}")
          message(WARNING "external lib '${lib_name}': expected directory '${CMAKE_MATCH_1}' not found after clone")
          continue()
      endif()
      file(COPY "${src_dir}/" DESTINATION "${dest_dir}")
    else()
      file(GLOB _matches "${clone_dir}/${pattern}")
      if(NOT _matches)
          message(WARNING "external lib '${lib_name}': glob '${pattern}' matched nothing after clone")
          continue()
      endif()
      foreach(_f ${_matches})
          file(COPY "${_f}" DESTINATION "${dest_dir}")
      endforeach()
    endif()
  endforeach()

  message(STATUS "external lib '${lib_name}' populated into ${dest_dir}")
endfunction()

function(melodia_fetch_external_libs type)
  file(MAKE_DIRECTORY "${clone_base}")
  foreach(entry ${type})
    _melodia_fetch_one("${entry}")
  endforeach()
  file(REMOVE_RECURSE "${clone_base}")
endfunction()
