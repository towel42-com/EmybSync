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

#ifndef __MEDIADATA_H
#define __MEDIADATA_H

#include <QString>
#include <QMap>
#include <QUrlQuery>
#include <QDateTime>

class QVariant;
class QTreeWidgetItem;
class QJsonObject;
class QTreeWidget;
class QColor;

struct CMediaDataBase
{
    QString fMediaID;
    bool fIsFavorite{ false };
    bool fPlayed{ false };
    QDateTime fLastPlayedDate;
    uint64_t fPlayCount;
    uint64_t fPlaybackPositionTicks; // 1 tick = 10000 ms

    void loadUserDataFromJSON( const QJsonObject & userDataObj );

    QTreeWidgetItem * fItem{ nullptr };
    bool fBeenLoaded{ false };
};

bool operator==( const CMediaDataBase & lhs, const CMediaDataBase & rhs );
inline bool operator!=( const CMediaDataBase & lhs, const CMediaDataBase & rhs )
{
    return !operator==( lhs, rhs );
}

class CMediaData
{
public:
    enum EColumn
    {
        eName = 0,
        eMediaID,
        eFavorite,
        ePlayed,
        eLastPlayed,
        ePlayCount,
        ePlaybackPosition
    };

    static QStringList getHeaderLabels();
    static void setMSecsToStringFunc( std::function< QString( uint64_t ) > func );

    CMediaData( const QString & name, const QString & type );

    QString name() const;
    static QString computeName( QJsonObject & media );

    bool isMissing( bool isLHS ) const;
    bool hasMissingInfo() const;

    void setMediaID( const QString & id, bool isLHS );
    bool beenLoaded( bool isLHS ) const;

    QString getMediaID( bool isLHS ) const;
    bool isFavorite( bool lhs ) const;
    bool isPlayed( bool lhs ) const;
    QDateTime lastPlayed( bool lhs ) const;
    uint64_t playCount( bool lhs ) const;
    uint64_t playbackPositionMSecs( bool lhs ) const; 
    uint64_t playbackPositionTicks( bool lhs ) const;// 10,000 ticks = 1 ms
    QString playbackPosition( bool lhs ) const;

    bool hasProviderIDs() const;
    QString getProviderList() const;

    bool serverDataEqual() const;

    bool bothPlayed() const;
    bool playbackPositionTheSame() const;
    bool bothFavorites() const;
    bool lastPlayedTheSame() const;

    void loadUserDataFromJSON( const QJsonObject & object, bool isLHSServer );
    bool lhsMoreRecent() const;
    void setItem( QTreeWidgetItem * item, bool lhs );
    QTreeWidgetItem * getItem( bool lhs );
    void updateFromOther( std::shared_ptr< CMediaData > other, bool toLHS );
    void createItems( QTreeWidget * lhsTree, QTreeWidget * rhsTree, const std::map< int, QString > & providersByColumn );
    QStringList getColumns( bool forLHS );
    std::pair< QStringList, QStringList > getColumns( const std::map< int, QString > & providersByColumn );
    void updateItems( const std::map< int, QString > & providersByColumn );
    void updateItems( const QStringList & columnsData, bool forLHS );
    void setItemColors();

    QUrlQuery getMissingItemQuery() const;

    void addProvider( const QString & providerName, const QString & providerID );
    const std::map< QString, QString > & getProviders() const { return fProviders; }


    QTreeWidgetItem * getItem( bool forLHS ) const;
private:
    QString fType;
    QString fName;
    std::map< QString, QString > fProviders;

    std::shared_ptr< CMediaDataBase > fLHSServer;
    std::shared_ptr< CMediaDataBase > fRHSServer;

    static std::function< QString( uint64_t ) > sMSecsToStringFunc;
};
#endif 
