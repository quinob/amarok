/* This file is part of the KDE project
   Copyright (C) 20078 Bart Cerneels <bart.cerneels@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include "debug.h"
#include "SqlPodcastMeta.h"
#include "SqlPodcastProvider.h"
#include "SqlStorage.h"

Meta::SqlPodcastEpisode::SqlPodcastEpisode( const QStringList &result )
    : Meta::PodcastEpisode()
    , m_batchUpdate( false )
{
    QStringList::ConstIterator iter = result.constBegin();
    m_id = (*(iter++)).toInt();
    m_url = KUrl( *(iter++) );
    m_sqlChannel = SqlPodcastChannelPtr::dynamicCast(
        SqlPodcastProvider::instance()->podcastChannelForId( (*(iter++)).toInt() ) );
    m_localUrl = KUrl( *(iter++) );
    m_guid = *(iter++);
    m_title = *(iter++);
    m_subtitle = *(iter++);
    m_sequenceNumber = (*(iter++)).toInt();
    m_description = *(iter++);
    m_mimeType = *(iter++);
    m_pubDate = *(iter++);
    m_duration = (*(iter++)).toInt();
    m_fileSize = (*(iter++)).toInt();
    m_isNew = SqlPodcastProvider::instance()->sqlStorage()->boolTrue() == (*(iter++));

    Q_ASSERT_X( iter == result.constEnd(), "SqlPodcastEpisode( PodcastCollection*, QStringList )", "number of expected fields did not match number of actual fields" );
}

Meta::SqlPodcastEpisode::SqlPodcastEpisode( Meta::PodcastEpisodePtr episode )
    : Meta::PodcastEpisode()
{
    m_url = KUrl( episode->url() );
    //m_sqlChannel = SqlPodcastChannelPtr::dynamicCast( episode->channel() );
    m_localUrl = episode->localUrl();
    m_title = episode->title();
    m_guid = episode->guid();

    //commit to the database
    updateInDb();
}

void
Meta::SqlPodcastEpisode::updateInDb()
{
    DEBUG_BLOCK
    QString boolTrue = SqlPodcastProvider::instance()->sqlStorage()->boolTrue();
    QString boolFalse = SqlPodcastProvider::instance()->sqlStorage()->boolFalse();
    #define escape(x) SqlPodcastProvider::instance()->sqlStorage()->escape(x)
    QString insert = "INSERT INTO podcastepisodes(url,channel,localurl,guid,title,subtitle,sequencenumber,description,mimetype,pubdate,duration,filesize,isnew) VALUES ( %1 );";
    QString data = "'%1','%2','%3','%4','%5','%6',%7,'%8','%9','%10',%11,%12,%13";
    data = data.arg( escape(m_url.url())).arg( /*m_sqlChannel->id()*/ 0 );
    data = data.arg( escape(m_localUrl.url()) ).arg( escape(m_guid) ).arg( escape(m_title) ).arg( escape(m_subtitle) );
    data = data.arg( QString::number(m_sequenceNumber) ).arg( escape(m_description) ).arg( escape(m_mimeType) );
    data = data.arg( escape(m_pubDate) ).arg( QString::number(m_duration) ).arg( QString::number(m_fileSize) );
    data = data.arg( m_isNew ? boolTrue : boolFalse );
    insert = insert.arg( data );

    m_id = SqlPodcastProvider::instance()->sqlStorage()->insert( insert, "podcastepisodes" );
}

Meta::SqlPodcastChannel::SqlPodcastChannel( const QStringList &result )
    : Meta::PodcastChannel()
{
    QStringList::ConstIterator iter = result.constBegin();
    m_id = (*(iter++)).toInt();
    m_url = KUrl( *(iter++) );
    m_title = *(iter++);
    m_webLink = *(iter++);
    QString imageUrl = *(iter++);
    m_description = *(iter++);
    m_copyright = *(iter++);
    m_directory = KUrl( *(iter++) );
    m_labels = QStringList( *(iter++) );
    m_autoScan = SqlPodcastProvider::instance()->sqlStorage()->boolTrue() == *(iter++);
    m_fetchType = (*(iter++)).toInt() == DownloadWhenAvailable ?
        DownloadWhenAvailable : StreamOrDownloadOnDemand;
    m_autoTransfer = SqlPodcastProvider::instance()->sqlStorage()->boolTrue() == *(iter++);
    m_hasPurge = SqlPodcastProvider::instance()->sqlStorage()->boolTrue() == *(iter++);
    m_purgeCount = (*(iter++)).toInt();
}

Meta::SqlPodcastChannel::SqlPodcastChannel( PodcastChannelPtr channel )
    : Meta::PodcastChannel()
{
    m_url = channel->url();
    m_title = channel->title();
    m_webLink = channel->webLink();
    m_description = channel->description();
    m_copyright = channel->copyright();
    m_labels = channel->labels();
}

void
Meta::SqlPodcastChannel::updateInDb()
{
    QString boolTrue = SqlPodcastProvider::instance()->sqlStorage()->boolTrue();
    QString boolFalse = SqlPodcastProvider::instance()->sqlStorage()->boolFalse();
    #define escape(x) SqlPodcastProvider::instance()->sqlStorage()->escape(x)
    QString insert = "INSERT INTO podcastchannels(url,title,weblink,image,description,copyright,labels,autoscan,fetchtype,autotransfer,haspurge,purgecount) VALUES ( %1 );";
    QString data = "'%1','%2','%3','%4','%5','%6','%7',%8,%9,%10,%11,%12";
    data = data.arg( escape(m_url.url()) ).arg( escape(m_title) ).arg( escape(m_webLink.url()) );
    //TODO:m_image.url()
    data = data.arg( escape(QString("")) ).arg( escape(m_description) ).arg( escape(m_copyright) );
    //TODO: QStringList -> comma seperated QString
    QString labels = QString("");
    data = data.arg( escape(labels) );
    data = data.arg( m_autoScan ? boolTrue : boolFalse );
    data = data.arg( QString::number(m_fetchType) );
    data = data.arg( m_autoTransfer ? boolTrue : boolFalse );
    data = data.arg( m_hasPurge ? boolTrue : boolFalse );
    data = data.arg( QString::number(m_purgeCount) );
    insert = insert.arg( data );

    m_id = SqlPodcastProvider::instance()->sqlStorage()->insert( insert, "podcastchannels" );
}

#include "SqlPodcastMeta.moc"
