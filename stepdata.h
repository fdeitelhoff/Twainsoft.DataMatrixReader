#ifndef STEPDATA_H
#define STEPDATA_H

#include <QIcon>
#include <QPixmap>
#include <QStringList>
#include <cv.h>

class StepData
{

public:
    StepData();

    StepData(QString name, QString description, QIcon icon, QPixmap image, QStringList additionalData);

    QIcon currentPicture;

    QString getName();

    QString getDescription();

    QIcon getIcon();

    QPixmap getImage();

    QStringList getMessages();

private:
    QString name;

    QString description;

    QIcon icon;

    QPixmap image;

    QStringList messages;
};

#endif // STEPDATA_H
