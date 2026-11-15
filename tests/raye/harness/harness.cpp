/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Built into tests/raye/test_ui_harness.cpp (tests/raye/CMakeLists.txt makes
// one program per test_*.cpp), so no Q_OBJECT class in here: moc only reads
// that file.

#include "harness.h"

#include "ui/carddialog.h"
#include "ui/flyout.h"
#include "ui/motion.h"
#include "ui/shadow.h"
#include "ui/showcase.h"
#include "ui/tokens.h"

#include <KLocalizedString>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <clocale>
#include <cstdio>
#include <memory>

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
#include <KLocalizedContext>
QT_WARNING_POP

namespace RayeHarness
{

namespace
{
QString s_lang = QStringLiteral("en");
bool s_shooting = false;

QString binaryDir()
{
    const QString fromEnv = qEnvironmentVariable("RAYE_FILES_BINARY_DIR");
    return fromEnv.isEmpty() ? QStringLiteral(RAYE_FILES_BINARY_DIR) : fromEnv;
}

QString qmlImportDir()
{
    return binaryDir() + QStringLiteral("/bin");
}

// The QML types of org.raye.files, read from the module the build made.
QStringList qmlTypes()
{
    QFile qmldir(qmlImportDir() + QStringLiteral("/org/raye/files/qmldir"));
    QStringList types;
    if (!qmldir.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return types;
    }
    static const QRegularExpression line(QStringLiteral("^(\\w+) [0-9.]+ \\S+\\.qml$"));
    while (!qmldir.atEnd()) {
        const auto match = line.match(QString::fromUtf8(qmldir.readLine()).trimmed());
        if (match.hasMatch() && !types.contains(match.captured(1))) {
            types.append(match.captured(1));
        }
    }
    types.sort();
    return types;
}

bool parseSize(const QString &value, QSize *size)
{
    static const QRegularExpression pattern(QStringLiteral("^(\\d{3,4})x(\\d{3,4})$"));
    const auto match = pattern.match(value);
    if (!match.hasMatch()) {
        return false;
    }
    const QSize s(match.captured(1).toInt(), match.captured(2).toInt());
    if (s.width() < 320 || s.height() < 240 || s.width() > 7680 || s.height() > 4320) {
        return false;
    }
    *size = s;
    return true;
}

void settle(int ms)
{
    // Long enough for queued showcase steps and the first paint.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

QQuickWidget *quickView(QWidget *parent)
{
    auto *view = new QQuickWidget(parent);
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->engine()->addImportPath(qmlImportDir());
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    auto *context = new KLocalizedContext(view->engine());
    QT_WARNING_POP
    context->setTranslationDomain(QStringLiteral("raye-files"));
    view->engine()->rootContext()->setContextObject(context);
    view->setClearColor(Raye::Ui::Tokens::current().color(Raye::Ui::TokenSet::bgBase));
    return view;
}

bool viewFailed(QQuickWidget *view, QString *error)
{
    if (view->status() != QQuickWidget::Error) {
        return false;
    }
    QStringList messages;
    for (const QQmlError &e : view->errors()) {
        messages.append(e.toString());
    }
    *error = messages.join(QLatin1Char('\n'));
    return true;
}

Raye::Ui::Elevation elevationOf(const QWidget *w)
{
    if (w->windowType() == Qt::ToolTip) {
        return Raye::Ui::Elevation::Tooltip;
    }
    if (w->windowType() == Qt::Popup) {
        return Raye::Ui::Elevation::Flyout;
    }
    return Raye::Ui::Elevation::Dialog;
}

qreal radiusOf(const QWidget *w)
{
    return w->windowType() == Qt::ToolTip ? Raye::Ui::Metrics::radiusControl : Raye::Ui::Metrics::radiusOverlay;
}
}

QString usage()
{
    return QStringLiteral(
        "Usage: raye_test_ui_harness [--widget NAME | --page TYPE | --qml FILE]\n"
        "                            [--theme night|day] [--lang en|fr] [--size WxH] [--shot FILE.png]\n"
        "       raye_test_ui_harness --list\n"
        "Without arguments it runs its own tests.\n");
}

bool parseArguments(const QStringList &arguments, Options *options, QString *error)
{
    *options = Options();
    int shown = 0;
    for (int i = 1; i < arguments.size(); ++i) {
        const QString arg = arguments.at(i);
        auto value = [&](QString *out) {
            if (i + 1 >= arguments.size() || arguments.at(i + 1).startsWith(QLatin1String("--"))) {
                *error = QStringLiteral("%1 needs a value").arg(arg);
                return false;
            }
            *out = arguments.at(++i);
            return true;
        };
        QString v;
        if (arg == QLatin1String("--list")) {
            options->list = true;
        } else if (arg == QLatin1String("--help") || arg == QLatin1String("-h")) {
            options->help = true;
        } else if (arg == QLatin1String("--widget")) {
            if (!value(&options->widget)) {
                return false;
            }
            ++shown;
        } else if (arg == QLatin1String("--page")) {
            if (!value(&options->page)) {
                return false;
            }
            ++shown;
        } else if (arg == QLatin1String("--qml")) {
            if (!value(&options->qmlFile)) {
                return false;
            }
            ++shown;
        } else if (arg == QLatin1String("--theme")) {
            if (!value(&v)) {
                return false;
            }
            if (v != QLatin1String("night") && v != QLatin1String("day")) {
                *error = QStringLiteral("--theme is night or day, not %1").arg(v);
                return false;
            }
            options->night = v == QLatin1String("night");
        } else if (arg == QLatin1String("--lang")) {
            if (!value(&v)) {
                return false;
            }
            static const QRegularExpression code(QStringLiteral("^[a-z]{2,3}(_[A-Z]{2})?$"));
            if (!code.match(v).hasMatch()) {
                *error = QStringLiteral("--lang wants a language code (en, fr), not %1").arg(v);
                return false;
            }
            options->lang = v;
        } else if (arg == QLatin1String("--size")) {
            if (!value(&v)) {
                return false;
            }
            if (!parseSize(v, &options->size)) {
                *error = QStringLiteral("--size is WIDTHxHEIGHT (320x240 to 7680x4320), not %1").arg(v);
                return false;
            }
        } else if (arg == QLatin1String("--shot")) {
            if (!value(&options->shot)) {
                return false;
            }
        } else {
            *error = QStringLiteral("unknown argument: %1").arg(arg);
            return false;
        }
    }
    if (shown > 1) {
        *error = QStringLiteral("show one thing: --widget, --page or --qml");
        return false;
    }
    if (shown == 0 && !options->list && !options->help) {
        *error = QStringLiteral("nothing to show: give --widget, --page, --qml or --list");
        return false;
    }
    return true;
}

void applyTheme(bool night)
{
    QApplication::setPalette(Raye::Ui::TokenSet::table(night).palette());
    QApplication::setFont(Raye::Ui::textFont(Raye::Ui::TextStyle::Body));
    // The Raye icon theme is not in the build tree: Breeze in the same mode.
    QIcon::setThemeName(night ? QStringLiteral("breeze-dark") : QStringLiteral("breeze"));
    Raye::Ui::Tokens::instance()->refresh();
}

bool applyLanguage(const QString &lang)
{
    s_lang = lang;
    // gettext reads no catalog under the C locale: the messages locale has
    // to be a real one (locales-all in the build, every locale in Raye).
    const QString name = lang.contains(QLatin1Char('_')) ? lang : lang + QLatin1Char('_') + lang.toUpper();
    bool found = std::setlocale(LC_MESSAGES, qPrintable(name + QStringLiteral(".UTF-8"))) != nullptr;
    if (!found) {
        std::setlocale(LC_MESSAGES, "C.UTF-8");
        found = lang == QLatin1String("en");
    }
    KLocalizedString::addDomainLocaleDir("raye-files", binaryDir() + QStringLiteral("/locale"));
    KLocalizedString::setLanguages({lang});
    QLocale::setDefault(QLocale(lang));
    return found;
}

QString language()
{
    return s_lang;
}

bool compositing()
{
    return s_shooting || Raye::Ui::compositorActive();
}

QString text(const char *english, const char *french)
{
    return QString::fromUtf8(s_lang.startsWith(QLatin1String("fr")) ? french : english);
}

QWidget *buildWindow(const Options &options, QString *error)
{
    auto window = std::make_unique<QWidget>();
    window->setObjectName(QStringLiteral("RayeHarnessWindow"));
    window->setAutoFillBackground(true);
    window->resize(options.size);
    auto *layout = new QVBoxLayout(window.get());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    if (!options.widget.isEmpty()) {
        const Raye::Ui::Showcase::Entry *entry = Raye::Ui::Showcase::find(options.widget);
        if (!entry) {
            QStringList names;
            for (const auto &e : Raye::Ui::Showcase::entries()) {
                names.append(e.name);
            }
            *error = QStringLiteral("no widget named %1; there are: %2").arg(options.widget, names.join(QStringLiteral(", ")));
            return nullptr;
        }
        window->setWindowTitle(entry->name);
        QWidget *content = entry->build(window.get());
        if (!content) {
            *error = QStringLiteral("%1 built nothing").arg(entry->name);
            return nullptr;
        }
        layout->addWidget(content);
    } else {
        QQuickWidget *view = quickView(window.get());
        if (!options.page.isEmpty()) {
            if (!qmlTypes().contains(options.page)) {
                *error = QStringLiteral("org.raye.files has no type %1; there are: %2").arg(options.page, qmlTypes().join(QStringLiteral(", ")));
                return nullptr;
            }
            window->setWindowTitle(options.page);
            view->loadFromModule("org.raye.files", options.page);
        } else {
            window->setWindowTitle(QFileInfo(options.qmlFile).fileName());
            view->setSource(QUrl::fromLocalFile(QFileInfo(options.qmlFile).absoluteFilePath()));
        }
        if (viewFailed(view, error)) {
            return nullptr;
        }
        layout->addWidget(view);
    }
    return window.release();
}

QImage composite(QWidget *window)
{
    QImage out = window->grab().toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage backdrop = out;
    const QPoint origin = window->mapToGlobal(QPoint(0, 0));
    const bool night = Raye::Ui::Tokens::current().night();

    // Dialogs first, popups and tooltips on top of them.
    QList<QWidget *> floating;
    for (QWidget *w : QApplication::topLevelWidgets()) {
        if (w == window || !w->isVisible() || w->size().isEmpty()) {
            continue;
        }
        const Qt::WindowType type = w->windowType();
        if (type == Qt::Popup || type == Qt::Dialog || type == Qt::ToolTip || type == Qt::Window) {
            floating.append(w);
        }
    }
    std::stable_sort(floating.begin(), floating.end(), [](QWidget *a, QWidget *b) {
        return (a->windowType() == Qt::Popup || a->windowType() == Qt::ToolTip) < (b->windowType() == Qt::Popup || b->windowType() == Qt::ToolTip);
    });

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (QWidget *w : std::as_const(floating)) {
        const QRect g(w->mapToGlobal(QPoint(0, 0)) - origin, w->size());
        const bool translucent = w->testAttribute(Qt::WA_TranslucentBackground);
        if (translucent) {
            const Raye::Ui::Elevation elevation = elevationOf(w);
            const qreal radius = radiusOf(w);
            const int margin = Raye::Ui::Shadow::margin(elevation, night);
            painter.drawImage(g.topLeft() - QPoint(margin, margin), Raye::Ui::Shadow::image(g.size(), radius, elevation, night));
            if (qobject_cast<Raye::Ui::Flyout *>(w) || qobject_cast<QMenu *>(w)) {
                // Acrylic: what lies behind, blurred, under the tint.
                const QRect area = g.intersected(backdrop.rect());
                if (!area.isEmpty()) {
                    const QImage behind = backdrop.copy(area);
                    const QImage blurred = behind.scaled(qMax(1, area.width() / 12), qMax(1, area.height() / 12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                               .scaled(area.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    QPainterPath clip;
                    clip.addRoundedRect(QRectF(g), radius, radius);
                    painter.save();
                    painter.setClipPath(clip);
                    painter.drawImage(area.topLeft(), blurred);
                    painter.restore();
                }
            }
        }
        painter.drawPixmap(g.topLeft(), w->grab());
    }
    painter.end();
    return out;
}

int run(const Options &options)
{
    if (options.help) {
        std::fputs(qPrintable(usage()), stdout);
        return 0;
    }
    if (options.list) {
        std::fputs("Widgets (--widget):\n", stdout);
        for (const auto &e : Raye::Ui::Showcase::entries()) {
            std::fputs(qPrintable(QStringLiteral("  %1  %2\n").arg(e.name, -16).arg(e.description)), stdout);
        }
        std::fputs("QML types of org.raye.files (--page):\n", stdout);
        for (const QString &type : qmlTypes()) {
            std::fputs(qPrintable(QStringLiteral("  %1\n").arg(type)), stdout);
        }
        return 0;
    }

    s_shooting = !options.shot.isEmpty();
    if (s_shooting) {
        Raye::Ui::Motion::setSpeedFactor(0);
    }
    applyTheme(options.night);
    if (!applyLanguage(options.lang)) {
        std::fputs(qPrintable(QStringLiteral("no locale for %1 here: Files' own strings stay English\n").arg(options.lang)), stderr);
    }

    QString error;
    std::unique_ptr<QWidget> window(buildWindow(options, &error));
    if (!window) {
        std::fputs(qPrintable(error + QLatin1Char('\n')), stderr);
        return 2;
    }
    window->move(0, 0);
    window->show();
    if (!s_shooting) {
        return QApplication::exec();
    }
    settle(600);
    const QImage image = composite(window.get());
    if (!image.save(options.shot, "PNG")) {
        std::fputs(qPrintable(QStringLiteral("could not write %1\n").arg(options.shot)), stderr);
        return 1;
    }
    return 0;
}

}
