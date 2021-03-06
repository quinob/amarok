/****************************************************************************************
 * Copyright (c) 2008 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/
 
#ifndef AMAROKURLRUNNERBASE_H
#define AMAROKURLRUNNERBASE_H

#include "AmarokUrl.h"

#include <QIcon>

#include <QString>

/**
Virtual base class for all classes that wants to be able to register to handle a particular type of amarok url

	@author Nikolaj Hald Nielsen <nhn@kde.org> 
*/
class AmarokUrlRunnerBase
{
public:
    virtual QString command() const = 0;
    virtual QString prettyCommand() const = 0;
    virtual bool run( AmarokUrl url ) = 0;
    virtual QIcon icon() const = 0;

protected:
    virtual ~AmarokUrlRunnerBase() {};
};

#endif
