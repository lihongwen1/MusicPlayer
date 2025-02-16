﻿#include "musicplayer.h"
#include "ui_musicplayer.h"
#include <QGraphicsDropShadowEffect>
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include "iconhelper.h"
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QPropertyAnimation>
#include "libzplay.h"
#include "musicmgr.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWindow>
#include <QKeyEvent>



#ifdef WIN32
#include "Windows.h"
#pragma comment(lib, "User32.lib")
#endif

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

using namespace libZPlay;

MusicPlayer::MusicPlayer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MusicPlayer)
{
    ui->setupUi(this);

    m_pMedialist = NULL;
    m_player = NULL;
    m_bCloseAnimationState = false;
    m_widgetVolume = NULL;
    m_sliderVolume = NULL;
    m_widgetTransparency = NULL;
    m_sliderTransparency = NULL;
    m_pCfgDlg = NULL;

    m_eCurPlaybackMode = PM_LOOP;

    ui->m_widgetMain->setAttribute(Qt::WA_StyledBackground);  // 禁止父窗口样式影响子控件样式
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    ui->m_widgetMain->setProperty("form", "mainwnd");
    setProperty("canMove", "true");

    this->installEventFilter(this);

    CreateAllChildWnd();
    InitCtrl();
    InitSolts();
    Relayout();
    InitTrayIcon();
    UpdatePlayLoopMode();
}

MusicPlayer::~MusicPlayer()
{
    delete ui;
}

void MusicPlayer::CreateAllChildWnd()
{
#define NEW_OBJECT(pObj, TYPE) \
    if (NULL == pObj) { pObj = new TYPE(this); }

    NEW_OBJECT(m_pMedialist, QMediaPlaylist);
    NEW_OBJECT(m_player, QMediaPlayer);
    NEW_OBJECT(m_widgetVolume, QWidget);
    NEW_OBJECT(m_widgetTransparency, QWidget);
    NEW_OBJECT(m_pCfgDlg, CfgDlg);
}

void MusicPlayer::InitCtrl()
{
    // 设置边框阴影
    this->setAttribute(Qt::WA_TranslucentBackground, true);
    // 设置具体阴影
    QGraphicsDropShadowEffect *shadowBorder = new QGraphicsDropShadowEffect(ui->m_widgetMain);
    shadowBorder->setOffset(0, 0);
    // 阴影颜色
    shadowBorder->setColor(QColor(0x44, 0x44, 0x44, 127));
    // 阴影半径
    shadowBorder->setBlurRadius(20);
    ui->m_widgetMain->setGraphicsEffect(shadowBorder);

    ui->m_btnPre->setProperty("m_btnPre", "true");
    ui->m_btnStart->setProperty("m_btnStart", "true");
    ui->m_btnNext->setProperty("m_btnNext", "true");

    IconHelper::SetIcon(ui->m_btnPre, QChar(0xe676), 15);
    IconHelper::SetIcon(ui->m_btnStart, QChar(0xe612), 24);
    IconHelper::SetIcon(ui->m_btnNext, QChar(0xe677), 15);

    IconHelper::SetIcon(ui->m_btnVolume, QChar(0xe62a), 17);
    IconHelper::SetIcon(ui->m_btnCollection, QChar(0xe600), 15);
    IconHelper::SetIcon(ui->m_btnWrapMode, QChar(0xe71f), 15);
    IconHelper::SetIcon(ui->m_btnRecovery, QChar(0xe604), 18);
    IconHelper::SetIcon(ui->m_btnMore, QChar(0xe603), 15);
    IconHelper::SetIcon(ui->m_btnClose, QChar(0xe608), 5);

    ui->m_btnCollection->setCheckable(true);

    ui->m_Slider->EnableRange(false);
    ui->m_Slider->SetRange(0, 100);
    ui->m_Slider->SetStep(1);
    ui->m_Slider->SetCurPos(0);
    ui->m_Slider->EnablePercent(true);
    ui->m_Slider->EnableTimeMode(true);

    m_widgetVolume->setWindowFlags(Qt::Popup);
    m_widgetVolume->hide();
    m_widgetVolume->setFixedWidth(8);

    m_sliderVolume = new QSlider(m_widgetVolume);
    connect(m_sliderVolume, SIGNAL(valueChanged(int)), this, SLOT(OnVolumeChanged(int)));

    QVBoxLayout *layoutMain = new QVBoxLayout();
    layoutMain->addWidget(m_sliderVolume);
    layoutMain->setMargin(1);
    m_widgetVolume->setLayout(layoutMain);
    m_widgetVolume->setProperty("form", "Volume");
    m_widgetVolume->installEventFilter(this);

    {
        m_widgetTransparency->setWindowFlags(Qt::Popup);
        m_widgetTransparency->hide();
        m_widgetTransparency->setFixedWidth(8);

        m_sliderTransparency = new QSlider(m_widgetTransparency);
        m_sliderTransparency->setRange(30, 100);
        m_sliderTransparency->setValue(100);
        connect(m_sliderTransparency, SIGNAL(valueChanged(int)), this, SLOT(OnTransparencyChanged(int)));

        QVBoxLayout *layoutMain = new QVBoxLayout();
        layoutMain->addWidget(m_sliderTransparency);
        layoutMain->setMargin(1);
        m_widgetTransparency->setLayout(layoutMain);
        m_widgetTransparency->setProperty("form", "Volume");
        m_widgetTransparency->installEventFilter(this);
    }

    m_player->setVolume(50);
    m_sliderVolume->setValue(50);
    m_sliderVolume->setRange(0, 100);

    OnMusicListChange();

    m_pCfgDlg->hide();

    this->grabKeyboard();
}

void MusicPlayer::InitSolts()
{
    connect(m_pMedialist, SIGNAL(currentIndexChanged(int)), this, SLOT(OnCurrentIndexChanged(int)));
    connect(m_player, SIGNAL(durationChanged(qint64)), this, SLOT(OnDurationChanged(qint64)));
    connect(m_player, SIGNAL(positionChanged(qint64)), this, SLOT(OnPositionChanged(qint64)));
    connect(m_player, SIGNAL(positionChanged(qint64)), ui->m_widgetMain, SLOT(OnPlayPosChange(qint64)));
    connect(ui->m_Slider, SIGNAL(SignalValueChange(int)), this, SLOT(OnValueChange(int)));

    connect(ui->m_btnClose, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->m_btnStart, SIGNAL(clicked()), this, SLOT(OnStartBtnClicked()));
    connect(ui->m_btnMore, SIGNAL(clicked()), this, SLOT(OnMoreBtnClicked()));
    connect(ui->m_btnPre, SIGNAL(clicked()), m_pMedialist, SLOT(previous()));
    connect(ui->m_btnNext, SIGNAL(clicked()), m_pMedialist, SLOT(next()));

    connect(ui->m_btnVolume, SIGNAL(clicked()), this, SLOT(OnVolumeBtnClicked()));
    connect(ui->m_btnRecovery, SIGNAL(clicked()), this, SLOT(OnTransparencyBtnClicked()));
    connect(ui->m_btnCollection, SIGNAL(clicked()), this, SLOT(OnCollectionBtnClicked()));
    connect(ui->m_btnWrapMode, SIGNAL(clicked()), this, SLOT(OnWrapModeBtnClicked()));

    connect(DATAMGR, SIGNAL(SignalMusicListChange()), this, SLOT(OnMusicListChange()));
}

void MusicPlayer::Relayout()
{

}

void MusicPlayer::UpdateCtrlText()
{

}

void MusicPlayer::InitTrayIcon()
{

}

void MusicPlayer::OnCurrentIndexChanged(int nIndex)
{
    MusicInfos item;
    DATAMGR->GetMusicInfo(nIndex, item);
    ui->m_widgetMain->SetParam(item);
}

void MusicPlayer::OnDurationChanged(qint64 duration)
{
    qDebug() << __FUNCTION__ << duration;
    ui->m_Slider->SetRange(0, duration / 1000);
}

void MusicPlayer::OnPositionChanged(qint64 duration)
{
    qDebug() << __FUNCTION__ << duration;
    ui->m_Slider->SetCurPos(duration / 1000);
}

void MusicPlayer::OnValueChange(int nValue)
{
    m_player->setPosition(nValue * 1000);
}

void MusicPlayer::OnStartBtnClicked()
{
    if (m_pMedialist->mediaCount() < 1)
        return;

    if (m_player->state() == QMediaPlayer::PlayingState)
    {
        m_player->pause();
        IconHelper::SetIcon(ui->m_btnStart, QChar(0xe612), 24);
    }
    else
    {
        m_player->play();
        IconHelper::SetIcon(ui->m_btnStart, QChar(0xe656), 24);
    }
}

void MusicPlayer::OnMoreBtnClicked()
{
    m_pCfgDlg->show();

    QPoint ptBtn = ui->m_btnWrapMode->mapToGlobal(ui->m_btnWrapMode->pos());
    QPoint point(ptBtn);
    point.setX(ptBtn.x() - ui->m_btnWrapMode->pos().x() + ui->m_btnWrapMode->width() / 2 - m_pCfgDlg->width() / 2 - 4);
    point.setY(ptBtn.y() - ui->m_btnWrapMode->pos().y() - m_pCfgDlg->height() - 2);
    //m_pCfgDlg->move(point);

    QRect rcStart, rcEnd;
    rcStart = QRect(point.x(), point.y() + m_pCfgDlg->height(), m_pCfgDlg->width(), m_pCfgDlg->height());
    rcEnd = QRect(point.x(), point.y(), m_pCfgDlg->width(), m_pCfgDlg->height());

    QPropertyAnimation *animation = new QPropertyAnimation(m_pCfgDlg, "geometry");
    animation->setDuration(200);
    animation->setStartValue(rcStart);
    animation->setEndValue(rcEnd);
    animation->start();
}

void MusicPlayer::OnVolumeBtnClicked()
{
    m_widgetVolume->show();

    QPoint ptBtn = ui->m_btnVolume->mapToGlobal(ui->m_btnVolume->pos());
    QPoint point(ptBtn);
    point.setX(ptBtn.x() - ui->m_btnVolume->pos().x() + ui->m_btnVolume->width() / 2 - m_widgetVolume->width() / 2);
    point.setY(ptBtn.y() - ui->m_btnVolume->pos().y() - m_widgetVolume->height() - 2);
    m_widgetVolume->move(point);
}

void MusicPlayer::OnTransparencyBtnClicked()
{
    m_widgetTransparency->show();

    QPoint ptBtn = ui->m_btnRecovery->mapToGlobal(ui->m_btnRecovery->pos());
    QPoint point(ptBtn);
    point.setX(ptBtn.x() - ui->m_btnRecovery->pos().x() + ui->m_btnRecovery->width() / 2 - m_widgetTransparency->width() / 2);
    point.setY(ptBtn.y() - ui->m_btnRecovery->pos().y() - m_widgetTransparency->height() - 2);
    m_widgetTransparency->move(point);
}

void MusicPlayer::OnCollectionBtnClicked()
{
    if (ui->m_btnCollection->isChecked())
    {
        QWindow* pWin = this->windowHandle();
        pWin->setFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);
        update();
    }
    else
    {
        QWindow* pWin = this->windowHandle();
        pWin->setFlags(this->windowFlags() & ~Qt::WindowStaysOnTopHint);
        update();
    }
}

void MusicPlayer::OnWrapModeBtnClicked()
{
    int nCurPlaybackMode = (int)m_eCurPlaybackMode;
    nCurPlaybackMode++;
    m_eCurPlaybackMode = (EPlaybackMode)(nCurPlaybackMode);
    UpdatePlayLoopMode();
}

void MusicPlayer::OnMenuTriggered(QAction *action)
{
    EMenuItem eItem = (EMenuItem)action->data().toInt();
    switch (eItem)
    {
    case MENUITEM_QUIT:
        this->close();
        break;
    default:
        break;
    }
}

void MusicPlayer::OnVolumeChanged(int nValue)
{
    m_player->setVolume(nValue);

    if (nValue <= 0)
        IconHelper::SetIcon(ui->m_btnVolume, QChar(0xe630), 17);
    else
        IconHelper::SetIcon(ui->m_btnVolume, QChar(0xe62a), 17);
}

void MusicPlayer::OnTransparencyChanged(int nValue)
{
    double fRadio = (double)nValue / 100.0;
    setWindowOpacity(fRadio);
    m_widgetVolume->setWindowOpacity(fRadio);
    m_widgetTransparency->setWindowOpacity(fRadio);
}

void MusicPlayer::OnMusicListChange()
{
    m_player->stop();
    m_pMedialist->clear();

    QList<MusicInfos> listMusics;
    DATAMGR->GetMusicList(listMusics);
    foreach (MusicInfos item, listMusics)
    {
        m_pMedialist->addMedia(item.strSongUrl);
    }

    m_pMedialist->setPlaybackMode(QMediaPlaylist::Loop);
    m_player->setPlaylist(m_pMedialist);

    if (m_pMedialist->mediaCount() < 1)
    {
        MusicInfos item;
        item.pixImage = QPixmap(QString(":/img/image/smile.png"));
        item.strSinger = tr("未知");
        item.strSong = tr("未知");
        ui->m_widgetMain->SetParam(item);
    }
    else
    {
        OnCurrentIndexChanged(0);
    }
}

void MusicPlayer::PlayCloseAnimation()
{
    setDisabled(true);
    setMinimumSize(0,0);
    QPropertyAnimation* closeAnimation = new QPropertyAnimation(this,"geometry");
    closeAnimation->setStartValue(geometry());
    closeAnimation->setEndValue(QRect(geometry().x(), geometry().y()+height()/2, width(), 0));
    closeAnimation->setDuration(500);
    m_bCloseAnimationState = true;

    closeAnimation->setEasingCurve(QEasingCurve::OutBounce);  // 缓和曲线风格

    connect(closeAnimation,SIGNAL(finished()),this,SLOT(close()));
    closeAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MusicPlayer::GetMusicInfo(QString strPath, MusicPlayer::MusicInfo &tMusicInfo)
{
    ZPlay *player = CreateZPlay();

    // chek if we have class instance
    if(player == 0)
    {
        printf("Error: Can't create class instance !\nPress key to exit.\n");
        return;
    }

    TID3InfoEx id3_info;
    if(player->OpenFile(strPath.toLocal8Bit(), sfAutodetect))
    {
        if(player->LoadID3Ex(&id3_info,1))
        {
            tMusicInfo.strSong = QString::fromLocal8Bit(id3_info.Title);
            tMusicInfo.strSinger = QString::fromLocal8Bit(id3_info.Artist);
        }
    }
}

void MusicPlayer::UpdatePlayLoopMode()
{
    switch (m_eCurPlaybackMode)
    {
    case PM_CURRENTITEMINLOOP:
        m_pMedialist->setPlaybackMode(QMediaPlaylist::CurrentItemInLoop);
        IconHelper::SetIcon(ui->m_btnWrapMode, QChar(0xe623), 15);
        break;
    case PM_SEQUENTIAL:
        m_pMedialist->setPlaybackMode(QMediaPlaylist::Sequential);
        IconHelper::SetIcon(ui->m_btnWrapMode, QChar(0xe614), 15);
        break;
    case PM_LOOP:
        m_pMedialist->setPlaybackMode(QMediaPlaylist::Loop);
        IconHelper::SetIcon(ui->m_btnWrapMode, QChar(0xe7b4), 15);
        break;
    case PM_RANDOM:
        m_pMedialist->setPlaybackMode(QMediaPlaylist::Random);
        IconHelper::SetIcon(ui->m_btnWrapMode, QChar(0xe71f), 15);
        break;
    default:
        m_pMedialist->setPlaybackMode(QMediaPlaylist::CurrentItemInLoop);
        m_eCurPlaybackMode = PM_CURRENTITEMINLOOP;
        IconHelper::SetIcon(ui->m_btnWrapMode, QChar(0xe623), 15);
        break;
    }
}

void MusicPlayer::closeEvent(QCloseEvent *event)
{
    if(!m_bCloseAnimationState)
    {
        PlayCloseAnimation();
        event->ignore();
    }
    else{
        event->accept();
    }
}

bool MusicPlayer::eventFilter(QObject *obj, QEvent *evt)
{
#ifdef WIN32
    if (obj == m_widgetVolume)
    {
        // class_ameneded 不能是custommenu的成员, 因为winidchange事件触发时, 类成员尚未初始化
        static bool class_amended = false;
        if (evt->type() == QEvent::WinIdChange)
        {
            HWND hwnd = reinterpret_cast<HWND>(m_widgetVolume->winId());
            if (class_amended == false)
            {
                class_amended = true;
                DWORD class_style = ::GetClassLong(hwnd, GCL_STYLE);
                class_style &= ~CS_DROPSHADOW;
                ::SetClassLong(hwnd, GCL_STYLE, class_style); // windows系统函数
            }
        }
    }
    else if (obj == m_widgetTransparency)
    {
        // class_ameneded 不能是custommenu的成员, 因为winidchange事件触发时, 类成员尚未初始化
        static bool class_amended = false;
        if (evt->type() == QEvent::WinIdChange)
        {
            HWND hwnd = reinterpret_cast<HWND>(m_widgetTransparency->winId());
            if (class_amended == false)
            {
                class_amended = true;
                DWORD class_style = ::GetClassLong(hwnd, GCL_STYLE);
                class_style &= ~CS_DROPSHADOW;
                ::SetClassLong(hwnd, GCL_STYLE, class_style); // windows系统函数
            }
        }
    }
#endif

    if (evt->type() == QEvent::KeyRelease)
    {
        QKeyEvent *e = (QKeyEvent *)evt;
        if (e->key() == Qt::Key_Space)
        {
            OnStartBtnClicked();
        }
        else if (e->key() == Qt::Key_Left)
        {
            m_pMedialist->previous();
        }
        else if (e->key() == Qt::Key_Right)
        {
            m_pMedialist->next();
        }

        return true;
    }

    return QWidget::eventFilter(obj, evt);
}
