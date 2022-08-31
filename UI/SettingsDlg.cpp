﻿// The MIT License( MIT )
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

#include "SettingsDlg.h"
#include "EditServerDlg.h"
#include "Core/Settings.h"
#include "Core/ServerInfo.h"
#include "Core/UserData.h"
#include "Core/SyncSystem.h"
#include "SABUtils/ButtonEnabler.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QDebug>
#include <QPushButton>

#include "ui_SettingsDlg.h"

CSettingsDlg::CSettingsDlg( std::shared_ptr< CSettings > settings, std::shared_ptr< CSyncSystem > syncSystem, const std::vector< std::shared_ptr< CUserData > > & knownUsers, QWidget * parentWidget )
    : QDialog( parentWidget ),
    fImpl( new Ui::CSettingsDlg ),
    fSettings( settings ),
    fSyncSystem( syncSystem )
{
    fImpl->setupUi( this );
    new NSABUtils::CButtonEnabler( fImpl->usersList, fImpl->delUser );
    new NSABUtils::CButtonEnabler( fImpl->usersList, fImpl->editUser );
    new NSABUtils::CButtonEnabler( fImpl->servers, fImpl->delServer );
    new NSABUtils::CButtonEnabler( fImpl->servers, fImpl->editServer );

    loadKnownUsers( knownUsers );

    fTestButton = fImpl->testButtonBox->addButton( tr( "Test" ), QDialogButtonBox::ButtonRole::ActionRole );
    fTestButton->setObjectName( "Test Button" );
    connect( fTestButton, &QPushButton::clicked, this, &CSettingsDlg::slotTestServers );
    connect( fSyncSystem.get(), &CSyncSystem::sigTestServerResults, this, &CSettingsDlg::slotTestServerResults );

    load();

    connect( fImpl->dataMissingColor, &QToolButton::clicked,
             [this]()
             {
                 auto newColor = QColorDialog::getColor( fDataMissingColor, this, tr( "Select Color" ) );
                 if ( newColor.isValid() )
                 {
                     fDataMissingColor = newColor;
                     updateColors();
                 }
             } );
    connect( fImpl->mediaSourceColor, &QToolButton::clicked,
             [this]()
             {
                 auto newColor = QColorDialog::getColor( fMediaSourceColor, this, tr( "Select Color" ) );
                 if ( newColor.isValid() )
                 {
                     fMediaSourceColor = newColor;
                     updateColors();
                 }
             } );
    connect( fImpl->mediaDestColor, &QToolButton::clicked,
             [this]()
             {
                 auto newColor = QColorDialog::getColor( fMediaDestColor, this, tr( "Select Color" ) );
                 if ( newColor.isValid() )
                 {
                     fMediaDestColor = newColor;
                     updateColors();
                 }
             } );
    connect( fImpl->addUser, &QToolButton::clicked,
        [ this ]()
        {
            editUser( nullptr );
        } );
    connect( fImpl->delUser, &QToolButton::clicked,
        [ this ]()
        {
            auto curr = fImpl->usersList->currentItem();
            if ( !curr )
                return;
            delete curr;
            updateKnownUsers();
        } );
    connect( fImpl->editUser, &QToolButton::clicked,
             [this]()
             {
                 auto curr = fImpl->usersList->currentItem();
                 editUser( curr );
             } );

    connect( fImpl->usersList, &QListWidget::itemDoubleClicked,
        [ this ]( QListWidgetItem *item )
        {
            return editUser( item );
        } );

    connect( fImpl->addServer, &QToolButton::clicked,
             [this]()
             {
                 editServer( nullptr );
             } );
    connect( fImpl->delServer, &QToolButton::clicked,
             [this]()
             {
                 auto curr = fImpl->servers->currentItem();
                 if ( !curr )
                     return;
                 delete curr;
             } );
    connect( fImpl->editServer, &QToolButton::clicked,
             [this]()
             {
                 auto curr = fImpl->servers->currentItem();
                 editServer( curr );
             } );

    connect( fImpl->servers, &QTreeWidget::itemDoubleClicked,
             [this]( QTreeWidgetItem * item )
             {
                 return editServer( item );
             } );
    connect( fImpl->servers, &QTreeWidget::itemSelectionChanged, this, &CSettingsDlg::slotCurrServerChanged );
    connect( fImpl->moveServerUp, &QToolButton::clicked, this, &CSettingsDlg::slotMoveServerUp );
    connect( fImpl->moveServerDown, &QToolButton::clicked, this, &CSettingsDlg::slotMoveServerDown );

    fImpl->tabWidget->setCurrentIndex( 0 );
}

void CSettingsDlg::slotMoveServerUp()
{
    moveCurrServer( true );
}

void CSettingsDlg::slotMoveServerDown()
{
    moveCurrServer( false );
}

void CSettingsDlg::slotCurrServerChanged()
{
    auto curr = fImpl->servers->currentItem();
    if ( !curr )
        return;

    auto itemAbove = fImpl->servers->itemAbove( curr );
    fImpl->moveServerUp->setEnabled( itemAbove != nullptr );

    auto itemBelow = fImpl->servers->itemBelow( curr );
    fImpl->moveServerDown->setEnabled( itemBelow != nullptr );
}


void CSettingsDlg::moveCurrServer( bool up )
{
    auto curr = fImpl->servers->currentItem();
    if ( !curr )
        return;

    auto parentItem = curr->parent();
    auto treeWidget = curr->treeWidget();
    if ( parentItem )
    {
        auto currIdx = parentItem->indexOfChild( curr );
        parentItem->takeChild( currIdx );
        currIdx += up ? -1 : +1;
        parentItem->insertChild( currIdx, curr );
    }
    else
    {
        auto currIdx = treeWidget->indexOfTopLevelItem( curr );
        treeWidget->takeTopLevelItem( currIdx );
        currIdx += up ? -1 : +1;
        treeWidget->insertTopLevelItem( currIdx, curr );
    }
    treeWidget->selectionModel()->clearSelection();
    curr->setSelected( true );
    treeWidget->setCurrentItem( curr );
    slotCurrServerChanged();
}

void CSettingsDlg::loadKnownUsers( const std::vector< std::shared_ptr< CUserData > > & knownUsers )
{
    auto headerLabels = QStringList() << tr( "Connected ID" );
    for ( int ii = 0; ii < fSettings->serverCnt(); ++ii )
        headerLabels << fSettings->serverInfo( ii )->displayName();
    fImpl->knownUsers->setColumnCount( headerLabels.count() );
    fImpl->knownUsers->setHeaderLabels( headerLabels );

    for ( auto && ii : knownUsers )
    {
        auto data = QStringList() << ii->connectedID();
        for ( int jj = 0; jj < fSettings->serverCnt(); ++jj )
            data << ii->name( fSettings->serverInfo( jj )->keyName() );

        auto item = new QTreeWidgetItem( fImpl->knownUsers, data );
        item->setIcon( 0, QIcon( QPixmap::fromImage( ii->anyAvatar() ) ) );

        fKnownUsers.push_back( std::make_pair( ii, item ) );
    }
}

void CSettingsDlg::editUser( QListWidgetItem * item )
{
    auto curr = item ? item->text() : QString();

    auto newUserName = QInputDialog::getText( this, tr( "Regular Expression" ), tr( "Regular Expression matching User names to sync:" ), QLineEdit::EchoMode::Normal, curr );
    if ( newUserName.isEmpty() || ( newUserName == item->text() ) )
        return;
    item->setText( newUserName );
    updateKnownUsers();
}

void CSettingsDlg::editServer( QTreeWidgetItem * item )
{
    auto displayName = item ? item->text( 0 ) : QString();
    auto url = item ? item->text( 1 ) : QString();
    auto apiKey = item ? item->text( 2 ) : QString();
    bool enabled = item ? ( item->checkState( 0 ) == Qt::CheckState::Checked ) : true;

    CEditServerDlg dlg( displayName, url, apiKey, enabled, this );
    if ( dlg.exec() == QDialog::Accepted )
    {
        if ( !item )
            item = new QTreeWidgetItem( fImpl->servers );
        item->setText( 0, dlg.name() );
        item->setCheckState( 0, dlg.enabled() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked );
        item->setIcon( 0, QIcon( QString::fromUtf8( ":/SABUtilsResources/unknownStatus.png" ) ) );
        item->setText( 1, dlg.url() );
        item->setText( 2, dlg.apiKey() );
    }
}

CSettingsDlg::~CSettingsDlg()
{
}

void CSettingsDlg::accept()
{
    save();
    QDialog::accept();
}


void CSettingsDlg::load()
{
    fImpl->servers->setColumnCount( 3 );
    for ( int ii = 0; ii < fSettings->serverCnt(); ++ii )
    {
        auto serverInfo = fSettings->serverInfo( ii );
        auto name = serverInfo->displayName();
        auto url = serverInfo->url();
        auto apiKey = serverInfo->apiKey();

        auto item = new QTreeWidgetItem( fImpl->servers, QStringList() << name << url << apiKey );
        item->setCheckState( 0, serverInfo->isEnabled() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked );
        item->setIcon( 0, QIcon( QString::fromUtf8( ":/SABUtilsResources/unknownStatus.png" ) ) );
    }

    fMediaSourceColor = fSettings->mediaSourceColor();
    fMediaDestColor = fSettings->mediaDestColor();
    fDataMissingColor = fSettings->dataMissingColor();
    updateColors();
    auto maxItems = fSettings->maxItems();
    if ( maxItems < fImpl->maxItems->minimum() )
        maxItems = fImpl->maxItems->minimum();
    fImpl->maxItems->setValue( maxItems );

    fImpl->syncAudio->setChecked( fSettings->syncAudio() );
    fImpl->syncVideo->setChecked( fSettings->syncVideo() );
    fImpl->syncEpisode->setChecked( fSettings->syncEpisode() );
    fImpl->syncMovie->setChecked( fSettings->syncMovie() );
    fImpl->syncTrailer->setChecked( fSettings->syncTrailer() );
    fImpl->syncAdultVideo->setChecked( fSettings->syncAdultVideo() );
    fImpl->syncMusicVideo->setChecked( fSettings->syncMusicVideo() );
    fImpl->syncGame->setChecked( fSettings->syncGame() );
    fImpl->syncBook->setChecked( fSettings->syncBook() );

    auto syncUsers = fSettings->syncUserList();
    for ( auto && ii : syncUsers )
    {
        if ( ii.isEmpty() )
            continue;
        new QListWidgetItem( ii, fImpl->usersList );
    }

    updateKnownUsers();

    fImpl->checkForLatest->setChecked( CSettings::checkForLatest() );
    fImpl->loadLastProject->setChecked( CSettings::loadLastProject() );
}

void CSettingsDlg::save()
{
    auto servers = getServerInfos( false );
    fSettings->setServers( servers );

    fSettings->setMediaSourceColor( fMediaSourceColor );
    fSettings->setMediaDestColor( fMediaDestColor );
    fSettings->setDataMissingColor( fDataMissingColor );
    fSettings->setMaxItems( ( fImpl->maxItems->value() == fImpl->maxItems->minimum() ) ? -1 : fImpl->maxItems->value() );

    fSettings->setSyncAudio( fImpl->syncAudio->isChecked() );
    fSettings->setSyncVideo( fImpl->syncVideo->isChecked() );
    fSettings->setSyncEpisode( fImpl->syncEpisode->isChecked() );
    fSettings->setSyncMovie( fImpl->syncMovie->isChecked() );
    fSettings->setSyncTrailer( fImpl->syncTrailer->isChecked() );
    fSettings->setSyncAdultVideo( fImpl->syncAdultVideo->isChecked() );
    fSettings->setSyncMusicVideo( fImpl->syncMusicVideo->isChecked() );
    fSettings->setSyncGame( fImpl->syncGame->isChecked() );
    fSettings->setSyncBook( fImpl->syncBook->isChecked() );
    fSettings->setSyncUserList( syncUserStrings() );

    CSettings::setCheckForLatest( fImpl->checkForLatest->isChecked() );
    CSettings::setLoadLastProject( fImpl->loadLastProject->isChecked() );
}

std::vector< std::shared_ptr< CServerInfo > > CSettingsDlg::getServerInfos( bool enabledOnly ) const
{
    std::vector< std::shared_ptr< CServerInfo > > servers;
    for ( int ii = 0; ii < fImpl->servers->topLevelItemCount(); ++ii )
    {
        auto curr = getServerInfo( ii );
        if ( !enabledOnly || curr->isEnabled() )
            servers.push_back( curr );
    }
    return std::move( servers );
}

std::shared_ptr< CServerInfo > CSettingsDlg::getServerInfo( int ii ) const
{
    auto item = fImpl->servers->topLevelItem( ii );
    
    auto name = item->text( 0 );
    auto url = item->text( 1 );
    auto apiKey = item->text( 2 );
    auto enabled = item->checkState( 0 ) == Qt::CheckState::Checked;

    return std::make_shared< CServerInfo >( name, url, apiKey, enabled );
}

QStringList CSettingsDlg::syncUserStrings() const
{
    QStringList syncUsers;
    for ( int ii = 0; ii < fImpl->usersList->count(); ++ii )
    {
        auto curr = fImpl->usersList->item( ii );
        if ( !curr )
            continue;
        syncUsers << curr->text();
    }
    return syncUsers;
}

void CSettingsDlg::updateKnownUsers()
{
    QStringList syncUsers;
    for ( int ii = 0; ii < fImpl->usersList->count(); ++ii )
    {
        auto curr = fImpl->usersList->item( ii );
        if ( !curr )
            continue;
        if ( !QRegularExpression( curr->text() ).isValid() )
        {
            curr->setBackground( Qt::red );
            continue;
        }
        else
            curr->setBackground( QBrush() );

        syncUsers << "(" + curr->text() + ")";
    }

    auto regExpStr = syncUsers.join( "|" );
    QRegularExpression regExp;
    if ( !regExpStr.isEmpty() )
        regExp = QRegularExpression( regExpStr );
    qDebug() << regExp << regExp.isValid() << regExp.pattern();
    for ( auto && ii : fKnownUsers )
    {
        bool isMatch = ii.first->isUser( regExp );
        for ( int column = 0; column < ii.second->columnCount(); ++column )
        {
            if ( isMatch )
                ii.second->setBackground( column, Qt::green );
            else
                ii.second->setBackground( column, QBrush() );
        }
    }
}

void CSettingsDlg::updateColors()
{
    updateColor( fImpl->mediaSource, fMediaSourceColor);
    updateColor( fImpl->mediaDest, fMediaDestColor );
    updateColor( fImpl->dataMissing, fDataMissingColor );
}

void CSettingsDlg::updateColor( QLabel * label, const QColor & color )
{
    QString styleSheet;
    if ( color == Qt::black )
        styleSheet = QString( "QLabel { background-color: %1; color: #ffffff }" ).arg( color.name() );
    else
        styleSheet = QString( "QLabel { background-color: %1 }" ).arg( color.name() );
    label->setStyleSheet( styleSheet );
}

bool CSettings::load( const QString & fileName, bool addToRecentFileList, QWidget * parentWidget )
{
    fFileName = fileName;
    return load( addToRecentFileList, parentWidget );
}

bool CSettings::load( bool addToRecentFileList, QWidget * parentWidget )
{
    if ( fFileName.isEmpty() )
    {
        auto fileName = QFileDialog::getOpenFileName( parentWidget, QObject::tr( "Select File" ), QString(), QObject::tr( "Settings File (*.json);;All Files (* *.*)" ) );
        if ( fileName.isEmpty() )
            return false;
        fFileName = QFileInfo( fileName ).absoluteFilePath();
    }

    return load( fFileName,
                 [ parentWidget ]( const QString & title, const QString & msg )
    {
        QMessageBox::critical( parentWidget, title, msg );
    }, addToRecentFileList );
}

//std::function<QString()> selectFileFunc, std::function<void( const QString & title, const QString & msg )> errorFunc
bool CSettings::save( QWidget * parentWidget )
{
    return save( parentWidget,
                 [parentWidget]() -> QString
                 {
                     auto fileName = QFileDialog::getSaveFileName( parentWidget, QObject::tr( "Select File" ), QString(), QObject::tr( "Settings File (*.json);;All Files (* *.*)" ) );
                     if ( fileName.isEmpty() )
                         return {};

                     return fileName;
                 },
                 [parentWidget]( const QString & title, const QString & msg )
                 {
                     QMessageBox::critical( parentWidget, title, msg );
                 } );
}
  
bool CSettings::saveAs( QWidget * parentWidget )
{
    auto fileName = QFileDialog::getSaveFileName( parentWidget, QObject::tr( "Select File" ), QString(), QObject::tr( "Settings File (*.json);;All Files (* *.*)" ) );
    if ( fileName.isEmpty() )
        return false;
    fFileName = fileName;
    return save( parentWidget );
}

bool CSettings::maybeSave( QWidget * parentWidget )
{
    return maybeSave( parentWidget,
                      [parentWidget]() -> QString
                      {
                          auto fileName = QFileDialog::getSaveFileName( parentWidget, QObject::tr( "Select File" ), QString(), QObject::tr( "Settings File (*.json);;All Files (* *.*)" ) );
                          if ( fileName.isEmpty() )
                              return {};

                          return fileName;
                      },
                      [parentWidget]( const QString & title, const QString & msg )
                      {
                          QMessageBox::critical( parentWidget, title, msg );
                      } );
}

bool CSettings::save( QWidget * parentWidget, std::function<QString()> selectFileFunc, std::function<void( const QString & title, const QString & msg )> errorFunc )
{
    if ( fFileName.isEmpty() )
        return maybeSave( parentWidget, selectFileFunc, errorFunc );

    return save( errorFunc );
}

bool CSettings::maybeSave( QWidget * parentWidget, std::function<QString()> selectFileFunc, std::function<void( const QString & title, const QString & msg )> errorFunc )
{
    if ( !fChanged )
        return true;

    if ( fFileName.isEmpty() )
    {
        if ( selectFileFunc )
        {
            auto fileName = selectFileFunc();
            if ( fileName.isEmpty() )
                return true;
            fFileName = fileName;
        }
    }

    if ( fFileName.isEmpty() )
        return false;
    return save( parentWidget, selectFileFunc, errorFunc );
}

void CSettingsDlg::slotTestServers()
{
    for ( int ii = 0; ii < fImpl->servers->topLevelItemCount(); ++ii )
    {
        fImpl->servers->topLevelItem( ii )->setIcon( 0, QIcon( QString::fromUtf8( ":/SABUtilsResources/unknownStatus.png" ) ) );
    }

    auto tmp = getServerInfos( true );
    std::vector< std::shared_ptr< const CServerInfo > > servers;
    for ( auto && ii : tmp )
        servers.push_back( std::const_pointer_cast<const CServerInfo>( ii ) );

    fSyncSystem->testServers( servers );
}

void CSettingsDlg::slotTestServerResults( const QString & serverName, bool results, const QString & msg )
{
    for ( int ii = 0; ii < fImpl->servers->topLevelItemCount(); ++ii )
    {
        auto serverInfo = getServerInfo( ii );
        if ( serverInfo->keyName() == serverName )
        {
            auto item = fImpl->servers->topLevelItem( ii );
            if ( results )
                item->setIcon( 0, QIcon( QString::fromUtf8( ":/SABUtilsResources/ok.png" ) ) );
            else
                item->setIcon( 0, QIcon( QString::fromUtf8( ":/SABUtilsResources/error.png" ) ) );
        }
    }
    if ( !results )
    {
        QMessageBox::critical( this, tr( "Error" ), tr( "Error in Testing: '%1' - %1" ).arg( serverName ).arg( msg ), QMessageBox::StandardButton::Ok );
    }
}
