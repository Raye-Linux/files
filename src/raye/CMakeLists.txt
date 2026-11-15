# Raye code. Nothing in this file names a package: every FX package drops a
# folder here and the globs below pick it up, so nobody has to edit it.
#
#   src/raye/*.cpp, src/raye/<folder>/*.cpp    C++ widgets, models and jobs
#   src/raye/<folder>/*.ui                      Designer forms, if a page needs one
#   src/raye/qml/<folder>/*.qml    pages of the QML module org.raye.files
#
# The C++ is compiled into one object library and folded into the private
# library, so the application, the KParts plugin, the configuration modules
# and the tests all see it. Strings here use the raye-files catalog, not
# Dolphin's: keep it that way, the two are merged separately.

# Dolphin's headers are built with the implicit QString conversions on, so
# anything that includes them needs the same relaxation src/ uses.
remove_definitions(
    -DQT_NO_CAST_FROM_BYTEARRAY
    -DQT_NO_CAST_FROM_ASCII
    -DQT_NO_CAST_TO_ASCII
)

remove_definitions(-DTRANSLATION_DOMAIN=\"dolphin\")
add_definitions(-DTRANSLATION_DOMAIN=\"raye-files\")

# Qt Quick is looked up in the top CMakeLists.txt: the targets below are
# linked into the private library, which lives one directory up.
include(ECMQmlModule)

file(GLOB_RECURSE raye_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
)
list(FILTER raye_sources EXCLUDE REGEX "/qml/")
# The entry point of raye-files-portal, built below.
list(FILTER raye_sources EXCLUDE REGEX "/portal/main\\.cpp$")
# The network drive helper (runs as root, links nothing of Files) and the
# reconnect tool have their own entry points, built below.
list(FILTER raye_sources EXCLUDE REGEX "/netdrive/(helper|reconnect)/")

# The archive engine (raye-archive): every format is read and written by it.
find_package(PkgConfig REQUIRED)
pkg_check_modules(RayeArchive REQUIRED IMPORTED_TARGET GLOBAL raye-archive)

if(raye_sources)
    add_library(rayefiles OBJECT ${raye_sources})

    # An object library, not a static one: every symbol reaches the private
    # library, so a widget registered in a constructor is never dropped.
    set_target_properties(rayefiles PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
    )

    # the archive engine reports errors with exceptions
    kde_target_enable_exceptions(rayefiles PRIVATE)

    target_include_directories(rayefiles PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_BINARY_DIR}/src"
    )

    target_link_libraries(rayefiles PUBLIC
        Qt6::Widgets
        Qt6::Gui
        Qt6::DBus
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickWidgets
        KF6::I18n
        KF6::KIOCore
        KF6::KIOWidgets
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::IconThemes
        KF6::WidgetsAddons
        KF6::WindowSystem
        KF6::ColorScheme
        KF6::Parts
        Qt6::Concurrent
        KF6::FileMetaData
        KF6::Wallet
        PkgConfig::RayeArchive
    )

    target_sources(dolphinprivate PRIVATE $<TARGET_OBJECTS:rayefiles>)
    target_link_libraries(dolphinprivate PUBLIC
        PkgConfig::RayeArchive
        KF6::Wallet
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickWidgets
        KF6::ConfigGui
    )
    target_include_directories(dolphinprivate PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

file(GLOB_RECURSE raye_qml_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml"
)

if(raye_qml_sources)
    # One module for the whole application: org.raye.files. A page is reached
    # as org.raye.files.<Type>, whichever folder of qml/ it sits in.
    ecm_add_qml_module(rayefilesqml URI "org.raye.files" GENERATE_PLUGIN_SOURCE)
    foreach(qml_file ${raye_qml_sources})
        ecm_target_qml_sources(rayefilesqml SOURCES "${qml_file}")
    endforeach()
    ecm_finalize_qml_module(rayefilesqml DESTINATION ${KDE_INSTALL_QMLDIR})
endif()

# The Open, Save and "Open with" dialogs of every app (data/portal has the
# files that make the system start it).
add_executable(raye-files-portal portal/main.cpp)
target_include_directories(raye-files-portal PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/portal")
target_compile_definitions(raye-files-portal PRIVATE RAYE_FILES_VERSION="${RELEASE_SERVICE_VERSION}")
target_link_libraries(raye-files-portal PRIVATE dolphinprivate KF6::CoreAddons KF6::I18n KF6::KIOGui KF6::Service)
set_target_properties(raye-files-portal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS raye-files-portal DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec)

# rayearchive:/ opens an archive like a folder (browse, drag out, add, delete).
kcoreaddons_add_plugin(kio_rayearchive SOURCES kioworker/kio_rayearchive.cc INSTALL_NAMESPACE "kf6/kio")
target_include_directories(kio_rayearchive PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(kio_rayearchive dolphinprivate KF6::KIOCore KF6::I18n)

# Map network drive: the root helper pkexec starts, and what reconnects the
# drives at sign-in.
add_executable(raye-files-netdrive-helper netdrive/helper/main.cpp netdrive/helperrules.cpp)
target_include_directories(raye-files-netdrive-helper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/netdrive")
target_compile_definitions(raye-files-netdrive-helper PRIVATE RAYE_NETDRIVE_HELPER)
set_target_properties(raye-files-netdrive-helper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS raye-files-netdrive-helper DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec)

add_executable(raye-files-netdrives netdrive/reconnect/main.cpp)
target_include_directories(raye-files-netdrives PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(raye-files-netdrives PRIVATE dolphinprivate KF6::I18n KF6::Notifications Qt6::Widgets)
set_target_properties(raye-files-netdrives PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS raye-files-netdrives DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec)
