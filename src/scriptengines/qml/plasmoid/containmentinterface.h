/*
 *   Copyright 2008-2013 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2010-2013 Marco Martin <mart@kde.org>
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

#ifndef CONTAINMENTINTERFACE_H
#define CONTAINMENTINTERFACE_H

#include <QMenu>

#include <Plasma/Containment>

#include "appletinterface.h"

class QmlObject;
class WallpaperInterface;

namespace KIO {
    class Job;
}

class ContainmentInterface : public AppletInterface
{
    Q_OBJECT

    /**
     * List of applets this containment has: the containments
     */
    Q_PROPERTY(QList <QObject *> applets READ applets NOTIFY appletsChanged)
    
    /**
     * True if the containment should draw a wallpaper TODO: notify
     */
    Q_PROPERTY(bool drawWallpaper READ drawWallpaper WRITE setDrawWallpaper)

    /**
     * Type of this containment TODO: notify
     */
    Q_PROPERTY(Plasma::Types::ContainmentType containmentType READ containmentType WRITE setContainmentType)

    /**
     * Activity name of this containment
     */
    Q_PROPERTY(QString activity READ activity NOTIFY activityChanged)

public:
    ContainmentInterface(DeclarativeAppletScript *parent);
//Not for QML
    inline Plasma::Containment *containment() const { return static_cast<Plasma::Containment *>(m_appletScriptEngine->applet()->containment()); }

    inline WallpaperInterface *wallpaperInterface() const { return m_wallpaperInterface;}

//For QML use
    QList<QObject *> applets();

    void setDrawWallpaper(bool drawWallpaper);
    bool drawWallpaper();
    Plasma::Types::ContainmentType containmentType() const;
    void setContainmentType(Plasma::Types::ContainmentType type);

    QString activity() const;

    /**
     * FIXME: either a property or not accessible at all. Lock or unlock widgets
     */
    Q_INVOKABLE void lockWidgets(bool locked);

    /**
     * Geometry of this screen
     */
    Q_INVOKABLE QRectF screenGeometry(int id) const;

    /**
     * The available region of this screen, panels excluded. It's a list of rectangles
     */
    Q_INVOKABLE QVariantList availableScreenRegion(int id) const;

    /**
     * Process the mime data arrived to a particular coordinate, either with a drag and drop or paste with middle mouse button
     */
    Q_INVOKABLE void processMimeData(QMimeData *data, int x, int y);

protected:
    void init();
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    void addAppletActions(QMenu &desktopMenu, Plasma::Applet *applet, QEvent *event);
    void addContainmentActions(QMenu &desktopMenu, QEvent *event);

Q_SIGNALS:
    /**
     * Emitted when an applet is added
     * @param applet the applet object: it's a qml graphical object and an instance of AppletInterface
     * @param x coordinate containment relative
     * @param y coordinate containment relative
     */
    void appletAdded(QObject *applet, int x, int y);

    /**
     * Emitted when an applet is removed
     * @param applet the applet object: it's a qml graphical object and an instance of AppletInterface.
     *               It's still valid, even if it will be deleted shortly
     */
    void appletRemoved(QObject *applet);

    //Property notifiers
    void activityChanged();
    void availableScreenRegionChanged();
    void appletsChanged();
    ///void immutableChanged();

protected Q_SLOTS:
    void appletAddedForward(Plasma::Applet *applet);
    void appletRemovedForward(Plasma::Applet *applet);
    void loadWallpaper();
    void dropJobResult(KJob *job);
    void mimeTypeRetrieved(KIO::Job *job, const QString &mimetype);

private:
    void clearDataForMimeJob(KIO::Job *job);
    void addApplet(const QString &plugin, const QVariantList &args, const QPoint &pos);

    WallpaperInterface *m_wallpaperInterface;
    QList<QObject *> m_appletInterfaces;
    QHash<KJob*, QPoint> m_dropPoints;
    QHash<KJob*, QMenu*> m_dropMenus;
};

#endif
