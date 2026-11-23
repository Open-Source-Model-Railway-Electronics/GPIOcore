#pragma once
// The DFPlayer Mini on the PC: in Simulate and Live test a sound effect output's track is played from a folder
// laid out like the MP3 folder on the DFPlayer's SD card: 0001.mp3, 0002.mp3, ... (the firmware plays them by name).
// Track 0 = stop; a new track interrupts the one that is playing, like the DFPlayer.
// The folder and mute are kept in QSettings (sound/folder, sound/muted); by default the folder is
// Documents/IOcore/sounds, next to the boards folder.

#include <QObject>
#include <QString>
#include <memory>

class SoundPlayer : public QObject
{
    Q_OBJECT
public:
    // What makes the sound: Qt Multimedia, or a stand-in in the tests
    struct Backend
    {
        virtual ~Backend() = default;
        virtual void play(const QString &file) = 0;
        virtual void stop() = 0;
    };

    explicit SoundPlayer(QObject *parent = nullptr);
    ~SoundPlayer() override;

    void    setBackend(Backend *backend);       // takes ownership (tests); otherwise Qt Multimedia on first use
    static QString defaultFolder();             // Documents/IOcore/sounds
    QString folder() const { return m_folder; }
    void    setFolder(const QString &dir);
    bool    isMuted() const { return m_muted; }
    void    setMuted(bool muted);               // muting also stops the sound
    QString trackFile(int track) const;         // the file for a track in the folder, empty when there is none

    void    playTrack(int track);               // OUTPUT_EVENT state 2 of a sound effect output: 0 = stop
    void    stop();

signals:
    void message(const QString &text);          // for the status bar: file not found, no folder yet

private:
    Backend *backend();

    std::unique_ptr<Backend> m_backend;
    QString m_folder;
    bool m_muted = false;
    bool m_hinted = false;                      // 'choose a sound folder' was shown once
};
