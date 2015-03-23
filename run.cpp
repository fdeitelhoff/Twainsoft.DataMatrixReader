#include "run.h"

Run::Run()
{
}

Run::Run(QString name, QString imageSize)
{
    this->name = name;
    this->imageSize = imageSize;
}

QString Run::getName()
{
    return this->name;
}

QString Run::getImageSize()
{
    return this->imageSize;
}

void Run::addStepData(StepData stepData)
{
    this->stepData.append(stepData);
}

QList<StepData> Run::getStepData()
{
    return this->stepData;
}

bool Run::isEmpty()
{
    return this->name.isEmpty();
}
