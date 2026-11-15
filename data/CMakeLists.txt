# Data files Raye adds to Files, one folder per kind (data/portal,
# data/notifyrc, ...). Each folder with its own CMakeLists.txt is built;
# a new folder needs no edit here.
file(GLOB raye_data_dirs LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/*)
list(SORT raye_data_dirs)
foreach(dir ${raye_data_dirs})
    if(IS_DIRECTORY ${dir} AND EXISTS ${dir}/CMakeLists.txt)
        add_subdirectory(${dir})
    endif()
endforeach()
