#include "stepdata.h"

StepData::StepData()
{
}

StepData::StepData(QString name, QString description, QIcon icon, QPixmap image, QStringList additionalData)
{
    this->name = name;
    this->description = description;
    this->icon = icon;
    this->image = image;
    this->messages = additionalData;
}

QString StepData::getName()
{
    return this->name;
}

QString StepData::getDescription()
{
    return this->description;
}

QIcon StepData::getIcon()
{
    return this->icon;
}

QPixmap StepData::getImage()
{
    return this->image;
}

QStringList StepData::getMessages()
{
    return this->messages;
}
