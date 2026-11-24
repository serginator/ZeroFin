# Install rules and the Debian package for the Cardputer Zero (CP0).
#
# Layout (the same one the launcher expects for the apps in the Store):
#   /usr/share/APPLaunch/bin/zerofin
#   /usr/share/APPLaunch/applications/zerofin.desktop
#   /usr/share/APPLaunch/share/images/zerofin.png   (icon, relative to /usr/share/APPLaunch)
#   /usr/share/zerofin/fonts/                       (the app looks here, see src/app/ui/fonts.cpp)
#
# Build the package:  cmake --build build --target package   (or: cpack --config build/CPackConfig.cmake)

set(ZEROFIN_MAINTAINER "serginator <serginator@gmail.com>"
    CACHE STRING "Maintainer field of the .deb (the Store rejects a placeholder)")
set(ZEROFIN_DEB_REVISION "" CACHE STRING "Optional Debian revision, for example 1 gives 0.1.0-1")

set(_zerofin_gen "${CMAKE_CURRENT_BINARY_DIR}/generated/package")
configure_file("${PROJECT_SOURCE_DIR}/packaging/zerofin.desktop.in" "${_zerofin_gen}/zerofin.desktop" COPYONLY)

# Everything is in the component "zerofin", so the package does not take the LVGL headers and libraries.
install(TARGETS zerofin RUNTIME DESTINATION share/APPLaunch/bin COMPONENT zerofin)
install(FILES "${_zerofin_gen}/zerofin.desktop" DESTINATION share/APPLaunch/applications COMPONENT zerofin)
install(FILES "${PROJECT_SOURCE_DIR}/assets/images/zerofin.png" DESTINATION share/APPLaunch/share/images COMPONENT zerofin)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets/fonts/" DESTINATION share/zerofin/fonts COMPONENT zerofin
        PATTERN ".DS_Store" EXCLUDE)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION share/doc/zerofin RENAME copyright COMPONENT zerofin)
install(FILES "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" DESTINATION share/doc/zerofin COMPONENT zerofin)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_PACKAGE_NAME "zerofin")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "${ZEROFIN_MAINTAINER}")
set(CPACK_PACKAGE_VENDOR "ZeroFin contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Jellyfin video client for the Cardputer Zero")
set(CPACK_PACKAGE_DESCRIPTION
    "Browse and play video from a Jellyfin server on the M5Stack Cardputer Zero. Not an official Jellyfin app.")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES TRUE)

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/serginator/ZeroFin")
if(ZEROFIN_DEB_REVISION)
  set(CPACK_DEBIAN_PACKAGE_RELEASE "${ZEROFIN_DEB_REVISION}")
endif()
# dpkg-shlibdeps finds the library packages. curl is a program the app starts, so it is listed by hand.
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL zerofin)
set(CPACK_DEBIAN_ZEROFIN_PACKAGE_NAME "zerofin")
set(CPACK_DEBIAN_ZEROFIN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_ZEROFIN_PACKAGE_DEPENDS "curl")
set(CPACK_DEBIAN_ZEROFIN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

include(CPack)
