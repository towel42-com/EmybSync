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

#include "UserDataWidget.h"
#include "Core/UserData.h"

#include "ui_UserDataWidget.h"

#include <QDebug>
#include <QFileDialog>
#include <QImageReader>

CUserDataWidget::CUserDataWidget( QWidget * parentWidget /*= nullptr */ ) :
    QGroupBox( parentWidget ),
    fImpl( new Ui::CUserDataWidget )

{
    fImpl->setupUi( this );
    connect( fImpl->setLastLoginDateToNow, &QToolButton::clicked,
             [this]()
             {
                 fImpl->lastLoginDate->setDateTime( QDateTime::currentDateTimeUtc() );
             } );
    connect( fImpl->setLastActivityDateToNow, &QToolButton::clicked,
             [this]()
             {
                 fImpl->lastActivityDate->setDateTime( QDateTime::currentDateTimeUtc() );
             } );
    connect( fImpl->setCreatedDateToNow, &QToolButton::clicked,
             [this]()
             {
                 fImpl->creationDate->setDateTime( QDateTime::currentDateTimeUtc() );
             } );

    connect( fImpl->setAvatarBtn, &QToolButton::clicked, this, &CUserDataWidget::slotSelectChangeAvatar );
    connect( fImpl->name, &QLineEdit::textChanged, this, &CUserDataWidget::slotChanged );
    connect( fImpl->connectID, &QLineEdit::textChanged, this, &CUserDataWidget::slotChanged );
    connect( fImpl->lastLoginDate, &QDateTimeEdit::dateTimeChanged, this, &CUserDataWidget::slotChanged );
    connect( fImpl->lastActivityDate, &QDateTimeEdit::dateTimeChanged, this, &CUserDataWidget::slotChanged );
    connect( fImpl->apply, &QPushButton::clicked, this, &CUserDataWidget::slotApplyFromServer );
}

void CUserDataWidget::slotApplyFromServer()
{
    emit sigApplyFromServer( this );
}

CUserDataWidget::CUserDataWidget( const QString & title, QWidget * parentWidget /*= nullptr */ ) :
    CUserDataWidget( parentWidget )
{
    setTitle( title );
}

CUserDataWidget::CUserDataWidget( std::shared_ptr< SUserServerData > userData, QWidget * parentWidget /*= nullptr */ ) :
    CUserDataWidget( "User Data:", parentWidget )
{
    setUserData( userData );
}

CUserDataWidget::~CUserDataWidget()
{
}

void CUserDataWidget::setUserData( std::shared_ptr< SUserServerData > userData )
{
    fUserData = userData;
    load( fUserData );
}

void CUserDataWidget::applyUserData( std::shared_ptr< SUserServerData > userData )
{
    if ( !userData )
        return;

    load( userData );
}

void CUserDataWidget::load( std::shared_ptr< SUserServerData > userData )
{
    if ( !userData )
    {
        fAvatar = {};
        fImpl->avatar->setPixmap( QPixmap( ":/resources/missingAvatar.png" ).scaled( 32, 32 ) );
        fImpl->name->setText( QString() );
        fImpl->connectID->setText( QString() );
        fImpl->creationDate->setDateTime( QDateTime() );
        fImpl->lastActivityDate->setDateTime( QDateTime() );
        fImpl->lastLoginDate->setDateTime( QDateTime() );
    }
    else
    {
        setAvatar( userData->fImage );
        fImpl->name->setText( userData->fName );
        fImpl->connectID->setText( userData->fConnectedIDOnServer );
        fImpl->creationDate->setDateTime( userData->fDateCreated );
        fImpl->lastActivityDate->setDateTime( userData->fLastActivityDate );
        fImpl->lastLoginDate->setDateTime( userData->fLastLoginDate );
    }
}

void CUserDataWidget::setAvatar( const QImage & image )
{
    fAvatar = image;
    auto scaled = fAvatar.isNull() ? fAvatar: fAvatar.scaled( 32, 32 );
    fImpl->avatar->setPixmap( QPixmap::fromImage( scaled ) );
}

std::shared_ptr< SUserServerData > CUserDataWidget::createUserData() const
{
    if ( !fUserData )
        return {};
    auto retVal = std::make_shared< SUserServerData >();
    retVal->fName = fImpl->name->text();
    retVal->fUserID = fUserData->fUserID;
    retVal->fImage = fAvatar;
    retVal->fConnectedIDOnServer = fImpl->connectID->text();
    retVal->fDateCreated = fImpl->creationDate->dateTime();
    retVal->fLastActivityDate = fImpl->lastActivityDate->dateTime();
    retVal->fLastLoginDate = fImpl->lastLoginDate->dateTime();
    return retVal;
}

void CUserDataWidget::setReadOnly( bool readOnly )
{
    fReadOnly = readOnly;
    fImpl->setAvatarBtn->setHidden( readOnly );
    fImpl->setLastLoginDateToNow->setHidden( readOnly );
    fImpl->setLastActivityDateToNow->setHidden( readOnly );
    fImpl->setCreatedDateToNow->setHidden( readOnly );

    fImpl->name->setEnabled( readOnly );
    fImpl->connectID->setEnabled( readOnly );
    fImpl->creationDate->setEnabled( readOnly );
    fImpl->lastActivityDate->setEnabled( readOnly );
    fImpl->lastLoginDate->setEnabled( readOnly );
}

void CUserDataWidget::slotChanged()
{
}

void CUserDataWidget::slotSelectChangeAvatar()
{
    auto formats = QImageReader::supportedImageFormats();
    QStringList exts;
    for ( auto && ii : formats )
    {
        exts << "*." + ii;
    }

    auto extensions = tr( "Image Files (%1);;All Files (* *.*)" ).arg( exts.join( " " ) );
    auto fileName = QFileDialog::getOpenFileName( this, QObject::tr( "Select Image File" ), QString(), extensions );
    if ( fileName.isEmpty() )
        return;
    setAvatar( QImage( fileName ) );
}
