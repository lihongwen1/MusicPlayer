﻿#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QWidget>
#include <QUrl>
#include <QSlider>

namespace Ui {
class MusicPlayer;
}

class QMediaPlaylist;
class QMediaPlayer;

class MusicPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit MusicPlayer(QWidget *parent = 0);
    ~MusicPlayer();

    enum EMenuItem
    {
        MENUITEM_QUIT,
        MENUITEM_TRAY,
    };

    struct MusicInfo
    {
        QPixmap pixImage;
        QString strSong;
        QString strSinger;
        QUrl strSongUrl;
    };

private:
    void CreateAllChildWnd();
    void InitCtrl();
    void InitSolts();
    void Relayout();
    void UpdateCtrlText();
    void InitTrayIcon();
    void PlayCloseAnimation();
    void GetMusicInfo(QString strPath, MusicInfo &tMusicInfo);

    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *evt);

public slots:
    void OnCurrentIndexChanged(int nIndex);
    void OnDurationChanged(qint64 duration);
    void OnPositionChanged(qint64 duration);
    void OnValueChange(int nValue);
    void OnStartBtnClicked();
    void OnMoreBtnClicked();
    void OnVolumeBtnClicked();
    void OnTransparencyBtnClicked();
    void OnCollectionBtnClicked();
    void OnMenuTriggered(QAction *action);
    void OnVolumeChanged(int nValue);
    void OnTransparencyChanged(int nValue);

private:
    Ui::MusicPlayer *ui;
    QMediaPlaylist *m_pMedialist;
    QMediaPlayer *m_player;

    QWidget *m_widgetVolume;
    QSlider *m_sliderVolume;

    QWidget *m_widgetTransparency;
    QSlider *m_sliderTransparency;

    bool m_bCloseAnimationState;
};

#endif // MUSICPLAYER_H
