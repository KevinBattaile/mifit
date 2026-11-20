#include "ui/Application.h"

#include <iostream>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QTimer>
#include <QSurfaceFormat>
#include "ui/Logger.h"
#include "core/Version.h"
#include "ui/MIMainWindow.h"

int main(int argc, char **argv)
{
    qputenv("QT_OPENGL", QByteArray("desktop"));
    qputenv("QSG_RHI_BACKEND", QByteArray("opengl"));
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    int result = -1;
    // First thing written to mifit.log; file-only because the main window does not exist yet
    Logger::logFileOnly("==================================================");
    Logger::logFileOnly("MIFit %s starting  %s", MIFit_version,
                        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString().c_str());
    Logger::logFileOnly("Build: %s %s, Qt %s, cwd: %s", __DATE__, __TIME__, qVersion(),
                        QDir::currentPath().toStdString().c_str());
    Logger::logFileOnly("==================================================");
    Application app(argc, argv);
#ifdef Q_OS_LINUX
    QApplication::setStyle("Plastique");
#endif

    try
    {
        MIMainWindow *mw = MIMainWindow::instance();
        QTimer::singleShot(0, &app, SLOT(AfterInit()));
        QTimer::singleShot(500, mw, SLOT(AfterInit()));

        mw->show();

        result = app.exec();
        delete mw;
    }
    catch (...)
    {
        std::cerr << "MIFit: unknown exception\n";
        result = 1;
    }
    return result;
}
