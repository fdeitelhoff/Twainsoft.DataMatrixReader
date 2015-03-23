#include "showimage.h"
#include "ui_showimage.h"

ShowImage::ShowImage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShowImage)
{
    ui->setupUi(this);

    this->setWindowFlags(Qt::Window);
}

ShowImage::~ShowImage()
{
    delete ui;
}

void ShowImage::on_pbClose_clicked()
{
    this->close();
}

void ShowImage::setImageData(QPixmap image, QString name, QString description)
{
    int width = image.width();
    int height = image.height();

    if (width < 900 && height < 700)
    {
        this->resize(width, height);
    }

    this->setWindowTitle(name);

    this->ui->lblImage->setPixmap(image);
    this->ui->lblImageDescription->setText(description);
}
