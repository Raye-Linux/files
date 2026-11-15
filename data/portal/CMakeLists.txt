# What makes the system use raye-files-portal: the portal backend, its D-Bus
# and systemd entries, the portal preference, the session setting that sends
# Qt and GTK apps to the portal, and the app started for a file no app opens.
set(RAYE_PORTAL_BINARY "${CMAKE_INSTALL_PREFIX}/libexec/raye-files-portal")

install(FILES raye-files.portal DESTINATION ${KDE_INSTALL_DATADIR}/xdg-desktop-portal/portals)

configure_file(org.freedesktop.impl.portal.desktop.rayefiles.service.in org.freedesktop.impl.portal.desktop.rayefiles.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.impl.portal.desktop.rayefiles.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})

configure_file(raye-files-portal.service.in raye-files-portal.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/raye-files-portal.service DESTINATION lib/systemd/user)

configure_file(org.raye.files.openwith.desktop.in org.raye.files.openwith.desktop @ONLY)
configure_file(org.raye.files.dialogs.desktop.in org.raye.files.dialogs.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.raye.files.openwith.desktop
              ${CMAKE_CURRENT_BINARY_DIR}/org.raye.files.dialogs.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})

install(FILES 60-raye-file-dialogs.conf DESTINATION lib/environment.d)
install(FILES portals.conf DESTINATION ${KDE_INSTALL_CONFDIR}/xdg-desktop-portal)
