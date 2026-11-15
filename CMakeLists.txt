cmake_minimum_required(VERSION 3.16)

# KDE Application Version, managed by release script
set (RELEASE_SERVICE_VERSION_MAJOR "25")
set (RELEASE_SERVICE_VERSION_MINOR "12")
set (RELEASE_SERVICE_VERSION_MICRO "3")
set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")
set (BALOO_WIDGETS_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.0")
project(Dolphin VERSION ${RELEASE_SERVICE_VERSION})

set(QT_MIN_VERSION "6.4.0")
set(KF6_MIN_VERSION "6.9.0")

# use C++20 like KF6 itself
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# ECM setup
find_package(ECM ${KF6_MIN_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(ECMGenerateDBusServiceFile)
include(ECMConfiguredInstall)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(FeatureSummary)
include(ECMQtDeclareLoggingCategory)
include(ECMDeprecationSettings)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckFunctionExists)

ecm_setup_version(${RELEASE_SERVICE_VERSION} VARIABLE_PREFIX DOLPHIN
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/dolphin_version.h"
)

# Raye: the version control interface is not ours. We link the system
# library of the same release so its plugins keep loading into Files, and
# we build and install nothing of it.
find_package(DolphinVcs REQUIRED)
set_package_properties(DolphinVcs PROPERTIES
                       DESCRIPTION "Dolphin's version control plugin interface"
                       TYPE REQUIRED
                       PURPOSE "So version control plugins load into Files"
                      )

ecm_setup_version(${RELEASE_SERVICE_VERSION} VARIABLE_PREFIX DOLPHINPRIVATE
                  SOVERSION 6
)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Concurrent
    Widgets
    Gui
    DBus
)

# Raye: Files' own pages are QML, shown in the widget window through
# QQuickWidget. Looked up here so every directory sees the targets.
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Qml
    Quick
    QuickWidgets
)

if (Qt6Gui_VERSION VERSION_GREATER_EQUAL "6.10.0")
    find_package(Qt6GuiPrivate ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)
endif()

if (UNIX AND NOT APPLE AND NOT HAIKU)
    set(HAVE_X11 TRUE)
endif()

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    KCMUtils
    NewStuff
    CoreAddons
    I18n
    DBusAddons
    Bookmarks
    Config
    KIO
    Parts
    Solid
    IconThemes
    Completion
    TextWidgets
    Notifications
    Crash
    WindowSystem
    WidgetsAddons
    Codecs
    GuiAddons
    ColorScheme
)

# Map network drive keeps share passwords in the wallet.
find_package(KF6Wallet ${KF6_MIN_VERSION} REQUIRED)

find_package(KF6UserFeedback ${KF6_MIN_VERSION})
set_package_properties(KF6UserFeedback
        PROPERTIES TYPE OPTIONAL
        PURPOSE "Used for submission of telemetry data"
        )

if(KF6UserFeedback_FOUND)
    set(HAVE_KUSERFEEDBACK TRUE)
endif()

find_package(KF6 ${KF6_MIN_VERSION} OPTIONAL_COMPONENTS
    DocTools
)

find_package(PackageKitQt6)
set_package_properties(PackageKitQt6
        PROPERTIES DESCRIPTION "Software Manager integration"
        TYPE RECOMMENDED
        PURPOSE "Used in the service menu installer"
        )
if(PackageKitQt6_FOUND)
    set(HAVE_PACKAGEKIT TRUE)
endif()

find_package(KF6FileMetaData ${KF6_MIN_VERSION})
set_package_properties(KF6FileMetaData PROPERTIES
                       URL "https://projects.kde.org/kfilemetadata"
                       TYPE REQUIRED
                       PURPOSE "For accessing file metadata labels and filesystem metadata"
                      )

find_package(KF6Baloo ${KF6_MIN_VERSION})
set_package_properties(KF6Baloo PROPERTIES DESCRIPTION "Baloo Core libraries"
                       URL "https://www.kde.org"
                       TYPE OPTIONAL
                       PURPOSE "For adding desktop-wide search and tagging support to dolphin"
                      )

find_package(KF6BalooWidgets ${BALOO_WIDGETS_VERSION})
set_package_properties(KF6BalooWidgets PROPERTIES DESCRIPTION "Baloos Widgets"
                       URL "https://www.kde.org"
                       TYPE REQUIRED
                      )

if (KF6Baloo_FOUND AND KF6BalooWidgets_FOUND)
    message(STATUS "Baloo packages are found")
    set(HAVE_BALOO TRUE)

    find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
        Multimedia
        MultimediaWidgets
    )
else ()
    message(WARNING "Baloo packages not found. They are needed for the metadata features of Dolphin (including the information panel).")
endif()

# TODO: drop HAVE_TERMINAL once we are sure the terminal panel works on Windows too.
if(WIN32)
    set(HAVE_TERMINAL FALSE)
else()
    set(HAVE_TERMINAL TRUE)
endif()

# Compatibility with platforms without native fts (e.g. musl)
check_include_files("sys/types.h;sys/stat.h;fts.h" HAVE_FTS_H)
if(HAVE_FTS_H AND NOT HAIKU)
    check_function_exists(fts_open HAVE_FTS_OPEN)
    if(NOT HAVE_FTS_OPEN)
        check_library_exists(fts fts_open "" HAVE_LIB_FTS)
    endif()
endif()

if(HAVE_LIB_FTS)
    set(FTS_LIB fts)
else()
    set(FTS_LIB "")
endif()

add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(appiumtests)
# Raye: the raye-files catalog (its pot and merge targets) and the Raye tests.
add_subdirectory(po)
# Raye: settings shipped under /etc/xdg (config/) and data files (data/*/).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config/CMakeLists.txt)
    add_subdirectory(config)
endif()
add_subdirectory(data)
if(BUILD_TESTING)
    add_subdirectory(tests/raye)
endif()

# Raye: no DolphinVcs CMake package, no development headers. Files is an
# application, not a library: it ships nothing under /usr/include.

if(FLATPAK)
    ecm_generate_dbus_service_file(
        NAME org.freedesktop.FileManager1
        EXECUTABLE "${KDE_INSTALL_FULL_BINDIR}/dolphin --daemon"
        SYSTEMD_SERVICE plasma-dolphin.service
        DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
    )
else()
    ecm_generate_dbus_service_file(
        NAME org.freedesktop.FileManager1
        EXECUTABLE "${KDE_INSTALL_FULL_BINDIR}/dolphin --daemon"
        SYSTEMD_SERVICE plasma-dolphin.service
        DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
        RENAME org.kde.dolphin.FileManager1.service
    )
endif()

ecm_install_configured_files(INPUT plasma-dolphin.service.in DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})

ecm_qt_install_logging_categories(
        EXPORT DOLPHIN
        FILE dolphin.categories
        DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
        )

ki18n_install(po)
if(KF6DocTools_FOUND)
    kdoctools_install(po)
endif()

install(FILES completions/dolphin.zsh RENAME _dolphin DESTINATION ${KDE_INSTALL_ZSHAUTOCOMPLETEDIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

include(KDEClangFormat)
# add clang-format target
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h *.c)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

include(KDEGitCommitHooks)
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
