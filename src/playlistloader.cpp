// Author: Max Howell (C) Copyright 2003-4
// Author: Mark Kretschmann (C) Copyright 2004
// Copyright: See COPYING file that comes with this distribution
//

///For pls and m3u specifications see:
///http://forums.winamp.com/showthread.php?s=dbec47f3a05d10a3a77959f17926d39c&threadid=65772

#include "collectiondb.h"
#include "enginecontroller.h"
#include "playlist.h"
#include "playlistloader.h"
#include "statusbar.h"

#include <qfile.h>       //::loadPlaylist()
#include <qfileinfo.h>
#include <qlistview.h>
#include <qmap.h>        //::recurse()
#include <qstringlist.h>
#include <qtextstream.h> //::loadPlaylist()

#include <kapplication.h>
#include <kdebug.h>
#include <kdirlister.h>
#include <kurl.h>


bool PlaylistLoader::s_stop;

PlaylistLoader::PlaylistLoader( const KURL::List &urls, QListViewItem *after, bool playFirstUrl )
    : ThreadWeaver::Job( "PlaylistLoader" )
    , m_URLs( urls )
    , m_afterItem( after )
    , m_playFirstUrl( playFirstUrl )
    , m_dirLister( new KDirLister() )
{
    s_stop = false;

    m_dirLister->setAutoUpdate( false );
    m_dirLister->setAutoErrorHandlingEnabled( false, 0 );

    // BEGIN Read folders recursively
    KURL::List::ConstIterator it;
    KURL::List::ConstIterator end = m_URLs.end();

    for( it = m_URLs.begin(); it != end && !s_stop; ++it )
    {
        const KURL url = *it;

        if ( url.protocol() == "fetchcover" ) {
            // ignore
            continue;
        }
        if ( url.protocol() == "album" ) {
           // url looks like:   album:<artist_id> @@@ <album_id>
           // extract artist_id, album_id
           QString myUrl = url.path();
           if ( myUrl.endsWith( " @@@" ) )
                myUrl += ' ';
           const QStringList list = QStringList::split( " @@@ ", myUrl, true );
           Q_ASSERT( !list.isEmpty() );
           QString artist_id = list.front();
           QString album_id  = list.back();

           // get tracks for album, and add them to the playlist
           QStringList trackValues = CollectionDB::instance()->albumTracks( artist_id, album_id );
           if ( !trackValues.isEmpty() )
              for ( uint j = 0; j < trackValues.count(); j++ )
              {
                   KURL url;
                   url.setPath(trackValues[j]);
                   url.setProtocol("file");
                   m_fileURLs.append( url );
              }
        } else
            if ( url.isLocalFile() )
            {
                if ( QFileInfo( url.path() ).isDir() )
                    recurse( url );
                else
                    m_fileURLs.append( url );
            }
            else if ( !recurse( url ) )
                m_fileURLs.append( url );
    }
    // END

    delete m_dirLister;
}


PlaylistLoader::~PlaylistLoader()
{
}


/////////////////////////////////////////////////////////////////////////////////////
// PUBLIC
/////////////////////////////////////////////////////////////////////////////////////

#include <ktempfile.h>
#include <kio/netaccess.h>
#include <kcursor.h>
#include <kmessagebox.h>
#include <klocale.h>
void
PlaylistLoader::downloadPlaylist( const KURL &url, QListView *listView, QListViewItem *item, bool directPlay )
{
    //KIO::NetAccess can make it's own tempfile
    //but we need to add .pls/.m3u extension or the Loader will fail
    QString path = url.filename();
    KTempFile tmpfile( QString::null, path.mid( path.findRev( '.' ) ) ); //use default prefix
    path = tmpfile.name();

    amaroK::StatusBar::instance()->message( i18n("Retrieving playlist...") );
    QApplication::setOverrideCursor( KCursor::waitCursor() );
        const bool succeeded = KIO::NetAccess::download( url, path, listView );
    QApplication::restoreOverrideCursor();
    amaroK::StatusBar::instance()->clear();

    if( succeeded )
    {
        //TODO delete the tempfile
        KURL url;
        url.setPath( path );

        ThreadWeaver::instance()->queueJob( new PlaylistLoader( KURL::List( url ), item, directPlay ) );

    } else {

        KMessageBox::sorry( listView, i18n( "<p>The playlist, <i>'%1'</i>, could not be downloaded." ).arg( url.prettyURL() ) );
        tmpfile.unlink();
    }
}


/////////////////////////////////////////////////////////////////////////////////////
// PROTECTED
/////////////////////////////////////////////////////////////////////////////////////

bool
PlaylistLoader::doJob()
{
    amaroK::StatusBar::startProgress();
    QApplication::postEvent( Playlist::instance(), new StartedEvent( m_afterItem, m_playFirstUrl ) );

    // BEGIN Read tags, post bundles to Playlist
    float increment = 100.0 / m_fileURLs.count();
    float progress = 0;

    KURL::List::ConstIterator it;
    KURL::List::ConstIterator end = m_fileURLs.end();

    for ( it = m_fileURLs.begin(); it != end && !s_stop; ++it )
    {
        const KURL &url = *it;

        if( url.isLocalFile() && isPlaylist( url ) && loadPlaylist( url.path() ) )
            continue;

        if ( EngineController::canDecode( url ) )
            postItem( url );

        progress += increment;
        amaroK::StatusBar::showProgress( uint(progress) );

        // Allow GUI thread some time to breathe
        msleep( 2 );
   }
   // END

    amaroK::StatusBar::stopProgress();
    QApplication::postEvent( Playlist::instance(), new DoneEvent( this ) );

    return true;
}


void
PlaylistLoader::postItem( const KURL &url, const QString &title, const uint length )
{
    MetaBundle bundle( url, true, CollectionDB::instance() );

    if (! title.isEmpty())
        bundle.setTitle( title );

    if (length != MetaBundle::Undetermined)
        bundle.setLength( length );

    QApplication::postEvent( Playlist::instance(), new ItemEvent( bundle ) );
}


bool
PlaylistLoader::loadPlaylist( const QString &path, Format type )
{
    QFile file( path );
    if ( !file.open( IO_ReadOnly ) )
    {
        kdDebug() << "[PLSLoader] Couldn't open file: " << path << endl;
        return false;
    }
    QTextStream stream( &file );

    switch( type )
    {
    case M3U:
    {
        const QString dir = path.left( path.findRev( '/' ) + 1 );
        QString str, title;
        int length = MetaBundle::Undetermined; // = -2

        while( !( str = stream.readLine() ).isNull() && !s_stop )
        {
            if ( str.startsWith( "#EXTINF" ) )
            {
                QString extinf = str.section( ':', 1);
                length = extinf.section( ',', 0, 0 ).toInt();
                title = extinf.section( ',', 1 );

                if ( length == 0 ) length = MetaBundle::Undetermined;
            }
            else if ( !str.startsWith( "#" ) && !str.isEmpty() )
            {

                if ( !( str[ 0 ] == '/' || str.contains( "://" ) ) )
                    str.prepend( dir );

                postItem( KURL::fromPathOrURL( str ), title, length );

                length = MetaBundle::Undetermined;
                title = QString();
            }
        }
        break;
    }
    case PLS:

        for( QString line = stream.readLine(); !line.isNull() && !s_stop; line = stream.readLine() )
        {
            if( line.startsWith( "File" ) )
            {
                const KURL url = KURL::fromPathOrURL( line.section( "=", -1 ) );
                QString title;
                int length = 0;

                line = stream.readLine();

                if ( line.startsWith( "Title" ) )
                {
                    title = line.section( "=", -1 );
                    line  = stream.readLine();
                }

                if ( line.startsWith( "Length" ) )
                    length = line.section( "=", -1 ).toInt();

                postItem( url, title, length );
            }
        }
        break;

    case XML:
    {
        stream.setEncoding( QTextStream::UnicodeUTF8 );

        QDomDocument d;
        if( !d.setContent( stream.read() ) ) return false;

        const QString ITEM( "item" ); //so we don't construct these QStrings all the time
        const QString URL( "url" );

        for( QDomNode n = d.namedItem( "playlist" ).firstChild();
             !n.isNull() && n.nodeName() == ITEM && !s_stop;
             n = n.nextSibling() )
        {
            const QDomElement e = n.toElement();

            if ( !e.isNull() )
                QApplication::postEvent( Playlist::instance(), new DomItemEvent( KURL(e.attribute( URL ) ), n ) );
        }
    }
    default:
        ;
    } //switch

    return true;
}


/////////////////////////////////////////////////////////////////////////////////////
// PRIVATE
/////////////////////////////////////////////////////////////////////////////////////

bool
PlaylistLoader::recurse( const KURL &url, bool recursing )
{
        static bool success;
        if ( !recursing ) success = false;

        typedef QMap<QString, KURL> FileMap;

        KURL::List dirs;
        FileMap files;

        m_dirLister->openURL( url );

        while ( !m_dirLister->isFinished() )
            //FIXME sigh, this is a crash waiting to happen
            kapp->processEvents( 100 );

        KFileItem* item;
        KFileItemList items = m_dirLister->items();

        success |= !items.isEmpty();

        for ( item = items.first(); item; item = items.next() ) {
            if ( item->url().fileName() == "." || item->url().fileName() == ".." )
                continue;
            if ( item->isFile() )
                files[item->url().fileName()] = item->url();
            if ( item->isDir() )
                dirs << item->url();
        }

        // Add files to URL list
        const FileMap::ConstIterator end1 = files.end();
        for ( FileMap::ConstIterator it = files.begin(); it != end1; ++it )
            m_fileURLs.append( it.data() );

        // Recurse folders
        const KURL::List::Iterator end2 = dirs.end();
        for ( KURL::List::Iterator it = dirs.begin(); it != end2; ++it )
            recurse( *it, true );

        return success;
}


void
PlaylistLoader::postItem( const KURL &url )
{
    MetaBundle bundle( url, true, CollectionDB::instance() );
    QApplication::postEvent( Playlist::instance(), new ItemEvent( bundle ) );
}
