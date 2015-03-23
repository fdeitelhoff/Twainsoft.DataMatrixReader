#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QModelIndex>
#include <cv.h>

#include "runmanager.h"
#include "datamatrixreader.h"
#include <stepdata.h>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    RunManager *runManager;

    DataMatrixReader *dataMatrixReader;

    void showOriginalSizedImage(StepData stepData);

    void selectLastStepRun();

    void selectLastRun();

private slots:
    void on_actionSaveImages_triggered();
    void on_actionAboutThisApplication_triggered();
    void on_tableWidgetSteps_doubleClicked(QModelIndex index);
    void on_pbFullCurrentImage_clicked();
    void on_pbFullInputImage_clicked();
    void on_action_Delete_previous_runs_triggered();
    void on_tableWidgetSteps_itemSelectionChanged();
    void on_tableWidgetRuns_itemSelectionChanged();
    void on_actionLoadImage_triggered();
    void onImageProcessed();
};

#endif // MAINWINDOW_H
