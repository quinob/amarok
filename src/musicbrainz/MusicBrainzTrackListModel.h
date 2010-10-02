/****************************************************************************************
 * Copyright (c) 2010 Sergey Ivanov <123kash@gmail.com>                                 *
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

#ifndef MUSICBRAINZTRACKLISTMODEL_H
#define MUSICBRAINZTRACKLISTMODEL_H

#include "core/meta/Meta.h"
#include <QAbstractItemModel>

class MusicBrainzTrackListModel: public QAbstractItemModel
{
    Q_OBJECT
    public:
        MusicBrainzTrackListModel( Meta::TrackList tracks, QObject *parent = 0 );
        Meta::TrackPtr getTrack( QModelIndex &index );

        QModelIndex index( int row, int column = 0, const QModelIndex &parent = QModelIndex() ) const;
        QModelIndex parent( const QModelIndex &index ) const;

        QVariant headerData( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
        QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const;

        int rowCount( const QModelIndex &parent = QModelIndex() ) const;
        int columnCount( const QModelIndex & ) const;

    private:
        Meta::TrackList m_tracks;
};

#endif // MUSICBRAINZTRACKLISTMODEL_H
