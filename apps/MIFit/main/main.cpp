#include "ui/Application.h"

#include <iostream>
#include <QApplication>
#include <QTimer>
#include <QSurfaceFormat>
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
