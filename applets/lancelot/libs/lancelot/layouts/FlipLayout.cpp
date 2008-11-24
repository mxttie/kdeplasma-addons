/*
 *   Copyright (C) 2007 Ivan Cukic <ivan.cukic+kde@gmail.com>
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

#include "FlipLayout.h"
#include <QSet>
#include <QMap>
#include <KDebug>
#include <QGraphicsWidget>

namespace Lancelot
{

FlipLayoutManager * FlipLayoutManager::m_instance = NULL;

class FlipLayoutManager::Private {
public:
    Plasma::Flip globalFlip;
    QSet < QGraphicsLayout * > globalFlipLayouts;
    QMap < QGraphicsLayout *, Plasma::Flip > flips;
};

FlipLayoutManager::FlipLayoutManager()
    : d(new Private())
{

}

FlipLayoutManager::~FlipLayoutManager()
{
    delete d;
}

FlipLayoutManager * FlipLayoutManager::instance()
{
    if (m_instance == NULL) {
        m_instance = new FlipLayoutManager();
    }
    return m_instance;
}

void FlipLayoutManager::setGlobalFlip(Plasma::Flip flip)
{
    d->globalFlip = flip;
}

Plasma::Flip FlipLayoutManager::globalFlip() const
{
    return d->globalFlip;
}

void FlipLayoutManager::setFlip(const QGraphicsLayout * layout, Plasma::Flip flip)
{
    QGraphicsLayout * l = const_cast<QGraphicsLayout *>(layout);
    d->globalFlipLayouts.remove(l);
    d->flips[l] = flip;
}

void FlipLayoutManager::setUseGlobalFlip(const QGraphicsLayout * layout)
{
    QGraphicsLayout * l = const_cast<QGraphicsLayout *>(layout);
    d->flips.remove(l);
    d->globalFlipLayouts.insert(l);
}

Plasma::Flip FlipLayoutManager::flip(const QGraphicsLayout * layout) const
{
    QGraphicsLayout * l = const_cast<QGraphicsLayout *>(layout);
    if (d->globalFlipLayouts.contains(l)) {
        return d->globalFlip;
    } else if (d->flips.contains(l)) {
        return d->flips[l];
    }
    return 0;
}

void FlipLayoutManager::setGeometry(QGraphicsLayout * layout) const
{
    int count = layout->count();
    QRectF rect = layout->geometry();

    if (flip(layout) == Plasma::NoFlip) {
        return;
    }

    QRectF childGeometry;
    for (int i = 0; i < count; i++) {
        QGraphicsLayoutItem * item = layout->itemAt(i);

        if (!item) continue;

        childGeometry = item->geometry();
        if (flip(layout) & Plasma::HorizontalFlip) {
            childGeometry.moveLeft(rect.left() + rect.right() - childGeometry.right());
        }
        if (flip(layout) & Plasma::VerticalFlip) {
            childGeometry.moveTop(rect.top() + rect.bottom() - childGeometry.bottom());
        }
        item->setGeometry(childGeometry);
    }
}

}

