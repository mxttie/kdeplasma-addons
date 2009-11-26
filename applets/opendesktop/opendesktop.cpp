/***************************************************************************
 *   Copyright 2009 by Sebastian K?gler <sebas@kde.org>                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

//own
#include "opendesktop.h"
#include "contactlist.h"
#include "utils.h"

//Qt

//KDE
#include <KConfigDialog>
#include <KLocale>
#include <KToolInvocation>
#include <KNotification>

//plasma
#include <Plasma/Label>
#include <Plasma/IconWidget>
#include <Plasma/TabBar>
#include <Plasma/PopupApplet>
#include <Plasma/DataEngine>
#include <Plasma/ScrollWidget>
#include <Plasma/ToolTipManager>

#include "actionstack.h"
#include "messagecounter.h"
#include "messagelist.h"
#include "ownidwatcher.h"
#include "friendlist.h"


K_EXPORT_PLASMA_APPLET(opendesktop, OpenDesktop)

using namespace Plasma;

struct GeoLocation {
    QString country;
    QString city;
    QString countryCode;
    int accuracy;
    qreal latitude;
    qreal longitude;
};


OpenDesktop::OpenDesktop(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
        m_tabs(0),
        m_friendList(0),
        m_nearList(0),
        m_provider("https://api.opendesktop.org/v1/")
{
    KGlobal::locale()->insertCatalog("plasma_applet_opendesktop");
    setBackgroundHints(StandardBackground);
    setAspectRatioMode(IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setPassivePopup(true);

    setPopupIcon("system-users");

    m_geolocation = new GeoLocation;


    (void)graphicsWidget();
}

OpenDesktop::~OpenDesktop()
{
    delete m_geolocation;
}

void OpenDesktop::init()
{
    kDebug() << "init: opendesktop";
    KConfigGroup cg = config();
    m_geolocation->city = cg.readEntry("geoCity", QString());
    m_geolocation->country = cg.readEntry("geoCountry", QString());
    m_geolocation->countryCode = cg.readEntry("geoCountryCode", QString());
    m_geolocation->latitude = cg.readEntry("geoLatitude", 0);
    m_geolocation->longitude = cg.readEntry("geoLongitude", 0);

    connectGeolocation();
}

void OpenDesktop::connectGeolocation()
{
    dataEngine("geolocation")->connectSource("location", this);
}


QGraphicsWidget* OpenDesktop::graphicsWidget()
{
    if (!m_tabs) {
        DataEngine* engine = dataEngine("ocs");

        // Watch for our own id
        m_ownIdWatcher = new OwnIdWatcher(engine, this);

        // Watch for our own id
        m_messageCounter = new MessageCounter(engine, this);

        // Friends
        m_friendList = new FriendList(engine);
        m_friendStack = new ActionStack(engine, m_friendList);

        // People near me
        m_nearList = new ContactList(engine);
        m_nearStack = new ActionStack(engine, m_nearList);

        // Messages
        m_messageList = new MessageList(engine);
        m_messageList->setFolder("0");

        m_tabs = new Plasma::TabBar;
        m_tabs->setPreferredSize(300, 400);
        m_tabs->setMinimumSize(150, 200);
        m_tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_tabs->addTab(i18n("Friends"), m_friendStack);
        m_tabs->addTab(i18n("Nearby"), m_nearStack);
        m_tabs->addTab(i18n("Messages"), m_messageList);

        connect(this, SIGNAL(providerChanged(QString)), m_friendList, SLOT(setProvider(QString)));
        connect(this, SIGNAL(providerChanged(QString)), m_friendStack, SLOT(setProvider(QString)));
        connect(this, SIGNAL(providerChanged(QString)), m_messageCounter, SLOT(setProvider(QString)));
        connect(this, SIGNAL(providerChanged(QString)), m_messageList, SLOT(setProvider(QString)));
        connect(this, SIGNAL(providerChanged(QString)), m_ownIdWatcher, SLOT(setProvider(QString)));
        connect(this, SIGNAL(providerChanged(QString)), m_nearList, SLOT(setProvider(QString)));
        connect(this, SIGNAL(providerChanged(QString)), m_nearStack, SLOT(setProvider(QString)));

        connect(m_friendList, SIGNAL(addFriend(QString)), m_friendStack, SLOT(addFriend(QString)));
        connect(m_friendList, SIGNAL(sendMessage(QString)), m_friendStack, SLOT(sendMessage(QString)));
        connect(m_friendList, SIGNAL(showDetails(QString)), m_friendStack, SLOT(showDetails(QString)));

        connect(m_friendStack, SIGNAL(endWork()), SLOT(endWork()));
        connect(m_friendStack, SIGNAL(startWork()), SLOT(startWork()));

        connect(m_nearList, SIGNAL(addFriend(QString)), m_nearStack, SLOT(addFriend(QString)));
        connect(m_nearList, SIGNAL(sendMessage(QString)), m_nearStack, SLOT(sendMessage(QString)));
        connect(m_nearList, SIGNAL(showDetails(QString)), m_nearStack, SLOT(showDetails(QString)));

        connect(m_nearStack, SIGNAL(endWork()), SLOT(endWork()));
        connect(m_nearStack, SIGNAL(startWork()), SLOT(startWork()));

        connect(m_ownIdWatcher, SIGNAL(changed(QString)), m_friendList, SLOT(setOwnId(QString)));
        connect(m_ownIdWatcher, SIGNAL(changed(QString)), m_friendStack, SLOT(setOwnId(QString)));
        connect(m_ownIdWatcher, SIGNAL(changed(QString)), m_nearList, SLOT(setOwnId(QString)));
        connect(m_ownIdWatcher, SIGNAL(changed(QString)), m_nearStack, SLOT(setOwnId(QString)));

        emit providerChanged(m_provider);
    }
    return m_tabs;
}


void OpenDesktop::connectNearby(int latitude, int longitude)
{
    QString src = QString("Near\\provider:%1\\latitude:%2\\longitude:%3")
        .arg(m_provider)
        .arg(latitude)
        .arg(longitude);

    m_nearList->setQuery(src);
}


void OpenDesktop::endWork()
{
    // FIXME: Count
    setBusy(false);
}


void OpenDesktop::startWork()
{
    // FIXME: Count
    setBusy(true);
}


void OpenDesktop::dataUpdated(const QString &source, const Plasma::DataEngine::Data &data)
{
    //kDebug() << "source updated:" << source << data;
    if (source == "location") {
        // The location from the geolocation engine arrived!
        m_geolocation->city = data["city"].toString();
        m_geolocation->country = data["country"].toString();
        m_geolocation->countryCode = data["country code"].toString();
        m_geolocation->accuracy = data["accuracy"].toInt();
        m_geolocation->latitude = data["latitude"].toDouble();
        m_geolocation->longitude = data["longitude"].toDouble();
        kDebug() << "geolocation:" << m_geolocation->city << m_geolocation->country <<
                m_geolocation->countryCode << m_geolocation->latitude << m_geolocation->longitude;
        connectNearby(m_geolocation->latitude, m_geolocation->longitude);
        saveGeoLocation();
        return;
    }

    kDebug() << "Don't know what to do with" << source;
}


void OpenDesktop::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget *widget = new QWidget(parent);
    ui.setupUi(widget);
    parent->addPage(widget, i18n("General"), Applet::icon());

    QWidget *locationWidget = new QWidget(parent);
    locationUi.setupUi(locationWidget);
    parent->addPage(locationWidget, i18n("Location"), "go-home");
    // TODO: connect finished() signal to null the ui

    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));
    connect(ui.registerButton, SIGNAL(clicked()), this, SLOT(registerAccount()));
    connect(locationUi.publishLocation, SIGNAL(clicked()), this, SLOT(publishGeoLocation()));

    locationUi.city->setText(m_geolocation->city);
    locationUi.latitude->setText(QString::number(m_geolocation->latitude));
    locationUi.longitude->setText(QString::number(m_geolocation->longitude));

    locationUi.countryCombo->setInsertPolicy(QComboBox::InsertAlphabetically);
    foreach ( const QString &cc, KGlobal::locale()->allCountriesList() ) {
        locationUi.countryCombo->addItem(KGlobal::locale()->countryCodeToName(cc), cc);
    }
    locationUi.countryCombo->setCurrentIndex(locationUi.countryCombo->findText(KGlobal::locale()->countryCodeToName(m_geolocation->countryCode)));

    // actually, 0,0 is a valid location, but here we're using it to see if we
    // actually have a location, a bit dirty but far less complex, especially given
    // that this point is located in the middle of the ocean off the coast of Ghana
    if (m_geolocation->latitude == 0 && m_geolocation->longitude == 0) {
        locationUi.publishLocation->setEnabled(false);
    }
}

void OpenDesktop::configAccepted()
{
    syncGeoLocation();
}

void OpenDesktop::registerAccount()
{
    kDebug() << "register new account";
    KToolInvocation::invokeBrowser("https://www.opendesktop.org/usermanager/new.php");
}

void OpenDesktop::syncGeoLocation()
{
    // Location tab
    m_geolocation->city = locationUi.city->text();
    m_geolocation->countryCode = locationUi.countryCombo->itemData(locationUi.countryCombo->currentIndex()).toString();
    m_geolocation->country = locationUi.countryCombo->currentText();
    m_geolocation->latitude = locationUi.latitude->text().toDouble();
    m_geolocation->longitude = locationUi.longitude->text().toDouble();

    kDebug() << "New location:" << m_geolocation->city << m_geolocation->country << m_geolocation->countryCode << m_geolocation->latitude << m_geolocation->longitude;

    saveGeoLocation();
}

void OpenDesktop::publishGeoLocation()
{
    syncGeoLocation();
    // FIXME: Use service
    QString source = QString("PostLocation-%1:%2:%3:%4").arg(
                                    QString("%1").arg(m_geolocation->latitude),
                                    QString("%1").arg(m_geolocation->longitude),
                                    m_geolocation->countryCode,
                                    m_geolocation->city);
    kDebug() << "updating location:" << source;
    dataEngine("ocs")->connectSource(source, this);
}

void OpenDesktop::saveGeoLocation()
{
    KConfigGroup cg = config();
    cg.writeEntry("geoCity", m_geolocation->city);
    cg.writeEntry("geoCountry", m_geolocation->country);
    cg.writeEntry("geoCountryCode", m_geolocation->countryCode);
    cg.writeEntry("geoLatitude", m_geolocation->latitude);
    cg.writeEntry("geoLongitude", m_geolocation->longitude);

    emit configNeedsSaving();
}


void OpenDesktop::unreadMessageCountChanged(int count)
{
    if (count) {
        m_tabs->setTabText(2, i18n("Messages (%1)", count));
    } else {
        m_tabs->setTabText(2, i18n("Messages"));
    }
}


#include "opendesktop.moc"
