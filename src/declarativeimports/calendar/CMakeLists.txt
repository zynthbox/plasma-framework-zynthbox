add_library(calendarplugin SHARED)

target_sources(calendarplugin PRIVATE
    calendarplugin.cpp
    #incidencemodifier.cpp
    calendar.cpp
    calendardata.cpp
    #calendardayhelper.cpp
    #calendarroleproxymodel.cpp
    #datetimerangefiltermodel.cpp
    daysmodel.cpp
    eventdatadecorator.cpp
    eventpluginsmanager.cpp
)

target_link_libraries(calendarplugin
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Gui
    KF5::I18n
    KF5::CalendarEvents
    KF5::CoreAddons
)

install(TARGETS calendarplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/calendar)
install(DIRECTORY qml/ DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/calendar)

ecm_generate_qmltypes(org.kde.plasma.calendar 2.0 DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/calendar)
