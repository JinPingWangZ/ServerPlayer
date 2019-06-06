#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <VLCQtWidgets/WidgetSeek.h>
#include <VLCQtWidgets/WidgetVideo.h>
#include <VLCQtCore/Enums.h>
#include <VLCQtCore/VideoDelegate.h>
#include <VLCQtCore/Common.h>
#include <VLCQtCore/Instance.h>
#include <VLCQtCore/Media.h>
#include <VLCQtCore/MediaPlayer.h>
#include <QVBoxLayout>
#include <QDir>
#include <Windows.h>
#include <QPalette>
#include <QDebug>
#include <QPixmap>
#include <QMessageBox>
#include <QSettings>
#include <QTextStream>
#include <QIODevice>
#include <QFile>
#include <iostream>
#include <qmediainfo.h>
#include <QMovie>
#include <QDirIterator>
#include <QFileInfo>

static inline qint32 ArrayToInt(QByteArray source);
qint32 ArrayToInt(QByteArray source)
{
    qint32 temp;
    QDataStream data( &source, QIODevice::ReadWrite );
    data >> temp;
    return temp;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _media(0)
{
    ui->setupUi(this);
    //environment updating
    ui->menuBar->setVisible(false);
    ui->mainToolBar->setVisible(false);
    ui->statusBar->setVisible(false);

    m_vlcPlayerCount = 0;
    QPalette pal = palette();
    pal.setColor(QPalette::Background, Qt::black);
    setAutoFillBackground(true);
    setPalette(pal);

    this->raise();
    this->setFocus();

    widget = new QWidget(this);
    widget->setGeometry(QRect(-9, -9, this->width() + 18, this->height() + 18));

    QApplication::setOverrideCursor(Qt::BlankCursor);


//    showFullScreen();

    this->setGeometry( QRect(-6, 0, 99999, 99999) );
//    setWindowFlags(Qt::Widget |  Qt::FramelessWindowHint);
    setWindowFlags(this->windowFlags() | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint );
    this->menuBar()->hide();
//            this->show();

//    ui->centralWidget->setGeometry(QRect(-9, -9, 3000, 1000));

    qDebug() << this->width() << ' ' << this->height();
//    ui->widget->setGeometry(QRect(-9, -9, this->width() + 18, this->height() + 18));

    //AllowSetForegroundWindow(NULL);
    //SetForegroundWindow((HWND)winId());
    //setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    //h_PlayLayout = new QBoxLayout(QBoxLayout::LeftToRight, ui->widget);
    //h_PlayLayout->setGeometry(QRect(-9, -9, this->width() + 18, this->height() + 18));

    // Create Media Folder
    QDir dir(QDir::currentPath() + "/media");
    if (!dir.exists())
        dir.mkpath(".");

    // TcpServer start
    server = new QTcpServer(this);
    QObject::connect( server, SIGNAL(newConnection()), this, SLOT(newConnection()) );
    server->listen( QHostAddress::Any, 1024 );
    m_serverSocket = new QTcpSocket(this);

    _instance = new VlcInstance(VlcCommon::args(), this);
    _vplayer = new VlcMediaPlayer(_instance);

    connect(_vplayer, SIGNAL(end()), this, SLOT(playFinished()));

    //Download Configuration
    pManager = new QNetworkAccessManager();
    QObject::connect(pManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(configureReceived(QNetworkReply*)));


    manager = new DownloadManager();
    QObject::connect(manager, SIGNAL(finished()), this, SLOT(finishedConfigureDownload()));

    //create a timer
    p_PlayTimer = new QTimer(this);
    //setup signal and slot
    QObject::connect(p_PlayTimer, SIGNAL(timeout()), this, SLOT(playNextOrder()));

    p_OrderPlayTimer = new QTimer(this);

    QObject::connect(p_OrderPlayTimer, SIGNAL(timeout()), this, SLOT(playNextInSameOrder()));

    p_OrderAllPlayTimer = new QTimer(this);

    QObject::connect(p_OrderAllPlayTimer, SIGNAL(timeout()), this, SLOT(playAllInSameOrder()));

    initialize();

}

MainWindow::~MainWindow()
{
    delete _instance;
    delete ui;
}

void MainWindow::initialize()
{

    m_IsDownloadingStatus = false;
    m_ConfigureInitialized = false;
    m_OrderItems.clear();
    n_MediaList.clear();
    m_CurrentPlayOrder = 0;
    m_CurrentPlayInSameOrder = 0;
    coor_count = 0;
    p_PlayTimer->stop();
    p_OrderPlayTimer->stop();
    p_OrderAllPlayTimer->stop();
    m_CurrentConfigure.clear();

    loadSettings();

    //ui->txtUrl->setText(mConfigureURL);
    //ui->txtPeriod->setText(QString::number(mCheckingPeriod));

    // Setup Max Client Count
    //mMaxClients = 100;
    //mClientCount = 0;


    //updateConnectedClientCount();

    //mDisplayItems.clear();

    m_ConfigureInitialized = false;
    if (m_confUrl.length() != 0 && m_CurrentConfigure.length() != 0)
        getConfigure();

}

void MainWindow::saveLastFile(QString fileName, QDateTime currentDateTime)
{
    QFile file(QDir::currentPath() + "/lastfile.csv");
    if(file.open(QIODevice::WriteOnly | QIODevice::Text)){
        QTextStream stream(&file);
        QString strToSave = fileName + ',' + currentDateTime.toString("yyyy-MM-dd hh:mm:ss");
        stream << strToSave;
        qDebug() << "Writing CSV finished";
        file.close();
    }

}

void MainWindow::saveLog(QString fileName, int logInfo)
{
    QFile file(QDir::currentPath() + "/log.txt");
    if(file.open(QFile::Append | QIODevice::Text)){
        file.seek(file.size());
        QTextStream stream(&file);
        QString logTemplate;
        switch(logInfo){
        case LOG_CANT_PLAY_IMAGE:
            logTemplate = "The image has illegal format.";
            break;
        case LOG_CANT_PLAY_VIDEO:
            logTemplate = "This video has illegal format.";
            break;
        case LOG_MEDIA_OUT_TIME:
            logTemplate = "This media is out of date and time.";
            break;
        }
        QString strToSave = fileName + " - " + logTemplate;
        stream << strToSave;
        stream << endl;
        qDebug() << "Writing log finished.";
        file.close();
    }
}

bool MainWindow::isValidVideo(QString fileName)
{
    QString filePath = QDir::currentPath() + "/media/" + fileName;
    QMediaInfo mi;
    mi.Open(filePath);
    if (mi.VideoCodec().isEmpty()){
        return false;
    }
    return true;
}

bool MainWindow::isValidImage(QString fileName)
{
    QString filePath = QDir::currentPath() + "/media/" + fileName;

    QPixmap pic(filePath);
    qDebug() << filePath << pic;
    if(pic.isNull())
        return false;
    return true;
}

int MainWindow::getVideoDuration(QString fileName)
{
    QString filePath = QDir::currentPath() + "/media/" + fileName;
    QMediaInfo mi;
    mi.Open(filePath);
    QString strTime = mi.Duration();
    int duration;
    int hh, mm, ss, mms;

    hh = strTime.mid(0, 2).toInt();
    mm = strTime.mid(3, 2).toInt();
    ss = strTime.mid(6, 2).toInt();
    mms = strTime.mid(9, 3).toInt();

    duration = ( hh * 3600 + mm * 60 + ss ) * 1000 + mms;
    return duration;
}

void MainWindow::showLoadingIcon()
{
    QMovie *movie = new QMovie(QDir::currentPath() + "/waiting.gif");
    //movie->setScaledSize(QSize(128, 128));
    QLabel *processLabel = new QLabel(this);
    QRect a(800, 200, 600, 600);
    processLabel->setGeometry(a);
    processLabel->setMovie(movie);
    if(m_IsDownloadingStatus)
    {
        movie->start();
        processLabel->show();
    }
    else
        processLabel->hide();
}

void MainWindow::removeAllMediaFiles()
{
    QDirIterator it(QDir::currentPath() + "/media", QDirIterator::Subdirectories);
    QStringList illegalFileTypes;
    illegalFileTypes << ".jpg" << ".gif" << ".png" << ".mov" << ".mp4" << ".php";

    while (it.hasNext())
    {
        qDebug() << "Processing: " <<it.next();

        bool illegalFile;

        foreach (QString illegalType, illegalFileTypes)
        {
            if (it.fileInfo().absoluteFilePath().endsWith(illegalType, Qt::CaseInsensitive))
            {
                illegalFile = true;
                break;
            }
            else
            {
                illegalFile = false;
            }
        }

        if (illegalFile)
        {
            QDir dir;
            dir.remove(it.filePath());
            qDebug() << "Removed unsafe file.";
        }
    }
}

void MainWindow::removeCertainMedia(QString fileName)
{
    QString filePath = QDir::currentPath() + "/media/" + fileName;
    QDir dir;
    dir.remove(filePath);
}

void MainWindow::DownloadInvalidMedia(QString fileUrl)
{
    QStringList fileList;
    fileList.push_back(fileUrl);
    manager->clearHistory();
    manager->append(fileList);
}


void MainWindow::finishedConfigureDownload()
{
    qDebug() << "Finished Configure Download!";
    //ui->downloading_label->setVisible(false);
    if (m_ConfigureInitialized == true)
        m_ConfigureInitialized = false;
    else
        playNextOrder();
    m_IsDownloadingStatus = false;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Q){
        p_PlayTimer->stop();
        this->close();
    }
    else if(event->key() == Qt::Key_L)
    {
        loadSettings();
        Settings *m_ConfigureDig = new Settings(this, m_confUrl);
        m_ConfigureDig->setWindowFlags(Qt::SplashScreen);
        if(m_ConfigureDig->exec() == QDialog::Accepted)
        {
            m_confUrl = m_ConfigureDig->getConfigureUrl();
            qDebug() << m_confUrl;
            m_IsDownloadingStatus = true;
            m_ConfigureInitialized = true;
            //Download php file
            manager->clearHistory();
            manager->append(QUrl(m_confUrl));

            this->raise();
            this->setFocus();
            QApplication::setOverrideCursor(Qt::BlankCursor);

            m_OrderItems.clear();
            n_MediaList.clear();
            m_CurrentPlayOrder = 0;

            qDebug() << "m_currentPlayOrder:------------ " << m_CurrentPlayOrder;
//            sendMessage( QString::number(m_CurrentPlayOrder) );

            m_CurrentPlayInSameOrder = 0;
            coor_count = 0;
            p_PlayTimer->stop();
            p_OrderPlayTimer->stop();
            p_OrderAllPlayTimer->stop();
            m_CurrentConfigure.clear();
            getConfigure();
        }
    }
}

QStringList MainWindow::getContents(QString body, QString name)
{
    QStringList out;

    body = body.replace("\t", "");
    body = body.replace("\n", "");

    if(body.indexOf("<" + name + ">") >= 0)
    {
        QStringList contents_tmp = body.split("<" + name + ">");

        foreach(QString part, contents_tmp)
        {
            if(part.indexOf("</" + name + ">") > 0)
            {
                QStringList part_lst = part.split("</" + name + ">");
                if(part_lst.length() > 1)
                    out.push_back(QString(part_lst.at(0)));
            }
        }
    }

    return out;
}

void MainWindow::getConfigure()
{
    manager->clearHistory();
    //remove all media files in media folder
    //removeAllMediaFiles();

    //send request to get new configure
    mRequest.setUrl(QUrl(m_confUrl));
    //showLoadingIcon();
    pManager->get(mRequest);
}

void MainWindow::configureReceived(QNetworkReply *reply)
{
    if(reply->error()){
        qDebug() << reply->errorString();
        return;
    }
    m_IsDownloadingStatus = false;
    //showLoadingIcon();
    QString answer = reply->readAll();
    qDebug() << answer;

    //Update new Configuration
    updateConfigure(answer);

}

void MainWindow::updateConfigure(QString conf)
{
    //Update new Configuration
    m_CurrentConfigure = conf;
    saveSettings();
    QStringList playlist = getContents(m_CurrentConfigure, "playlist");

    QStringList arguments;
    if(playlist.length() == 1)
    {
        //Parse Playlist
        QString playlist_contents = (QString)(playlist.at(0));
        QStringList itemlist = getContents(playlist_contents, "item");

        foreach(QString item_contents, itemlist)
        {
           //Parse a Item(Order)
           QStringList orderId_contents = getContents(item_contents, "order");
           QStringList embed_contents = getContents(item_contents, "embed");
           QStringList coordinate_contents = getContents(item_contents, "coordinates");
           QStringList display_contents;
           QStringList duration_contents;
           QStringList startDate_contents;
           QStringList endDate_contents;
           QStringList startTime_contents;
           QStringList endTime_contents;
           QString real_display_content;
           QDate sdate, edate;
           QTime stime, etime;
           PlayDate pDate;

           QString coorRectStr = coordinate_contents.at(0);
           QRect coorRect = getCoordinateRect(coorRectStr);

           int new_order_number = QString(orderId_contents.at(0)).toInt();

           //Create new order
           OrderItem *new_order = new OrderItem();
           new_order->setOrderNumber(QString(orderId_contents.at(0)).toInt());

           if (embed_contents.length() == 1){
                QString entrylist_content = (QString)embed_contents.at(0);
                QStringList entrylist = getContents(entrylist_content, "entry");

                foreach (QString item_entrylist, entrylist) {

                    display_contents = getContents(item_entrylist, "file");
                    duration_contents = getContents(item_entrylist, "duration");

                    startDate_contents = getContents(item_entrylist, "startdate");
                    endDate_contents = getContents(item_entrylist, "stopdate");
                    startTime_contents = getContents(item_entrylist, "starttime");
                    endTime_contents = getContents(item_entrylist, "stoptime");
                    if(startDate_contents.length() != 0)
                        sdate = QDate::fromString(QString(startDate_contents.at(0)), "yyyy-MM-dd");
                    else
                        sdate = QDate::fromString(QString("0000-00-00"), "yyyy-MM-dd");
                    if(endDate_contents.length() != 0)
                        edate = QDate::fromString(QString(endDate_contents.at(0)), "yyyy-MM-dd");
                    else
                        edate = QDate::fromString(QString("0000-00-00"), "yyyy-MM-dd");
                    if(startTime_contents.length() != 0)
                        stime = QTime::fromString(QString(startTime_contents.at(0)), "hh:mm:ss");
                    else
                        stime = QTime::fromString(QString("00:00:00"), "hh:mm:ss");
                    if(endTime_contents.length() != 0)
                        etime = QTime::fromString(QString(endTime_contents.at(0)), "hh:mm:ss");
                    else
                        etime = QTime::fromString(QString("00:00:00"), "hh:mm:ss");
                    pDate.init(sdate, edate, stime, etime);

                    if(orderId_contents.length() < 0 || duration_contents.length() < 0)
                        continue;

                    new_order->setOrderDuration(QString(duration_contents.at(0)).toInt());
                    new_order->setOrderStatus("embed");

                    if(display_contents.length() == 1)
                    {
                        real_display_content = display_contents.at(0);
                        real_display_content.remove('\r');
                        qDebug() << display_contents.at(0);
                        arguments.push_back(real_display_content);
                        new_order->pushMedia(real_display_content);
                        new_order->pushCoordinate(coorRect);
                        new_order->pushPlayDate(pDate);
                    }
                    else
                    {
                        new_order->pushMedia("");
                        new_order->pushCoordinate(coorRect);
                        new_order->pushPlayDate(pDate);
                    }
                }
           }
           else{
               duration_contents = getContents(item_contents, "duration");
               startDate_contents = getContents(item_contents, "startdate");
               endDate_contents = getContents(item_contents, "stopdate");
               startTime_contents = getContents(item_contents, "starttime");
               endTime_contents = getContents(item_contents, "stoptime");
               sdate = QDate::fromString(QString(startDate_contents.at(0)), "yyyy-MM-dd");
               edate = QDate::fromString(QString(endDate_contents.at(0)), "yyyy-MM-dd");
               stime = QTime::fromString(QString(startTime_contents.at(0)), "hh:mm:ss");
               etime = QTime::fromString(QString(endTime_contents.at(0)), "hh:mm:ss");
               pDate.init(sdate, edate, stime, etime);

               if(orderId_contents.length() < 0 || duration_contents.length() < 0)
                   continue;

               new_order->setOrderDuration(QString(duration_contents.at(0)).toInt());
               new_order->setOrderStatus("");

               qDebug() << "order : " << new_order->getOrderNumber();
               qDebug() << "duration : " << new_order->getOrderDuration();

               display_contents = getContents(item_contents, "file");

               if(display_contents.length() == 1)
               {
                   real_display_content = display_contents.at(0);
                   real_display_content.remove('\r');
                   qDebug() << display_contents.at(0);
                   arguments.push_back(real_display_content);
                   new_order->pushMedia(real_display_content);
                   new_order->pushCoordinate(coorRect);
                   new_order->pushPlayDate(pDate);
               }
               else
               {
                   new_order->pushMedia("");
                   new_order->pushCoordinate(coorRect);
                   new_order->pushPlayDate(pDate);
               }
           }

           //Checking with previous order
           if(!m_OrderItems.empty()){
                OrderItem *prev_order = m_OrderItems.back();
                int prev_order_number = prev_order->getOrderNumber();
                if(prev_order_number == new_order_number && prev_order->getOrderStatus() != "embed"){
                    //add media to previous order
                    duration_contents = getContents(item_contents, "duration");
                    duration_contents = getContents(item_contents, "duration");
                    startDate_contents = getContents(item_contents, "startdate");
                    endDate_contents = getContents(item_contents, "stopdate");
                    startTime_contents = getContents(item_contents, "starttime");
                    endTime_contents = getContents(item_contents, "stoptime");
                    sdate = QDate::fromString(QString(startDate_contents.at(0)), "yyyy-MM-dd");
                    edate = QDate::fromString(QString(endDate_contents.at(0)), "yyyy-MM-dd");
                    stime = QTime::fromString(QString(startTime_contents.at(0)), "hh:mm:ss");
                    etime = QTime::fromString(QString(endTime_contents.at(0)), "hh:mm:ss");
                    pDate.init(sdate, edate, stime, etime);

                    if(orderId_contents.length() < 0 || duration_contents.length() < 0)
                        continue;

                    prev_order->setOrderDuration(QString(duration_contents.at(0)).toInt());

                    qDebug() << "order : " << prev_order->getOrderNumber();
                    qDebug() << "duration : " << prev_order->getOrderDuration();

                    display_contents = getContents(item_contents, "file");

                    if(display_contents.length() == 1)
                    {
                        real_display_content = display_contents.at(0);
                        real_display_content.remove('\r');
                        qDebug() << display_contents.at(0);
                        arguments.push_back(real_display_content);
                        prev_order->pushMedia(real_display_content);
                        prev_order->pushCoordinate(coorRect);
                        prev_order->pushPlayDate(pDate);
                    }
                    else
                    {
                        prev_order->pushMedia("");
                        prev_order->pushCoordinate(coorRect);
                        prev_order->pushPlayDate(pDate);
                    }
                }
                else{
                    m_OrderItems.push_back(new_order);
                }
           }
           else{
               m_OrderItems.push_back(new_order);
           }

        }
    }

    manager->clearHistory();
    manager->append(arguments);

}

//play next order
void MainWindow::playNextOrder()
{

    if(_vplayer->state() == Vlc::Playing)
        _vplayer->stop();

    qDebug() << "Current Play Order : " << m_CurrentPlayOrder + 1;
    p_PlayTimer->stop();

    QString fileName;
    /*
    delete widget;
    widget = new QWidget(this);
    */
    qDebug() << widget->width() << widget->height();
    n_MediaList = m_OrderItems.at(m_CurrentPlayOrder)->getAllMedia();
    QRect coorRect = m_OrderItems.at(m_CurrentPlayOrder)->getCoordinate(0);

    widget->setGeometry(QRect(0, 0, this->width(), this->height()));

    clearWidgets();
    if(n_MediaList.length() == 1){

        fileName = m_OrderItems.at(m_CurrentPlayOrder)->getMedia(1);

        qDebug() << "Playing file Name : " << fileName;
        qDebug() << "Playing file Rect : " << coorRect;
        qDebug() << "Playing duration : "  << m_OrderItems.at(m_CurrentPlayOrder)->getOrderDuration();
        PlayDate pd = m_OrderItems.at(m_CurrentPlayOrder)->getPlayDate(0);
        qDebug() << "Available time : " << pd.startDate << ' ' << pd.endDate << ' ' << pd.startTime << ' ' << pd.endTime;

        QUrl n_FileUrl(fileName);
        QString fn = n_FileUrl.fileName();
        QDateTime currentDateTime = QDateTime::currentDateTime();

        if (pd.isValidDate(currentDateTime)){

            qDebug() << "m_CurrentPlayOrder:++++++++ " << m_CurrentPlayOrder;
            sendMessage( QString::number(m_CurrentPlayOrder) );

            if (getMediaType(fn) == IS_IMAGE){
                m_imageList[0] = new QLabel(widget);
                m_imageList[0]->setGeometry(QRect(coorRect.x() - 9, coorRect.y() - 9, coorRect.width(), coorRect.height()));

                //this->setCentralWidget(v_PlayLayout);
                //ui->widget->setLayout(v_PlayLayout);

                saveLastFile(fn, currentDateTime);
                if (!isValidImage(fn)){
                    saveLog(fn, LOG_CANT_PLAY_IMAGE);
                    p_PlayTimer->start(0);
                }
                else
                {
                    playImage(fn, m_imageList[0], coorRect.width(), coorRect.height());
                    p_PlayTimer->start(1000 * m_OrderItems.at(m_CurrentPlayOrder)->getOrderDuration());
                }

                m_CurrentPlayOrder ++;
//                sendMessage( QString::number(m_CurrentPlayOrder) );

                //h_PlayLayout->addWidget(imagewidget);
            }
            else if(getMediaType(fn) == IS_VIDEO){
                //qDebug() << ui->widget->width() << ui->widget->height();
                m_vlcwidgetList[0] = new VlcWidgetVideo(widget);
                saveLastFile(fn, currentDateTime);
                if(!isValidVideo(fn)){
                    saveLog(fn, LOG_CANT_PLAY_VIDEO);
                    p_PlayTimer->start(0);
                }
                else
                {
                    playVideo(fn, coorRect, m_vlcwidgetList[0]);
                    //p_PlayTimer->start(1000 * m_OrderItems.at(m_CurrentPlayOrder)->getOrderDuration());
                    p_PlayTimer->start(getVideoDuration(fn));
                }

                m_CurrentPlayOrder ++;
            }
            else{
                p_PlayTimer->start(0);
                m_CurrentPlayOrder ++;
//                sendMessage( QString::number(m_CurrentPlayOrder) );
            }
        }
        else{
            saveLog(fn, LOG_MEDIA_OUT_TIME);
            p_PlayTimer->start(0);
            m_CurrentPlayOrder ++;
//            sendMessage( QString::number(m_CurrentPlayOrder) );
        }
    }
    else{
        if(m_OrderItems.at(m_CurrentPlayOrder)->getOrderStatus() == "embed"){
            m_CurrentPlayInSameOrder = -1;
            i_Rect = coorRect;
            playNextInSameOrder();
        }
        else{
            coor_count = 0;
            playAllInSameOrder();
        }
    }

    if(m_CurrentPlayOrder >= m_OrderItems.length()) {
        m_CurrentPlayOrder = 0;
        sendMessage( QString::number(m_CurrentPlayOrder) );
    }
}

//For plaing embed medias in one order
void MainWindow::playNextInSameOrder()
{
    if(_vplayer->state() == Vlc::Playing)
        _vplayer->stop();

    p_OrderPlayTimer->stop();
    clearWidgets();

    m_CurrentPlayInSameOrder ++;
    qDebug() << m_OrderItems.at(m_CurrentPlayOrder)->getOrderDuration();


    if(m_CurrentPlayInSameOrder >= n_MediaList.length()){
        p_OrderPlayTimer->stop();
        m_CurrentPlayOrder ++;
        if(m_CurrentPlayOrder >= m_OrderItems.length()) {

            m_CurrentPlayOrder = 0;
            sendMessage( QString::number(m_CurrentPlayOrder) );
        }
        playNextOrder();
        return;
    }

//    delete widget;
//    widget = new QWidget(this);
    widget->setGeometry(QRect(-9, -9, this->width() + 18, this->height() + 18));

    PlayDate pd = m_OrderItems.at(m_CurrentPlayOrder)->getPlayDate(m_CurrentPlayInSameOrder);
    QString fileName = n_MediaList.at(m_CurrentPlayInSameOrder);
    qDebug() << "Playing file Name : " << fileName;
    qDebug() << "Playing file Rect : " << i_Rect;
    qDebug() << "Available time : " << pd.startDate << ' ' << pd.endDate << ' ' << pd.startTime << ' ' << pd.endTime;

    QUrl n_FileUrl(fileName);
    QString fn = n_FileUrl.fileName();

    QDateTime currentDateTime = QDateTime::currentDateTime();

    if (pd.isValidDate(currentDateTime)){
        if (getMediaType(fn) == IS_IMAGE){
            m_imageList[0] = new QLabel(widget);
            m_imageList[0]->setGeometry(QRect(i_Rect.x(), i_Rect.y(), i_Rect.width(), i_Rect.height()));
            saveLastFile(fn, currentDateTime);
            if (!isValidImage(fn)){
                saveLog(fn, LOG_CANT_PLAY_IMAGE);
                p_OrderPlayTimer->start(0);
            }
            else{
                playImage(fn, m_imageList[0], i_Rect.width(), i_Rect.height());
                p_OrderPlayTimer->start(1000 * m_OrderItems.at(m_CurrentPlayOrder)->getOrderDuration());
            }
        }
        else if(getMediaType(fn) == IS_VIDEO){
            m_vlcwidgetList[0] = new VlcWidgetVideo(widget);
            saveLastFile(fn, currentDateTime);
            if (!isValidVideo(fn)){
                saveLog(fn, LOG_CANT_PLAY_VIDEO);
                p_OrderPlayTimer->start(0);
            }
            else{
                playVideo(fn, i_Rect, m_vlcwidgetList[0]);
                //p_OrderPlayTimer->start(1000 * m_OrderItems.at(m_CurrentPlayOrder)->getOrderDuration());
                p_OrderPlayTimer->start(getVideoDuration(fn));
            }
        }
        else{
            p_OrderPlayTimer->start(0);
        }
    }
    else{
        saveLog(fn, LOG_MEDIA_OUT_TIME);
        p_OrderPlayTimer->start(0);
    }

}

//For playing all medias in same order
void MainWindow::playAllInSameOrder(){

    if(_vplayer->state() == Vlc::Playing)
        _vplayer->stop();

    p_OrderAllPlayTimer->stop();
    clearWidgets();


    if(coor_count >= m_OrderItems.at(m_CurrentPlayOrder)->getMediaCount())
    {
        qDebug() << "media count" << m_OrderItems.at(m_CurrentPlayOrder)->getMediaCount();
        p_OrderAllPlayTimer->stop();
        m_playerItems.clear();
        coor_count = 0;
        m_CurrentPlayOrder ++;
        sendMessage( QString::number(m_CurrentPlayOrder) );
        if(m_CurrentPlayOrder >= m_OrderItems.length()) {
            m_CurrentPlayOrder = 0;
            sendMessage( QString::number(m_CurrentPlayOrder) );
        }
        playNextOrder();
        return;
    }

    //delete widget;
    //widget = new QWidget(this);
    widget->setGeometry(QRect(-9, -9, this->width() + 18, this->height() + 18));

    m_playerItems.clear();
    m_vlcPlayerCount = 0;
    coor_count = 0;

    int out_date_media_count = 0;

    foreach (QString item_media, n_MediaList) {

        QRect display_rect = m_OrderItems.at(m_CurrentPlayOrder)->getCoordinate(coor_count);
        PlayDate pd = m_OrderItems.at(m_CurrentPlayOrder)->getPlayDate(coor_count);
        qDebug() << "Playing file Name : " << item_media;
        qDebug() << "Playing file Rect : " << display_rect;
        qDebug() << "Playing duration : " << m_OrderItems.at(m_CurrentPlayOrder)->getOrderDuration();
        qDebug() << "Available time : " << pd.startDate << ' ' << pd.endDate << ' ' << pd.startTime << ' ' << pd.endTime;

        QUrl n_FileUrl(item_media);
        QString fn = n_FileUrl.fileName();

        QDateTime currentDateTime = QDateTime::currentDateTime();

        if (pd.isValidDate(currentDateTime)){
            if (getMediaType(fn) == IS_IMAGE){
                m_imageList[coor_count] = new QLabel(widget);
                m_imageList[coor_count]->setGeometry(display_rect);
                saveLastFile(fn, currentDateTime);
                playImage(fn, m_imageList[coor_count], display_rect.width(), display_rect.height());

            }
            else if(getMediaType(fn) == IS_VIDEO){
                VlcMediaPlayer *vplayer = new VlcMediaPlayer(_instance);
                m_playerItems.push_back(vplayer);
                m_vlcwidgetList[coor_count] = new VlcWidgetVideo(widget);
                saveLastFile(fn, currentDateTime);
                playVideoConcurrently(fn, display_rect, m_playerItems.at(m_vlcPlayerCount), m_vlcwidgetList[coor_count]);
                m_vlcPlayerCount ++;
            }
        }
        else{
            saveLog(fn, LOG_MEDIA_OUT_TIME);
            out_date_media_count ++;
        }

        coor_count ++;
    }

    if(out_date_media_count == coor_count)
        p_OrderAllPlayTimer->start(0);
    else
        p_OrderAllPlayTimer->start(1000 * m_OrderItems.at(m_CurrentPlayOrder)->getOrderDuration());
}

int MainWindow::getMediaType(QString fileName)
{
    QString filePath = QDir::currentPath() + "/media/" + fileName;
    QFileInfo fi(fileName);
    QString ext = fi.completeSuffix();
    if(ext == "jpg" || ext == "png" || ext == "gif")
        return IS_IMAGE;
    else if(ext == "mov" || ext == "mp4")
        return IS_VIDEO;
    return 0;
}

void MainWindow::playImage(QString fileName, QLabel *imagewidget, int width, int height)
{
    if(_vplayer->state() == Vlc::Playing)
        _vplayer->stop();

    QString filePath = QDir::currentPath() + "/media/" + fileName;
    QPixmap pic(filePath);
    if(pic.isNull()){
        saveLog(fileName, LOG_CANT_PLAY_IMAGE);
        return;
    }
    else
    {
        QPixmap scaled = pic.scaled( width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation );
        imagewidget->setPixmap(scaled);
        imagewidget->show();
    }

    /*
    ui->imagewidget->setScaledContents(true);
    ui->imagewidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    */
}

void MainWindow::playVideo(QString fileName, QRect coorRect, VlcWidgetVideo *_vlcWidget)
{

    qDebug() << "---------playVideo-----------------------";
    QString filePath = QDir::currentPath() + "/media/" + fileName;

    _vlcWidget->setGeometry(QRect(coorRect.x() - 9, coorRect.y()  - 9, coorRect.width() + 18, coorRect.height() + 18));

    _vplayer->setVideoWidget(_vlcWidget);
    _vlcWidget->setMediaPlayer(_vplayer);


    VlcMedia *vmedia = new VlcMedia(filePath, true, _instance);
    if(_vplayer->state() == Vlc::Playing)
        _vplayer->stop();

//    if ( !isValidVideo(fileName)){
//        saveLog(fileName, LOG_CANT_PLAY_VIDEO);
//        return;
//    }
    else{
        _vplayer->open(vmedia);
        _vlcWidget->show();
    }
    delete vmedia;
}

void MainWindow::playVideoConcurrently(QString fileName, QRect coorRect, VlcMediaPlayer *vplayer, VlcWidgetVideo *_vlcWidget)
{

    qDebug() << "---------playVideoConcurrently-----------------------";
    QString filePath = QDir::currentPath() + "/media/" + fileName;

    _vlcWidget->setGeometry(QRect(coorRect.x() - 9, coorRect.y() - 9, coorRect.width() + 18, coorRect.height() + 18));

    vplayer->setVideoWidget(_vlcWidget);
    _vlcWidget->setMediaPlayer(vplayer);

    VlcMedia *vmedia = new VlcMedia(filePath, true, _instance);
    if(vplayer->state() == Vlc::Playing)
        vplayer->stop();

    if ( !isValidVideo(fileName)){
        saveLog(fileName, LOG_CANT_PLAY_VIDEO);
        return;
    }
    else{
        vplayer->open(vmedia);
        _vlcWidget->show();
    }

    delete vmedia;
}

void MainWindow::playFinished()
{
    _vplayer->stop();
    //delete _vplayer;
}

void MainWindow::newConnection()
{

    while ( server->hasPendingConnections() ) {

        m_serverSocket = server->nextPendingConnection();
//        socket = new QTcpSocket(this);
        qDebug() << "newConnection: " << m_serverSocket->state();
        serverConnectedState = true;

        connect( m_serverSocket, SIGNAL(readyRead()), this, SLOT(onReadyRead()) );
        connect( m_serverSocket, SIGNAL(connected()), this, SLOT(onConnected()), Qt::UniqueConnection);
        connect( m_serverSocket, SIGNAL(disconnected()), this, SLOT(onDisconnected()) );

        connect( m_serverSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onConnectionError()), Qt::UniqueConnection );
        connect( m_serverSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onStateChange()), Qt::UniqueConnection);

        QByteArray* buffer = buffers.value( m_serverSocket );
        qint32* s = new qint32(0);
        buffers.insert( m_serverSocket, buffer );
        sizes.insert( m_serverSocket, s );

        sendMessage( QString::number(m_CurrentPlayOrder) );
    }
}

void MainWindow::onConnected()
{
    qDebug() << "Connected!";

}

void MainWindow::onDisconnected()
{
    qDebug() << "disConnected!";
    serverConnectedState = false;

    m_serverSocket = static_cast<QTcpSocket*>( sender() );
    QByteArray* buffer = buffers.value( m_serverSocket );
    qint32* s = sizes.value( m_serverSocket );
    m_serverSocket->deleteLater();
    delete buffer;
    delete s;
}

void MainWindow::onConnectionError()
{
    qDebug() << "connectionError! " << m_serverSocket->errorString();
}

void MainWindow::onStateChange()
{
    qDebug() << "stateChange!: " << m_serverSocket->state();
}

void MainWindow::onReadyRead()
{
    qDebug() << "Server::readyRead()";
//    QTcpSocket* socket = static_cast<QTcpSocket*>( sender() );
    m_serverSocket = static_cast<QTcpSocket*>( sender() );
    QByteArray buffer;
    qint32* s = sizes.value( m_serverSocket );
    qint32 size = *s;

    qDebug() << "m_serverSocket.bytesAvailable: " << m_serverSocket->bytesAvailable();
    while ( m_serverSocket->bytesAvailable() > 0 ) {

//        buffer->append( (QByteArray&)socket->readAll() );
        buffer.append( m_serverSocket->readAll() );
        qDebug() << "buffer.size: " << buffer.size();

        size = ArrayToInt( buffer.mid(0, 4) );
        *s = size;
        buffer.remove( 0, 4 );

        QString str = QString::fromUtf8( buffer );
        qDebug() << "Recevie: " << str;

    }
}

QRect MainWindow::getCoordinateRect(QString rec)
{
    QStringList strList;
    strList = rec.split(',');
    int position[4], c = 0;
    foreach(QString item, strList){
        position[c] = item.toInt();
        c ++;
    }
    QRect rect;
    rect.setX(position[0]);
    rect.setY(position[1]);
    rect.setWidth(position[2] - position[0]);
    rect.setHeight(position[3] - position[1]);
    return rect;
}

void MainWindow::sendMessage(QString msgToSend)
{
//    if ( m_serverSocket->state() != QAbstractSocket::ConnectedState )
//        return;
    if ( !serverConnectedState )
        return;

    QByteArray vDataToSend;
    QDataStream vStream( &vDataToSend, QIODevice::WriteOnly );
    vStream.setByteOrder( QDataStream::LittleEndian );
    vStream << msgToSend.length();
    vDataToSend.append( msgToSend );

    m_serverSocket->write( vDataToSend, vDataToSend.length() );
    m_serverSocket->waitForBytesWritten();
}

void MainWindow::clearWidgets()
{
    int i;
    for (i = 0 ; i < m_imageList.count() ; i ++)
        delete m_imageList[i];
    for (i = 0 ; i < m_vlcwidgetList.count() ; i ++)
        delete m_vlcwidgetList[i];
    for (i = 0 ; i < m_playerItems.count() ; i ++)
        delete m_playerItems[i];
    m_imageList.clear();
    m_vlcwidgetList.clear();
    m_playerItems.clear();
}

void MainWindow::loadSettings()
{
    QSettings settings(APP_NAME, QSettings::IniFormat);

    m_confUrl = settings.value(CONFIGURE_URL, "http://52.40.117.153/cache/files/343.php").toString();
    //mCheckingPeriod = settings.value(CHECKING_PERIOD, 10).toInt();
    m_LastUpdatedTime = settings.value(LAST_UPDATED_TIME).toString();
    m_CurrentConfigure = settings.value(LAST_CONFIGURATION).toString();
}

void MainWindow::saveSettings()
{
    QSettings settings(APP_NAME, QSettings::IniFormat);

    settings.setValue(CONFIGURE_URL, m_confUrl);
    //settings.setValue(CHECKING_PERIOD, mCheckingPeriod);
    settings.setValue(LAST_UPDATED_TIME, m_LastUpdatedTime);
    settings.setValue(LAST_CONFIGURATION, m_CurrentConfigure);
}

