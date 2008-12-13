/***************************************************************************
 *   Copyright (C) 2007 by André Duffeck <duffeck@kde.org>                 *
 *   Copyright (C) 2007 Chani Armitage <chanika@gmail.com>                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "twitter.h"

#include <QApplication>
#include <QGridLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QFontMetrics>
#include <QGraphicsView>
#include <QGraphicsLinearLayout>
#include <QGraphicsProxyWidget>
#include <QAction>
#include <QLabel>

#include <KDebug>
#include <KIcon>
#include <KSharedConfig>
#include <KConfigDialog>
#include <KLineEdit>
#include <KTextEdit>
#include <KTextBrowser>
#include <KStringHandler>
#include <KWallet/Wallet>
#include <KMessageBox>
#include <KColorScheme>
#include <KRun>

#include <Plasma/Svg>
#include <Plasma/Theme>
#include <Plasma/DataEngine>
#include <Plasma/Service>
#include <Plasma/FlashingLabel>
#include <Plasma/IconWidget>
#include <Plasma/SvgWidget>
#include <Plasma/TextEdit>
#include <Plasma/Frame>
#include <Plasma/ServiceJob>

Q_DECLARE_METATYPE(Plasma::DataEngine::Data)

Twitter::Twitter(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
      m_graphicsWidget(0),
      m_newTweets(0),
      m_service(0),
      m_profileService(0),
      m_lastTweet(0),
      m_wallet(0),
      m_walletWait(None),
      m_colorScheme(0)
{
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setPopupIcon("view-pim-journal");
}

void Twitter::init()
{
    m_flash = new Plasma::FlashingLabel( this );
    m_theme = new Plasma::Svg(this);
    m_theme->setImagePath("widgets/twitter");
    m_theme->setContainsMultipleImages(true);
}

void Twitter::constraintsEvent(Plasma::Constraints constraints)
{
    //i am an icon?
    if ((constraints|Plasma::SizeConstraint || constraints|Plasma::FormFactorConstraint) &&
        layout()->itemAt(0) != m_graphicsWidget) {
        paintIcon();
    }
}

void Twitter::paintIcon()
{
    int size = qMin(contentsRect().width(), contentsRect().height());
    if (size < 1) {
        size = KIconLoader::SizeSmall;
    }

    QPixmap icon(size, size);
    if (m_popupIcon.isNull()) {
        icon = KIconLoader::global()->loadIcon("view-pim-journal", KIconLoader::NoGroup, size);
    } else {
        icon.fill(Qt::transparent);
    }

    QPainter p(&icon);
    p.drawPixmap(icon.rect(), m_popupIcon, m_popupIcon.rect());
    //4.3: a notification system for popupapplets would be cool
    if (m_newTweets > 0) {
        QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont);
        QFontMetrics fm(font);
        QRect textRect(fm.boundingRect(QString::number(m_newTweets)));
        int textSize = qMax(textRect.width(), textRect.height());
        textRect.setSize(QSize(textSize, textSize));
        textRect.moveBottomRight(icon.rect().bottomRight());

        QColor c(Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor));
        c.setAlphaF(0.6);

        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.setRenderHints(QPainter::Antialiasing);
        p.drawEllipse(textRect);

        p.setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
        p.drawText(textRect, Qt::AlignCenter, QString::number(m_newTweets));
    }
    p.end();

    setPopupIcon(icon);
}

void Twitter::popupEvent(bool show)
{
    if (show) {
        m_newTweets = 0;
        paintIcon();
    }
}

void Twitter::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event);

    m_statusEdit->setFocus();
}

QGraphicsWidget *Twitter::graphicsWidget()
{
    if (m_graphicsWidget) {
        return m_graphicsWidget;
    }

    m_graphicsWidget = new QGraphicsWidget(this);
    m_graphicsWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_colorScheme = new KColorScheme(QPalette::Active, KColorScheme::View, Plasma::Theme::defaultTheme()->colorScheme());
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(themeChanged()));

    //config stuff
    KConfigGroup cg = config();
    m_username = cg.readEntry( "username" );
    m_password = KStringHandler::obscure( cg.readEntry( "password" ) );
    m_historySize = cg.readEntry( "historySize", 2 );
    m_historyRefresh = cg.readEntry( "historyRefresh", 5 );
    m_includeFriends = cg.readEntry( "includeFriends", true );

    m_engine = dataEngine("twitter");
    if (! m_engine->isValid()) {
        setFailedToLaunch(true, i18n("Failed to load twitter DataEngine"));
        return m_graphicsWidget;
    }

    //ui setup
    m_layout = new QGraphicsLinearLayout( Qt::Vertical, m_graphicsWidget );
    m_layout->setSpacing( 3 );

    QGraphicsLinearLayout *flashLayout = new QGraphicsLinearLayout( Qt::Horizontal );

    m_flash->setAutohide( true );
    m_flash->setMinimumSize( 0, 20 );
    m_flash->setColor( Qt::gray );
    QFont fnt = qApp->font();
    fnt.setBold( true );
    QFontMetrics fm( fnt );
    m_flash->setFont( fnt );
    m_flash->flash( "", 20000 );
    m_flash->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QGraphicsLinearLayout *titleLayout = new QGraphicsLinearLayout( Qt::Vertical );
    Plasma::SvgWidget *svgTitle = new Plasma::SvgWidget(m_theme, "twitter", this);
    svgTitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    svgTitle->setPreferredSize(75, 14);
    titleLayout->addItem(svgTitle);

    flashLayout->addItem(m_flash);
    flashLayout->setStretchFactor(m_flash, 2);
    flashLayout->addItem( titleLayout );
    m_layout->addItem( flashLayout );

    Plasma::Frame *headerFrame = new Plasma::Frame(this);
    m_headerLayout = new QGraphicsLinearLayout( Qt::Horizontal, headerFrame );
    m_headerLayout->setContentsMargins( 5, 5, 5, 10 );
    m_headerLayout->setSpacing( 5 );
    m_layout->addItem( headerFrame );


    m_icon = new Plasma::IconWidget( this );
    m_icon->setIcon( KIcon( "user-identity" ) );
    m_icon->setText( m_username );
    QSizeF iconSize = m_icon->sizeFromIconSize(48);
    m_icon->setMinimumSize( iconSize );
    m_icon->setMaximumSize( iconSize );
    m_headerLayout->addItem( m_icon );

    Plasma::Frame *statusEditFrame = new Plasma::Frame(this);
    statusEditFrame->setFrameShadow(Plasma::Frame::Sunken);
    QGraphicsLinearLayout *statusEditLayout = new QGraphicsLinearLayout(statusEditFrame);
    m_statusEdit = new Plasma::TextEdit();
    m_statusEdit->nativeWidget()->setFrameShape( QFrame::NoFrame );
    m_statusEdit->nativeWidget()->setTextBackgroundColor( QColor(0,0,0,0) );
    m_statusEdit->nativeWidget()->viewport()->setAutoFillBackground( false );
    connect(m_statusEdit, SIGNAL(textChanged()), this, SLOT(editTextChanged()));
    statusEditLayout->addItem(m_statusEdit);

    //FIXME: m_statusEdit->setTextColor( m_colorScheme->foreground().color() );
    // seems to have no effect
    QPalette editPal = m_statusEdit->palette();
    editPal.setColor(QPalette::Text, m_colorScheme->foreground().color());
    m_statusEdit->nativeWidget()->setPalette(editPal);
    m_statusEdit->nativeWidget()->installEventFilter(this);
    m_headerLayout->addItem( statusEditFrame );

    m_layout->addStretch();
    m_layout->addStretch();

    //hook up some sources
    m_engine->connectSource("UserImages", this);
    m_engine->connectSource("Error:UserImages", this);
    m_engine->connectSource("Error", this);

    //set things in motion
    if(! m_username.isEmpty()) {
        if (m_password.isEmpty()) {
            m_walletWait = Read;
            getWallet();
        } else { //use config value
            downloadHistory();
        }
    }else{
        setAuthRequired(true);
    }

    return m_graphicsWidget;
}

void Twitter::getWallet()
{
    //TODO: maybe Plasma in general should handle the wallet
    if (m_wallet) {
        //user must be a dumbass. kill that old attempt.
        delete m_wallet;
    }

    QGraphicsView *v = view();
    WId w = 0;
    if (v) {
        w = v->winId();
    }

    m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(),
                                           w, KWallet::Wallet::Asynchronous);

    if (m_walletWait == Write) {
        connect(m_wallet, SIGNAL(walletOpened(bool)), SLOT(writeWallet(bool)));
    } else {
        connect(m_wallet, SIGNAL(walletOpened(bool)), SLOT(readWallet(bool)));
    }
}

void Twitter::writeWallet(bool success)
{
    //kDebug();
    if (!(success && enterWalletFolder(QString::fromLatin1("Plasma-Twitter"))
                && (m_wallet->writePassword(m_username, m_password) == 0))) {
        kDebug() << "failed to write password";
        writeConfigPassword();
    }
    m_walletWait = None;
    delete m_wallet;
    m_wallet = 0;
}

void Twitter::readWallet(bool success)
{
    //kDebug();
    QString pwd;
    if (success && enterWalletFolder(QString::fromLatin1("Plasma-Twitter"))
            && (m_wallet->readPassword(m_username, pwd) == 0)) {
        m_password = pwd;
        downloadHistory();
    } else {
        kDebug() << "failed to read password";
    }
    m_walletWait = None;
    delete m_wallet;
    m_wallet = 0;
}

bool Twitter::enterWalletFolder(const QString &folder)
{
    //TODO: seems a bit silly to have a function just for this here
    //why doesn't kwallet have this itself?
    m_wallet->createFolder(folder);
    if (! m_wallet->setFolder(folder)) {
        kDebug() << "failed to open folder" << folder;
        return false;
    }
    return true;
}

void Twitter::setAuthRequired(bool required)
{
    setConfigurationRequired(required);
    m_statusEdit->setEnabled(!required);
}

void Twitter::writeConfigPassword()
{
    //kDebug();
    if (KMessageBox::warningYesNo(0, i18n("Failed to access kwallet. Store password in config file instead?"))
            == KMessageBox::Yes) {
        KConfigGroup cg = config();
        cg.writeEntry( "password", KStringHandler::obscure(m_password) );
    }
}

void Twitter::dataUpdated(const QString& source, const Plasma::DataEngine::Data &data)
{
    //kDebug() << source;
    if (data.isEmpty()) {
        if (source.startsWith("Error")) {
            m_flash->kill(); //FIXME only clear it if it was showing an error msg
        } else {
            //this is a fake update from a new source
            return;
        }
    }

    if (source == m_curTimeline) {
        m_flash->flash( i18n("Refreshing timeline...") );

        //add the newbies
        int newCount = 0;
        uint maxId = m_lastTweet;
        foreach (const QString &id, data.keys()) {
            uint i=id.toUInt();
            if (i > m_lastTweet) {
                newCount++;
                QVariant v = data.value(id);
                //Warning: This function is not available with MSVC 6
                Plasma::DataEngine::Data t = v.value<Plasma::DataEngine::Data>();
                m_tweetMap[id] = t;
                if (i > maxId) {
                    maxId = i;
                }
            }
        }
        m_lastTweet = maxId;
        m_newTweets = qMin(newCount, m_historySize);
        m_flash->flash( i18np( "1 new tweet", "%1 new tweets", m_newTweets ), 20*1000 );
        showTweets();
    } else if (source == "UserImages") {
        foreach (const QString &user, data.keys()) {
            QPixmap pm = data[user].value<QPixmap>();

            if( !pm.isNull() ) {
                if( user == m_username ) {
                    QAction *profile = new QAction(QIcon(pm), m_username, this);
                    profile->setData(m_username);

                    QSizeF iconSize = m_icon->sizeFromIconSize(48);
                    m_icon->setAction(profile);
                    m_icon->setMinimumSize( iconSize );
                    m_icon->setMaximumSize( iconSize );
                    connect(profile, SIGNAL(triggered(bool)), this, SLOT(openProfile()));
                }
                m_pictureMap[user] = pm;
                //TODO it would be nice to check whether the updated image is actually in use
                showTweets();
            }
        }
    } else if (source.startsWith("Error")) {
        QString desc = data["description"].toString();

        if (desc == "Authentication required"){
            setAuthRequired(true);
        }

        m_flash->flash(desc, 60 * 1000); //I'd really prefer it to stay there. and be red.
    }
    updateGeometry();
}

void Twitter::themeChanged()
{
    delete m_colorScheme;
    m_colorScheme = new KColorScheme(QPalette::Active, KColorScheme::View, Plasma::Theme::defaultTheme()->colorScheme());
    showTweets();
}

void Twitter::showTweets()
{
    prepareGeometryChange();
    // Adjust the number of the TweetWidgets if the configuration has changed
    // Add more tweetWidgets if there are not enough
    while( m_tweetWidgets.size() < m_historySize ) {
        Plasma::Frame *tweetFrame = new Plasma::Frame(this);

        QGraphicsLinearLayout *tweetLayout = new QGraphicsLinearLayout( Qt::Horizontal, tweetFrame );
        tweetLayout->setContentsMargins( 0, 5, 0, 5 );
        tweetLayout->setSpacing( 5 );
        m_layout->insertItem( m_layout->count()-1, tweetFrame );

        KTextBrowser *c = new KTextBrowser();
        QGraphicsProxyWidget *proxy = new QGraphicsProxyWidget( this );
        proxy->setWidget( c );
        c->setCursor( Qt::ArrowCursor );
        c->setFrameShape(QFrame::NoFrame);
        c->setAttribute(Qt::WA_NoSystemBackground);
        c->setTextBackgroundColor(QColor(0,0,0,0));
        c->viewport()->setAutoFillBackground(false);
        c->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        c->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        c->setReadOnly(true);

        Plasma::IconWidget *icon = new Plasma::IconWidget( this );
        QSizeF iconSize = icon->sizeFromIconSize(30);
        icon->setMinimumSize( iconSize );
        icon->setMaximumSize( iconSize );

        Plasma::IconWidget *favIcon = new Plasma::IconWidget( this );
        QSizeF favIconSize = icon->sizeFromIconSize(16);
        favIcon->setMinimumSize( favIconSize );
        favIcon->setMaximumSize( favIconSize );

        tweetLayout->addItem( icon );
        tweetLayout->addItem( proxy );
        tweetLayout->addItem( favIcon );
        tweetLayout->updateGeometry();

        Tweet t;
        t.frame = tweetFrame;
        t.icon = icon;
        t.content = c;
        t.contentProxy = proxy;
        t.favIcon = favIcon;

        m_tweetWidgets.append( t );
    }
    //clear out tweet widgets if there are too many
    while( m_tweetWidgets.size() > m_historySize ) {
        Tweet t = m_tweetWidgets[m_tweetWidgets.size()-1];
        m_layout->removeItem( t.frame );
        delete t.icon;
        delete t.content;
        delete t.frame;
        m_tweetWidgets.removeAt( m_tweetWidgets.size()-1 );
    }

    int i = 0;
    int pos = m_tweetMap.keys().size() - 1;
    while(i < m_historySize && pos >= 0 ) {
        Plasma::DataEngine::Data tweetData = m_tweetMap[m_tweetMap.keys()[pos]];
        QString user = tweetData.value( "User" ).toString();
        QPixmap favIcon = tweetData.value("SourceFavIcon").value<QPixmap>();

        if (i == 0) {
            m_popupIcon = m_pictureMap[user];
        }

        QAction *profile = new QAction(QIcon(m_pictureMap[user]), QString(), this);
        profile->setData(user);

        Tweet t = m_tweetWidgets[i];
        t.icon->setAction(profile);
        QSizeF iconSize = t.icon->sizeFromIconSize(30);
        t.icon->setMinimumSize( iconSize );
        t.icon->setMaximumSize( iconSize );
        connect(profile, SIGNAL(triggered(bool)), this, SLOT(openProfile()));

        QString sourceString;
        if( favIcon.isNull() ) {
            sourceString = i18n(" from %1", tweetData.value( "Source" ).toString());
        }
        QString html = "<table cellspacing='0' spacing='5' width='100%'>";
        html += QString("<tr height='1em'><td align='left' width='1%'><font color='%2'>%1</font></td><td align='right' width='auto'><p align='right'><font color='%2'>%3%4</font></p></td></tr>").arg( user).arg(m_colorScheme->foreground(KColorScheme::InactiveText).color().name())
                .arg(timeDescription( tweetData.value( "Date" ).toDateTime() )).arg( sourceString);
        QString status = tweetData.value( "Status" ).toString();

        status.replace(QRegExp("((http|https)://[^\\s<>'\"]+[^!,\\.\\s<>'\"\\]])"), "<a href='\\1'>\\1</a>");

        html += QString( "<tr><td colspan='2'><font color='%1'>%2</font></td></tr>" )
                .arg( m_colorScheme->foreground().color().name()).arg( status );
        html += "</table>";
        t.content->setHtml( html );
        t.content->document()->setDefaultStyleSheet(QString("a{color:%1} a:visited{color:%2}")
                                            .arg( m_colorScheme->foreground(KColorScheme::LinkText).color().name())
                                            .arg(m_colorScheme->foreground(KColorScheme::VisitedText).color().name()));
        t.content->document()->setTextWidth(t.content->width());
        t.content->setMinimumSize(t.content->document()->size().toSize());
        t.content->setMaximumHeight(t.content->minimumSize().height());

        t.content->update();

        //FIXME: this hopefully would get useless in QGL of 4.5
        qreal left, top, right, bottom;
        t.frame->getContentsMargins(&left, &top, &right, &bottom);
        t.frame->setMinimumHeight(t.content->size().height() + top + bottom);
        t.frame->setPreferredHeight(t.frame->minimumHeight());

        if( !favIcon.isNull() ) {
            t.favIcon->setIcon( QIcon(favIcon) );
        }

        ++i;
        --pos;
    }

    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    //this line break things, strange
    m_graphicsWidget->setMinimumSize(m_layout->sizeHint(Qt::MinimumSize));
    m_graphicsWidget->setPreferredSize(m_layout->sizeHint(Qt::PreferredSize));
    //are we complete?
    if (layout()->itemAt(0) == m_graphicsWidget) {
        setMinimumSize(m_layout->sizeHint(Qt::MinimumSize) + QSizeF(left+right, top+bottom));
        setPreferredSize(m_layout->sizeHint(Qt::PreferredSize) + QSizeF(left+right, top+bottom));
        resize(preferredSize());
    } else {
        paintIcon();
    }
}

void Twitter::createConfigurationInterface(KConfigDialog *parent)
{
    connect( parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()) );
    connect( parent, SIGNAL(okClicked()), this, SLOT(configAccepted()) );

    QWidget *configWidget = new QWidget();
    configUi.setupUi(configWidget);

    configUi.usernameEdit->setText(m_username);
    configUi.passwordEdit->setText(m_password);
    configUi.historySizeSpinBox->setValue(m_historySize);
    configUi.historyRefreshSpinBox->setValue(m_historyRefresh);
    configUi.checkIncludeFriends->setCheckState(m_includeFriends ? Qt::Checked : Qt::Unchecked);

    parent->addPage(configWidget, i18n("General"), icon());
}

void Twitter::configAccepted()
{
    QString username = configUi.usernameEdit->text();
    QString password = configUi.passwordEdit->text();
    int historyRefresh = configUi.historyRefreshSpinBox->value();
    int historySize = configUi.historySizeSpinBox->value();
    bool includeFriends = configUi.checkIncludeFriends->isChecked();
    bool changed = false;

    KConfigGroup cg = config();

    if (m_username != username) {
        changed = true;
        m_username = username;
        m_icon->setIcon( QIcon() );
        m_icon->setText( m_username );
        cg.writeEntry( "username", m_username );
    }

    if (m_password != password) {
        changed = true;
        m_password = password;
    }
    //if (m_walletWait == Read)
    //then the user is a dumbass.
    //we're going to ignore that, which drops the read attempt
    //I hope that doesn't cause trouble.
    //XXX if there's a value in the config, the wallet will never be read
    //if a user saves their password in the config and later changes their mind
    //then they might not understand that they have to delete the password from plasma-appletsrc
    //FIXME if the username is blank, don't connect to the engine, set needsconfiguring, etc
    if (! m_username.isEmpty() && (changed || m_password.isEmpty())) {
        //a change in name *or* pass means we need to update the wallet
        //if the user doesn't set a password, see if it's already in our wallet
        m_walletWait = m_password.isEmpty() ? Read : Write;
        getWallet();
    }

    if (m_historyRefresh != historyRefresh) {
        changed = true;
        m_historyRefresh = historyRefresh;
        cg.writeEntry("historyRefresh", m_historyRefresh);
    }

    if (m_includeFriends != includeFriends) {
        changed = true;
        m_includeFriends = includeFriends;
    }

    if (m_historySize != historySize) {
        m_historySize = historySize;
        cg.writeEntry("historySize", m_historySize);
        if (! changed) {
            showTweets();
        }
    }

    if (changed) {
        //TODO we only *need* to wipe the map if name or includeFriends changed
        m_tweetMap.clear();
        m_lastTweet=0;
        downloadHistory();

        emit configNeedsSaving();
    }

    setAuthRequired(m_username.isEmpty() || m_password.isEmpty());
}

Twitter::~Twitter()
{
    delete m_colorScheme;
    delete m_service;
}

void Twitter::editTextChanged()
{
    m_flash->flash( i18np("%1 character left", "%1 characters left", 140-m_statusEdit->nativeWidget()->toPlainText().length()), 2000 );
}

bool Twitter::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_statusEdit->nativeWidget()) {
        //FIXME:it's nevessary this eventfilter to intercept keypresses in
        // QTextEdit (or KTextedit) is it intended?
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

            //use control modifiers to allow multiline input (even if twitter seems to flatten everything to a slingle line)
            if (!(keyEvent->modifiers() & Qt::ControlModifier) &&
                (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return)) {
                updateStatus();
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        return Plasma::Applet::eventFilter(obj, event);
    }
}

void Twitter::updateStatus()
{
    QString status = m_statusEdit->nativeWidget()->toPlainText();

    delete m_service;
    m_service = m_engine->serviceForSource(m_curTimeline);
    KConfigGroup cg = m_service->operationDescription("update");
    cg.writeEntry("password", m_password);
    cg.writeEntry("status", status);
    m_service->startOperationCall(cg);

    m_statusEdit->nativeWidget()->setPlainText("");
}

//what this really means now is 'reconnect to the timeline source'
void Twitter::downloadHistory()
{
    //kDebug() ;
    if (m_username.isEmpty() || m_password.isEmpty()) {
        if (!m_curTimeline.isEmpty()) {
            m_engine->disconnectSource(m_curTimeline, this);
            m_engine->disconnectSource("Error:" + m_curTimeline, this);
        }
        return;
    }

    m_flash->flash( i18n("Refreshing timeline..."), -1 );

    QString query;
    if( m_includeFriends) {
        query = QString("TimelineWithFriends:%1");
    } else {
        query = QString("Timeline:%1");
    }

    query = query.arg(m_username);
    if (m_curTimeline != query) {
        //ditch the old one, if needed
        if (!m_curTimeline.isEmpty()) {
            m_engine->disconnectSource(m_curTimeline, this);
            m_engine->disconnectSource("Error:" + m_curTimeline, this);
        }
        m_curTimeline = query;
    }

    //kDebug() << "Connecting to source " << query << "with refresh rate" << m_historyRefresh * 60 * 1000;
    m_engine->connectSource(query, this, m_historyRefresh * 60 * 1000);
    m_engine->connectSource("Error:" + query, this);

    delete m_service;
    m_service = m_engine->serviceForSource(query);
    KConfigGroup cg = m_service->operationDescription("auth");
    cg.writeEntry("password", m_password);
    m_service->startOperationCall(cg);
    connect(m_service, SIGNAL(finished(Plasma::ServiceJob*)), this, SLOT(serviceFinished(Plasma::ServiceJob*)));

    //get the profile to retrieve the user icon
    QString profileQuery(QString("Profile:%1").arg(m_username));
    m_engine->connectSource(profileQuery, this, m_historyRefresh * 60 * 1000);

    delete m_profileService;
    Plasma::Service *m_profileService = m_engine->serviceForSource(profileQuery);
    KConfigGroup profileConf = m_profileService->operationDescription("auth");
    profileConf.writeEntry("password", m_password);
    m_profileService->startOperationCall(profileConf);
    connect(m_profileService, SIGNAL(finished(Plasma::ServiceJob*)), this, SLOT(serviceFinished(Plasma::ServiceJob*)));
}

void Twitter::serviceFinished(Plasma::ServiceJob *job)
{
    if (job->error()) {
        m_flash->flash(job->errorString(), 2000);
    }
}

void Twitter::openProfile()
{
    QAction *action = qobject_cast<QAction *>(sender());

    if (action) {
        KRun::runUrl( KUrl("http://www.twitter.com/" + action->data().toString()), "text/html", 0 );
    }
}

QString Twitter::timeDescription( const QDateTime &dt )
{
    int diff = dt.secsTo( QDateTime::currentDateTime() );
    QString desc;

    if( diff < 60 ) {
        desc = i18n( "Less than a minute ago" );
    }else if( diff < 60*60 ) {
        desc = i18np( "1 minute ago", "%1 minutes ago", diff/60 );
    } else if( diff < 2*60*60 ) {
        desc = i18n( "Over an hour ago");
    } else if( diff < 24*60*60 ) {
        desc = i18np( "1 hour ago", "%1 hours ago", diff/3600 );
    } else {
        desc = dt.toString( Qt::LocaleDate );
    }
    return desc;
}

#include "twitter.moc"
