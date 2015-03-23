#ifndef RUN_H
#define RUN_H

#include <QString>
#include <QList>

#include <stepdata.h>

class Run
{

public:
    Run();

    Run(QString name, QString imageSize);

    QString getName();

    QString getImageSize();

    void addStepData(StepData stepData);

    QList<StepData> getStepData();

    bool isEmpty();

private:
    QString name;

    QString imageSize;

    QList<StepData> stepData;
};

#endif // RUN_H
