/*
 *   Copyright 2008 Chani Armitage <chani@kde.org>
 *   Copyright 2008, 2009 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2010 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "containmentinterface.h"
#include "wallpaperinterface.h"
#include <kdeclarative/qmlobject.h>

#include <QClipboard>
#include <QQmlExpression>
#include <QQmlProperty>
#include <QMimeData>

#include <KActionCollection>
#include <KAuthorized>
#include <QDebug>
#include <KLocalizedString>

#include <plasma.h>
#include <Plasma/ContainmentActions>
#include <Plasma/Corona>
#include <Plasma/Package>
#include <Plasma/PluginLoader>

#include "kdeclarative/configpropertymap.h"

ContainmentInterface::ContainmentInterface(DeclarativeAppletScript *parent)
    : AppletInterface(parent),
      m_wallpaperInterface(0)
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(QQuickItem::ItemAcceptsDrops);

    qmlRegisterType<ContainmentInterface>();

    connect(containment(), &Plasma::Containment::appletRemoved,
            this, &ContainmentInterface::appletRemovedForward);
    connect(containment(), &Plasma::Containment::appletAdded,
            this, &ContainmentInterface::appletAddedForward);
    connect(containment(), &Plasma::Containment::screenChanged,
            this, &ContainmentInterface::screenChanged);
    connect(containment(), &Plasma::Containment::activityChanged,
            this, &ContainmentInterface::activityChanged);
    connect(containment(), &Plasma::Containment::wallpaperChanged,
            this, &ContainmentInterface::loadWallpaper);

     if (containment()->corona()) {
         connect(containment()->corona(), &Plasma::Corona::availableScreenRegionChanged,
                 this, &ContainmentInterface::availableScreenRegionChanged);
     }
}

void ContainmentInterface::init()
{
    AppletInterface::init();

    foreach (QObject *appletObj, m_appletInterfaces) {
        AppletInterface *applet = qobject_cast<AppletInterface *>(appletObj);
        if (applet) {
            if (!applet->qmlObject()) {
                applet->init();
            }
            m_appletInterfaces << applet;
            emit appletAdded(applet);
        }
    }

    if (!m_appletInterfaces.isEmpty()) {
        emit appletsChanged();
    }
}

QList <QObject *> ContainmentInterface::applets()
{
    return m_appletInterfaces;
}

void ContainmentInterface::setDrawWallpaper(bool drawWallpaper)
{
    if (drawWallpaper == m_appletScriptEngine->drawWallpaper()) {
        return;
    }

    m_appletScriptEngine->setDrawWallpaper(drawWallpaper);

    loadWallpaper();
}

bool ContainmentInterface::drawWallpaper()
{
    return m_appletScriptEngine->drawWallpaper();
}

Plasma::Types::ContainmentType ContainmentInterface::containmentType() const
{
    return m_appletScriptEngine->containmentType();
}

void ContainmentInterface::setContainmentType(Plasma::Types::ContainmentType type)
{
    m_appletScriptEngine->setContainmentType(type);
}

int ContainmentInterface::screen() const
{
    return containment()->screen();
}

void ContainmentInterface::lockWidgets(bool locked)
{
    containment()->setImmutability(!locked ? Plasma::Types::Mutable : Plasma::Types::UserImmutable);
    emit immutableChanged();
}

QRectF ContainmentInterface::screenGeometry(int id) const
{
    QRectF rect;
    if (containment()->corona()) {
        rect = QRectF(containment()->corona()->screenGeometry(id));
    }

    return rect;
}

QVariantList ContainmentInterface::availableScreenRegion(int id) const
{
    QRegion reg;
    if (containment()->corona()) {
        reg = containment()->corona()->availableScreenRegion(id);
    }

    QVariantList regVal;
    foreach (QRect rect, reg.rects()) {
        regVal << QVariant::fromValue(QRectF(rect));
    }
    return regVal;
}

void ContainmentInterface::processMimeData(QMimeData *mimeData, int x, int y)
{
    if (!mimeData) {
        return;
    }

    //const QMimeData *mimeData = data;

    qDebug() << "Arrived mimeData" << mimeData->urls() << mimeData->formats() << "at" << x << ", " << y;

    if (mimeData->hasFormat("text/x-plasmoidservicename")) {
        QString data = mimeData->data("text/x-plasmoidservicename");
        const QStringList appletNames = data.split('\n', QString::SkipEmptyParts);
        foreach (const QString &appletName, appletNames) {
            qDebug() << "adding" << appletName;
            QRectF geom(QPoint(x, y), QSize(0, 0));
            //HACK
            //This is necessary to delay the appletAdded signal (of containmentInterface) AFTER the applet graphics object has been created
            blockSignals(true);
            Plasma::Applet *applet = containment()->createApplet(appletName);

            QObject *appletGraphicObject;
            if (applet) {
                appletGraphicObject = applet->property("graphicObject").value<QObject *>();
                if (appletGraphicObject) {
                    appletGraphicObject->setProperty("x", x);
                    appletGraphicObject->setProperty("y", y);
                }
            }

            blockSignals(false);
            emit appletAdded(appletGraphicObject);
            emit appletsChanged();
        }
    }
}

void ContainmentInterface::appletAddedForward(Plasma::Applet *applet)
{
    if (!applet) {
        return;
    }

    QObject *appletGraphicObject = applet->property("graphicObject").value<QObject *>();
    QObject *contGraphicObject = containment()->property("graphicObject").value<QObject *>();

    qDebug() << "Applet added on containment:" << containment()->title() << contGraphicObject
             << "Applet: " << applet << applet->title() << appletGraphicObject;

    if (contGraphicObject && appletGraphicObject) {
        appletGraphicObject->setProperty("visible", false);
        appletGraphicObject->setProperty("parent", QVariant::fromValue(contGraphicObject));

    //if an appletGraphicObject is not set, we have to display some error message
    } else if (contGraphicObject) {
        QObject *errorUi = qmlObject()->createObjectFromSource(QUrl::fromLocalFile(containment()->corona()->package().filePath("appleterror")));

        if (errorUi) {
            errorUi->setProperty("visible", false);
            errorUi->setProperty("parent", QVariant::fromValue(contGraphicObject));
            errorUi->setProperty("reason", applet->launchErrorMessage());
            appletGraphicObject = errorUi;
        }
    }

    m_appletInterfaces << appletGraphicObject;
    emit appletAdded(appletGraphicObject);
    emit appletsChanged();
}

void ContainmentInterface::appletRemovedForward(Plasma::Applet *applet)
{
    QObject *appletGraphicObject = applet->property("graphicObject").value<QObject *>();
    m_appletInterfaces.removeAll(appletGraphicObject);
    emit appletRemoved(appletGraphicObject);
    emit appletsChanged();
}

void ContainmentInterface::loadWallpaper()
{
    if (m_appletScriptEngine->drawWallpaper() && !containment()->wallpaper().isEmpty()) {
        delete m_wallpaperInterface;

        m_wallpaperInterface = new WallpaperInterface(this);
        m_wallpaperInterface->setZ(-1000);
        //Qml seems happier if the parent gets set in this way
        m_wallpaperInterface->setProperty("parent", QVariant::fromValue(this));

        //set anchors
        QQmlExpression expr(qmlObject()->engine()->rootContext(), m_wallpaperInterface, "parent");
        QQmlProperty prop(m_wallpaperInterface, "anchors.fill");
        prop.write(expr.evaluate());

        containment()->setProperty("wallpaperGraphicsObject", QVariant::fromValue(m_wallpaperInterface));
    } else {
        if (m_wallpaperInterface) {
            m_wallpaperInterface->deleteLater();
            m_wallpaperInterface = 0;
        }
    }
}

QString ContainmentInterface::activity() const
{
    return containment()->activity();
}





//PROTECTED--------------------

void ContainmentInterface::mousePressEvent(QMouseEvent *event)
{
    event->setAccepted(containment()->containmentActions().contains(Plasma::ContainmentActions::eventToString(event)));
}

void ContainmentInterface::mouseReleaseEvent(QMouseEvent *event)
{
    const QString trigger = Plasma::ContainmentActions::eventToString(event);
    Plasma::ContainmentActions *plugin = containment()->containmentActions().value(trigger);

    if (!plugin || plugin->contextualActions().isEmpty()) {
        event->setAccepted(false);
        return;
    }


    //the plugin can be a single action or a context menu
    //Don't have an action list? execute as single action
    //and set the event position as action data
    if (plugin->contextualActions().length() == 1) {
        QAction *action = plugin->contextualActions().first();
        action->setData(event->pos());
        action->trigger();
        event->accept();
        return;
    }

    //FIXME: very inefficient appletAt() implementation
    Plasma::Applet *applet = 0;
    foreach (QObject *appletObject, m_appletInterfaces) {
        if (AppletInterface *ai = qobject_cast<AppletInterface *>(appletObject)) {
            if (ai->contains(ai->mapFromItem(this, event->posF()))) {
                applet = ai->applet();
                break;
            } else {
                ai = 0;
            }
        }
    }
    qDebug() << "Invoking menu for applet" << applet;

    QMenu desktopMenu;

    if (applet) {
        addAppletActions(desktopMenu, applet, event);
    } else {
        addContainmentActions(desktopMenu, event);
    }
    desktopMenu.exec(event->globalPos());
    event->accept();
}

void ContainmentInterface::wheelEvent(QWheelEvent *event)
{
    const QString trigger = Plasma::ContainmentActions::eventToString(event);
    Plasma::ContainmentActions *plugin = containment()->containmentActions().value(trigger);

    if (plugin) {
        if (event->delta() < 0) {
            plugin->performNextAction();
        } else {
            plugin->performPreviousAction();
        }
    } else {
        event->setAccepted(false);
    }
}



void ContainmentInterface::addAppletActions(QMenu &desktopMenu, Plasma::Applet *applet, QEvent *event)
{
    foreach (QAction *action, applet->contextualActions()) {
        if (action) {
            desktopMenu.addAction(action);
        }
    }

    if (!applet->failedToLaunch()) {
        QAction *configureApplet = applet->actions()->action("configure");
        if (configureApplet && configureApplet->isEnabled()) {
            desktopMenu.addAction(configureApplet);
        }

        QAction *runAssociatedApplication = applet->actions()->action("run associated application");
        if (runAssociatedApplication && runAssociatedApplication->isEnabled()) {
            desktopMenu.addAction(runAssociatedApplication);
        }
    }

    QMenu *containmentMenu = new QMenu(i18nc("%1 is the name of the containment", "%1 Options", containment()->title()), &desktopMenu);
    addContainmentActions(*containmentMenu, event);

    if (!containmentMenu->isEmpty()) {
        int enabled = 0;
        //count number of real actions
        QListIterator<QAction *> actionsIt(containmentMenu->actions());
        while (enabled < 3 && actionsIt.hasNext()) {
            QAction *action = actionsIt.next();
            if (action->isVisible() && !action->isSeparator()) {
                ++enabled;
            }
        }

        if (enabled) {
            //if there is only one, don't create a submenu
            if (enabled < 2) {
                foreach (QAction *action, containmentMenu->actions()) {
                    if (action->isVisible() && !action->isSeparator()) {
                        desktopMenu.addAction(action);
                    }
                }
            } else {
                desktopMenu.addMenu(containmentMenu);
            }
        }
    }

    if (containment()->immutability() == Plasma::Types::Mutable) {
        QAction *closeApplet = applet->actions()->action("remove");
        //qDebug() << "checking for removal" << closeApplet;
        if (closeApplet) {
            if (!desktopMenu.isEmpty()) {
                desktopMenu.addSeparator();
            }

            //qDebug() << "adding close action" << closeApplet->isEnabled() << closeApplet->isVisible();
            desktopMenu.addAction(closeApplet);
        }
    }
}

void ContainmentInterface::addContainmentActions(QMenu &desktopMenu, QEvent *event)
{
    if (containment()->corona()->immutability() != Plasma::Types::Mutable &&
        !KAuthorized::authorizeKAction("plasma/containment_actions")) {
        //qDebug() << "immutability";
        return;
    }

    //this is what ContainmentPrivate::prepareContainmentActions was
    const QString trigger = Plasma::ContainmentActions::eventToString(event);
    Plasma::ContainmentActions *plugin = containment()->containmentActions().value(trigger);

    if (!plugin) {
        return;
    }

    if (plugin->containment() != containment()) {
        plugin->setContainment(containment());

        // now configure it
        KConfigGroup cfg(containment()->corona()->config(), "ActionPlugins");
        cfg = KConfigGroup(&cfg, QString::number(containment()->containmentType()));
        KConfigGroup pluginConfig = KConfigGroup(&cfg, trigger);
        plugin->restore(pluginConfig);
    }


    QList<QAction*> actions = plugin->contextualActions();

    if (actions.isEmpty()) {
        //it probably didn't bother implementing the function. give the user a chance to set
        //a better plugin.  note that if the user sets no-plugin this won't happen...
        if ((containment()->containmentType() != Plasma::Types::PanelContainment &&
             containment()->containmentType() != Plasma::Types::CustomPanelContainment) &&
            containment()->actions()->action("configure")) {
            desktopMenu.addAction(containment()->actions()->action("configure"));
        }
    } else {
        desktopMenu.addActions(actions);
    }


    return;
}

#include "moc_containmentinterface.cpp"
