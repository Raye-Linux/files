# What hides the system's own partitions and folders (This PC, the Raye
# drive): a udev rule, the helper it calls, and the list behind /.hidden.
install(FILES 90-raye-hide-system.rules DESTINATION lib/udev/rules.d)
install(PROGRAMS raye-is-system-dm DESTINATION lib/raye/udev)

# /.hidden belongs at the very top of the file system, so it only goes there
# for a system install (prefix /usr); anywhere else it stays in the prefix.
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    install(FILES dot-hidden DESTINATION / RENAME .hidden)
else()
    install(FILES dot-hidden DESTINATION ${KDE_INSTALL_DATADIR}/raye-files RENAME root.hidden)
endif()
