#ifndef DATAMATRIXREADER_H
#define DATAMATRIXREADER_H

#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QThread>
#include <houghresultdata.h>
#include <cv.h>
#include <highgui.h>
#include <blockdata.h>
#include <QProcess>
#include <QTemporaryFile>

#include <stepdata.h>

class DataMatrixReader : public QObject
{
    Q_OBJECT

public:
    DataMatrixReader();

    ~DataMatrixReader();

    void processImage(QString fileName);

    void clearAllRuns();

    int getPreviousRunCount();

    void setSaveImagesStatus(bool saveImages);

private:
    QString runPath;

    QString allRunsPath;

    QProcess *dmtxProcess;

    QTemporaryFile *dmtxReadInputImage;

    QTemporaryFile *dmtxReadOutputFile;

    bool saveImageStatus;

    int saveImageCount;

    IplImage* loadSourceImage(QString fileName);

    IplImage* createGrayImage(IplImage* sourceImage);

    QString createRunPath(QString filePath);

    void rotateDataMatrixEdges(CvPoint* dataMatrixEdges, CvPoint center, double angle);

    void decodeDataMatrixBlock(IplImage* newMatrixImage);

    void saveImage(QString imagePath, IplImage* image);

    CvPoint setCvPoint(int x, int y);

    CvPoint rotatePoint( CvPoint center, CvPoint toTurn, double alpha);

    HoughResultData* processHoughResult(CvSeq *lines, int width, int height);

    CvPoint* calculateEdgesAfterRotation(HoughResultData *houghResultData, IplImage* sourceGrayImage, IplImage* sourceColorImage, IplImage* rotatedColorImage, double angle);

    int detectFinderPosition(CvPoint* edge, IplImage* currentWorkThreshImage);

    CvPoint detectSecondMaxPoint(CvSeq *lines, CvPoint max2, CvPoint finder);

    void paintCurrentPositions(IplImage* currentWorkThreshImage, CvPoint max1, CvPoint max2, CvPoint center, CvPoint finder, CvPoint finderCorr);

    void paintHoughLines(IplImage* sourceGrayImage, HoughResultData *houghResultData);

    double calculateRotationAngle(CvPoint *edge, int finderPos);

    IplImage* rotateImage(IplImage* currentImage, CvPoint center, double angle);

    void calculateMissingPoint(CvPoint* edge, int finderPos, CvPoint max);

    void paintDataMatrixEdges(IplImage *currentWorkRotatedImage, CvPoint *edge, CvPoint finder);

    IplImage* removePerspective(IplImage* currentWorkRotatedImage, CvPoint* edge, int finderPos);

    IplImage* createROI(IplImage *currentWorkImage, CvPoint *dataMatrixEdges, int finderPos);

    IplImage* processCanny(IplImage *sourceGrayImage);

    HoughResultData* processHough(IplImage* sourceGrayImage);

    CvPoint calculateCenterPoint(CvPoint* dataMatrixEdges);

    IplImage* smoothImage(IplImage* currentImage);

    void processDataBlock(IplImage* currentImage);

    void saveAndSendImage(IplImage* currentImage, QString imageName, QString description, QStringList additionalData);

    IplImage* processThresh(IplImage *currentImage, QString fileName, QString description);

    BlockData calculateAvgBlockSize(IplImage* currentImage, CvPoint startPoint, CvPoint endPoint);

    void drawNewMatrix(int blockCount, bool **fieldValues);

    QString cvPointToString(CvPoint point);

    QString cvPointToString(CvPoint2D32f point);

signals:
    void stepProcessed(StepData stepData);

    void imageProcessed();
};

#endif // DATAMATRIXREADER_H
