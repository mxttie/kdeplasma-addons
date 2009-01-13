/*
 * Copyright (C) 2008 Petri Damstén <damu@iki.fi>
 * Copyright (C) 2008 Kristof Bal <kristof.bal@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mass.h"
#include <KLocale>

Mass::Mass(QObject* parent)
: SimpleUnit(parent)
{
    setObjectName("mass");

    m_default = "g";

    addSIUnit(m_default, i18nc("mass unit", "gram"), i18n("grams"));

    m_units[i18nc("mass unit", "ton")]          = "t";
    m_units[i18nc("mass unit", "tons")]         = "t";
    m_units[i18nc("mass unit", "tonne")]        = "t";
    m_units["t"]                  		= 1000000.0;

	//I guess it's useful...
	m_units[i18nc("mass unit", "carat")]    = "CD";
	m_units[i18nc("mass unit", "carats")]   = "CD";
	m_units["CD"]                 		= 0.2;

    //http://en.wikipedia.org/wiki/Pound_(mass)#International_pound
    m_units[i18nc("mass unit", "pound")]        = "lb";
    m_units[i18nc("mass unit", "pounds")]       = "lb";
    m_units[i18nc("mass unit: pounds", "lb")]   = 453.592;
    //International avoirdupois ounce
    m_units[i18nc("mass unit", "ounce")]        = "oz";
    m_units[i18nc("mass unit", "ounces")]       = "oz";
    m_units["oz"]                 		= 28.349523125;
    m_units[i18nc("mass unit", "troy ounce")]   = "t oz";
    m_units[i18nc("mass unit", "troy ounces")]  = "t oz";
    m_units["t oz"]               		= 31.1034768;
    m_units[i18nc("mass unit", "newton")]       = "N";       //on earth
    m_units["N"]                  		= 1000.0/9.81;
	//used a lot in industry (aircraft engines for example)
    m_units[i18nc("mass unit", "kilonewton")]   = "kN";
    m_units["kN"]                 		= 1000000.0/9.81;
}

QString Mass::name() const
{
    return i18n("Mass");
}
