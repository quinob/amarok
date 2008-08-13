/***************************************************************************
 *   Copyright (c) 2007, 2008  Casey Link <unnamedrambler@gmail.com>       *
 *                                                                         *
 *   This program is free software{} you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation{} either version 2 of the License, or    *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY{} without even the implied warranty of       *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program{} if not, write to the                        *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "Mp3tunesLocker.h"

#include "Debug.h"

#include <QByteArray>

Mp3tunesLocker::Mp3tunesLocker ( const QString & partnerToken )
{
    DEBUG_BLOCK
    m_locker = 0;
    debug() << "Creating New Locker";
    char *c_tok = convertToChar ( partnerToken );
    debug() << "Wrapper Token: " << c_tok;
    mp3tunes_locker_init ( &m_locker, c_tok );
    debug() << "New Locker created";
}

Mp3tunesLocker::Mp3tunesLocker ( const QString & partnerToken, const QString & userName,
const QString & password )
{
    char *c_tok = convertToChar ( partnerToken );
    mp3tunes_locker_init ( &m_locker, c_tok );

    this->login ( userName, password );
}

Mp3tunesLocker::~Mp3tunesLocker()
{
    mp3tunes_locker_deinit ( &m_locker );
}

QString
Mp3tunesLocker::login ( const QString &userName, const QString &password )
{
    DEBUG_BLOCK
    char *c_user = convertToChar ( userName );
    char *c_pass = convertToChar ( password );
    //result = 0 Login successful
    //result != 0 Login failed
    debug() << "Wrapper Logging on with: " << userName << ":" << password;
    int result = mp3tunes_locker_login ( m_locker, c_user, c_pass );

    if ( result == 0 )
    {
        //login successful
        debug() << "Wrapper Login succeeded. result: " << result;
        return this->sessionId();
    }
    debug() << "Wrapper Login failed. result: " << result;
    return QString(); //login failed
}

QString
Mp3tunesLocker::login ()
{
    return login( userName(), password() );
}
bool
Mp3tunesLocker::sessionValid() const
{
    //result = 0 session valid
    //result != 0 session invalid
    int result = mp3tunes_locker_session_valid ( m_locker );
    if ( result == 0 )
        return true;
    return false;
}

QList<Mp3tunesLockerArtist>
Mp3tunesLocker::artists() const
{
    DEBUG_BLOCK
    QList<Mp3tunesLockerArtist> artistsQList; // to be returned
    mp3tunes_locker_artist_list_t *artists_list;
    mp3tunes_locker_list_item_t *artist_item;

    //get the list of artists
    mp3tunes_locker_artists ( m_locker, &artists_list );

    artist_item = artists_list->first; // the current node

    //looping through the list of artists
    while ( artist_item != 0 )
    {
        // get the artist from the c lib
        mp3tunes_locker_artist_t *artist = ( mp3tunes_locker_artist_t* ) artist_item->value;
        //debug() << "Wrapper Artist: " << artist->artistName;

        //wrap it up
        Mp3tunesLockerArtist artistWrapped (  artist );
        //and stick it in the QList
        artistsQList.append ( artistWrapped );
        //advance to next artist
        //debug() << "Going to next artist";
        artist_item = artist_item->next;
    }
    mp3tunes_locker_artist_list_deinit ( &artists_list );
    debug() << "Wrapper deinit Complete";
    return artistsQList;
}

QList<Mp3tunesLockerArtist>
Mp3tunesLocker::artistsSearch ( const QString &query ) const
{
    DEBUG_BLOCK
    Mp3tunesSearchResult container;
    container.searchFor = Mp3tunesSearchResult::ArtistQuery;
    search ( container, query );
    return container.artistList;
}

QList<Mp3tunesLockerAlbum>
Mp3tunesLocker::albums() const
{
    QList<Mp3tunesLockerAlbum> albumsQList; // to be returned
    mp3tunes_locker_album_list_t *albums_list;
    mp3tunes_locker_list_item_t  *album_item;

    //get the list of albums
    mp3tunes_locker_albums ( m_locker, &albums_list );

    mp3tunes_locker_album_t *album; // the value holder
    album_item = albums_list->first; // the current node

    //looping through the list of albums
    while ( album_item != 0 )
    {
        // get the album from the c lib
        album = ( mp3tunes_locker_album_t* ) album_item->value;
        //wrap it up and stick it in the QList
        Mp3tunesLockerAlbum albumWrapped ( album );
        albumsQList.append ( albumWrapped );

        album_item = album_item->next;
    }
    mp3tunes_locker_album_list_deinit ( &albums_list );

    return albumsQList;
}

QList<Mp3tunesLockerAlbum>
Mp3tunesLocker::albumsSearch ( const QString &query ) const
{
    Mp3tunesSearchResult container;
    container.searchFor = Mp3tunesSearchResult::AlbumQuery;
    search ( container, query );
    return container.albumList;
}

QList<Mp3tunesLockerAlbum>
Mp3tunesLocker::albumsWithArtistId ( int artistId ) const
{
    QList<Mp3tunesLockerAlbum> albumsQList; // to be returned
    mp3tunes_locker_album_list_t *albums_list;
    mp3tunes_locker_list_item_t *album_item;

    //get the list of albums
    mp3tunes_locker_albums_with_artist_id ( m_locker, &albums_list, artistId );

    mp3tunes_locker_album_t *album; // the value holder
    album_item = albums_list->first; // the current node

    //looping through the list of albums
    while ( album_item != 0 )
    {
        // get the album from the c lib
        album = ( mp3tunes_locker_album_t* ) album_item->value;
        //wrap it up
        Mp3tunesLockerAlbum albumWrapped ( album );
        albumsQList.append ( albumWrapped );

        album_item = album_item->next;
    }
    mp3tunes_locker_album_list_deinit ( &albums_list );

    return albumsQList;
}

QList<Mp3tunesLockerPlaylist>
Mp3tunesLocker::playlists() const
{
    QList<Mp3tunesLockerPlaylist> playlistsQList; // to be returned

    mp3tunes_locker_playlist_list_t *playlist_list;
    mp3tunes_locker_list_item_t *playlist_item;
    mp3tunes_locker_playlist_t *playlist;

    mp3tunes_locker_playlists ( this->m_locker, &playlist_list );

    playlist_item = playlist_list->first;
    while ( playlist_item != 0 )
    {
        playlist = ( mp3tunes_locker_playlist_t* ) playlist_item->value;

        Mp3tunesLockerPlaylist playlistWrapped ( playlist );
        playlistsQList.append ( playlistWrapped );

        playlist_item = playlist_item->next;
    }
    mp3tunes_locker_playlist_list_deinit ( &playlist_list );

    return playlistsQList;
}

QList<Mp3tunesLockerTrack>
Mp3tunesLocker::tracks() const
{
    QList<Mp3tunesLockerTrack> tracksQList; // to be returned

    mp3tunes_locker_track_list_t *tracks_list;
    mp3tunes_locker_list_item_t *track_item;
    mp3tunes_locker_track_t *track;

    mp3tunes_locker_tracks ( m_locker, &tracks_list );

    track_item = tracks_list->first;
    while ( track_item != 0 )
    {
        track = ( mp3tunes_locker_track_t* ) track_item->value;

        Mp3tunesLockerTrack trackWrapped ( track );
        tracksQList.append ( trackWrapped );

        track_item = track_item->next;
    }
    mp3tunes_locker_track_list_deinit ( &tracks_list );

    return tracksQList;
}

QList<Mp3tunesLockerTrack>
Mp3tunesLocker::tracksSearch ( const QString &query ) const
{
    Mp3tunesSearchResult container;
    container.searchFor = Mp3tunesSearchResult::TrackQuery;
    search ( container, query );
    return container.trackList;
}

QList<Mp3tunesLockerTrack>
Mp3tunesLocker::tracksWithPlaylistId ( const QString & playlistId ) const
{
    char *c_playlistid = convertToChar ( playlistId );

    QList<Mp3tunesLockerTrack> tracksQList; // to be returned

    mp3tunes_locker_track_list_t *tracks_list;
    mp3tunes_locker_list_item_t  *track_item;
    mp3tunes_locker_track_t      *track;

    mp3tunes_locker_tracks_with_playlist_id ( m_locker, &tracks_list, c_playlistid );

    track_item = tracks_list->first;
    while ( track_item != 0 )
    {
        track = ( mp3tunes_locker_track_t* ) track_item->value;

        Mp3tunesLockerTrack trackWrapped ( track );
        tracksQList.append ( trackWrapped );

        track_item = track_item->next;
    }
    mp3tunes_locker_track_list_deinit ( &tracks_list );

    return tracksQList;
}

QList<Mp3tunesLockerTrack>
Mp3tunesLocker::tracksWithAlbumId ( int albumId ) const
{
    QList<Mp3tunesLockerTrack> tracksQList; // to be returned

    mp3tunes_locker_track_list_t *tracks_list;
    mp3tunes_locker_list_item_t *track_item;
    mp3tunes_locker_track_t *track;

    mp3tunes_locker_tracks_with_album_id ( m_locker, &tracks_list, albumId );

    track_item = tracks_list->first;
    while ( track_item != 0 )
    {
        track = ( mp3tunes_locker_track_t* ) track_item->value;

        Mp3tunesLockerTrack trackWrapped ( track );
        tracksQList.append ( trackWrapped );

        track_item = track_item->next;
    }
    mp3tunes_locker_track_list_deinit ( &tracks_list );

    return tracksQList;
}

QList<Mp3tunesLockerTrack>
Mp3tunesLocker::tracksWithArtistId ( int artistId ) const
{
    QList<Mp3tunesLockerTrack> tracksQList; // to be returned

    mp3tunes_locker_track_list_t *tracks_list;
    mp3tunes_locker_list_item_t *track_item;
    mp3tunes_locker_track_t *track;

    mp3tunes_locker_tracks_with_artist_id ( m_locker, &tracks_list, artistId );

    track_item = tracks_list->first;
    while ( track_item != 0 )
    {
        track = ( mp3tunes_locker_track_t* ) track_item->value;

        Mp3tunesLockerTrack trackWrapped ( track );
        tracksQList.append ( trackWrapped );

        track_item = track_item->next;
    }
    mp3tunes_locker_track_list_deinit ( &tracks_list );

    return tracksQList;
}

QList<Mp3tunesLockerTrack>
Mp3tunesLocker::tracksWithFileKeys( QStringList filekeys ) const
{
    QString keys = QString();
    foreach( QString key, filekeys )
    {
       keys.append(key);
       keys.append(",");
    }
    keys.chop(1);
    char* c_keys = convertToChar ( keys );

    mp3tunes_locker_track_list_t *tracks_list;
    mp3tunes_locker_list_item_t *track_item;
    mp3tunes_locker_track_t *track;
    QList<Mp3tunesLockerTrack> tracksQList; // to be returned

    mp3tunes_locker_tracks_with_file_key ( m_locker, c_keys, &tracks_list );

     while ( track_item != 0 )
     {
         track = ( mp3tunes_locker_track_t* ) track_item->value;
         Mp3tunesLockerTrack trackWrapped ( track );
         tracksQList.append ( trackWrapped );

         track_item = track_item->next;
     }
     mp3tunes_locker_track_list_deinit ( &tracks_list );
     return tracksQList;
}

Mp3tunesLockerTrack
Mp3tunesLocker::trackWithFileKey( const QString &filekey ) const
{
    DEBUG_BLOCK
    char* c_key = convertToChar ( filekey );

    mp3tunes_locker_track_t *track;
    mp3tunes_locker_track_with_file_key ( m_locker, c_key, &track );
    debug() << "Got track: " << track->trackTitle << "  from filekey: " << filekey;
    Mp3tunesLockerTrack trackWrapped ( track );
    debug() << "returning";
    return trackWrapped;
}

bool
Mp3tunesLocker::search ( Mp3tunesSearchResult &container, const QString &query ) const
{
    // setup vars
    mp3tunes_locker_artist_list_t *artists_list;
    mp3tunes_locker_list_item_t *artist_item;
    mp3tunes_locker_artist_t *artist;

    mp3tunes_locker_album_list_t *albums_list;
    mp3tunes_locker_list_item_t *album_item;
    mp3tunes_locker_album_t *album;

    mp3tunes_locker_track_list_t *tracks_list;
    mp3tunes_locker_list_item_t *track_item;
    mp3tunes_locker_track_t *track;

    if ( container.searchFor & Mp3tunesSearchResult::ArtistQuery )
        artists_list = 0;

    if ( container.searchFor & Mp3tunesSearchResult::AlbumQuery )
        albums_list = 0;

    if ( container.searchFor & Mp3tunesSearchResult::TrackQuery )
        tracks_list = 0;

    char* c_query = convertToChar ( query );

    int res = mp3tunes_locker_search ( m_locker, &artists_list, &albums_list,
                                       &tracks_list, c_query );
    if ( res != 0 )
        return false;
    if ( container.searchFor & Mp3tunesSearchResult::ArtistQuery )
    {
        artist_item = artists_list->first;
        while ( artist_item != 0 )
        {
            artist = ( mp3tunes_locker_artist_t* ) artist_item->value;

            Mp3tunesLockerArtist artistWrapped ( artist );
            container.artistList.append ( artistWrapped );

            artist_item = artist_item->next;
        }
        mp3tunes_locker_artist_list_deinit ( &artists_list );
    }

    if ( container.searchFor & Mp3tunesSearchResult::AlbumQuery )
    {
        album_item = albums_list->first;
        while ( album_item != 0 )
        {
            album = ( mp3tunes_locker_album_t* ) album_item->value;

            Mp3tunesLockerAlbum albumWrapped ( album );
            container.albumList.append ( albumWrapped );

            album_item = album_item->next;
        }
        mp3tunes_locker_album_list_deinit ( &albums_list );
    }

    if ( container.searchFor & Mp3tunesSearchResult::TrackQuery )
    {
        track_item = tracks_list->first;
        while ( track_item != 0 )
        {
            track = ( mp3tunes_locker_track_t* ) track_item->value;

            Mp3tunesLockerTrack trackWrapped ( track );
            container.trackList.append ( trackWrapped );

            track_item = track_item->next;
        }
        mp3tunes_locker_track_list_deinit ( &tracks_list );
    }
    return true;
}

bool
Mp3tunesLocker::uploadTrack ( const QString &path )
{
    char* c_path = convertToChar ( path );

    int res = mp3tunes_locker_upload_track ( m_locker, c_path );
    if ( res == 0 )
        return true;
    return false;
}

QString
Mp3tunesLocker::fileKey ( const QString &path )
{
    char* c_path = convertToChar ( path );

    char* file_key = ( char* ) malloc ( 4096 * sizeof ( char ) );
    file_key = mp3tunes_locker_generate_filekey ( c_path );

    return QString ( file_key );
}

bool
Mp3tunesLocker::lockerLoad( const QString &url )
{
    char* c_url = convertToChar( url );

    int res = mp3tunes_locker_load_track ( m_locker, c_url);
    if ( res == 0)
        return true;
    return false;
}

QString
Mp3tunesLocker::userName() const
{
    return QString ( m_locker->username );
}

QString
Mp3tunesLocker::password() const
{
    return QString ( m_locker->password );
}

QString
Mp3tunesLocker::sessionId() const
{
    return QString ( m_locker->session_id );
}

QString
Mp3tunesLocker::firstName() const
{
    return QString ( m_locker->firstname );
}

QString
Mp3tunesLocker::lastName() const
{
    return QString ( m_locker->lastname );
}

QString
Mp3tunesLocker::nickName() const
{
    return QString ( m_locker->nickname );
}

QString
Mp3tunesLocker::partnerToken() const
{
    return QString ( m_locker->partner_token );
}

QString
Mp3tunesLocker::serverApi() const
{
    return QString ( m_locker->server_api );
}

QString
Mp3tunesLocker::serverContent() const
{
    return QString ( m_locker->server_content );
}

QString
Mp3tunesLocker::serverLogin() const
{
    return QString ( m_locker->server_login );
}

QString
Mp3tunesLocker::errorMessage() const
{
    if( m_locker->error_message != 0 )
    {
        return QString ( m_locker->error_message );
    }
    return QString();
}
bool
Mp3tunesLocker::authenticated() const
{
    // do it in this order to avoid making an extra http request
    if( sessionId().isEmpty() )
        return false;
    else if( sessionValid() )
        return true;
    return false;
}
char *
Mp3tunesLocker::convertToChar ( const QString &source ) const
{
    QByteArray b = source.toAscii();
    const char *c_tok = b.constData();
    char * ret = ( char * ) malloc ( strlen ( c_tok ) + 1 );
    strcpy ( ret, c_tok );
    return ret;
}
