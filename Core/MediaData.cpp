﻿// The MIT License( MIT )
//
// Copyright( c ) 2022 Scott Aron Bloom
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sub-license, and/or sell
// copies of the Software, and to permit persons to whom the Software iRHS
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "MediaData.h"
#include "MediaServerData.h"
#include "Settings.h"
#include "ServerInfo.h"
#include "ServerModel.h"
#include "MediaModel.h"
#include "SyncSystem.h"
#include "MovieStub.h"
#include "SABUtils/StringUtils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QObject>
#include <QVariant>
#include <QFileInfo>
#include <QDesktopServices>
#include <chrono>
#include <optional>
#include <QDebug>
#include <QAction>
#include <QMenu>

QStringList CMediaData::getHeaderLabels()
{
    return QStringList() << QObject::tr( "Name" ) << QObject::tr( "ID" ) << QObject::tr( "Is Favorite?" ) << QObject::tr( "Played?" ) << QObject::tr( "Last Played" ) << QObject::tr( "Play Count" ) << QObject::tr( "Play Position" );
}

std::function< QString( uint64_t ) > CMediaData::sMSecsToStringFunc;
void CMediaData::setMSecsToStringFunc( std::function< QString( uint64_t ) > func )
{
    sMSecsToStringFunc = func;
}

std::function< QString( uint64_t ) > CMediaData::mecsToStringFunc()
{
    return sMSecsToStringFunc;
}

CMediaData::CMediaData( const QJsonObject &mediaObj, std::shared_ptr< CServerModel > serverModel )
{
    // auto tmp = ;
    // qDebug() << tmp.toJson();

    computeName( mediaObj );
    //qDebug().nospace().noquote() << QJsonDocument( mediaObj ).toJson();
    fType = mediaObj[ "Type" ].toString();
    fOriginalTitle = mediaObj[ "OriginalTitle" ].toString();
    fIsMissing = mediaObj[ "IsMissing" ].toBool();

    for ( auto &&serverInfo : *serverModel )
    {
        if ( !serverInfo->isEnabled() )
            continue;
        fInfoForServer[ serverInfo->keyName() ] = std::make_shared< SMediaServerData >();
    }
}

CMediaData::CMediaData( const SMovieStub &movieStub, const QString &type )
{
    fName = movieStub.fName;
    fOriginalTitle = fName;
    fType = type;
    fPremiereDate = QDate( movieStub.fYear, 1, 1 );
    if ( movieStub.hasResolution() )
        fResolution = movieStub.fResolution.value();
    else
        fResolution = { 0, 0 };
}

QString CMediaData::searchKey() const
{
    QString searchKey;
    auto pos = fProviders.find( "imdb" );
    if ( pos != fProviders.end() )
    {
        searchKey = ( *pos ).second;
    }
    if ( searchKey.isEmpty() )
        searchKey = QString( R"("%1")" ).arg( fName );

    if ( this->mediaType() == "Episode" )
    {
        searchKey = fSeriesName;
        searchKey = searchKey.replace( R"((US))", "" ).trimmed();

        QString subKey;
        if ( fSeason.has_value() )
            subKey += QString( "S%1" ).arg( fSeason.value(), 2, 10, QChar( '0' ) );

        if ( fEpisode.has_value() )
            subKey += QString( "E%1" ).arg( fEpisode.value(), 2, 10, QChar( '0' ) );
        searchKey += " " + subKey;
    }
    else if ( this->mediaType() == "Movie" )
    {
        if ( fPremiereDate.isValid() )
        {
            searchKey += " " + QString::number( fPremiereDate.year() );
        }
    }
    return searchKey;
}

QUrl CMediaData::getDefaultSearchURL( const std::shared_ptr< CSettings > &settings ) const
{
    auto searchServers = settings->searchServers();
    if ( searchServers.empty() )
        return {};

    auto retVal = searchServers.front()->searchUrl( searchKey() );
    return retVal;
}

void CMediaData::addSearchMenu( const std::shared_ptr< CSettings > &settings, QMenu *menu ) const
{
    auto searchServers = settings->searchServers();
    for ( auto &&ii : searchServers )
    {
        auto action = menu->addAction( ii->displayName() );
        QObject::connect(
            action, &QAction::triggered,
            [ this, ii ]()
            {
                auto url = ii->searchUrl( searchKey() );
                QDesktopServices::openUrl( url );
            } );
    }
}

bool CMediaData::isExtra( const QJsonObject &obj )
{
    if ( !obj.contains( "Path" ) )
        return false;
    auto fullPath = obj[ "Path" ].toString();
    auto path = QFileInfo( fullPath );
    auto parentDir = path.absolutePath();
    auto parentDirName = QFileInfo( parentDir ).baseName();
    parentDirName = parentDirName.toLower();
    if ( parentDirName.contains( "extras" ) || parentDirName.contains( "featurettes" ) || parentDirName.contains( "interviews" ) || parentDirName.contains( "season 00" ) )
        return true;
    return false;
}

std::shared_ptr< SMediaServerData > CMediaData::userMediaData( const QString &serverName ) const
{
    auto pos = fInfoForServer.find( serverName );
    if ( pos != fInfoForServer.end() )
        return ( *pos ).second;
    return {};
}

QString CMediaData::name() const
{
    return fName;
}

QString CMediaData::seriesName() const
{
    return fSeriesName;
}

QString CMediaData::mediaType() const
{
    return fType;
}

void CMediaData::computeName( const QJsonObject &media )
{
    auto name = fName = media[ "Name" ].toString();
    if ( media[ "Type" ] == "Episode" )
    {
        // auto tmp = QJsonDocument( media );
        // qDebug() << tmp.toJson();

        fSeriesName = media[ "SeriesName" ].toString();
        auto season = media[ "SeasonName" ].toString();
        auto pos = season.lastIndexOf( ' ' );
        bool aOK = false;
        if ( pos != -1 )
        {
            fSeason = season.mid( pos + 1 ).toInt( &aOK );
            if ( aOK )
                season = QString( "S%1" ).arg( fSeason.value(), 2, 10, QChar( '0' ) );
        }
        if ( !aOK )
        {
            season.clear();
            fSeason.reset();
            if ( media.contains( "ParentIndexNumber" ) )
            {
                fSeason = media[ "ParentIndexNumber" ].toInt();
                if ( fSeason.value() == 0 )
                    fSeason.reset();
            }
            season = fSeason.has_value() ? ( QString( "S%1" ).arg( fSeason.value(), 2, 10, QChar( '0' ) ) ) : QString();
        }
        fEpisode = media[ "IndexNumber" ].toInt();
        if ( fEpisode.value() == 0 )
            fEpisode.reset();

        auto episode = fEpisode.has_value() ? QString( "E%1" ).arg( fEpisode.value(), 2, 10, QChar( '0' ) ) : QString();
        fName = QString( "%1 - %2%3" ).arg( fSeriesName ).arg( season ).arg( episode );
        auto episodeName = media[ "EpisodeTitle" ].toString();
        if ( !episodeName.isEmpty() )
            fName += QString( " - %1" ).arg( episodeName );
        if ( !name.isEmpty() )
            fName += QString( " - %1" ).arg( name );
    }
}

void CMediaData::loadData( const QString &serverName, const QJsonObject &media )
{
    //qDebug().noquote().nospace() << QJsonDocument( media ).toJson( QJsonDocument::Indented );

    auto externalUrls = media[ "ExternalUrls" ].toArray();
    for ( auto &&ii : externalUrls )
    {
        auto urlObj = ii.toObject();
        auto name = urlObj[ "Name" ].toString();
        auto url = urlObj[ "Url" ].toString();
        fExternalUrls[ name ] = url;
    }

    auto userDataObj = media[ "UserData" ].toObject();

    // auto tmp = QJsonDocument( userDataObj );
    // qDebug() << tmp.toJson();

    auto mediaData = userMediaData( serverName );
    mediaData->loadUserDataFromJSON( userDataObj );

    auto providerIDsObj = media[ "ProviderIds" ].toObject();
    for ( auto &&ii = providerIDsObj.begin(); ii != providerIDsObj.end(); ++ii )
    {
        auto providerName = ii.key();
        auto providerID = ii.value().toString();
        addProvider( providerName, providerID );
    }

    fPremiereDate = media[ "PremiereDate" ].toVariant().toDate();
    if ( media.contains( "MediaSources" ) )
    {
        loadResolution( media[ "MediaSources" ].toArray() );
    }
}

void CMediaData::loadResolution( const QJsonArray &mediaSources )
{
    //qDebug().noquote().nospace() << QJsonDocument( mediaSources ).toJson( QJsonDocument::Indented );

    for ( auto &&ii : mediaSources )
    {
        auto mediaSource = ii.toObject();
        //qDebug().noquote().nospace() << QJsonDocument( mediaSource ).toJson( QJsonDocument::Indented );
        if ( mediaSource.contains( "MediaStreams" ) )
        {
            auto streams = mediaSource[ "MediaStreams" ].toArray();
            for ( auto &&jj : streams )
            {
                auto stream = jj.toObject();
                //qDebug().noquote().nospace() << QJsonDocument( stream ).toJson( QJsonDocument::Indented );
                auto type = stream[ "Type" ].toString().toLower();

                if ( type == "video" )
                {
                    fResolution = { stream[ "Width" ].toInt(), stream[ "Height" ].toInt() };
                };
            }
        }
    }
}

QString CMediaData::externalUrlsText() const
{
    auto retVal = QString( "External Urls:</br>\n<ul>\n%1\n</ul>" );

    QStringList externalUrls;
    for ( auto &&ii : fExternalUrls )
    {
        auto curr = QString( R"(<li>%1 - <a href="%2">%2</a></li>)" ).arg( ii.first ).arg( ii.second );
        externalUrls << curr;
    }
    retVal = retVal.arg( externalUrls.join( "\n" ) );
    return retVal;
}

bool CMediaData::hasProviderIDs() const
{
    return !fProviders.empty();
}

QString CMediaData::getProviderList() const
{
    QStringList retVal;
    for ( auto &&ii : fProviders )
    {
        retVal << ii.first.toLower() + "." + ii.second.toLower();
    }
    return retVal.join( "," );
}

bool CMediaData::isPlayed( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return false;
    return mediaData->fPlayed;
}

uint64_t CMediaData::playCount( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return 0;
    return mediaData->fPlayCount;
}

bool CMediaData::allPlayCountEqual() const
{
    return allEqual< uint64_t >( []( std::shared_ptr< SMediaServerData > data ) { return data->fPlayCount; } );
}

bool CMediaData::isFavorite( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return false;
    return mediaData->fIsFavorite;
}

bool CMediaData::allFavoriteEqual() const
{
    return allEqual< bool >( []( std::shared_ptr< SMediaServerData > data ) { return data->fIsFavorite; } );
}

QDateTime CMediaData::lastPlayed( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return {};
    return mediaData->fLastPlayedDate;
}

bool CMediaData::allLastPlayedEqual() const
{
    return allEqual< QDateTime >( []( std::shared_ptr< SMediaServerData > data ) { return data->fLastPlayedDate; } );
}

// 1 tick = 10000 ms
uint64_t CMediaData::playbackPositionTicks( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return 0;
    return mediaData->fPlaybackPositionTicks;
}

// stored in ticks
// 1 tick = 10000 ms
uint64_t CMediaData::playbackPositionMSecs( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return 0;
    return mediaData->playbackPositionMSecs();
}

QString CMediaData::playbackPosition( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return {};
    return mediaData->playbackPosition();
}

QTime CMediaData::playbackPositionTime( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return {};
    return mediaData->playbackPositionTime();
}

QString CMediaData::resolution() const
{
    if ( ( fResolution != std::make_pair( 0, 0 ) ) && ( fResolution != std::make_pair( -1, -1 ) ) )
        return QString( "%1x%2" ).arg( fResolution.first ).arg( fResolution.second );
    return {};
}

std::pair< int, int > CMediaData::resolutionValue() const
{
    return fResolution;
}

bool CMediaData::allPlayedEqual() const
{
    return allEqual< bool >( []( std::shared_ptr< SMediaServerData > data ) { return data->fPlayed; } );
}

bool CMediaData::allPlaybackPositionTicksEqual() const
{
    return allEqual< int64_t >( []( std::shared_ptr< SMediaServerData > data ) { return data->fPlaybackPositionTicks; } );
}

QUrlQuery CMediaData::getSearchForMediaQuery() const
{
    QUrlQuery query;

    query.addQueryItem( "IncludeItemTypes", "Movie,Episode,Video" );
    query.addQueryItem( "AnyProviderIdEquals", getProviderList() );
    query.addQueryItem( "SortBy", "SortName" );
    query.addQueryItem( "SortOrder", "Ascending" );
    query.addQueryItem( "Recursive", "True" );

    return query;
}

QString CMediaData::getProviderID( const QString &provider )
{
    auto pos = fProviders.find( provider );
    if ( pos == fProviders.end() )
        return {};
    return ( *pos ).second;
}

std::map< QString, QString > CMediaData::getProviders( bool addKeyIfEmpty /*= false */ ) const
{
    auto retVal = fProviders;
    if ( addKeyIfEmpty && retVal.empty() )
    {
        retVal[ fType ] = fName;
    }

    return std::move( retVal );
}

void CMediaData::addProvider( const QString &providerName, const QString &providerID )
{
    fProviders[ providerName ] = providerID;
}

void CMediaData::setMediaID( const QString &serverName, const QString &mediaID )
{
    auto mediaData = userMediaData( serverName );
    mediaData->fMediaID = mediaID;
    updateCanBeSynced();
}

void CMediaData::updateCanBeSynced()
{
    int serverCnt = 0;
    for ( auto &&ii : fInfoForServer )
    {
        if ( ii.second->isValid() )
            serverCnt++;
    }
    fCanBeSynced = serverCnt > 1;
}

QString CMediaData::getMediaID( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return {};
    return mediaData->fMediaID;
}

bool CMediaData::beenLoaded( const QString &serverName ) const
{
    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return {};
    return mediaData->fBeenLoaded;
}

QIcon CMediaData::getDirectionIcon( const QString &serverName ) const
{
    static QIcon sErrorIcon( ":/resources/error.png" );
    static QIcon sEqualIcon( ":/resources/equal.png" );
    static QIcon sArrowUpIcon( ":/resources/arrowup.png" );
    static QIcon sArrowDownIcon( ":/resources/arrowdown.png" );
    QIcon retVal;
    if ( !isValidForServer( serverName ) )
        retVal = sErrorIcon;
    else if ( !canBeSynced() )
        return {};
    else if ( validUserDataEqual() )
        retVal = sEqualIcon;
    else if ( needsUpdating( serverName ) )
        retVal = sArrowDownIcon;
    else
        retVal = sArrowUpIcon;

    return retVal;
}

QJsonObject CMediaData::toJson( const std::shared_ptr< CSettings > &settings, bool includeSearchURL ) const
{
    QJsonObject retVal;

    retVal[ "type" ] = fType;
    retVal[ "name" ] = fName;
    if ( !fSeriesName.isEmpty() )
        retVal[ "seriesname" ] = fSeriesName;
    if ( fSeason.has_value() )
        retVal[ "season" ] = fSeason.value();
    if ( fEpisode.has_value() )
        retVal[ "season" ] = fEpisode.value();
    retVal[ "premiere_date" ] = fPremiereDate.toString( "MM/dd/yyyy" );

    QJsonArray serverInfos;
    for ( auto &&ii : fInfoForServer )
    {
        if ( !ii.second->isValid() )
            continue;
        ;
        auto serverInfo = ii.second->toJson();
        serverInfo[ "server_url" ] = ii.first;
        serverInfos.push_back( serverInfo );
    }
    retVal[ "server_infos" ] = serverInfos;

    if ( includeSearchURL )
        retVal[ "searchurl" ] = getDefaultSearchURL( settings ).toString();

    return retVal;
}

bool CMediaData::onServer() const
{
    return !this->fInfoForServer.empty();
}

void CMediaData::updateFromOther( const QString &otherServerName, std::shared_ptr< CMediaData > other )
{
    auto otherMediaData = other->userMediaData( otherServerName );
    if ( !otherMediaData )
        return;

    fInfoForServer[ otherServerName ] = otherMediaData;
    updateCanBeSynced();
}

bool CMediaData::needsUpdating( const QString &serverName ) const
{
    // TODO: When Emby supports last modified use that

    auto mediaData = userMediaData( serverName );
    if ( !mediaData )
        return false;
    return ( mediaData != newestMediaData() );
}

std::shared_ptr< SMediaServerData > CMediaData::newestMediaData() const
{
    std::shared_ptr< SMediaServerData > retVal;
    for ( auto &&ii : fInfoForServer )
    {
        if ( !ii.second->isValid() )
            continue;

        if ( !retVal )
            retVal = ii.second;
        else
        {
            if ( ii.second->fLastPlayedDate > retVal->fLastPlayedDate )
                retVal = ii.second;
        }
    }
    return retVal;
}

bool CMediaData::isValidForServer( const QString &serverName ) const
{
    auto mediaInfo = userMediaData( serverName );
    if ( !mediaInfo )
        return false;
    return mediaInfo->isValid();
}

bool CMediaData::isValidForAllServers() const
{
    for ( auto &&ii : fInfoForServer )
    {
        if ( !ii.second->isValid() )
            return false;
    }
    return true;
}

bool CMediaData::canBeSynced() const
{
    return fCanBeSynced;
}

EMediaSyncStatus CMediaData::syncStatus() const
{
    if ( !canBeSynced() )
        return EMediaSyncStatus::eNoServerPairs;

    if ( validUserDataEqual() )
        return EMediaSyncStatus::eMediaEqualOnValidServers;
    return EMediaSyncStatus::eMediaNeedsUpdating;
}

bool CMediaData::validUserDataEqual() const
{
    std::list< std::pair< QString, std::shared_ptr< SMediaServerData > > > validServerData;
    for ( auto &&ii : fInfoForServer )
    {
        if ( ii.second->isValid() )
            validServerData.push_back( ii );
    }

    auto pos = validServerData.begin();
    auto nextPos = validServerData.begin();
    nextPos++;
    for ( ; ( pos != validServerData.end() ) && ( nextPos != validServerData.end() ); ++pos, ++nextPos )
    {
        if ( !( *pos ).second->userDataEqual( *( ( *nextPos ).second ) ) )
            return false;
    }
    return true;
}

bool CMediaData::isMatch( const QString &name, int year ) const
{
    bool isMatch = ( ( year >= premiereDate().year() - 3 ) && ( year <= premiereDate().year() + 3 ) );

    if ( !isMatch )
        return false;

    if ( SMovieStub::nameKey( name ) == SMovieStub::nameKey( fName ) )
        return true;
    if ( SMovieStub::nameKey( name ) == SMovieStub::nameKey( fOriginalTitle ) )
        return true;
    if ( NSABUtils::NStringUtils::isSimilar( fName, name, true ) )
        return true;
    if ( NSABUtils::NStringUtils::isSimilar( fOriginalTitle, name, true ) )
        return true;
    return false;
}

bool CMediaData::isMissingProvider( EMissingProviderIDs missingIdsType ) const
{
    if ( missingIdsType == EMissingProviderIDs::eNone )
        return true;

    if ( ( missingIdsType & EMissingProviderIDs::eIMDBid ) != 0 )
    {
        auto pos = fProviders.find( "Imdb" );
        if ( pos == fProviders.end() )
            return true;
        return ( *pos ).second.isEmpty();
    }

    if ( ( missingIdsType & EMissingProviderIDs::eTVRageid ) != 0 )
    {
        auto pos = fProviders.find( "TvRage" );
        if ( pos == fProviders.end() )
            return true;
        return ( *pos ).second.isEmpty();
    }

    if ( ( missingIdsType & EMissingProviderIDs::eTMDBid ) != 0 )
    {
        auto pos = fProviders.find( "Tmdb" );
        if ( pos == fProviders.end() )
            return true;
        return ( *pos ).second.isEmpty();
    }

    if ( ( missingIdsType & EMissingProviderIDs::eTVDBid ) != 0 )
    {
        auto pos = fProviders.find( "Tvdb" );
        if ( pos == fProviders.end() )
            return true;
        return ( *pos ).second.isEmpty();
    }
    return false;
}

CMediaCollection::CMediaCollection( const QString &serverName, const QString &name, const QString &id, int pos ) :
    fName( name ),
    fServerName( serverName ),
    fPosition( pos )
{
    fCollectionInfo = std::make_shared< SCollectionServerInfo >( id );
    fCollectionInfo->setId( id );
}

bool SCollectionServerInfo::missingMedia() const
{
    for ( auto &&ii : fItems )
    {
        if ( !ii->fData || !ii->fData->onServer() )
            return true;
    }
    return false;
}

int SCollectionServerInfo::numMissing() const
{
    int retVal = 0;
    for ( auto &&ii : fItems )
    {
        if ( !ii->fData || !ii->fData->onServer() )
            ++retVal;
    }
    return retVal;
}

void SCollectionServerInfo::createCollection( std::shared_ptr< const CServerInfo > serverInfo, const QString &collectionName, std::shared_ptr< CSyncSystem > syncSystem )
{
    if ( collectionExists() )
        return;

    std::list< std::shared_ptr< CMediaData > > media;
    for ( auto &&ii : fItems )
    {
        if ( ii->fData )
            media.emplace_back( ii->fData );
    }

    syncSystem->createCollection( serverInfo, collectionName, media );
}

QVariant CMediaCollection::data( int column, int role ) const
{
    if ( role == Qt::DisplayRole )
    {
        switch ( column )
        {
            case 0:
                return fName;
            case 1:
                return {};
            case 2:
                return fCollectionInfo->collectionExists() ? QObject::tr( "Yes" ) : QString( "No" );
            case 3:
                return missingMedia() ? QObject::tr( "Yes" ) : QString();
        }
    }
    return {};
}

std::shared_ptr< SMediaCollectionData > CMediaCollection::addMovie( const QString &name, int year, const std::pair< int, int > &resolution, int rank )
{
    return fCollectionInfo->addMovie( name, year, resolution, this, rank );
}

void CMediaCollection::setItems( const std::list< std::shared_ptr< CMediaData > > &items )
{
    fCollectionInfo->fItems.clear();
    fCollectionInfo->fItems.reserve( items.size() );
    for ( auto &&ii : items )
    {
        auto curr = std::make_shared< SMediaCollectionData >( ii, this );
        fCollectionInfo->fItems.push_back( curr );
    }
}

bool CMediaCollection::updateWithRealCollection( std::shared_ptr< CMediaCollection > realCollection )
{
    (void)realCollection;
    return false;
}

QString CMediaCollection::fileBaseName() const
{
    return QFileInfo( fFileName ).baseName();
}

SCollectionServerInfo::SCollectionServerInfo( const QString &id ) :
    fCollectionID( id )
{
}

bool SCollectionServerInfo::updateMedia( std::shared_ptr< CMediaModel > mediaModel )
{
    bool retVal = false;
    for ( auto &&ii : fItems )
    {
        if ( ii->fData && !ii->fData->onServer() )
        {
            retVal = ii->updateMedia( mediaModel ) || retVal;
        }
    }
    return retVal;
}

std::shared_ptr< SMediaCollectionData > SCollectionServerInfo::addMovie( const QString &name, int year, const std::pair< int, int > &resolution, CMediaCollection *parent, int rank )
{
    auto retVal = std::make_shared< SMediaCollectionData >( std::make_shared< CMediaData >( SMovieStub( name, year, resolution ), "Movie" ), parent );

    if ( ( rank > 0 ) && ( rank - 1 ) >= fItems.size() )
    {
        fItems.resize( rank );
    }
    if ( rank == -1 )
        fItems.push_back( retVal );
    else
        fItems[ rank - 1 ] = retVal;
    for ( size_t ii = 0; ii < fItems.size(); ++ii )
    {
        if ( !fItems[ ii ] )
        {
            auto tmp = std::make_shared< SMediaCollectionData >( nullptr, parent );
            fItems[ ii ] = tmp;
        }
    }
    return retVal;
}

QVariant SMediaCollectionData::data( int column, int role ) const
{
    if ( !fData )
        return {};

    if ( role == Qt::DisplayRole )
    {
        switch ( column )
        {
            case 0:
                return fData->name();
            case 1:
                return fData->premiereDate().year();
            case 3:
                return fData->onServer() ? QString() : QObject::tr( "Yes" );
        }
    }
    return {};
}

bool SMediaCollectionData::updateMedia( std::shared_ptr< CMediaModel > mediaModel )
{
    if ( fData->onServer() )
        return false;

    auto data = mediaModel->findMedia( fData->name(), fData->premiereDate().year() );
    if ( data )
    {
        fData = data;
        return true;
    }
    return false;
}

namespace NJSON
{
    std::optional< std::shared_ptr< CCollections > > CCollections::fromJSON( const QString &fileName, QString *msg /*= nullptr */ )
    {
        if ( fileName.isEmpty() )
        {
            if ( msg )
                *msg = QString( "Filename is empty" );
            return {};
        }

        QFile fi( fileName );
        if ( !fi.open( QFile::ReadOnly ) )
        {
            if ( msg )
                *msg = QString( "Could not open file '%1', please chek permissions" ).arg( fileName );

            return {};
        }

        auto data = fi.readAll();
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson( data, &error );
        if ( error.error != QJsonParseError::NoError )
        {
            if ( msg )
                *msg = QString( "Error: %1 @ %2" ).arg( error.errorString() ).arg( error.offset );
            return {};
        }

        if ( !doc.isObject() )
        {
            if ( msg )
                *msg = QString( "Error: Top level item should be object" );
            return {};
        }

        auto root = doc.object();
        auto collectionsObj = root[ "collections" ];
        auto moviesObj = root[ "movies" ];
        if ( !collectionsObj.isArray() && !moviesObj.isArray() )
        {
            if ( msg )
                *msg = QString( "Error: Top level item should contain an array called movies or collections" );
            return {};
        }

        if ( moviesObj.isArray() )
        {
            QJsonObject collectionObj;
            collectionObj[ "collection" ] = "<Unnamed Collection>";
            collectionObj[ "movies" ] = moviesObj;
            auto tmp = QJsonArray();
            tmp.push_back( collectionObj );
            collectionsObj = tmp;
        }
        QJsonArray collections;
        if ( !collectionsObj.isArray() )
        {
            if ( msg )
                *msg = QString( "Error: Top level item 'collections' should be an array" );
            return {};
        }

        collections = collectionsObj.toArray();
        auto retVal = std::make_shared< CCollections >();

        for ( auto &&curr : collections )
        {
            auto collection = std::make_shared< CCollection >( curr );
            retVal->fCollections.push_back( collection );
            auto movies = collection->movies();
            retVal->fMovies.insert( retVal->fMovies.end(), movies.begin(), movies.end() );
        }

        return retVal;
    }

    CMovie::CMovie( const QJsonValue &curr )
    {
        fRank = curr.toObject()[ "rank" ].toInt();
        if ( !curr.toObject().contains( "rank" ) )
            fRank = -1;
        fName = curr.toObject()[ "name" ].toString();
        fYear = curr.toObject()[ "year" ].toInt();
        //Q_ASSERT( hasYear() );
        if ( curr.toObject().contains( "width" ) && curr.toObject().contains( "height" ) )
            fResolution = { curr.toObject()[ "width" ].toInt(), curr.toObject()[ "width" ].toInt() };
        else if ( curr.toObject().contains( "type" ) )
        {
            auto type = curr.toObject()[ "type" ].toString().toLower();
            if ( type == "uhd" )
                fResolution = { 3840, 2160 };
            else if ( type == "dvd" )
                fResolution = { 720, 480 };
            else if ( type.isEmpty() )
                fResolution = { 1920, 1080 };
        }
    }

    CCollection::CCollection( const QJsonValue &curr )
    {
        fName = curr.toObject()[ "collection" ].toString();
        auto movies = curr.toObject()[ "movies" ].toArray();
        for ( auto &&curr : movies )
        {
            auto movie = std::make_shared< CMovie >( curr );
            fMovies.push_back( movie );
        }
    }

}