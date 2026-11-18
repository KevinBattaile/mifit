#include "Logger.h"

#include <cstdarg>
#include <string>

#include <util/utillib.h>

#include "MIMainWindow.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>

static void writeToLogFile(const char *level, const std::string &s)
{
    QFile logFile(QDir::currentPath() + "/mifit.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        QTextStream out(&logFile);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        QString msg = QString::fromStdString(s);
        if (msg.endsWith('\n'))
        {
            msg.chop(1);
        }
        out << "[" << timestamp << "] [" << level << "] " << msg << "\n";
        out.flush();
    }
}

void Logger::log(const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    std::string s = format_arg_list(format, argp);
    va_end(argp);
    writeToLogFile("LOG", s);
    MIMainWindowLog(s);
}

void Logger::message(const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    std::string s = format_arg_list(format, argp);
    va_end(argp);
    QMessageBox::information(0, "Info", s.c_str());
}

void Logger::footer(const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    std::string s = format_arg_list(format, argp);
    va_end(argp);
    MIMainWindowLeftFooter(s);
}

void Logger::debug(const char *format, ...)
{
#ifdef DEBUG
    va_list argp;
    va_start(argp, format);
    std::string s = format_arg_list(format, argp);
    va_end(argp);
    writeToLogFile("DEBUG", s);
    MIMainWindowDebug(s);
#else
    (void)format;
#endif
}

