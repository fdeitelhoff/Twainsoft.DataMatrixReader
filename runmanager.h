#ifndef RUNMANAGER_H
#define RUNMANAGER_H

#include <QString>
#include <QHash>
#include <QDebug>

#include <run.h>
#include <stepdata.h>

class RunManager : public QObject
{
    Q_OBJECT

public:
    RunManager();

    void createNewRun(QString fileName);

    Run getLastRun();

    void replaceLastRun(Run run);

    void setSelectedRun(int index);

    Run getSelectedRun();

    Run getRun(int index);

private:
    QList<Run> runs;

    int lastRun;

    int selectedRun;

private slots:
    void stepProcessed(StepData stepData);
};

#endif // RUNMANAGER_H
