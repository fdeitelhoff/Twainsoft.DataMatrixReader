#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QString>
#include <QtDebug>
#include <QDir>
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>

#include <infoabout.h>
#include <showimage.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Die Hauptklassen erstellen.
    this->runManager = new RunManager();
    this->dataMatrixReader = new DataMatrixReader();

    // Jeder vom DataMatrixReader durchgeführte Schritte im in einem Run im
    // RunManager festgehalten. Der DataMatrixReader schickt hierfür ein
    // Signal an den RunManager.
    this->connect(this->dataMatrixReader,
                  SIGNAL(stepProcessed(StepData)),
                  this->runManager,
                  SLOT(stepProcessed(StepData)),
                  Qt::DirectConnection);

    // Ist ein Bild fertig analysiert, wird das MainWindow durch ein Signal darüber
    // benachrichtigt.
    this->connect(this->dataMatrixReader,
                  SIGNAL(imageProcessed()),
                  this,
                  SLOT(onImageProcessed()),
                  Qt::DirectConnection);

    this->setWindowState(this->windowState() ^ Qt::WindowMaximized);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionLoadImage_triggered()
{
    QDir appDirPath = QDir(QApplication::applicationDirPath());
    appDirPath.cdUp();
    QString initialPath = appDirPath.absolutePath() + "/Bilder";

    QStringList fileNames = QFileDialog::getOpenFileNames(this,
        tr("Open Image(s)"), initialPath, tr("Image Files (*.png *.jpg *.bmp)"));

    foreach (QString fileName, fileNames)
    {
        this->runManager->createNewRun(fileName);
        Run lastRun = this->runManager->getLastRun();

        QTableWidgetItem *newRunEntryName = new QTableWidgetItem(lastRun.getName() + "  (" + lastRun.getImageSize() + ")");

        this->ui->tableWidgetRuns->setRowCount(this->ui->tableWidgetRuns->rowCount() + 1);
        this->ui->tableWidgetRuns->setItem(this->ui->tableWidgetRuns->rowCount() - 1, 0, newRunEntryName);

        this->dataMatrixReader->processImage(fileName);
    }
}

void MainWindow::onImageProcessed()
{
    this->selectLastRun();
}

void MainWindow::selectLastRun()
{
    if (this->ui->tableWidgetRuns->rowCount() > 0)
    {
        this->ui->tableWidgetRuns->selectRow(this->ui->tableWidgetRuns->rowCount() - 1);

        this->ui->tableWidgetRuns->scrollToBottom();
    }
}

void MainWindow::selectLastStepRun()
{
    if (this->ui->tableWidgetSteps->rowCount() > 0)
    {
        int selectedRow = this->ui->tableWidgetSteps->rowCount() - 1;

        this->ui->tableWidgetSteps->selectRow(selectedRow);

        this->ui->tableWidgetSteps->scrollToBottom();
    }
}

void MainWindow::on_tableWidgetRuns_itemSelectionChanged()
{
    if (this->ui->tableWidgetRuns->selectionModel()->selectedRows().count() == 1)
    {
        QTableWidgetItem *selectedRunEntry = this->ui->tableWidgetRuns->selectedItems().first();

        int selectedRowNumber = selectedRunEntry->row();

        this->runManager->setSelectedRun(selectedRowNumber);
        Run run = this->runManager->getRun(selectedRowNumber);

        if (!run.getStepData().isEmpty())
        {
            this->ui->lblInputImage->setPixmap(run.getStepData().first().getImage().scaled(400, 250, Qt::KeepAspectRatio));

            this->ui->tableWidgetSteps->setRowCount(0);
            foreach (StepData currentStepData, run.getStepData())
            {
                QTableWidgetItem *newStepImageEntry = new QTableWidgetItem(currentStepData.getIcon(), "");

                this->ui->tableWidgetSteps->setRowCount(this->ui->tableWidgetSteps->rowCount() + 1);
                this->ui->tableWidgetSteps->setItem(this->ui->tableWidgetSteps->rowCount() - 1, 0, newStepImageEntry);
            }

            this->selectLastStepRun();
        }
    }
}

void MainWindow::on_tableWidgetSteps_itemSelectionChanged()
{
    if (this->ui->tableWidgetSteps->selectionModel()->selectedRows().count() == 1)
    {
        QTableWidgetItem *selectedStepEntry = this->ui->tableWidgetSteps->selectedItems().first();

        int currentRowNumber = selectedStepEntry->row();

        Run run = this->runManager->getSelectedRun();
        StepData stepData = run.getStepData().value(currentRowNumber);

        this->ui->lblCurrentImage->setPixmap(stepData.getImage().scaled(400, 250, Qt::KeepAspectRatio));

        this->ui->lblCurrentImageName->setText(QString("Infos for the current image (" + stepData.getName() + "):"));

        this->ui->tbMessages->clear();

        if (stepData.getMessages().count() > 0)
        {
            foreach (QString message, stepData.getMessages())
            {
                this->ui->tbMessages->append(message);
            }
        }
    }
}

void MainWindow::on_action_Delete_previous_runs_triggered()
{
    int runCount = this->dataMatrixReader->getPreviousRunCount();

    if (runCount > 0)
    {
        int result = QMessageBox::question(this,
                                           tr("Delete runs"),
                                           "Delete all (" + QString::number(runCount) + ") previous saved runs?",
                                           QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::Yes)
        {
            this->dataMatrixReader->clearAllRuns();
        }
    }
    else
    {
        QMessageBox::information(this,
                                 tr("Delete runs"),
                                 tr("There are no previous saved runs."),
                                 QMessageBox::Ok);
    }
}

void MainWindow::on_pbFullInputImage_clicked()
{
    Run run = this->runManager->getSelectedRun();

    if (!run.isEmpty() && run.getStepData().count() > 0)
    {
        StepData stepData = run.getStepData().first();

        this->showOriginalSizedImage(stepData);
    }
    else
    {
        QMessageBox::information(this,
                                 tr("No image available"),
                                 tr("There is no input image available to display with the original size."),
                                 QMessageBox::Ok);
    }
}

void MainWindow::on_pbFullCurrentImage_clicked()
{
    if (this->ui->tableWidgetSteps->selectionModel()->selectedRows().count() == 1)
    {
        QTableWidgetItem *selectedStepEntry = this->ui->tableWidgetSteps->selectedItems().first();

        int currentRowNumber = selectedStepEntry->row();

        Run run = this->runManager->getSelectedRun();
        StepData stepData = run.getStepData().value(currentRowNumber);

        this->showOriginalSizedImage(stepData);
    }
    else
    {
        QMessageBox::information(this,
                                 tr("No image available"),
                                 tr("There is no current image available to display with the original size."),
                                 QMessageBox::Ok);
    }
}

void MainWindow::showOriginalSizedImage(StepData stepData)
{
    ShowImage *showImage = new ShowImage(this);
    showImage->setModal(false);
    showImage->setImageData(stepData.getImage(), stepData.getName(), stepData.getDescription());
    showImage->show();
}

void MainWindow::on_tableWidgetSteps_doubleClicked(QModelIndex index)
{
    Run run = this->runManager->getSelectedRun();

    if (!run.isEmpty() && run.getStepData().size() > 0)
    {
        StepData stepData = run.getStepData().value(index.row());

        this->showOriginalSizedImage(stepData);
    }
}

void MainWindow::on_actionAboutThisApplication_triggered()
{
    InfoAbout(this).exec();
}

void MainWindow::on_actionSaveImages_triggered()
{
    this->dataMatrixReader->setSaveImagesStatus(this->ui->actionSaveImages->isChecked());
}
