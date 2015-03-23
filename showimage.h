#ifndef SHOWIMAGE_H
#define SHOWIMAGE_H

#include <QDialog>
#include <QPixmap>
#include <QString>

namespace Ui {
    class ShowImage;
}

class ShowImage : public QDialog
{
    Q_OBJECT

public:
    explicit ShowImage(QWidget *parent = 0);

    ~ShowImage();

    void setImageData(QPixmap image, QString name, QString description);

private:
    Ui::ShowImage *ui;

private slots:
    void on_pbClose_clicked();
};

#endif // SHOWIMAGE_H
