if(HAVE_X11 AND XCB_XCB_FOUND AND XCB_COMPOSITE_FOUND AND XCB_DAMAGE_FOUND)
    set(HAVE_XCB_COMPOSITE TRUE)
else()
    set(HAVE_XCB_COMPOSITE FALSE)
endif()

configure_file(config-x11.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-x11.h)

add_library(corebindingsplugin SHARED)

target_sources(corebindingsplugin PRIVATE
    corebindingsplugin.cpp
    colorscope.cpp
    datamodel.cpp
    datasource.cpp
    #    runnermodel.cpp
    svgitem.cpp
    framesvgitem.cpp
    quicktheme.cpp
    tooltip.cpp
    tooltipdialog.cpp
    serviceoperationstatus.cpp
    iconitem.cpp
    units.cpp
    windowthumbnail.cpp
    managedtexturenode.cpp
)

target_link_libraries(corebindingsplugin
        Qt${QT_MAJOR_VERSION}::Quick
        Qt${QT_MAJOR_VERSION}::Qml
        KF5::Declarative
        KF5::IconThemes
        KF5::I18n
        Qt${QT_MAJOR_VERSION}::Svg
        KF5::Service #for kplugininfo.h
        KF5::WindowSystem
        KF5::Plasma
        KF5::PlasmaQuick
        KF5::QuickAddons
)

if(HAVE_X11)
  target_link_libraries(corebindingsplugin ${X11_LIBRARIES} XCB::XCB )
  if (TARGET Qt5::X11Extras)
      target_link_libraries(corebindingsplugin Qt5::X11Extras)
  elseif(TARGET Qt6::Gui)
      target_link_libraries(corebindingsplugin Qt6::GuiPrivate)
  endif()

  if(XCB_COMPOSITE_FOUND AND XCB_DAMAGE_FOUND)
    target_link_libraries(corebindingsplugin
        XCB::DAMAGE
        XCB::COMPOSITE
        )
  endif()

  if(HAVE_GLX)
    target_link_libraries(corebindingsplugin OpenGL::GLX)
  endif()
endif()

if(HAVE_EGL)
    target_link_libraries(corebindingsplugin OpenGL::EGL)
else()

endif()

if(QtGui_Enabled_Features MATCHES "opengles2")
    target_link_libraries(corebindingsplugin Qt${QT_MAJOR_VERSION}::Gui_GLESv2)
endif()

install(TARGETS corebindingsplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/core)
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/core)

install(FILES private/DefaultToolTip.qml DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/core/private)

ecm_generate_qmltypes(org.kde.plasma.core 2.0 DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/core)

#also "install" to $BUILD_DIR/bin for unit tests
add_custom_command(TARGET corebindingsplugin POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E
                        copy ${CMAKE_CURRENT_SOURCE_DIR}/qmldir ${CMAKE_BINARY_DIR}/bin/org/kde/plasma/core/qmldir)
add_custom_command(TARGET corebindingsplugin POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E
                    copy $<TARGET_FILE:corebindingsplugin> ${CMAKE_BINARY_DIR}/bin/org/kde/plasma/core/)
add_custom_command(TARGET corebindingsplugin POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E
                        copy ${CMAKE_CURRENT_SOURCE_DIR}/private/DefaultToolTip.qml ${CMAKE_BINARY_DIR}/bin/org/kde/plasma/core/private/DefaultToolTip.qml)

