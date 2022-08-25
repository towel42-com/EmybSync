// The MIT License( MIT )
//
// Copyright( c ) 2022 Scott Aron Bloom
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sub-license, and/or sell
// copies of the Software, and to permit persons to whom the Software is
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

#ifndef __SYNCSYSTEM_H
#define __SYNCSYSTEM_H

#include <QObject>
#include <QMap>
#include <QDateTime>
#include <QNetworkRequest>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <functional>

#include "SABUtils/HashUtils.h"

#include <memory>
#include <set>

class CUsersModel;
class CMediaModel;

class QNetworkReply;
class QAuthenticator;
class QSslPreSharedKeyAuthenticator;
class QNetworkProxy;
class QSslError;
class QNetworkAccessManager;
class CUserData;
class CMediaData;
struct SMediaUserData;
class CSettings;
class CProgressSystem;
class QTimer;
class CServerInfo;

enum class ERequestType
{
    eNone,
    eGetUsers,
    eGetMediaList,
    eReloadMediaData,
    eUpdateData,
    eUpdateFavorite,
    eTestServer,
    eDeleteConnectedID,
    eSetConnectedID
};

QString toString( ERequestType request );

struct SServerReplyInfo
{
    void clear()
    {
        fType = ERequestType::eNone;
        fReply = nullptr;
        fExtraData.clear();
    }

    ERequestType fType{ ERequestType::eNone };
    QNetworkReply * fReply{ nullptr };
    QString fExtraData;
};

constexpr int kServerName = QNetworkRequest::User + 1; // QString
constexpr int kRequestType = QNetworkRequest::User + 2; // ERequestType
constexpr int kExtraData = QNetworkRequest::User + 3; // QVariant

using TMediaIDToMediaData = std::map< QString, std::shared_ptr< CMediaData > >;
enum EMsgType
{
    eError,
    eWarning,
    eInfo
};

QString toString( EMsgType type );
QString createMessage( EMsgType msgType, const QString & msg );

class CSyncSystem : public QObject
{
    Q_OBJECT
public:
    CSyncSystem( std::shared_ptr< CSettings > settings, QObject * parent = nullptr );

    void setProcessNewMediaFunc( std::function< void( std::shared_ptr< CMediaData > userData ) > processMediaFunc );
    void setUserMsgFunc( std::function< void( const QString & title, const QString & msg, bool isCritical ) > userMsgFunc );
    void setProgressSystem( std::shared_ptr< CProgressSystem > funcs );
    void setLoadUserFunc( std::function< std::shared_ptr< CUserData >( const QString & serverName, const QJsonObject & user ) > loadUserFunc );
    void setUpdateUserConnectIDFunc( std::function< void( const QString & serverName, const QString & userID, const QString & connectID ) > updateUserConnectID );
    void setLoadMediaFunc( std::function< std::shared_ptr< CMediaData >( const QString & serverName, const QJsonObject & media ) > loadMediaFunc );
    void setReloadMediaFunc( std::function< std::shared_ptr< CMediaData >( const QString & serverName, const QJsonObject & media, const QString & itemID ) > reloadMediaFunc );
    void setGetMediaDataForIDFunc( std::function< std::shared_ptr< CMediaData >( const QString & serverName, const QString & mediaID ) > getMediaDataForIDFunc );
    void setMergeMediaFunc( std::function< bool( std::shared_ptr< CProgressSystem > progressSystem ) > mergeMediaFunc );
    void setGetAllMediaFunc( std::function< std::unordered_set< std::shared_ptr< CMediaData > >() > getAllMediaFunc );

    void testServers( const std::vector< std::shared_ptr< const CServerInfo > > & serverInfo );
    void testServer( std::shared_ptr< const CServerInfo > serverInfo );
    void testServer( const QString & serverName );

    bool isRunning() const;

    void loadUsers();

    void reset();

    void loadUsersMedia( std::shared_ptr< CUserData > user );
    void clearCurrUser();

    std::shared_ptr< CUserData > currUser() const;

    void updateUserDataForMedia( const QString & serverName, std::shared_ptr<CMediaData> mediaData, std::shared_ptr<SMediaUserData> newData );

    void selectiveProcess( const QString & selectedServer );
Q_SIGNALS:
    void sigAddToLog( int msgType, const QString & msg );
    void sigLoadingUsersFinished();
    void sigUserMediaLoaded();
    void sigProcessingFinished( const QString & name );
    void sigTestServerResults( const QString & serverName, bool results, const QString & msg );
public Q_SLOTS:
    void slotProcess();
    void slotCanceled();
private:
    bool processMedia( std::shared_ptr< CMediaData > mediaData, const QString & selectedServer );

    void setServerName( QNetworkReply * reply, const QString & serverName );
    QString serverName( QNetworkReply * reply );

    void setRequestType( QNetworkReply * reply, ERequestType requestType );
    QString hostName( QNetworkReply * reply );

    ERequestType requestType( QNetworkReply * reply );

    void setExtraData( QNetworkReply * reply, QVariant extraData );
    QVariant extraData( QNetworkReply * reply );

private Q_SLOTS:
    void slotRequestFinished( QNetworkReply * reply );
    void postHandleRequest( QNetworkReply * reply, const QString & serverName, ERequestType requestType );
    void decRequestCount( QNetworkReply * reply, ERequestType requestType );

    void slotMergeMedia();

    void slotAuthenticationRequired( QNetworkReply * reply, QAuthenticator * authenticator );
    void slotEncrypted( QNetworkReply * reply );
    void slotPreSharedKeyAuthenticationRequired( QNetworkReply * reply, QSslPreSharedKeyAuthenticator * authenticator );
    void slotProxyAuthenticationRequired( const QNetworkProxy & proxy, QAuthenticator * authenticator );
    void slotSSlErrors( QNetworkReply * reply, const QList<QSslError> & errors );

    void slotCheckPendingRequests();
private:
    QNetworkReply * makeRequest( const QNetworkRequest & request );

    std::shared_ptr<CUserData> loadUser( const QString & serverName, const QJsonObject & user );
    std::shared_ptr< CMediaData> loadMedia( const QString & serverName, const QJsonObject & media );

    bool isLastRequestOfType( ERequestType type ) const;

    bool handleError( QNetworkReply * reply, const QString & serverName, QString & errorMsg, bool reportMsg );

    void requestGetUsers( const QString & serverName );
    void handleGetUsersResponse( const QString & serverName, const QByteArray & data );

    void requestGetMediaList( const QString & serverNameServer );
    void handleGetMediaListResponse( const QString & serverName, const QByteArray & data );

    void requestReloadMediaItemData( const QString & serverName, const QString & mediaID );
    void requestReloadMediaItemData( const QString & serverName, std::shared_ptr< CMediaData > mediaData );
    void requestSetFavorite( const QString & serverName, std::shared_ptr< CMediaData > mediaData, std::shared_ptr< SMediaUserData > newData );
    void requestUpdateUserDataForMedia( const QString & serverName, std::shared_ptr< CMediaData > mediaData, std::shared_ptr< SMediaUserData > newData );
    void handleReloadMediaResponse( const QString & serverName, const QByteArray & data, const QString & id );

    void requestTestServer( std::shared_ptr< const CServerInfo > serverInfo );

    void requestDeleteConnectedID( const QString & serverName, std::shared_ptr< CUserData > user );
    void handleDeleteConnectedID( const QString & serverName, const QString & userName, const QString & userID, const QString & connectID, const QByteArray & data );

    void requestSetConnectedID( const QString & serverName, std::shared_ptr< CUserData > user );
    void requestSetConnectedID( const QString & serverName, const QString & userName, const QString & userID, const QString & connectID );
    void handleSetConnectedID( const QString & serverName, const QString & userID, const QString & connectID, const QByteArray & data );

    std::shared_ptr< CSettings > fSettings;

    QNetworkAccessManager * fManager{ nullptr };

    QTimer * fPendingRequestTimer{ nullptr };

    std::unordered_map< ERequestType, std::unordered_map< QString, int > > fRequests; // request type -> host -> count
    std::unordered_map< QNetworkReply *, std::unordered_map< int, QVariant > > fAttributes;

    std::function< std::shared_ptr< CUserData >( const QString & serverName, const QJsonObject & user ) > fLoadUserFunc;
    std::function< std::shared_ptr< CMediaData >( const QString & serverName, const QJsonObject & media ) > fLoadMediaFunc;
    std::function< std::shared_ptr< CMediaData >( const QString & serverName, const QJsonObject & media, const QString & itemID ) > fReloadMediaFunc;
    std::function< std::shared_ptr< CMediaData >( const QString & serverName, const QString & mediaID ) > fGetMediaDataForIDFunc;
    std::function< bool( std::shared_ptr< CProgressSystem > progessSystem ) > fMergeMediaFunc;
    std::function< std::unordered_set< std::shared_ptr< CMediaData > >() > fGetAllMediaFunc;
    std::function< void( const QString & serverName, const QString & userID, const QString & connectID) > fUpdateUserConnectID;

    std::function< void( std::shared_ptr< CMediaData > mediaData ) > fProcessNewMediaFunc;
    std::function< void( const QString & title, const QString & msg, bool isCritical ) > fUserMsgFunc;
    std::shared_ptr< CProgressSystem > fProgressSystem;

    using TOptionalBoolPair = std::pair< std::optional< bool >, std::optional< bool > >;
    std::unordered_map< QString, TOptionalBoolPair > fLeftAndRightFinished;
    std::shared_ptr< CUserData > fCurrUserData;
    std::unordered_map< QString, std::shared_ptr< const CServerInfo > > fTestServers;
};
#endif
