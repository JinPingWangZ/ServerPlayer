#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QKeyEvent>
#include <QLabel>

#include "downloadmanager.h"
#include "settings.h"
#include "orderitem.h"
#include "playdate.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QMap>
#include <QtCore>
#include <QtWidgets>
#include <QtNetwork>
#include <QtGui>

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QElapsedTimer>

#define     IS_IMAGE 1
#define     IS_VIDEO 2
#define     LOG_CANT_PLAY_IMAGE 3
#define     LOG_CANT_PLAY_VIDEO 4
#define     LOG_MEDIA_OUT_TIME 5
#define     APP_NAME "SignageServer@bstar"
#define     LAST_UPDATED_TIME "LastUpdatedTime"
#define     LAST_CONFIGURATION  "LastConfiguration"
#define     CONFIGURE_URL "ConfigureURL"
#define     CHECKING_PERIOD "CheckingPeriod"

namespace Ui {
class MainWindow;
}

class VlcInstance;
class VlcMediaPlayer;
class VlcWidgetVideo;
class VlcMedia;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public:
    QStringList getContents(QString body, QString name);
    int getMediaType(QString);
    QRect getCoordinateRect(QString);

    void sendMessage( QString msgToSend );


protected:
    void keyPressEvent(QKeyEvent *event);
    void getConfigure();
    void updateConfigure(QString conf);
    void playImage(QString, QLabel*, int, int);
    void playVideo(QString, QRect, VlcWidgetVideo*);
    void playVideoConcurrently(QString, QRect, VlcMediaPlayer*, VlcWidgetVideo*);
    void clearWidgets();
    void loadSettings();
    void saveSettings();
    void initialize();
    void saveLastFile(QString, QDateTime);
    void saveLog(QString, int);
    bool isValidVideo(QString);
    bool isValidImage(QString);
    int getVideoDuration(QString);
    void showLoadingIcon();
    void removeAllMediaFiles();
    void removeCertainMedia(QString);
    void DownloadInvalidMedia(QString);


private:
    QString                         m_confUrl;
    QString                         m_CurrentConfigure;
    QString                         m_LastUpdatedTime;

    QTcpSocket* m_serverSocket;
    QTcpServer* server;

    QVector<QTcpSocket*> mClients;

    QHash<QTcpSocket*, QByteArray*> buffers;
    QHash<QTcpSocket*, qint32*> sizes;

    bool serverConnectedState;

private:
    Ui::MainWindow                  *ui;
    //Vlc classes
    VlcInstance                     *_instance;
    VlcMedia                        *_media;
    VlcMediaPlayer                  *_vplayer;
    QList<VlcMediaPlayer*>          m_playerItems;

    QNetworkAccessManager           *pManager;
    QNetworkRequest                 mRequest;
    QList<OrderItem*>               m_OrderItems;
    QStringList                     n_MediaList; //For several medias in one order
    DownloadManager                 *manager;

    QTimer*                         p_PlayTimer;
    QTimer*                         p_OrderPlayTimer; // serveral plays for one order
    QTimer*                         p_OrderAllPlayTimer; //to show on one screen for one order

    QRect                           i_Rect;
    QWidget                         *widget; //playing widget
    QMap<int, QLabel*>              m_imageList;
    QMap<int, VlcWidgetVideo*>      m_vlcwidgetList;

    int                             m_CurrentPlayOrder;
    int                             m_CurrentPlayInSameOrder;
    int                             m_vlcPlayerCount;
    int                             coor_count;
    bool                            m_IsDownloadingStatus;
    bool                            m_ConfigureInitialized;

private slots:
    void finishedConfigureDownload();
    void configureReceived(QNetworkReply*);
    void playNextOrder(); //plays next media
    void playNextInSameOrder(); //plays next media in embed of same order
    void playAllInSameOrder();  //plays all medias in same order
    void playFinished(); //if vlc player playing finished

    void newConnection();
    void onConnected();
    void onDisconnected();
    void onConnectionError();
    void onStateChange();
    void onReadyRead();
};

#endif // MAINWINDOW_H
