plasma_install_package(package org.kde.plasma.mediaframe)

set(mediaframeplugin_SRCS
    plugin/mediaframe.cpp
    plugin/mediaframeplugin.cpp
)

add_library(mediaframeplugin SHARED ${mediaframeplugin_SRCS})

target_link_libraries(mediaframeplugin
                      Qt5::Core
                      Qt5::Qml
                      Qt5::Quick
                      KF5::I18n
                      KF5::KIOCore
                      KF5::KIOWidgets)

install(TARGETS mediaframeplugin DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/private/mediaframe)
install(FILES plugin/qmldir DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/private/mediaframe)





