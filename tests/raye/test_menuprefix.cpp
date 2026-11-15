/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Files started outside the desktop session (a remote shell, a service):
// XDG_MENU_PREFIX is not set, there is no applications.menu, and the app
// database was built empty, so "Open with" offered no app at all. Files
// now picks the session's menu itself before the database is read.

#include "raye/openwith/appchoices.h"
#include "raye/openwith/menuprefix.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using Raye::OpenWith::AppChoicesModel;

namespace
{
QTemporaryDir *s_root = nullptr;

const QByteArray menu =
    "<!DOCTYPE Menu PUBLIC \"-//freedesktop//DTD Menu 1.0//EN\"\n"
    " \"http://www.freedesktop.org/standards/menu-spec/menu-1.0.dtd\">\n"
    "<Menu>\n  <Name>Applications</Name>\n  <DefaultAppDirs/>\n  <DefaultDirectoryDirs/>\n"
    "  <Include><All/></Include>\n</Menu>\n";

void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(body);
}
}

class TestMenuPrefix : public QObject
{
    Q_OBJECT

public:
    // Before the application exists, as a remote shell leaves it: no menu
    // prefix, and only the Raye menu in the system config folder.
    static void initMain()
    {
        s_root = new QTemporaryDir;
        const QString root = s_root->path();
        writeFile(root + QStringLiteral("/xdg/menus/raye-applications.menu"), menu);
        writeFile(root + QStringLiteral("/data/applications/rayetest-notes.desktop"),
                  "[Desktop Entry]\nType=Application\nName=Raye Test Notes\nExec=rayetest-notes %f\nMimeType=text/plain;\n");
        qunsetenv("XDG_MENU_PREFIX");
        qputenv("XDG_CONFIG_DIRS", (root + QStringLiteral("/xdg")).toLocal8Bit());
        qputenv("XDG_DATA_DIRS", (root + QStringLiteral("/data:/usr/share")).toLocal8Bit());
        qputenv("HOME", root.toLocal8Bit());
        qputenv("XDG_CONFIG_HOME", (root + QStringLiteral("/home/config")).toLocal8Bit());
        qputenv("XDG_DATA_HOME", (root + QStringLiteral("/home/data")).toLocal8Bit());
        qputenv("XDG_CACHE_HOME", (root + QStringLiteral("/home/cache")).toLocal8Bit());
    }

private Q_SLOTS:
    void prefixIsSetAtStart();
    void openWithListsAppsOutsideTheSession();
    void prefixChoice_data();
    void prefixChoice();
};

void TestMenuPrefix::prefixIsSetAtStart()
{
    QCOMPARE(qgetenv("XDG_MENU_PREFIX"), QByteArray("raye-"));
}

void TestMenuPrefix::openWithListsAppsOutsideTheSession()
{
    AppChoicesModel model;
    model.setMimeType(QStringLiteral("text/plain"));
    QStringList ids;
    for (int i = 0; i < model.rowCount(); ++i) {
        ids.append(model.index(i).data(AppChoicesModel::StorageIdRole).toString());
    }
    QVERIFY2(ids.contains(QStringLiteral("rayetest-notes.desktop")), qPrintable(ids.join(QLatin1Char(' '))));
    QVERIFY(model.index(0).data(AppChoicesModel::RecommendedRole).toBool());
}

void TestMenuPrefix::prefixChoice_data()
{
    QTest::addColumn<QByteArray>("current");
    QTest::addColumn<QStringList>("menus");
    QTest::addColumn<QByteArray>("expected");

    const QStringList both{QStringLiteral("raye-applications.menu"), QStringLiteral("plasma-applications.menu")};
    QTest::newRow("session sets it") << QByteArray("plasma-") << both << QByteArray();
    QTest::newRow("raye first") << QByteArray() << both << QByteArray("raye-");
    QTest::newRow("plasma alone") << QByteArray() << QStringList{QStringLiteral("plasma-applications.menu")} << QByteArray("plasma-");
    QTest::newRow("plain menu there") << QByteArray() << QStringList{QStringLiteral("applications.menu"), QStringLiteral("raye-applications.menu")}
                                      << QByteArray();
    QTest::newRow("no menu at all") << QByteArray() << QStringList() << QByteArray();
}

void TestMenuPrefix::prefixChoice()
{
    QFETCH(QByteArray, current);
    QFETCH(QStringList, menus);
    QFETCH(QByteArray, expected);

    QTemporaryDir dir;
    for (const QString &name : std::as_const(menus)) {
        writeFile(dir.path() + QStringLiteral("/menus/") + name, menu);
    }
    QCOMPARE(Raye::OpenWith::missingMenuPrefix(current, {QStringLiteral("/nonexistent"), dir.path()}), expected);
}

QTEST_MAIN(TestMenuPrefix)

#include "test_menuprefix.moc"
