#ifndef INFOABOUT_H
#define INFOABOUT_H

#include <QDialog>

namespace Ui {
    class InfoAbout;
}

class InfoAbout : public QDialog
{
    Q_OBJECT

public:
    explicit InfoAbout(QWidget *parent = 0);
    ~InfoAbout();

private:
    Ui::InfoAbout *ui;

private slots:
    void on_pbClose_clicked();
};

#endif // INFOABOUT_H
