/*
 *   Copyright 2005 by Aaron Seigo <aseigo@kde.org>
 *   Copyright 2007 by Riccardo Iaconelli <riccardo@kde.org>
 *   Copyright 2008 by Ménard Alexis <darktears31@gmail.com>
 *   Copyright (c) 2009 Chani Armitage <chani@kde.org>
 *   Copyright 2012 by Marco Martin <mart@kde.org>
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

#include "private/applet_p.h"

#include <config-plasma.h>

#include <QFile>
#include <QVBoxLayout>
#include <qstandardpaths.h>

#include <kaction.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <klocale.h>
#include <klocalizedstring.h>
#include <kkeysequencewidget.h>
#include <kstandarddirs.h>
#include <kglobal.h>

#include "containment.h"
#include "corona.h"
#include "pluginloader.h"
#include "scripting/scriptengine.h"
#include "scripting/appletscript.h"
#include "private/containment_p.h"

namespace Plasma
{

AppletPrivate::AppletPrivate(KService::Ptr service, const KPluginInfo *info, int uniqueID, Applet *applet)
        : appletId(uniqueID),
          q(applet),
          immutability(Mutable),
          appletDescription(info ? *info : KPluginInfo(service)),
          mainConfig(0),
          pendingConstraints(NoConstraint),
          script(0),
          package(0),
          configLoader(0),
          actions(AppletPrivate::defaultActions(applet)),
          activationAction(0),
          itemStatus(UnknownStatus),
          modificationsTimer(0),
          hasConfigurationInterface(false),
          isContainment(false),
          failed(false),
          transient(false),
          needsConfig(false),
          started(false)
{
    if (appletId == 0) {
        appletId = ++s_maxAppletId;
    } else if (appletId > s_maxAppletId) {
        s_maxAppletId = appletId;
    }
}

AppletPrivate::~AppletPrivate()
{
    if (activationAction && activationAction->isGlobalShortcutEnabled()) {
        //kDebug() << "reseting global action for" << q->title() << activationAction->objectName();
        activationAction->forgetGlobalShortcut();
    }

    delete script;
    script = 0;
    delete package;
    package = 0;
    delete configLoader;
    configLoader = 0;
    delete mainConfig;
    mainConfig = 0;
    delete modificationsTimer;
}

void AppletPrivate::init(const QString &packagePath)
{
    // WARNING: do not access config() OR globalConfig() in this method!
    //          that requires a scene, which is not available at this point

    q->setHasConfigurationInterface(true); //FIXME why not default it to true in the constructor?

    QAction *closeApplet = actions->action("remove");
    if (closeApplet) {
        closeApplet->setText(i18nc("%1 is the name of the applet", "Remove this %1", q->title()));
    }

    QAction *configAction = actions->action("configure");
    if (configAction) {
        configAction->setText(i18nc("%1 is the name of the applet", "%1 Settings", q->title()));
    }

    QObject::connect(q, SIGNAL(activate()), q, SLOT(setFocus()));
    if (!appletDescription.isValid()) {
#ifndef NDEBUG
        kDebug() << "Check your constructor! "
                 << "You probably want to be passing in a Service::Ptr "
                 << "or a QVariantList with a valid storageid as arg[0].";
#endif
        return;
    }

    QString api = appletDescription.property("X-Plasma-API").toString();

    // we have a scripted plasmoid
    if (api.isEmpty()) {
        q->setLaunchErrorMessage(i18n("The %2 widget did not define which ScriptEngine to use.", appletDescription.name()));
        return;
    }


    const QString path = packagePath.isEmpty() ? appletDescription.pluginName() : packagePath;
    package = new Package(PluginLoader::self()->loadPackage("Plasma/Applet", api));
    package->setPath(path);

    if (!package->isValid()) {
        delete package;
        package = 0;
        q->setLaunchErrorMessage(i18nc("Package file, name of the widget",
                                         "Could not open the %1 package required for the %2 widget.",
                                         appletDescription.pluginName(), appletDescription.name()));
        return;
    }

    // create the package and see if we have something real
    //kDebug() << "trying for" << path;

        // now we try and set up the script engine.
        // it will be parented to this applet and so will get
        // deleted when the applet does

    script = Plasma::loadScriptEngine(api, q);

    if (!script) {
        delete package;
        package = 0;
        q->setLaunchErrorMessage(
                             i18nc("API or programming language the widget was written in, name of the widget",
                                   "Could not create a %1 ScriptEngine for the %2 widget.",
                                   api, appletDescription.name()));
    }
}

void AppletPrivate::cleanUpAndDelete()
{
    // reimplemented in the UI specific library
    if (configLoader) {
        configLoader->setDefaults();
    }

    resetConfigurationObject();

    if (isContainment) {
        // prematurely emit our destruction if we are a Containment,
        // giving Corona a chance to remove this Containment from its collection
        emit q->QObject::destroyed(q);
    }

    q->deleteLater();
}

void AppletPrivate::showConfigurationRequiredMessage(bool show, const QString &reason)
{
    // reimplemented in the UI specific library
    Q_UNUSED(show)
    Q_UNUSED(reason)
}

void AppletPrivate::globalShortcutChanged()
{
    if (!activationAction) {
        return;
    }

    KConfigGroup shortcutConfig(mainConfigGroup(), "Shortcuts");
    shortcutConfig.writeEntry("global", activationAction->globalShortcut().toString());
    scheduleModificationNotification();
    //kDebug() << "after" << shortcut.primary() << d->activationAction->globalShortcut().primary();
}

KActionCollection* AppletPrivate::defaultActions(QObject *parent)
{
    KActionCollection *actions = new KActionCollection(parent);
    actions->setConfigGroup("Shortcuts-Applet");

    KAction *configAction = actions->addAction("configure");
    configAction->setAutoRepeat(false);
    configAction->setText(i18n("Widget Settings"));
    configAction->setIcon(KDE::icon("configure"));
    configAction->setShortcut(KShortcut("alt+d, s"));
    configAction->setData(Plasma::ConfigureAction);

    KAction *closeApplet = actions->addAction("remove");
    closeApplet->setAutoRepeat(false);
    closeApplet->setText(i18n("Remove this Widget"));
    closeApplet->setIcon(KDE::icon("edit-delete"));
    closeApplet->setShortcut(KShortcut("alt+d, r"));
    closeApplet->setData(Plasma::DestructiveAction);

    KAction *runAssociatedApplication = actions->addAction("run associated application");
    runAssociatedApplication->setAutoRepeat(false);
    runAssociatedApplication->setText(i18n("Run the Associated Application"));
    runAssociatedApplication->setIcon(KDE::icon("system-run"));
    runAssociatedApplication->setShortcut(KShortcut("alt+d, t"));
    runAssociatedApplication->setVisible(false);
    runAssociatedApplication->setEnabled(false);
    runAssociatedApplication->setData(Plasma::ControlAction);

    return actions;
}

QString AppletPrivate::configDialogId() const
{
    return QString("%1settings%2").arg(appletId).arg(q->title());
}

QString AppletPrivate::configWindowTitle() const
{
    return i18nc("@title:window", "%1 Settings", q->title());
}

KConfigDialog *AppletPrivate::generateGenericConfigDialog()
{
    KConfigSkeleton *nullManager = new KConfigSkeleton(0);
    KConfigDialog *dialog = new AppletConfigDialog(0, configDialogId(), nullManager);
    nullManager->setParent(dialog);
    dialog->setFaceType(KPageDialog::Auto);
    dialog->setWindowTitle(configWindowTitle());
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    q->createConfigurationInterface(dialog);
    dialog->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    QObject::connect(dialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), q, SLOT(configDialogFinished()));
    QObject::connect(dialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), q, SLOT(configDialogFinished()));
    return dialog;
}

void AppletPrivate::addStandardConfigurationPages(KConfigDialog *dialog)
{
    addGlobalShortcutsPage(dialog);
}

void AppletPrivate::addGlobalShortcutsPage(KConfigDialog *dialog)
{
#if !PLASMA_NO_GLOBAL_SHORTCUTS
    if (isContainment) {
        return;
    }

    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);

    if (!shortcutEditor) {
        shortcutEditor = new KKeySequenceWidget(page);
        QObject::connect(shortcutEditor.data(), SIGNAL(keySequenceChanged(QKeySequence)), dialog, SLOT(settingsModified()));
    }

    shortcutEditor.data()->setKeySequence(q->globalShortcut().primary());
    layout->addWidget(shortcutEditor.data());
    layout->addStretch();
    dialog->addPage(page, i18n("Keyboard Shortcut"), "preferences-desktop-keyboard");

    QObject::connect(dialog, SIGNAL(applyClicked()), q, SLOT(configDialogFinished()), Qt::UniqueConnection);
    QObject::connect(dialog, SIGNAL(okClicked()), q, SLOT(configDialogFinished()), Qt::UniqueConnection);
#endif
}

void AppletPrivate::configDialogFinished()
{
    if (shortcutEditor) {
        QKeySequence sequence = shortcutEditor.data()->keySequence();
        if (sequence != q->globalShortcut().primary()) {
            q->setGlobalShortcut(KShortcut(sequence));
            emit q->configNeedsSaving();
        }
    }

    if (!configLoader) {
        // the config loader will trigger this for us, so we don't need to.
        propagateConfigChanged();
        if (KConfigDialog *dialog = qobject_cast<KConfigDialog *>(q->sender())) {
            dialog->button(QDialogButtonBox::Apply)->setEnabled(false);
        }
    }
}

void AppletPrivate::updateShortcuts()
{
    if (isContainment) {
        //a horrible hack to avoid clobbering corona settings
        //we pull them out, then read, then put them back
        QList<QString> names;
        QList<QAction*> qactions;
        names << "add sibling containment" << "configure shortcuts" << "lock widgets";
        foreach (const QString &name, names) {
            QAction *a = actions->action(name);
            actions->takeAction(a); //FIXME this is stupid, KActionCollection needs a takeAction(QString) method
            qactions << a;
        }

        actions->readSettings();

        for (int i = 0; i < names.size(); ++i) {
            QAction *a = qactions.at(i);
            if (a) {
                actions->addAction(names.at(i), a);
            }
        }
    } else {
        actions->readSettings();
    }
}

void AppletPrivate::propagateConfigChanged()
{
    if (isContainment) {
        Containment *c = qobject_cast<Containment *>(q);
        if (c) {
            c->d->configChanged();
        }
    }

    q->configChanged();
}

void AppletPrivate::setIsContainment(bool nowIsContainment, bool forceUpdate)
{
    if (isContainment == nowIsContainment && !forceUpdate) {
        return;
    }

    isContainment = nowIsContainment;
    //FIXME I do not like this function.
    //currently it's only called before ctmt/applet init, with (true,true), and I'm going to assume it stays that way.
    //if someone calls it at some other time it'll cause headaches. :P

    delete mainConfig;
    mainConfig = 0;
}

// put all setup routines for script here. at this point we can assume that
// package exists and that we have a script engine
void AppletPrivate::setupScriptSupport()
{
    if (!package) {
        return;
    }

#ifndef NDEBUG
    kDebug() << "setting up script support, package is in" << package->path()
             << ", main script is" << package->filePath("mainscript");
#endif

    const QString translationsPath = package->filePath("translations");
    if (!translationsPath.isEmpty()) {
        KGlobal::dirs()->addResourceDir("locale", translationsPath);
        KLocalizedString::insertCatalog(appletDescription.pluginName());
    }

    const QString xmlPath = package->filePath("mainconfigxml");
    if (!xmlPath.isEmpty()) {
        QFile file(xmlPath);
        KConfigGroup config = q->config();
        configLoader = new ConfigLoader(&config, &file);
        QObject::connect(configLoader, SIGNAL(configChanged()), q, SLOT(propagateConfigChanged()));
    }

    if (!package->filePath("mainconfigui").isEmpty()) {
        q->setHasConfigurationInterface(true);
    }
}

QString AppletPrivate::globalName() const
{
    if (!appletDescription.isValid()) {
        return QString();
    }

    return appletDescription.service()->library();
}

void AppletPrivate::scheduleConstraintsUpdate(Plasma::Constraints c)
{
    // Don't start up a timer if we're just starting up
    // flushPendingConstraints will be called by Corona
    if (started && !constraintsTimer.isActive() && !(c & Plasma::StartupCompletedConstraint)) {
        constraintsTimer.start(0, q);
    }

    if (c & Plasma::StartupCompletedConstraint) {
        started = true;
    }

    pendingConstraints |= c;
}

void AppletPrivate::scheduleModificationNotification()
{
    // modificationsTimer is not allocated until we get our notice of being started
    if (modificationsTimer) {
        // schedule a save
        if (modificationsTimer->isActive()) {
            modificationsTimer->stop();
        }

        modificationsTimer->start(1000, q);
    }
}

KConfigGroup *AppletPrivate::mainConfigGroup()
{
    if (mainConfig) {
        return mainConfig;
    }

    if (isContainment) {
        Corona *corona = static_cast<Containment*>(q)->corona();
        KConfigGroup containmentConfig;
        //kDebug() << "got a corona, baby?" << (QObject*)corona << (QObject*)q;

        if (corona) {
            containmentConfig = KConfigGroup(corona->config(), "Containments");
        } else {
            containmentConfig =  KConfigGroup(KSharedConfig::openConfig(), "Containments");
        }

        mainConfig = new KConfigGroup(&containmentConfig, QString::number(appletId));
    } else {
        KConfigGroup appletConfig;

        Containment *c = q->containment();
        Applet *parentApplet = qobject_cast<Applet *>(q->parent());
        if (parentApplet && parentApplet != static_cast<Applet *>(c)) {
            // this applet is nested inside another applet! use it's config
            // as the parent group in the config
            appletConfig = parentApplet->config();
            appletConfig = KConfigGroup(&appletConfig, "Applets");
        } else if (c) {
            // applet directly in a Containment, as usual
            appletConfig = c->config();
            appletConfig = KConfigGroup(&appletConfig, "Applets");
        } else {
            kWarning() << "requesting config for" << q->title() << "without a containment!";
            appletConfig = KConfigGroup(KSharedConfig::openConfig(), "Applets");
        }

        mainConfig = new KConfigGroup(&appletConfig, QString::number(appletId));
    }

    return mainConfig;
}

void AppletPrivate::resetConfigurationObject()
{
    // make sure mainConfigGroup exists in all cases
    mainConfigGroup();

    mainConfig->deleteGroup();
    delete mainConfig;
    mainConfig = 0;

    if (!q->containment()) {
        return;
    }
    Corona * corona = q->containment()->corona();
    if (corona) {
        corona->requireConfigSync();
    }
}

uint AppletPrivate::s_maxAppletId = 0;

} //namespace Plasma
