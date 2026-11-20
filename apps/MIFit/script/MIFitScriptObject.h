#ifndef script_MIFitScriptObject_h
#define script_MIFitScriptObject_h

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

/**
 * Executes the commands that external scripts (MIExpert) can send to MIFit.
 * Commands: version, directory, setJobWorkDir, writeCurrentModel,
 * dictionaryResidueList, spacegroupList, addJob.
 */
class MIFitScriptObject
{
public:
    // Returns {"ok": true, "result": ...} or {"ok": false, "error": "..."}
    QJsonObject execute(const QString &cmd, const QJsonArray &args);

private:
    QString version();
    QString directory();
    bool writeCurrentModel(const QString &file, QString &error);
    QStringList dictionaryResidueList();
    QStringList spacegroupList();
    void addJob(const QString &menuName, const QString &jobName, const QString &executable, const QStringList &arguments, const QString &workingDirectory);
    bool setJobWorkDir(const QString &jobId, const QString &workDir);
};

#endif // ifndef script_MIFitScriptObject_h
