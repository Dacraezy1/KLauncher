#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QFont>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

#include "ui/MainWindow.h"
#include "utils/AppConfig.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("KLauncher");
    app.setApplicationVersion(KLAUNCHER_VERSION);
    app.setOrganizationName("KLauncher");
    app.setOrganizationDomain("klauncher.dev");

    // High DPI
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Ensure app data dirs exist
    AppConfig::instance().ensureDirectories();

    // Splash screen
    QPixmap splashPix(":/images/splash.png");
    QSplashScreen splash(splashPix.isNull()
        ? QPixmap(600, 350)
        : splashPix);

    if (splashPix.isNull()) {
        splash.setStyleSheet(
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "stop:0 #0f0f1a, stop:1 #1a1a2e);"
            "color: white; font-size: 18px; font-weight: bold;"
        );
    }
    splash.showMessage("Loading KLauncher...",
        Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
    splash.show();
    app.processEvents();

    MainWindow w;
    w.show();
    splash.finish(&w);

    return app.exec();
}
