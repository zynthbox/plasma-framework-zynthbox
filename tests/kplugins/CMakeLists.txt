add_executable(plugintest
    main.cpp
    plugintest.cpp
)

target_link_libraries(plugintest KF5::Plasma KF5::I18n Qt${QT_MAJOR_VERSION}::DBus Qt${QT_MAJOR_VERSION}::Widgets)

#install(TARGETS plugintest ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

