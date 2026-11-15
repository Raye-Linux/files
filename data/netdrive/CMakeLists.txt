# Map network drive: who may run the mount helper, and what reconnects the
set(RAYE_LIBEXEC "${CMAKE_INSTALL_PREFIX}/libexec")
# drives at sign-in.
configure_file(org.raye.files.network-drive.policy ${CMAKE_CURRENT_BINARY_DIR}/org.raye.files.network-drive.policy @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.raye.files.network-drive.policy DESTINATION ${KDE_INSTALL_DATADIR}/polkit-1/actions)

configure_file(raye-files-netdrives.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/raye-files-netdrives.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/raye-files-netdrives.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
