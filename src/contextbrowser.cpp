// (c) Christian Muehlhaeuser 2004
// See COPYING file for licensing information


#include "amarokconfig.h"
#include "collectionbrowser.h"
#include "collectiondb.h"
#include "contextbrowser.h"
#include "coverfetcher.h"
#include "enginecontroller.h"
#include "metabundle.h"
#include "playlist.h"     //appendMedia()
#include "sqlite/sqlite.h"

#include <kapplication.h> //kapp->config(), QApplication::setOverrideCursor()
#include <kconfig.h>      //config object
#include <kcursor.h>      //waitCursor()
#include <kdebug.h>
#include <kglobal.h>
#include <khtml_part.h>
#include <klineedit.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kurlcombobox.h>

#include <qlabel.h>
#include <qpushbutton.h>
#include <qdatetime.h>

#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>


ContextBrowser::ContextBrowser( const char *name )
        : QVBox( 0, name )
        , m_currentTrack( 0 )
        , m_db( new CollectionDB() )
//         , m_loaded( false )
{
    kdDebug() << k_funcinfo << endl;
    EngineController::instance()->attach( this );

    setSpacing( 4 );
    setMargin( 5 );
    QWidget::setFont( AmarokConfig::useCustomFonts() ? AmarokConfig::playlistWindowFont() : QApplication::font() );

    QHBox *hb1 = new QHBox( this );
    hb1->setSpacing( 4 );

    browser = new KHTMLPart( hb1 );
    browser->setDNDEnabled( true );
    setStyleSheet();

    connect( browser->browserExtension(), SIGNAL( openURLRequest( const KURL &, const KParts::URLArgs & ) ),
             this,                          SLOT( openURLRequest( const KURL & ) ) );

    if ( m_db->isEmpty() || !m_db->isDbValid() )
        showIntroduction();
    else
        showHome();

    setFocusProxy( hb1 ); //so focus is given to a sensible widget when the tab is opened
}


ContextBrowser::~ContextBrowser()
{
    delete m_db;
    delete m_currentTrack;

    EngineController::instance()->detach( this );
}

/*void ContextBrowser::showEvent( QShowEvent *)
{
    kdDebug() << k_funcinfo << endl;
    if( m_loaded ) return;
    QTime t;
    t.start();
    
    if ( m_db->isEmpty() || !m_db->isDbValid() )
        showIntroduction();
    else
        showHome();
    kdDebug()<<"END show"<<endl;
    qDebug( "Context show elapsed: %d ms", t.elapsed() );
    
    m_loaded = true;
}*/

//////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::setFont( const QFont &newFont ) //virtual
{
    if( font() != newFont )
    {
        QWidget::setFont( newFont );
        setStyleSheet();
        browser->setUserStyleSheet( m_styleSheet );
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC SLOTS
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::openURLRequest( const KURL &url )
{
    m_url = url;

    if ( url.protocol() == "album" )
    {
        QStringList info = QStringList::split( "/", url.path() );
        QStringList values;
        QStringList names;

        m_db->execSql( QString( "SELECT DISTINCT url FROM tags WHERE artist = %1 AND album = %2 ORDER BY track DESC;" )
                       .arg( info[0] )
                       .arg( info[1] ), &values, &names );

        for ( uint i = 0; i < values.count(); i++ )
        {
            if ( values[i].isEmpty() ) continue;

            KURL tmp;
            tmp.setPath( values[i] );
            Playlist::instance()->appendMedia( tmp, false, true );
        }
    }

    if ( url.protocol() == "file" )
        Playlist::instance()->appendMedia( url, true, true );

    if ( m_url.protocol() == "show" )
    {
        if ( m_url.path() == "home" )
            showHome();
        else if ( m_url.path() == "context" )
            showCurrentTrack();
        else if ( m_url.path() == "collectionSetup" )
        {
            //TODO if we do move the configuration to the main configdialog change this,
            //     otherwise we need a better solution
            QObject *o = parent()->child( "CollectionBrowser" );
            if( o ) static_cast<CollectionBrowser*>(o)->setupDirs();
        }
    }

    if ( m_url.protocol() == "fetchcover" )
        m_db->fetchCover( this, m_url.path(), false );
}


//////////////////////////////////////////////////////////////////////////////////////////
// PROTECTED
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::engineNewMetaData( const MetaBundle &bundle, bool /*trackChanged*/ )
{
    //prevents segfault when playing streams
    if ( !bundle.url().isLocalFile() ) return;

    delete m_currentTrack;
    m_currentTrack = new MetaBundle( bundle );

    if ( !m_db->isEmpty() )
        showCurrentTrack();

    // increase song counter
    m_db->incSongCounter( m_currentTrack->url().path() );
}


void ContextBrowser::engineStateChanged( Engine::State state )
{
    if ( m_db->isEmpty() )
        return;

    switch( state )
    {
        case Engine::Empty:
            showHome();
        default:
            break;
    }
}


void ContextBrowser::paletteChange( const QPalette& pal )
{
    kdDebug() << k_funcinfo << endl;

    QVBox::paletteChange( pal );

    setStyleSheet();
    showHome();
}


//////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE SLOTS
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::showHome() //SLOT
{
    QStringList values;
    QStringList names;

    // take care of sql updates (schema changed errors)
    delete m_db;
    m_db = new CollectionDB();
    // Triggers redisplay when new cover image is downloaded
    connect( m_db, SIGNAL( coverFetched(const QString&) ), this, SLOT( showCurrentTrack() ) );

    browser->begin();
    browser->setUserStyleSheet( m_styleSheet );

    // <Favorite Tracks Information>
    browser->write( "<html><div class='menu'><a class='menu' href='show:home'>Home</a>&nbsp;&nbsp;<a class='menu' href='show:context'>Current Track</a></div>");
    browser->write( "<div class='rbcontent'>" );
    browser->write( "<table width='100%' border='0' cellspacing='0' cellpadding='0'>" );
    browser->write( "<tr><td class='head'>&nbsp;" + i18n( "Your Favorite Tracks:" ) + "</td></tr>" );
    browser->write( "<tr><td height='1' bgcolor='black'></td></tr>" );
    browser->write( "</table>" );
    browser->write( "<table width='100%' border='0' cellspacing='1' cellpadding='1'>" );

    m_db->execSql( QString( "SELECT tags.title, tags.url, statistics.playcounter, artist.name, album.name "
                            "FROM tags, artist, album, statistics "
                            "WHERE artist.id = tags.artist AND album.id = tags.album AND statistics.url = tags.url "
                            "ORDER BY statistics.playcounter DESC "
                            "LIMIT 0,10;" ), &values, &names );

    if ( values.count() )
    {
        for ( uint i = 0; i < values.count(); i = i + 5 )
            browser->write( QString ( "<tr><td class='song'><a class='song' href=\"file:"
                                    + values[i+1].replace( "\"", QCString( "%22" ) ) + "\"><b>" + values[i]
                                    + "</b> <i>(" + values[i+2] + ")</i><br>" + values[i+3] + " - " + values[i+4] + "</a></td></tr>" ) );
    }

    values.clear();
    names.clear();

    browser->write( "</table></div>" );
    // </Favorite Tracks Information>

    // <Recent Tracks Information>
    browser->write( "<br><div class='rbcontent'>" );
    browser->write( "<table width='100%' border='0' cellspacing='0' cellpadding='0'>" );
    browser->write( "<tr><td class='head'>&nbsp;" + i18n( "Newest Tracks:" ) + "</td></tr>" );
    browser->write( "<tr><td height='1' bgcolor='black'></td></tr>" );
    browser->write( "</table>" );
    browser->write( "<table width='100%' border='0' cellspacing='1' cellpadding='1'>" );

    m_db->execSql( QString( "SELECT tags.title, tags.url, artist.name, album.name "
                            "FROM tags, artist, album "
                            "WHERE artist.id = tags.artist AND album.id = tags.album "
                            "ORDER BY tags.createdate DESC "
                            "LIMIT 0,10;" ), &values, &names );

    if ( values.count() )
    {
        for ( uint i = 0; i < values.count(); i = i + 4 )
            browser->write( QString ( "<tr><td class='song'><a class='song' href=\"file:"
                                    + values[i+1].replace( "\"", QCString( "%22" ) ) + "\"'><b>" + values[i]
                                    + "</b><br>" + values[i+2] + " - " + values[i+3] + "</a></td></tr>" ) );
    }

    values.clear();
    names.clear();

    browser->write( "</table></div>" );
    // </Recent Tracks Information>

    browser->write( "<br></html>" );
    browser->end();
}


void ContextBrowser::showCurrentTrack() //SLOT
{
    if ( !m_currentTrack )
        return;

    QStringList values;
    QStringList names;

    // take care of sql updates (schema changed errors)
    delete m_db;
    m_db = new CollectionDB();
    
    // Triggers redisplay when new cover image is downloaded
    connect( m_db, SIGNAL( coverFetched() ), this, SLOT( showCurrentTrack() ) );

    browser->begin();
    browser->setUserStyleSheet( m_styleSheet );

    // <Current Track Information>
    browser->write( "<html><div class='menu'><a class='menu' href='show:home'>Home</a>&nbsp;&nbsp;<a class='menu' href='show:context'>Current Track</a></div>");
    if ( !m_db->isFileInCollection( m_currentTrack->url().path() ) )
    {
        browser->write( "<div><br>");
        browser->write( i18n( "This song is not in your current collection. If you like further contextual information about this song, add it to your collection!" )
                        + "&nbsp;<a href='show:collectionSetup'>" + i18n( "Click here to change your collection setup." ) + "</a>" );
        browser->write( "<br><br></div>");
    }

    browser->write( "<div class='rbcontent'>" );
    browser->write( "<table width='100%' border='0' cellspacing='0' cellpadding='0'>" );
    browser->write( "<tr><td class='head'>&nbsp;" + i18n( "Currently playing:" ) + "</td></tr>" );
    browser->write( "<tr><td height='1' bgcolor='black'></td></tr>" );
    browser->write( "</table>" );
    browser->write( "<table width='100%' border='0' cellspacing='1' cellpadding='1'>" );

    m_db->execSql( QString( "SELECT album.id, artist.id, datetime( datetime( statistics.createdate, 'unixepoch' ), 'localtime' ), datetime( datetime( statistics.accessdate, 'unixepoch' ), 'localtime' ), statistics.playcounter "
                            "FROM album, tags, artist, statistics "
                            "WHERE album.id = tags.album AND artist.id = tags.artist AND statistics.url = tags.url AND tags.url = '%1';" )
                   .arg( m_db->escapeString( m_currentTrack->url().path() ) ), &values, &names );

    if ( !values.isEmpty() )
        browser->write( QString ( "<tr><td height='42' valign='top' class='rbcurrent'>"
                                  "<span class='album'>%1 - %2</span><br>%3<br><br><a class='menu' href='fetchcover:%4 - %5'><img align='left' valign='center' hspace='2' src=\"%6\"></a>"
                                  "<i>First play: %7<br>Last play: %8<br>Total plays: %9</i></td>"
                                  "</tr>" )
                        .arg( m_currentTrack->artist() )
                        .arg( m_currentTrack->title() )
                        .arg( m_currentTrack->album() )
                        .arg( m_currentTrack->artist() )
                        .arg( m_currentTrack->album() )
                        .arg( m_db->getImageForAlbum( values[1], values[0], locate( "data", "amarok/images/sound.png" ) ) )
                        .arg( values[2].left( values[2].length() - 3 ) )
                        .arg( values[3].left( values[3].length() - 3 ) )
                        .arg( values[4] ) );
    else
    {
        m_db->execSql( QString( "SELECT album.id, artist.id "
                                "FROM album, tags, artist "
                                "WHERE album.id = tags.album AND artist.id = tags.artist AND tags.url = '%1';" )
                      .arg( m_db->escapeString( m_currentTrack->url().path() ) ), &values, &names );

        QString imageurl;
        if ( !values.isEmpty() )
            imageurl = m_db->getImageForAlbum( values[1], values[0], locate( "data", "amarok/images/sound.png" ) );
        else
            imageurl = locate( "data", "amarok/images/sound.png" );

        browser->write( QString ( "<tr><td height='42' valign='top' class='rbcurrent'>"
                                  "<span class='album'>%1 - %2</span><br>%3<br><br><a class='menu' href='fetchcover:%4 - %5'><img align='left' valign='center' hspace='2' src=\"%6\"></a>"
                                  "<i>Never played before</i></td>"
                                  "</tr>" )
                        .arg( m_currentTrack->artist() )
                        .arg( m_currentTrack->title() )
                        .arg( m_currentTrack->album() )
                        .arg( m_currentTrack->artist() )
                        .arg( m_currentTrack->album() )
                        .arg( imageurl ) );
    }
    values.clear();
    names.clear();

    browser->write( "</table></div>" );
    // </Current Track Information>

    // <Favourite Tracks Information>
    m_db->execSql( QString( "SELECT tags.title, tags.url, statistics.playcounter "
                            "FROM tags, artist, statistics "
                            "WHERE tags.artist = artist.id AND artist.name LIKE '%1' AND statistics.url = tags.url "
                            "ORDER BY statistics.playcounter DESC "
                            "LIMIT 0,5;" )
                   .arg( m_db->escapeString( m_currentTrack->artist() ) ), &values, &names );

    if ( !values.isEmpty() )
    {
        browser->write( "<br><div class='rbcontent'>" );
        browser->write( "<table width='100%' border='0' cellspacing='0' cellpadding='0'>" );
        browser->write( "<tr><td class='head'>&nbsp;" + i18n( "Favorite tracks by this artist:" ) + "</td></tr>" );
        browser->write( "<tr><td height='1' bgcolor='black'></td></tr>" );
        browser->write( "</table>" );
        browser->write( "<table width='100%' border='0' cellspacing='1' cellpadding='1'>" );

        for ( uint i = 0; i < values.count(); i += 3 )
            browser->write( QString ( "<tr><td class='song'><a class='song' href=\"file:" + values[i + 1].replace( "\"", QCString( "%22" ) ) + "\">" + values[i] + " <i>(" + values[i + 2] + ")</i></a></td></tr>" ) );

        values.clear();
        names.clear();

        browser->write( "</table></div>" );
    }
    // </Favourite Tracks Information>

    // <Tracks on this album>
    if ( !m_currentTrack->album().isEmpty() && !m_currentTrack->artist().isEmpty() )
    {
        m_db->execSql( QString( "SELECT tags.title, tags.url, tags.track "
                                "FROM tags, artist, album "
                                "WHERE tags.album = album.id AND album.name LIKE '%1' AND "
                                      "tags.artist = artist.id AND artist.name LIKE '%2' "
                                "ORDER BY tags.track;" )
                      .arg( m_db->escapeString( m_currentTrack->album() ) )
                      .arg( m_db->escapeString( m_currentTrack->artist() ) ), &values, &names );

        if ( !values.isEmpty() )
        {
            browser->write( "<br><div class='rbcontent'>" );
            browser->write( "<table width='100%' border='0' cellspacing='0' cellpadding='0'>" );
            browser->write( "<tr><td class='head'>&nbsp;" + i18n( "Tracks on this album:" ) + "</td></tr>" );
            browser->write( "<tr><td height='1' bgcolor='black'></td></tr>" );
            browser->write( "</table>" );
            browser->write( "<table width='100%' border='0' cellspacing='1' cellpadding='1'>" );

            for ( uint i = 0; i < values.count(); i += 3 )
            {
                QString tmp = values[i + 2] == "" ? "" : values[i + 2] + ". ";
                browser->write( QString ( "<tr><td class='song'><a class='song' href=\"file:" + values[i + 1].replace( "\"", QCString( "%22" ) ) + "\">" + tmp + values[i] + "</a></td></tr>" ) );
            }

            values.clear();
            names.clear();

            browser->write( "</table></div>" );
        }
    }
    // </Tracks on this album>

    // <Albums by this artist>
    m_db->execSql( QString( "SELECT DISTINCT album.name, artist.name, album.id, artist.id "
                            "FROM album, tags, artist "
                            "WHERE album.id = tags.album AND tags.artist = artist.id AND album.name <> '' AND artist.name LIKE '%1' "
                            "ORDER BY album.name;" )
                   .arg( m_db->escapeString( m_currentTrack->artist() ) ), &values, &names );

    if ( !values.isEmpty() )
    {
        browser->write( "<br><div class='rbcontent'>" );
        browser->write( "<table width='100%' border='0' cellspacing='0' cellpadding='0'>" );
        browser->write( "<tr><td class='head'>&nbsp;" + i18n( "Albums by this artist:" ) + "</td></tr>" );
        browser->write( "<tr><td height='1' bgcolor='black'></td></tr>" );
        browser->write( "</table>" );
        browser->write( "<table width='100%' border='0' cellspacing='1' cellpadding='1'>" );

        for ( uint i = 0; i < values.count(); i += 4 )
        {
            browser->write( QString ( "<tr><td onClick='window.location.href=\"album:%1/%2\"' height='42' valign='top' class='rbalbum'>"
                                      "<a class='menu' href='fetchcover:%3 - %4'><img align='left' hspace='2' src=\"%5\"></a><span class='album'>%6</span><br>%7 Tracks</td>"
                                      "</tr>" )
                            .arg( values[i + 3] ) // artist.id
                            .arg( values[i + 2] ) // album.id
                            .arg( values[i + 1] ) // artist.name
                            .arg( values[i + 0] ) // album.name
                            .arg( m_db->getImageForAlbum( values[i + 3], values[i + 2], locate( "data", "amarok/images/sound.png" ) ) )
                            .arg( values[i + 0] ) // album.name
                            .arg( m_db->albumSongCount( values[i + 3], values[i + 2] ) ) );
        }

        browser->write( "</table></div>" );
    }
    // </Albums by this artist>

    browser->write( "<br></html>" );
    browser->end();
}


//////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::setStyleSheet()
{
    int pxSize = fontMetrics().height() - 4;

    m_styleSheet =  QString( "div { color: %1; font-size: %2px; text-decoration: none; }" )
                    .arg( colorGroup().text().name() ).arg( pxSize );
    m_styleSheet += QString( "td { color: %1; font-size: %2px; text-decoration: none; }" )
                    .arg( colorGroup().text().name() ).arg( pxSize );
    m_styleSheet += QString( ".menu { color: %1; font-weight: bold; }" )
                    .arg( colorGroup().text().name() );
    m_styleSheet += QString( ".song { color: %1; font-size: %2px; text-decoration: none; }" )
                    .arg( colorGroup().text().name() ).arg( pxSize );
    m_styleSheet += QString( ".song:hover { color: %1; cursor: default; background-color: %2; }" )
                    .arg( colorGroup().highlightedText().name() ).arg( colorGroup().highlight().name() );
    m_styleSheet += QString( "A.song { color: %1; font-size: %2px; text-decoration: none; display: block; }" )
                    .arg( colorGroup().text().name() ).arg( pxSize );
    m_styleSheet += QString( "A.song:hover { color: %1; font-size: %2px; text-decoration: none; display: block; }" )
                    .arg( colorGroup().highlightedText().name() ).arg( pxSize );
    m_styleSheet += QString( ".album { font-weight: bold; font-size: %1px; text-decoration: none; }" )
                    .arg( pxSize );
    m_styleSheet += QString( ".title { color: %1; font-size: %2px; font-weight: bold; }" )
                    .arg( colorGroup().text().name() ).arg( pxSize + 3 );
    m_styleSheet += QString( ".head { color: %1; font-size: %2px; font-weight: bold; background-color: %3; }" )
                    .arg( colorGroup().highlightedText().name() ).arg( pxSize + 2 ).arg( colorGroup().highlight().name() );
    m_styleSheet += QString( ".rbcurrent { color: %1; border: solid %2 1px; }" )
                    .arg( colorGroup().text().name() ).arg( colorGroup().base().name() );
    m_styleSheet += QString( ".rbalbum { color: %1; border: solid %2 1px; }" )
                    .arg( colorGroup().text().name() ).arg( colorGroup().base().name() );
    m_styleSheet += QString( ".rbalbum:hover { color: %1; cursor: default; background-color: %2; border: solid %3 1px; }" )
                    .arg( colorGroup().highlightedText().name() ).arg( colorGroup().highlight().name() ).arg( colorGroup().text().name() );
    m_styleSheet += QString( ".rbcontent { border: solid %1 1px; }" )
                    .arg( colorGroup().highlight().name() );
    m_styleSheet += QString( ".rbcontent:hover { border: solid %1 1px; }" )
                    .arg( colorGroup().text().name() );
}


void ContextBrowser::showIntroduction()
{
    browser->begin();
    browser->setUserStyleSheet( m_styleSheet );

    browser->write( "<html><div>");
    browser->write( i18n( "Hello amaroK user!" ) );
    browser->write( "<br><br>" );
    browser->write( i18n( "This is the Context Browser: it shows you contextual information about the currently playing track."
                          "In order to use this feature of amaroK, you need to build a collection." )
                    + "&nbsp;<a href='show:collectionSetup'>" + i18n( "Click here to build one." ) + "</a>" );
    browser->write( "</div></html>");

    browser->end();
}


#include "contextbrowser.moc"
