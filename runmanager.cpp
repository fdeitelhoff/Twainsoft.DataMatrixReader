#include "runmanager.h"

#include <QFileInfo>
#include <QPixmap>

RunManager::RunManager()
{
    this->lastRun = -1;
}

void RunManager::createNewRun(QString fileName)
{
    QFileInfo fileInfo = QFileInfo(fileName);

    QPixmap image = QPixmap(fileName);

    this->runs.append(Run(fileInfo.fileName(), QString::number(image.width()) + "x" + QString::number(image.height())));

    this->lastRun = this->runs.count() - 1;
}

void RunManager::stepProcessed(StepData stepData)
{
    Run run = this->getLastRun();
    run.addStepData(stepData);

    this->replaceLastRun(run);
}

Run RunManager::getRun(int index)
{
    return this->runs.value(index);
}

Run RunManager::getLastRun()
{
    return this->runs.value(this->lastRun);
}

void RunManager::replaceLastRun(Run run)
{
    this->runs.replace(this->lastRun, run);
}

void RunManager::setSelectedRun(int index)
{
    this->selectedRun = index;
}

Run RunManager::getSelectedRun()
{
    return this->runs.value(this->selectedRun);
}
