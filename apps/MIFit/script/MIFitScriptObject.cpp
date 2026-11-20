#include "MIFitScriptObject.h"
#include <core/Version.h>
#include <jobs/BatchJob.h>
#include <jobs/BatchJobManager.h>
#include <map/maplib.h>
#include <QFileInfo>
#include <ui/Application.h>
#include <ui/Displaylist.h>
#include <ui/MIGLWidget.h>
#include <ui/MIMainWindow.h>

namespace
{
QJsonObject okReply(const QJsonValue &result)
{
    QJsonObject o;
    o["ok"] = true;
    o["result"] = result;
    return o;
}

QJsonObject errorReply(const QString &message)
{
    QJsonObject o;
    o["ok"] = false;
    o["error"] = message;
    return o;
}

QJsonArray toJson(const QStringList &list)
{
    QJsonArray a;
    foreach (const QString &s, list)
    {
        a.append(s);
    }
    return a;
}
} // anonymous namespace

QJsonObject MIFitScriptObject::execute(const QString &cmd, const QJsonArray &args)
{
    // every argument is a string, except addJob's argument list
    const int needed = cmd == "setJobWorkDir" ? 2 : cmd == "writeCurrentModel" ? 1 : cmd == "addJob" ? 5 : 0;
    const bool known = cmd == "version" || cmd == "directory" || cmd == "dictionaryResidueList"
                       || cmd == "spacegroupList" || needed > 0;
    if (!known)
    {
        return errorReply("unknown command: " + cmd);
    }
    if (args.size() < needed)
    {
        return errorReply(QString("%1 requires %2 arguments").arg(cmd).arg(needed));
    }

    if (cmd == "version")
    {
        return okReply(version());
    }
    if (cmd == "directory")
    {
        return okReply(directory());
    }
    if (cmd == "dictionaryResidueList")
    {
        return okReply(toJson(dictionaryResidueList()));
    }
    if (cmd == "spacegroupList")
    {
        return okReply(toJson(spacegroupList()));
    }
    if (cmd == "setJobWorkDir")
    {
        return okReply(setJobWorkDir(args[0].toString(), args[1].toString()));
    }
    if (cmd == "writeCurrentModel")
    {
        QString error;
        bool ok = writeCurrentModel(args[0].toString(), error);
        return error.isEmpty() ? okReply(ok) : errorReply(error);
    }

    // addJob
    if (!args[3].isArray())
    {
        return errorReply("addJob: arguments must be an array");
    }
    QStringList jobArgs;
    foreach (const QJsonValue &v, args[3].toArray())
    {
        jobArgs << v.toString();
    }
    addJob(args[0].toString(), args[1].toString(), args[2].toString(), jobArgs, args[4].toString());
    return okReply(true);
}

QString MIFitScriptObject::version()
{
    return MIFit_version;
}

QString MIFitScriptObject::directory()
{
    return Application::instance()->GetMolimageHome().c_str();
}

bool MIFitScriptObject::writeCurrentModel(const QString &file, QString &error)
{
    MIMainWindow *window = MIMainWindow::instance();
    MIGLWidget *doc = window ? window->currentMIGLWidget() : NULL;
    Displaylist *displaylist = doc ? doc->GetDisplaylist() : NULL;
    Molecule *model = displaylist ? displaylist->GetCurrentModel() : NULL;
    if (!model)
    {
        error = "no active model";
        return false;
    }
    QFileInfo fileInfo(file);
    return model->SaveCoordinateFile(fileInfo.absoluteFilePath().toLocal8Bit().constData());
}

QStringList MIFitScriptObject::dictionaryResidueList()
{
    QStringList result;
    std::vector<std::string> choices = MIFitDictionary()->GetDictResList();
    for (size_t i = 0; i < choices.size(); ++i)
    {
        result << choices[i].c_str();
    }
    return result;
}

QStringList MIFitScriptObject::spacegroupList()
{
    std::vector<std::string> sg;
    MIGetSpacegroups(sg);
    QStringList result;
    for (size_t i = 0; i < sg.size(); ++i)
    {
        result << sg[i].c_str();
    }
    return result;
}

void MIFitScriptObject::addJob(const QString &menuName, const QString &jobName, const QString &executable, const QStringList &arguments, const QString &workingDirectory)
{
    if (!MIMainWindow::instance())
    {
        return;
    }
    MIMainWindow::instance()->addJob(menuName, jobName, executable, arguments, workingDirectory);
}

bool MIFitScriptObject::setJobWorkDir(const QString &jobId, const QString &workDir)
{
    bool ok;
    unsigned long jobIdNum = jobId.toULong(&ok);
    if (!ok || !MIMainWindow::instance() || !MIMainWindow::instance()->GetJobManager())
    {
        return false;
    }
    std::vector<BatchJob*> &jobs = *MIMainWindow::instance()->GetJobManager()->GetJobList();
    foreach (BatchJob *job, jobs)
    {
        if (jobIdNum == job->jobId())
        {
            job->setWorkingDirectory(workDir);
            return true;
        }
    }
    return false;
}
