/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The share flyout (explorer spec 3.11): targets named in Raye's words and
// placed in the "Share with" row or under "More options", Files' own Copy and
// Copy link, Purpose's real list of Export targets (email must be there), and
// the QML page loading in the flyout.

#include "share/shareflyout.h"
#include "share/sharemodel.h"
#include "share/sharetargets.h"
#include "ui/motion.h"

#include <QApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QMimeData>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTest>

using namespace Raye::Share;

namespace
{
QStandardItemModel *fakePurpose(QObject *parent)
{
    auto *model = new QStandardItemModel(parent);
    for (const auto &[id, name] : {std::pair{"saveasplugin", "Save As..."},
                                   std::pair{"emailplugin", "Send via Email..."},
                                   std::pair{"clipboardplugin", "Copy to Clipboard"},
                                   std::pair{"imgurplugin", "Imgur"},
                                   std::pair{"kdeconnectplugin", "Send to Device"},
                                   std::pair{"bluetoothplugin", "Send via Bluetooth..."},
                                   std::pair{"nextcloudplugin", "Nextcloud"}}) {
        auto *item = new QStandardItem(QString::fromLatin1(name));
        item->setData(QString::fromLatin1(id), ShareModel::pluginIdRole);
        item->setData(QStringLiteral("purpose-icon"), ShareModel::iconNameRole);
        model->appendRow(item);
    }
    return model;
}
}

class RayeShareTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void describe_data();
    void describe();
    void rowInWindowsOrder();
    void moreOptionsSortedAndClean();
    void copyLinkOnlyForNetworkFiles();
    void copyPutsTheFilesOnTheClipboard();
    void copyLinkNeverCarriesAPassword();
    void titleAndType();
    void inputDataForPlugins();
    void purposeListsEmailAndCopy();
    void sheetLoadsInTheFlyout();
    void sheetCopyClosesTheFlyout();

private:
    QTemporaryDir m_dir;
    QUrl m_png;
};

void RayeShareTest::initTestCase()
{
    Raye::Ui::Motion::setSpeedFactor(0);
    QVERIFY(m_dir.isValid());
    QFile f(m_dir.filePath(QStringLiteral("holiday.png")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray::fromHex("89504e470d0a1a0a")); // a PNG signature is enough for the type
    f.close();
    m_png = QUrl::fromLocalFile(f.fileName());
    ShareFlyout::addImportPath(QStringLiteral(RAYE_FILES_BINARY_DIR "/bin"));
}

void RayeShareTest::describe_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("place");
    QTest::newRow("email") << "emailplugin" << "Email" << int(Place::Row);
    QTest::newRow("kde connect") << "kdeconnectplugin" << "Nearby devices" << int(Place::Row);
    QTest::newRow("bluetooth") << "bluetoothplugin" << "Bluetooth" << int(Place::Row);
    QTest::newRow("copy") << copyTarget << "Copy" << int(Place::Row);
    QTest::newRow("copy link") << copyLinkTarget << "Copy link" << int(Place::Row);
    QTest::newRow("clipboard hidden") << "clipboardplugin" << "Copy to Clipboard" << int(Place::Hidden);
    QTest::newRow("sms hidden") << "kdeconnectsmsplugin" << "Send SMS" << int(Place::Hidden);
    QTest::newRow("save as") << "saveasplugin" << "Save a copy" << int(Place::More);
    QTest::newRow("unknown keeps its name") << "telegramplugin" << "Send to Telegram" << int(Place::More);
}

void RayeShareTest::describe()
{
    QFETCH(QString, id);
    QFETCH(QString, name);
    QFETCH(int, place);
    // Purpose's own name is only used when Raye has none.
    const Target t = Raye::Share::describe(id, id == QLatin1String("telegramplugin") ? QStringLiteral("Send to Telegram")
                                                 : id == QLatin1String("clipboardplugin")  ? QStringLiteral("Copy to Clipboard")
                                                 : id == QLatin1String("kdeconnectsmsplugin") ? QStringLiteral("Send SMS")
                                                                                              : QStringLiteral("KDE words"));
    QCOMPARE(t.name, name);
    QCOMPARE(int(t.place), place);
    QVERIFY(!t.iconName.isEmpty() || t.place == Place::Hidden);
}

void RayeShareTest::rowInWindowsOrder()
{
    ShareModel model;
    model.setSource(fakePurpose(&model));
    model.setUrls({m_png});
    QCOMPARE(model.rowIds(), (QStringList{"kdeconnectplugin", "bluetoothplugin", "emailplugin", "raye-copy"}));
    const QVariantMap email = model.rowTargets().at(2).toMap();
    QCOMPARE(email.value(QStringLiteral("name")).toString(), QStringLiteral("Email"));
    QCOMPARE(email.value(QStringLiteral("row")).toInt(), 1); // Purpose's row, to run it
    QCOMPARE(model.rowTargets().at(3).toMap().value(QStringLiteral("row")).toInt(), -1);
}

void RayeShareTest::moreOptionsSortedAndClean()
{
    ShareModel model;
    model.setSource(fakePurpose(&model));
    model.setUrls({m_png});
    QCOMPARE(model.moreIds(), (QStringList{"imgurplugin", "nextcloudplugin", "saveasplugin"}));
    QVERIFY(!model.moreIds().contains(QStringLiteral("clipboardplugin")));
    QVERIFY(!model.rowIds().contains(QStringLiteral("clipboardplugin")));
}

void RayeShareTest::copyLinkOnlyForNetworkFiles()
{
    ShareModel model;
    model.setUrls({m_png});
    QVERIFY(!model.rowIds().contains(QString::fromLatin1(copyLinkTarget)));
    model.setUrls({QUrl(QStringLiteral("smb://nas/share/a.pdf"))});
    QVERIFY(model.rowIds().contains(QString::fromLatin1(copyLinkTarget)));
    model.setUrls({});
    QVERIFY(model.rowIds().isEmpty()); // nothing to share, nothing to copy
}

void RayeShareTest::copyPutsTheFilesOnTheClipboard()
{
    ShareModel model;
    model.setUrls({m_png});
    QSignalSpy done(&model, &ShareModel::done);
    QVERIFY(model.activateOwn(QString::fromLatin1(copyTarget)));
    QCOMPARE(done.count(), 1);
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    QCOMPARE(mime->urls(), QList<QUrl>{m_png});
    QCOMPARE(mime->data(QStringLiteral("application/x-kde-cutselection")), QByteArrayLiteral("0"));
    QVERIFY(!model.activateOwn(QStringLiteral("emailplugin")));
}

void RayeShareTest::copyLinkNeverCarriesAPassword()
{
    ShareModel model;
    model.setUrls({QUrl(QStringLiteral("sftp://ann:secret@host/srv/a.txt"))});
    QVERIFY(model.activateOwn(QString::fromLatin1(copyLinkTarget)));
    const QString text = QGuiApplication::clipboard()->text();
    QVERIFY(!text.contains(QLatin1String("secret")));
    QVERIFY(text.contains(QLatin1String("host/srv/a.txt")));
}

void RayeShareTest::titleAndType()
{
    ShareModel model;
    model.setUrls({m_png});
    QCOMPARE(model.title(), QStringLiteral("holiday.png"));
    QCOMPARE(model.iconName(), QStringLiteral("image-png"));
    QVERIFY(!model.subtitle().isEmpty());
    model.setUrls({m_png, QUrl::fromLocalFile(QStringLiteral("/tmp/b.txt"))});
    QCOMPARE(model.title(), QStringLiteral("2 items"));
    QCOMPARE(model.iconName(), QStringLiteral("document-multiple"));
}

void RayeShareTest::inputDataForPlugins()
{
    const QJsonObject one = ShareModel::inputData({m_png}, {QStringLiteral("image/png")});
    QCOMPARE(one.value(QStringLiteral("mimeType")).toString(), QStringLiteral("image/png"));
    QCOMPARE(one.value(QStringLiteral("urls")).toArray().first().toString(), m_png.toString());
    const QJsonObject pictures = ShareModel::inputData({m_png, m_png}, {QStringLiteral("image/png"), QStringLiteral("image/jpeg")});
    QCOMPARE(pictures.value(QStringLiteral("mimeType")).toString(), QStringLiteral("image/*"));
    const QJsonObject mixed = ShareModel::inputData({m_png, m_png}, {QStringLiteral("image/png"), QStringLiteral("text/plain")});
    QCOMPARE(mixed.value(QStringLiteral("mimeType")).toString(), QStringLiteral("application/octet-stream"));
}

void RayeShareTest::purposeListsEmailAndCopy()
{
    QQmlEngine engine;
    QString error;
    QAbstractItemModel *purpose = ShareModel::createPurposeModel(&engine, ShareModel::inputData({m_png}, {QStringLiteral("image/png")}), &engine, &error);
    QVERIFY2(purpose, qPrintable(QStringLiteral("org.kde.purpose missing (qml6-module-org-kde-purpose): ") + error));
    ShareModel model;
    model.setSource(purpose);
    model.setUrls({m_png});
    QTRY_VERIFY(model.rowIds().contains(QStringLiteral("emailplugin")));
    QVERIFY(model.rowIds().contains(QString::fromLatin1(copyTarget)));
    const QVariantMap email = model.rowTargets().at(model.rowIds().indexOf(QStringLiteral("emailplugin"))).toMap();
    QCOMPARE(email.value(QStringLiteral("name")).toString(), QStringLiteral("Email"));
    QVERIFY(email.value(QStringLiteral("row")).toInt() >= 0);
}

void RayeShareTest::sheetLoadsInTheFlyout()
{
    ShareFlyout flyout;
    flyout.setUrls({m_png});
    QVERIFY(flyout.isReady());
    QQuickItem *root = flyout.view()->rootObject();
    QCOMPARE(root->property("page").toString(), QStringLiteral("main"));
    QCOMPARE(qRound(root->width()), ShareFlyout::sheetWidth - 2 * flyout.padding());
    QVERIFY(root->implicitHeight() > 150);
    QVERIFY(flyout.purposeModel());
}

void RayeShareTest::sheetCopyClosesTheFlyout()
{
    auto *anchor = new QWidget;
    anchor->resize(40, 32);
    anchor->show();
    ShareFlyout *flyout = ShareFlyout::share(anchor, {m_png});
    QVERIFY(flyout->isVisible());
    QSignalSpy closed(flyout, &Raye::Ui::Flyout::closed);
    QVariantMap copy;
    for (const QVariant &v : flyout->model()->rowTargets()) {
        if (v.toMap().value(QStringLiteral("id")) == QLatin1String(copyTarget)) {
            copy = v.toMap();
        }
    }
    QVERIFY(!copy.isEmpty());
    QMetaObject::invokeMethod(flyout->view()->rootObject(), "activate", Q_ARG(QVariant, QVariant(copy)));
    QTRY_COMPARE(closed.count(), 1);
    QCOMPARE(QGuiApplication::clipboard()->mimeData()->urls(), QList<QUrl>{m_png});
    delete anchor; // takes the flyout with it, if it is still there
}

QTEST_MAIN(RayeShareTest)

#include "test_share.moc"
