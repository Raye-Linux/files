# The raye-files catalog. Translations live next to Dolphin's, one file per
# language: po/<lang>/raye-files.po. ki18n_install(po) in the top
# CMakeLists.txt builds and installs them with Dolphin's own catalogs.
#
#   cmake --build build --target raye-files-pot     refreshes po/raye-files.pot
#   cmake --build build --target raye-files-po-merge merges it into every
#                                                    po/<lang>/raye-files.po

find_program(RAYE_MSGMERGE msgmerge)

add_custom_target(raye-files-pot
    COMMAND "${CMAKE_SOURCE_DIR}/Messages.sh"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Extracting Raye strings into po/raye-files.pot"
    VERBATIM
)

if(RAYE_MSGMERGE)
    file(GLOB raye_po_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*/raye-files.po")
    set(raye_merge_commands)
    foreach(po_file ${raye_po_files})
        list(APPEND raye_merge_commands
            COMMAND "${RAYE_MSGMERGE}" --quiet --update --backup=none
                    "${po_file}" "${CMAKE_CURRENT_SOURCE_DIR}/raye-files.pot")
    endforeach()
    add_custom_target(raye-files-po-merge
        ${raye_merge_commands}
        COMMENT "Merging po/raye-files.pot into the translations"
        VERBATIM
    )
    add_dependencies(raye-files-po-merge raye-files-pot)
endif()
