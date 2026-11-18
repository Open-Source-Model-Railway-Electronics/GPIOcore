#include "soundplayer.h"

#include <QAudioOutput>
#include <QDir>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QSettings>
#include <QUrl>

namespace {

const QString kFolderKey = QStringLiteral("sound/folder");
const QString kMutedKey  = QStringLiteral("sound/muted");

// Qt Multimedia: on Windows the Windows Media Foundation plugin plays MP3 and WAV
class MediaBackend : public SoundPlayer::Backend
{
public:
    MediaBackend() { m_player.setAudioOutput(&m_output); }
    void play(const QString &file) override
    {
        m_player.stop();                          // the same track again starts from the beginning
        m_player.setSource(QUrl::fromLocalFile(file));
        m_player.play();
    }
    void stop() override { m_player.stop(); }

private:
    QAudioOutput m_output;
    QMediaPlayer m_player;
};

} // namespace

SoundPlayer::SoundPlayer(QObject *parent)
    : QObject(parent),
      m_folder(QSettings().value(kFolderKey).toString()),
      m_muted(QSettings().value(kMutedKey, false).toBool())
{
}

SoundPlayer::~SoundPlayer() = default;

void SoundPlayer::setBackend(Backend *backend)
{
    m_backend.reset(backend);
}

SoundPlayer::Backend *SoundPlayer::backend()
{
    if (!m_backend)
        m_backend = std::make_unique<MediaBackend>();
    return m_backend.get();
}

void SoundPlayer::setFolder(const QString &dir)
{
    m_folder = dir;
    QSettings().setValue(kFolderKey, dir);
}

void SoundPlayer::setMuted(bool muted)
{
    m_muted = muted;
    QSettings().setValue(kMutedKey, muted);
    if (muted)
        stop();
}

QString SoundPlayer::trackFile(int track) const
{
    const QDir dir(folder());
    if (folder().isEmpty() || track <= 0)
        return QString();
    const QString names[3] = { QStringLiteral("%1").arg(track, 4, 10, QLatin1Char('0')),
                               QStringLiteral("%1").arg(track, 3, 10, QLatin1Char('0')),
                               QString::number(track) };
    for (const QString &name : names)
        for (const char *ext : { ".mp3", ".wav" })
            if (QFileInfo(dir.filePath(name + QLatin1String(ext))).isFile())
                return dir.filePath(name + QLatin1String(ext));
    return QString();
}

void SoundPlayer::playTrack(int track)
{
    if (track <= 0)
    {
        stop();
        return;
    }
    if (isMuted())
        return;
    if (folder().isEmpty())
    {
        if (!m_hinted)
            emit message(tr("Sound effect track %1: choose a sound folder (View > Sound folder…) to hear it.").arg(track));
        m_hinted = true;
        return;
    }
    const QString file = trackFile(track);
    if (file.isEmpty())
    {
        emit message(tr("%1 not found in %2").arg(QStringLiteral("%1.mp3").arg(track, 4, 10, QLatin1Char('0')),
                                                  QDir::toNativeSeparators(folder())));
        return;
    }
    backend()->play(file);
}

void SoundPlayer::stop()
{
    if (m_backend)                              // nothing played yet: no need to start Qt Multimedia
        m_backend->stop();
}
