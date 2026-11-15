# System defaults of Files, installed under /etc/xdg (explorer spec 3.18).
# Accounts keep their own copy in ~/.config, which wins over these.
install(FILES
    dolphinrc
    kded_device_automounterrc
    DESTINATION ${KDE_INSTALL_CONFDIR}
)

# The first run (src/raye/firstrun) works on the shared places list, which
# needs KIO's file widgets. The Raye objects are built in src/raye, linked
# into the private library that already uses them; give the object target
# the headers too. This directory is read after src/, so the target exists.
if(TARGET rayefiles)
    target_link_libraries(rayefiles PRIVATE KF6::KIOFileWidgets)
endif()
