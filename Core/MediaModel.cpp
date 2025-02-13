#include "MediaModel.h"
#include "MediaData.h"
#include "MergeMedia.h"
#include "MovieStub.h"

#include "Settings.h"
#include "ServerInfo.h"
#include "ServerModel.h"
#include "SABUtils/StringUtils.h"
#include "ProgressSystem.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include <QColor>
#include <QInputDialog>
#include <QTimer>

#include <optional>
#include <vector>
#include <unordered_map>

CMediaModel::CMediaModel( std::shared_ptr< CSettings > settings, std::shared_ptr< CServerModel > serverModel, QObject *parent ) :
    QAbstractTableModel( parent ),
    fSettings( settings ),
    fServerModel( serverModel ),
    fMergeSystem( new CMergeMedia )
{
    connect( this, &CMediaModel::dataChanged, this, &CMediaModel::sigMediaChanged );
    connect( this, &CMediaModel::modelReset, this, &CMediaModel::sigMediaChanged );
}

int CMediaModel::rowCount( const QModelIndex &parent /* = QModelIndex() */ ) const
{
    if ( parent.isValid() )
        return 0;

    return static_cast< int >( fData.size() );
}

int CMediaModel::columnsPerServer( bool includeProviders ) const
{
    auto retVal = static_cast< int >( eFirstServerColumn ) + 1;
    if ( includeProviders )
        retVal += static_cast< int >( fProviderNames.size() );
    return retVal;
}

int CMediaModel::columnCount( const QModelIndex &parent /* = QModelIndex() */ ) const
{
    if ( parent.isValid() )
        return 0;
    auto retVal = fServerModel->serverCnt() * columnsPerServer();
    return retVal;
}

std::list< int > CMediaModel::columnsForBaseColumn( int baseColumn ) const
{
    std::list< int > retVal;

    auto providerInfo = getProviderInfoForColumn( baseColumn );
    if ( providerInfo )
        return { baseColumn };

    auto curr = baseColumn;
    while ( curr <= columnCount() )
    {
        retVal.push_back( curr );
        curr += columnsPerServer( false );
    }
    return retVal;
}

QString CMediaModel::serverForColumn( int column ) const
{
    auto providerInfo = getProviderInfoForColumn( column );
    if ( providerInfo )
        return providerInfo.value().first;

    auto serverNum = column / columnsPerServer( false );
    return fServerModel->getServerInfo( serverNum )->keyName();
}

std::list< int > CMediaModel::providerColumns() const
{
    std::list< int > retVal;
    for ( auto &&ii : fProviderColumnsByColumn )
        retVal.push_back( ii.first );
    return retVal;
}

std::optional< std::pair< QString, QString > > CMediaModel::getProviderInfoForColumn( int column ) const
{
    auto pos = fProviderColumnsByColumn.find( column );
    if ( pos != fProviderColumnsByColumn.end() )
        return ( *pos ).second;

    return {};
}

bool CMediaModel::hasMediaToProcess() const
{
    for ( auto &&ii : fData )
    {
        if ( !ii->canBeSynced() )
            continue;
        if ( !ii->validUserDataEqual() )
            return true;
    }
    return false;
}

QVariant CMediaModel::data( const QModelIndex &index, int role /*= Qt::DisplayRole */ ) const
{
    if ( !index.isValid() || index.parent().isValid() || ( index.row() >= rowCount() ) )
        return {};

    auto mediaData = fData[ index.row() ];

    if ( role == ECustomRoles::eMediaNameRole )
        return mediaData->name();
    if ( role == ECustomRoles::eDirSortRole )
        return static_cast< int >( fDirSort );
    if ( role == ECustomRoles::ePremiereDateRole )
        return mediaData->premiereDate();
    if ( role == ECustomRoles::eResolutionRole )
        return QPoint( mediaData->resolutionValue().first, mediaData->resolutionValue().second );
    if ( role == ECustomRoles::eColumnsPerServerRole )
        return columnsPerServer( false );
    if ( role == ECustomRoles::ePerServerColumnRole )
        return perServerColumn( index.column() );
    if ( role == ECustomRoles::eIsProviderColumnRole )
    {
        auto providerInfo = getProviderInfoForColumn( index.column() );
        return providerInfo.has_value();
    }

    if ( role == ECustomRoles::eShowInSearchMovieRole )
    {
        auto column = perServerColumn( index.column() );
        switch ( column )
        {
            case eName:
            case ePremiereDate:
            case eResolution:
                return true;
            default:
                return false;
        }
    }
    if ( role == ECustomRoles::eShowItemRole )
    {
        if ( !mediaData )
            return false;

        auto mediaSyncStatus = mediaData->syncStatus();
        if ( mediaSyncStatus == EMediaSyncStatus::eNoServerPairs )
            return fSettings->showMediaWithIssues();
        if ( mediaSyncStatus == EMediaSyncStatus::eMediaEqualOnValidServers )
            return !fSettings->onlyShowMediaWithDifferences();
        else if ( mediaSyncStatus == EMediaSyncStatus::eMediaNeedsUpdating )
            return true;
        return false;
    }
    if ( role == ECustomRoles::eSeriesNameRole )
    {
        return mediaData->seriesName();
    }
    if ( role == ECustomRoles::eSeasonNumRole )
    {
        return mediaData->season().has_value() ? mediaData->season().value() : QVariant();
    }
    if ( role == ECustomRoles::eEpisodeNumRole )
    {
        return mediaData->episode().has_value() ? mediaData->episode().value() : QVariant();
    }
    if ( role == ECustomRoles::eOnServerRole )
    {
        return mediaData->onServer();
    }

    int column = index.column();
    auto serverName = this->serverForColumn( column );

    // reverse for black background
    if ( role == Qt::ForegroundRole )
    {
        auto color = getColor( index, serverName, false );
        if ( !color.isValid() )
            return {};
        return color;
    }

    if ( role == Qt::BackgroundRole )
    {
        auto color = getColor( index, serverName, true );
        if ( !color.isValid() )
            return {};
        return color;
    }

    if ( ( role == Qt::DecorationRole ) && ( perServerColumn( column ) == eName ) )
    {
        return mediaData->getDirectionIcon( serverName );
    }

    if ( role != Qt::DisplayRole )
        return {};

    auto providerInfo = getProviderInfoForColumn( column );
    if ( providerInfo )
    {
        return mediaData->getProviderID( providerInfo.value().second );
    }

    bool isValid = mediaData->isValidForServer( serverName );

    switch ( perServerColumn( column ) )
    {
        case eName:
            return isValid ? mediaData->name() : tr( "%1 - <Missing from Server>" ).arg( mediaData->name() );
        case eType:
            return mediaData->mediaType();
        case ePremiereDate:
            {
                if ( ( mediaData->premiereDate().month() ) == 1 && ( mediaData->premiereDate().day() == 1 ) )
                    return mediaData->premiereDate().year();
                else
                    return mediaData->premiereDate();
            }
        case eMediaID:
            return isValid ? mediaData->getMediaID( serverName ) : QString();
        case eFavorite:
            return isValid ? ( mediaData->isFavorite( serverName ) ? "Yes" : "No" ) : QString();
        case ePlayed:
            return isValid ? ( mediaData->isPlayed( serverName ) ? "Yes" : "No" ) : QString();
        case eLastPlayed:
            return isValid ? ( mediaData->lastPlayed( serverName ).toString() ) : QString();
        case ePlayCount:
            return isValid ? QString::number( mediaData->playCount( serverName ) ) : QString();
        case ePlaybackPosition:
            return isValid ? mediaData->playbackPosition( serverName ) : QString();
        case eResolution:
            return isValid ? ( mediaData->resolution() ) : QString();
        case eIsMissing:
            return isValid ? ( mediaData->isMissing() ? "Yes" : "No" ) : QString();
        default:
            return {};
    }

    return {};
}

int CMediaModel::perServerColumn( int column ) const
{
    if ( getProviderInfoForColumn( column ) )
        return -1;

    return column % columnsPerServer( false );
}

void CMediaModel::addMediaInfo( const QString &serverName, std::shared_ptr< CMediaData > mediaData, const QJsonObject &mediaInfo )
{
    mediaData->loadData( serverName, mediaInfo );

    fMediaMap[ serverName ][ mediaData->getMediaID( serverName ) ] = mediaData;

    fMergeSystem->addMediaInfo( serverName, mediaData );
    updateMediaData( mediaData );
}

void CMediaModel::updateMediaData( std::shared_ptr< CMediaData > mediaData )
{
    auto pos = fMediaToPos.find( mediaData );
    if ( pos == fMediaToPos.end() )
        return;

    int row = static_cast< int >( ( *pos ).second );
    emit dataChanged( index( row, 0 ), index( row, columnCount() - 1 ) );
}

void CMediaModel::beginBatchLoad()
{
    beginResetModel();
}

void CMediaModel::endBatchLoad()
{
    endResetModel();
}

QVariant CMediaModel::headerData( int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole */ ) const
{
    if ( section < 0 || section > columnCount() )
        return QAbstractTableModel::headerData( section, orientation, role );
    if ( orientation != Qt::Horizontal )
        return QAbstractTableModel::headerData( section, orientation, role );

    if ( role != Qt::DisplayRole )
        return QAbstractTableModel::headerData( section, orientation, role );

    QString retVal;
    int columnNum = -1;
    auto providerInfo = getProviderInfoForColumn( section );
    if ( providerInfo )
        retVal = providerInfo.value().second;
    else
    {
        columnNum = perServerColumn( section );

        switch ( columnNum )
        {
            case eName:
                retVal = "Name";
                break;
            case eType:
                retVal = "Media Type";
                break;
            case ePremiereDate:
                retVal = "Premiere Date";
                break;
            case eMediaID:
                retVal = "ID on Server";
                break;
            case eFavorite:
                retVal = "Is Favorite?";
                break;
            case ePlayed:
                retVal = "Played?";
                break;
            case eLastPlayed:
                retVal = "Last Played";
                break;
            case ePlayCount:
                retVal = "Play Count";
                break;
            case ePlaybackPosition:
                retVal = "Play Position";
                break;
            case eResolution:
                retVal = "Resolution";
                break;
            case eIsMissing:
                retVal = "Missing?";
                break;
        };
    }
    // retVal = QString( "%1 - %2 - %3" ).arg( retVal ).arg( columnNum ).arg( section );
    return retVal;
}

void CMediaModel::clear()
{
    beginResetModel();

    fMergeSystem->clear();
    fAllMedia.clear();
    fMediaMap.clear();
    // fCollections.clear();
    fData.clear();
    fDataMap.clear();
    fMediaToPos.clear();
    fProviderNames.clear();
    fProviderColumnsByColumn.clear();
    fDirSort = eNoSort;

    endResetModel();
}

void CMediaModel::updateProviderColumns( std::shared_ptr< CMediaData > mediaData )
{
    for ( auto &&ii : mediaData->getProviders() )
    {
        int colCount = this->columnCount();
        auto pos = fProviderNames.find( ii.first );
        if ( pos == fProviderNames.end() )
        {
            auto serverModel = fServerModel;
            beginInsertColumns( QModelIndex(), colCount, colCount + serverModel->serverCnt() - 1 );
            fProviderNames.insert( ii.first );
            for ( int jj = 0; jj < serverModel->serverCnt(); ++jj )
            {
                fProviderColumnsByColumn[ colCount + jj ] = { serverModel->getServerInfo( jj )->keyName(), ii.first };   // gets duplicated lhs vs rhs
            }
            endInsertColumns();
        }
    }
}

void CMediaModel::settingsChanged()
{
    beginResetModel();
    endResetModel();
    emit sigSettingsChanged();
}

std::shared_ptr< CMediaData > CMediaModel::getMediaData( const QModelIndex &idx ) const
{
    if ( !idx.isValid() )
        return {};
    if ( idx.model() != this )
        return {};

    return fData[ idx.row() ];
}

std::shared_ptr< CMediaData > CMediaModel::getMediaDataForID( const QString &serverName, const QString &mediaID ) const
{
    auto pos = fMediaMap.find( serverName );
    if ( pos == fMediaMap.end() )
        return {};

    auto pos2 = ( *pos ).second.find( mediaID );
    if ( pos2 != ( *pos ).second.end() )
        return ( *pos2 ).second;
    return {};
}

std::shared_ptr< CMediaData > CMediaModel::loadMedia( const QString &serverName, const QJsonObject &media )
{
    qDebug().nospace().noquote() << QJsonDocument( media ).toJson();

    std::shared_ptr< CMediaData > mediaData;

    auto pos = fMediaMap.find( serverName );
    if ( pos == fMediaMap.end() )
        pos = fMediaMap.insert( std::make_pair( serverName, TMediaIDToMediaData() ) ).first;

    auto seasonID = media[ "ParentIndexNumber" ].toInt();
    auto episodeID = media[ "IndexNumber" ].toInt();
    auto id = QString( "S%1E%2" ).arg( seasonID, 2, 10, QChar( '0' ) ).arg( episodeID, 2, 10, QChar( '0' ) );

    if ( !media.contains( "ParentIndexNumber" ) || !media.contains( "IndexNumber" ) )
        id = QString::number( ( *pos ).second.size() );

    auto pos2 = ( *pos ).second.find( id );
    if ( pos2 == ( *pos ).second.end() )
        mediaData = std::make_shared< CMediaData >( media, fServerModel );
    else
        mediaData = ( *pos2 ).second;
    // qDebug() << isLHSServer << mediaData->name();

    mediaData->setMediaID( serverName, id );

    /*
    "UserData": {
    "IsFavorite": false,
    "LastPlayedDate": "2022-01-15T20:28:39.0000000Z",
    "PlayCount": 1,
    "PlaybackPositionTicks": 0,
    "Played": true
    }
    */

    addMediaInfo( serverName, mediaData, media );
    return mediaData;
}

void CMediaModel::removeMedia( const QString &serverName, const std::shared_ptr< CMediaData > &mediaData )
{
    auto pos = fMediaMap.find( serverName );
    if ( pos != fMediaMap.end() )
    {
        auto mediaID = mediaData->getMediaID( serverName );
        auto pos2 = ( *pos ).second.find( mediaID );
        if ( pos2 != ( *pos ).second.end() )
            ( *pos ).second.erase( pos2 );

        auto pos3 = fMediaToPos.find( mediaData );
        if ( pos3 != fMediaToPos.end() )
            fMediaToPos.erase( pos3 );
    }
    fMergeSystem->removeMedia( serverName, mediaData );
}

std::shared_ptr< CMediaData > CMediaModel::reloadMedia( const QString &serverName, const QJsonObject &media, const QString &mediaID )
{
    auto mediaData = getMediaDataForID( serverName, mediaID );
    if ( !mediaData )
        return {};

    if ( media.find( "UserData" ) == media.end() )
        return {};

    addMediaInfo( serverName, mediaData, media );
    //emit sigPendingMediaUpdate();
    return mediaData;
}

bool CMediaModel::mergeMedia( std::shared_ptr< CProgressSystem > progressSystem )
{
    if ( fMergeSystem->merge( progressSystem ) )
    {
        std::tie( fAllMedia, fMediaMap ) = fMergeSystem->getMergedData( progressSystem );

        loadMergedMedia( progressSystem );
    }
    else
    {
        clear();
    }
    return !progressSystem->wasCanceled();
}

void CMediaModel::loadMergedMedia( std::shared_ptr< CProgressSystem > progressSystem )
{
    progressSystem->pushState();
    progressSystem->setTitle( tr( "Loading merged media data" ) );
    progressSystem->setMaximum( static_cast< int >( fAllMedia.size() ) );
    progressSystem->setValue( 0 );
    beginResetModel();
    fData.reserve( fAllMedia.size() );
    for ( auto &&ii : fAllMedia )
    {
        addMedia( ii, false );
    }
    endResetModel();
    progressSystem->popState();
}

void CMediaModel::addMedia( const std::shared_ptr< CMediaData > &media, bool emitUpdate )
{
    if ( emitUpdate )
        beginInsertRows( QModelIndex(), static_cast< int >( fData.size() ), static_cast< int >( fData.size() ) );
    fMediaToPos[ media ] = fData.size();
    fData.push_back( media );
    fDataMap[ SMovieStub::nameKey( media->name() ) ] = media;
    fDataMap[ SMovieStub::nameKey( media->originalTitle() ) ] = media;
    updateProviderColumns( media );
    if ( emitUpdate )
        endInsertRows();
}

void CMediaModel::removeMovieStub( const SMovieStub &movieStub )
{
    auto pos = fDataMap.find( movieStub.nameKey() );
    if ( pos == fDataMap.end() )
        return;

    removeMovieStub( ( *pos ).second );
}

void CMediaModel::removeMovieStub( const std::shared_ptr< CMediaData > &media )
{
    beginResetModel();
    if ( media->onServer() )
        return;

    auto it = std::remove_if( fData.begin(), fData.end(), [ media ]( std::shared_ptr< CMediaData > data ) { return data == media; } );
    fData.erase( it, fData.end() );

    auto pos = fDataMap.find( SMovieStub::nameKey( media->name() ) );
    if ( pos != fDataMap.end() )
        fDataMap.erase( pos );

    auto pos2 = fMediaToPos.find( media );
    if ( pos2 != fMediaToPos.end() )
        fMediaToPos.erase( pos2 );
    endResetModel();
}

void CMediaModel::clearAllMovieStubs()
{
    beginResetModel();
    auto it = std::remove_if( fData.begin(), fData.end(), []( std::shared_ptr< CMediaData > data ) { return !data->onServer(); } );
    fData.erase( it, fData.end() );

    for ( auto ii = fDataMap.begin(); ii != fDataMap.end(); )
    {
        if ( !( *ii ).second->onServer() )
        {
            ii = fDataMap.erase( ii );
            continue;
        }
        ++ii;
    }

    for ( auto ii = fMediaToPos.begin(); ii != fMediaToPos.end(); )
    {
        if ( !( *ii ).first->onServer() )
        {
            ii = fMediaToPos.erase( ii );
            continue;
        }
        ++ii;
    }
    endResetModel();
}

void CMediaModel::addMovieStub( const SMovieStub &movieStub, std::function< bool( std::shared_ptr< CMediaData > mediaData ) > equal )
{
    auto pos = fDataMap.find( movieStub.nameKey() );
    if ( pos != fDataMap.end() )
    {
        if ( equal( ( *pos ).second ) )
            return;
    }

    for ( auto &&ii : fData )
    {
        if ( equal( ii ) )
            return;
    }

    auto mediaData = std::make_shared< CMediaData >( movieStub, "Movie" );
    addMedia( mediaData, true );
}

std::set< QString > CMediaModel::getKnownShows() const
{
    std::set< QString > knownShows;
    for ( auto &&ii : fAllMedia )
    {
        if ( ii->mediaType() != "Episode" )
            continue;
        auto name = ii->seriesName();
        if ( name.isEmpty() )
            continue;

        knownShows.insert( ii->seriesName() );
    }
    return knownShows;
}

std::shared_ptr< CMediaData > CMediaModel::findMedia( const QString &name, int year ) const
{
    for ( auto &&ii : fAllMedia )
    {
        if ( ii->isMatch( name, year ) )
            return ii;
    }
    return {};
}

QVariant CMediaModel::getColor( const QModelIndex &index, const QString &serverName, bool background ) const
{
    if ( !index.isValid() )
        return {};

    if ( index.column() > fServerModel->serverCnt() * columnsPerServer( false ) )
        return {};
    auto mediaData = fData[ index.row() ];

    if ( !mediaData->isValidForServer( serverName ) )
    {
        switch ( perServerColumn( index.column() ) )
        {
            case eName:
            case eType:
                return {};
            case eMediaID:
            case eFavorite:
            case ePlayed:
            case eLastPlayed:
            case ePlayCount:
            case ePlaybackPosition:
            case eResolution:
                return fSettings->dataMissingColor( background );
                break;
            default:
                return {};
        }
    }

    if ( !mediaData->validUserDataEqual() )
    {
        bool dataSame = false;

        switch ( perServerColumn( index.column() ) )
        {
            case eName:
                dataSame = !mediaData->canBeSynced();
                break;
            case eFavorite:
                dataSame = mediaData->allFavoriteEqual();
                break;
            case ePlayed:
                dataSame = mediaData->allPlayedEqual();
                break;
            case eLastPlayed:
                dataSame = mediaData->allLastPlayedEqual();
                break;
            case ePlayCount:
                dataSame = mediaData->allPlayCountEqual();
                break;
            case ePlaybackPosition:
                dataSame = mediaData->allPlaybackPositionTicksEqual();
                break;
            case eMediaID:
            default:
                return {};
        }

        if ( dataSame )
            return {};

        auto older = fSettings->mediaDestColor( background );
        auto newer = fSettings->mediaSourceColor( background );
        auto serverName = this->serverForColumn( index.column() );

        auto isOlder = mediaData->needsUpdating( serverName );

        return isOlder ? older : newer;
    }
    return {};
}

SMediaSummary::SMediaSummary( std::shared_ptr< CMediaModel > model )
{
    auto serverModel = model->fServerModel;
    for ( auto &&ii : model->fData )
    {
        fTotalMedia++;
        for ( auto &&serverInfo : *serverModel )
        {
            if ( !serverInfo->isEnabled() )
                continue;

            if ( !ii->isValidForServer( serverInfo->keyName() ) )
            {
                fMissingData[ serverInfo->displayName() ]++;
                continue;
            }

            if ( ii->needsUpdating( serverInfo->keyName() ) )
            {
                fNeedsUpdating[ serverInfo->displayName() ]++;
            }
        }
    }
}

QString SMediaSummary::getSummaryText() const
{
    QStringList allMsgs = { QObject::tr( "%1 Total Items" ).arg( fTotalMedia ) };

    QStringList missingStringList;
    for ( auto &&ii : fMissingData )
    {
        missingStringList << QObject::tr( "%1 from %2" ).arg( ii.second ).arg( ii.first );
    }
    auto missingString = missingStringList.join( ", " );
    if ( !missingString.isEmpty() )
    {
        allMsgs << QObject::tr( "Missing Media: %1" ).arg( missingString );
    }

    QStringList mediaUpdateStringList;
    for ( auto &&ii : fNeedsUpdating )
    {
        mediaUpdateStringList << QObject::tr( "%1 from %2" ).arg( ii.second ).arg( ii.first );
    }
    auto mediaUpdateString = mediaUpdateStringList.join( ", " );
    if ( !mediaUpdateString.isEmpty() )
        allMsgs << QObject::tr( "Media Needing Update: %1" ).arg( mediaUpdateString );

    auto retVal = QObject::tr( "Media Summary: %1" ).arg( allMsgs.join( ", " ) );

    return retVal;
}

CMediaFilterModel::CMediaFilterModel( QObject *parent ) :
    QSortFilterProxyModel( parent )
{
    setDynamicSortFilter( false );
}

bool CMediaFilterModel::filterAcceptsRow( int source_row, const QModelIndex &source_parent ) const
{
    if ( !sourceModel() )
        return true;
    auto childIdx = sourceModel()->index( source_row, 0, source_parent );
    return childIdx.data( CMediaModel::eShowItemRole ).toBool();
}

void CMediaFilterModel::sort( int column, Qt::SortOrder order /*= Qt::AscendingOrder */ )
{
    QSortFilterProxyModel::sort( column, order );
}

bool CMediaFilterModel::lessThan( const QModelIndex &source_left, const QModelIndex &source_right ) const
{
    return QSortFilterProxyModel::lessThan( source_left, source_right );
}

CMediaMissingFilterModel::CMediaMissingFilterModel( std::shared_ptr< CSettings > settings, QObject *parent ) :
    QSortFilterProxyModel( parent ),
    fSettings( settings )
{
    fRegEx = fSettings->ignoreShowRegEx();
    setDynamicSortFilter( false );
    connect(
        this, &QSortFilterProxyModel::sourceModelChanged,
        [ this ]()
        {
            connect(
                dynamic_cast< CMediaModel * >( sourceModel() ), &CMediaModel::sigSettingsChanged,
                [ this ]()
                {
                    fRegEx = fSettings->ignoreShowRegEx();
                    invalidateFilter();
                } );
        } );
}

void CMediaMissingFilterModel::setShowFilter( const std::list< std::shared_ptr< SShowFilter > > &filter )
{
    fShowFilter.clear();
    for (auto&& ii : filter)
    {
        fShowFilter[ ii->fSeriesName ] = ii;
    }
    auto col = sortColumn();
    auto order = sortOrder();
    invalidateFilter();
    sort( col, order );
}

bool CMediaMissingFilterModel::filterAcceptsRow( int source_row, const QModelIndex &source_parent ) const
{
    if ( !sourceModel() )
        return true;
    auto childIdx = sourceModel()->index( source_row, 0, source_parent );
    auto seriesName = childIdx.data( CMediaModel::eSeriesNameRole ).toString();
    if ( seriesName.isEmpty() )
        return false;

    if ( !fShowFilter.empty() )
    {
        auto pos = fShowFilter.find( seriesName );
        if ( pos == fShowFilter.end() )
            return false;

        auto &&filter = ( *pos ).second;
        if ( !filter->fEnabled )
            return false;

        int seasonNum = childIdx.data( CMediaModel::eSeasonNumRole ).toInt();
        if ( filter->fMinSeason.has_value() )
        {
            if ( seasonNum < filter->fMinSeason.value() )
                return false;
        }

        if ( filter->fMaxSeason.has_value() )
        {
            if ( seasonNum > filter->fMaxSeason.value() )
                return false;
        }
    }

    if ( fRegEx.has_value() && fRegEx.value().isValid() )
    {
        auto match = fRegEx.value().match( seriesName );
        bool isMatch = ( match.hasMatch() && ( match.captured( 0 ).length() == seriesName.length() ) );
        if ( !isMatch )
            return false;
    }

    return true;
}

bool CMediaMissingFilterModel::filterAcceptsColumn( int source_column, const QModelIndex &source_parent ) const
{
    auto idx = sourceModel()->index( 0, source_column, source_parent );
    return !idx.data( CMediaModel::ECustomRoles::eIsProviderColumnRole ).toBool();
}

void CMediaMissingFilterModel::sort( int column, Qt::SortOrder order /*= Qt::AscendingOrder */ )
{
    QSortFilterProxyModel::sort( column, order );
}

bool CMediaMissingFilterModel::lessThan( const QModelIndex &source_left, const QModelIndex &source_right ) const
{
    return QSortFilterProxyModel::lessThan( source_left, source_right );
}

QVariant CMediaMissingFilterModel::data( const QModelIndex &index, int role /*= Qt::DisplayRole */ ) const
{
    if ( ( role != Qt::ForegroundRole ) && ( role != Qt::BackgroundRole ) )
        return QSortFilterProxyModel::data( index, role );

    auto premiereDate = index.data( CMediaModel::ECustomRoles::ePremiereDateRole ).toDate();
    if ( premiereDate > QDate::currentDate() )
        return {};

    // reverse for black background
    if ( role == Qt::ForegroundRole )
    {
        auto color = fSettings->dataMissingColor( false );
        if ( !color.isValid() )
            return {};
        return color;
    }

    if ( role == Qt::BackgroundRole )
    {
        auto color = fSettings->dataMissingColor( true );
        if ( !color.isValid() )
            return {};
        return color;
    }
    return {};
}
