/*
 *   Copyright (C) 2007 Ivan Cukic <ivan.cukic+kde@gmail.com>
 *   Copyright (C) 2007 Robert Knight <robertknight@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser/Library General Public License version 2,
 *   or (at your option) any later version, as published by the Free
 *   Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser/Library General Public License for more details
 *
 *   You should have received a copy of the GNU Lesser/Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "Devices.h"
#include <KRun>
#include <KDebug>
#include <KLocalizedString>

#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/devicenotifier.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>

namespace Lancelot {
namespace Models {

#define StringCoalesce(A, B) (A == "")?(B):(A)

Devices::Devices(Type filter)
    : m_filter(filter)
{
    load();

    kDebug() << "connecting\n";
    kDebug() << connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)),
            this, SLOT(deviceAdded(QString)));
    kDebug() << connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)),
            this, SLOT(deviceRemoved(QString)));
    kDebug() << "connected\n";
}

Devices::~Devices()
{
}

void Devices::deviceRemoved(const QString & udi)
{
    // TODO: implement
    kDebug() << "deviceRemoved " << udi << "\n";
    QMutableListIterator<Item> i(m_items);
    int index = 0;

    while (i.hasNext()) {
        Item & item = i.next();
        if (item.data.toString() == udi) {
            i.remove();
            // TODO: m_udis.removeAll(udi);
            emit itemDeleted(index);
            return;
        }
        ++index;
    }

}

void Devices::deviceAdded(const QString & udi)
{
    kDebug() << "deviceAdded " << udi << "\n";
    addDevice(Solid::Device(udi));
}

void Devices::addDevice(const Solid::Device & device)
{
    const Solid::StorageAccess * access = device.as<Solid::StorageAccess>();

    if (!access) return;

    // Testing if it is removable
    if (m_filter != All) {
        bool removable;

        Solid::StorageDrive *drive = 0;
        Solid::Device parentDevice = device;

        while (parentDevice.isValid() && !drive) {
            drive = parentDevice.as<Solid::StorageDrive>();
            parentDevice = parentDevice.parent();
        }

        removable = (drive && (drive->isHotpluggable() || drive->isRemovable()));
        if ((m_filter == Removable) == !removable) return; // Dirty trick simulating XOR
    }

    connect (
        access, SIGNAL(accessibilityChanged(bool)),
        this, SLOT(udiAccessibilityChanged(bool))
    );
    m_udis[access] = device.udi();

    add(
        device.product(),
        StringCoalesce(access->filePath(), i18n("Unmounted")),
        new KIcon(device.icon()),
        device.udi()
    );
}

void Devices::udiAccessibilityChanged(bool accessible)
{
    Solid::StorageAccess * access = (Solid::StorageAccess *) sender();
    // TODO: implement
    if (!m_udis.contains(access)) {
        return;
    }
    QString udi = m_udis[access];
    kDebug() << udi << "\n";
    QMutableListIterator<Item> i(m_items);
    int index = 0;

    while (i.hasNext()) {
        Item & item = i.next();
        if (item.data.toString() == udi) {
            break;
        }
        ++index;
    }
    kDebug() << index << "\n";

    m_items[index].description = StringCoalesce(access->filePath(), i18n("Unmounted"));
    emit itemAltered(index);

}


void Devices::load()
{
    QList<Solid::Device> deviceList =
        Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess, QString());

    foreach(const Solid::Device & device, deviceList) {
        addDevice(device);
    }
}

void Devices::freeSpaceInfoAvailable(const QString & mountPoint, quint64 kbSize, quint64 kbUsed, quint64 kbAvailable)
{

}

void Devices::activate(int index)
{
    if (index > m_items.size() - 1) return;
    Solid::StorageAccess * access = Solid::Device(m_items.at(index).data.toString()).as<Solid::StorageAccess>();

    if (!access) return;

    if (!access->isAccessible()) {
        access->setup();
    }

    new KRun(KUrl(access->filePath()), 0);
    LancelotApplication::application()->hide(true);
}

}
}
