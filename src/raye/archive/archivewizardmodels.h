/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveexport.h"
#include "archiveformat.h"

#include <QObject>
#include <QStringList>

// What the archive wizards (src/raye/qml/archive) show and check. The QML
// pages bind to these; the dialogs in archivewizards.cpp read them back.
// In QML: import org.raye.files.archive.
namespace Raye::Archive
{

// "Extract compressed (zipped) folders": where to, and whether to open it.
class RAYE_ARCHIVE_EXPORT ExtractAllModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString archivePath READ archivePath WRITE setArchivePath NOTIFY archivePathChanged)
    Q_PROPERTY(QString archiveName READ archiveName NOTIFY archivePathChanged)
    Q_PROPERTY(QString destination READ destination WRITE setDestination NOTIFY destinationChanged)
    Q_PROPERTY(bool showWhenDone READ showWhenDone WRITE setShowWhenDone NOTIFY showWhenDoneChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
public:
    explicit ExtractAllModel(QObject *parent = nullptr);

    QString archivePath() const
    {
        return m_archive;
    }
    // Also sets the destination to the default: <folder>/<name>.
    void setArchivePath(const QString &path);
    QString archiveName() const;

    QString destination() const
    {
        return m_destination;
    }
    void setDestination(const QString &destination);

    bool showWhenDone() const
    {
        return m_show;
    }
    void setShowWhenDone(bool show);

    QString errorText() const
    {
        return m_error;
    }

    // The destination as an absolute, clean path ("~" expanded, a
    // relative path read from the archive's folder).
    QString resolvedDestination() const;

    // Checks the destination; on a problem errorText says what, in plain
    // words, and it returns false.
    Q_INVOKABLE bool validate();

    // <folder of the archive>/<archive name without its extension>
    static QString defaultDestination(const QString &archivePath);

Q_SIGNALS:
    void archivePathChanged();
    void destinationChanged();
    void showWhenDoneChanged();
    void errorTextChanged();

private:
    void setError(const QString &error);

    QString m_archive;
    QString m_destination;
    bool m_show = true;
    QString m_error;
};

// The "Add to archive" window (Compress to › Additional options): name,
// folder, format, level and password up front; method, dictionary, word
// size, solid blocks, threads, volumes, update mode, deleting after,
// timestamps, exclusions and saved profiles under "Advanced options".
class RAYE_ARCHIVE_EXPORT CompressOptionsModel : public QObject
{
    Q_OBJECT
    // 0 ZIP, 1 7z, 2 TAR (Format)
    Q_PROPERTY(int format READ format WRITE setFormat NOTIFY changed)
    // the engine level, 0 Store .. 9 Ultra (Level)
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY changed)
    // 0 Store, 1 Fastest, 2 Fast, 3 Normal, 4 Maximum, 5 Ultra
    Q_PROPERTY(int levelIndex READ levelIndex WRITE setLevelIndex NOTIFY changed)
    // 0 none, 1 gzip, 2 bzip2, 3 xz, 4 zstd (TarFilter)
    Q_PROPERTY(int tarFilter READ tarFilter WRITE setTarFilter NOTIFY changed)
    Q_PROPERTY(bool keepSymlinks READ keepSymlinks WRITE setKeepSymlinks NOTIFY changed)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY changed)
    Q_PROPERTY(QString extension READ extension NOTIFY changed)
    Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
    // TAR with no compression has no level to choose
    Q_PROPERTY(bool levelEnabled READ levelEnabled NOTIFY changed)
    Q_PROPERTY(bool isTar READ isTar NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY changed)
    Q_PROPERTY(QString passwordAgain READ passwordAgain WRITE setPasswordAgain NOTIFY changed)
    Q_PROPERTY(bool canEncrypt READ canEncrypt NOTIFY changed)
    Q_PROPERTY(bool encryptNames READ encryptNames WRITE setEncryptNames NOTIFY changed)
    Q_PROPERTY(bool canEncryptNames READ canEncryptNames NOTIFY changed)
    // ZIP: 0 AES-256, 1 ZipCrypto
    Q_PROPERTY(int zipEncryption READ zipEncryption WRITE setZipEncryption NOTIFY changed)

    Q_PROPERTY(QStringList methods READ methods NOTIFY changed)
    Q_PROPERTY(int methodIndex READ methodIndex WRITE setMethodIndex NOTIFY changed)
    Q_PROPERTY(QStringList dictionaries READ dictionaries NOTIFY changed)
    Q_PROPERTY(int dictionaryIndex READ dictionaryIndex WRITE setDictionaryIndex NOTIFY changed)
    Q_PROPERTY(bool dictionaryEnabled READ dictionaryEnabled NOTIFY changed)
    Q_PROPERTY(QStringList wordSizes READ wordSizes CONSTANT)
    Q_PROPERTY(int wordIndex READ wordIndex WRITE setWordIndex NOTIFY changed)
    Q_PROPERTY(QStringList solidBlocks READ solidBlocks CONSTANT)
    Q_PROPERTY(int solidIndex READ solidIndex WRITE setSolidIndex NOTIFY changed)
    Q_PROPERTY(QStringList threadChoices READ threadChoices CONSTANT)
    Q_PROPERTY(int threadIndex READ threadIndex WRITE setThreadIndex NOTIFY changed)
    // 0 none, 1 100 MB, 2 700 MB, 3 4 GB (FAT32), 4 custom
    Q_PROPERTY(int volumeIndex READ volumeIndex WRITE setVolumeIndex NOTIFY changed)
    Q_PROPERTY(QString customVolume READ customVolume WRITE setCustomVolume NOTIFY changed)
    // 0 new archive, 1 add and replace, 2 update, 3 synchronize (UpdateMode)
    Q_PROPERTY(int updateMode READ updateMode WRITE setUpdateMode NOTIFY changed)
    Q_PROPERTY(bool deleteAfter READ deleteAfter WRITE setDeleteAfter NOTIFY changed)
    Q_PROPERTY(bool storeAccessed READ storeAccessed WRITE setStoreAccessed NOTIFY changed)
    Q_PROPERTY(bool storeCreated READ storeCreated WRITE setStoreCreated NOTIFY changed)
    Q_PROPERTY(QString excludes READ excludes WRITE setExcludes NOTIFY changed)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString estimate READ estimate NOTIFY changed)
public:
    explicit CompressOptionsModel(QObject *parent = nullptr);

    // The items to compress: name and folder start from the first one.
    void setSources(const QStringList &sources);
    QStringList sources() const
    {
        return m_sources;
    }

    int format() const
    {
        return int(m_options.format);
    }
    void setFormat(int format);
    int level() const
    {
        return int(m_options.level);
    }
    void setLevel(int level);
    int levelIndex() const;
    void setLevelIndex(int index);
    int tarFilter() const
    {
        return int(m_options.tarFilter);
    }
    void setTarFilter(int filter);
    bool keepSymlinks() const
    {
        return m_options.keepSymlinks;
    }
    void setKeepSymlinks(bool keep);
    QString name() const
    {
        return m_name;
    }
    void setName(const QString &name);
    QString folder() const
    {
        return m_folder;
    }
    void setFolder(const QString &folder);

    QString extension() const
    {
        return m_options.extension();
    }
    QString fileName() const
    {
        return m_name + extension();
    }
    bool levelEnabled() const;
    bool isTar() const
    {
        return m_options.format == Format::Tar;
    }
    QString errorText() const
    {
        return m_error;
    }

    QString password() const
    {
        return m_options.password;
    }
    void setPassword(const QString &password);
    QString passwordAgain() const
    {
        return m_passwordAgain;
    }
    void setPasswordAgain(const QString &password);
    bool canEncrypt() const
    {
        return !isTar();
    }
    bool encryptNames() const
    {
        return m_options.encryptNames;
    }
    void setEncryptNames(bool encrypt);
    bool canEncryptNames() const
    {
        return m_options.format == Format::SevenZip;
    }
    int zipEncryption() const
    {
        return m_options.zipCrypto ? 1 : 0;
    }
    void setZipEncryption(int index);

    QStringList methods() const;
    int methodIndex() const;
    void setMethodIndex(int index);
    QStringList dictionaries() const;
    int dictionaryIndex() const;
    void setDictionaryIndex(int index);
    bool dictionaryEnabled() const;
    QStringList wordSizes() const;
    int wordIndex() const;
    void setWordIndex(int index);
    QStringList solidBlocks() const;
    int solidIndex() const;
    void setSolidIndex(int index);
    QStringList threadChoices() const;
    int threadIndex() const
    {
        return m_options.threads;
    }
    void setThreadIndex(int index);
    int volumeIndex() const
    {
        return m_volumeIndex;
    }
    void setVolumeIndex(int index);
    QString customVolume() const
    {
        return m_customVolume;
    }
    void setCustomVolume(const QString &text);
    int updateMode() const
    {
        return int(m_options.mode);
    }
    void setUpdateMode(int mode);
    bool deleteAfter() const
    {
        return m_options.deleteAfter;
    }
    void setDeleteAfter(bool on);
    bool storeAccessed() const
    {
        return m_options.storeAccessed;
    }
    void setStoreAccessed(bool on);
    bool storeCreated() const
    {
        return m_options.storeCreated;
    }
    void setStoreCreated(bool on);
    QString excludes() const
    {
        return m_excludes;
    }
    void setExcludes(const QString &patterns);

    // Saved settings, by name (in dolphinrc). The password is never saved.
    QStringList profiles() const;
    Q_INVOKABLE void saveProfile(const QString &name);
    Q_INVOKABLE void loadProfile(const QString &name);
    Q_INVOKABLE void removeProfile(const QString &name);

    // "2.4 GB in 1,284 files · about 1.1 GB of memory to compress, 64 MB to
    // open": what the settings cost, before starting.
    QString estimate() const;
    // The bytes of memory compressing and opening the archive take (rough).
    qint64 compressMemory() const;
    qint64 openMemory() const;

    // "250 MB", "1.5 GB", "4G": bytes, or 0 when it can't be read.
    static qint64 parseSize(const QString &text);

    CreateOptions options() const;
    QString archivePath() const;

    Q_INVOKABLE bool validate();

Q_SIGNALS:
    void changed();
    void profilesChanged();

private:
    void touch();
    qint64 effectiveDictionary() const;
    bool usesLzma2() const;

    QStringList m_sources;
    CreateOptions m_options;
    QString m_name;
    QString m_folder;
    QString m_error;
    QString m_passwordAgain;
    int m_volumeIndex = 0;
    QString m_customVolume;
    QString m_excludes;
    qint64 m_sourceBytes = 0;
    int m_sourceFiles = 0;
};

// Registers org.raye.files.archive for QML (done at start, like the kit).
RAYE_ARCHIVE_EXPORT void registerQmlTypes();

}
