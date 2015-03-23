#include "datamatrixreader.h"

#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QThread>
#include <houghresultdata.h>
#include <cv.h>
#include <highgui.h>
#include <QProcess>
#include <QTextEdit>
#include <QTemporaryFile>
#include <QTextStream>
#include <QFile>
#include <iplimageconverter.h>

/*
 *  Konstruktor.
 */
DataMatrixReader::DataMatrixReader()
{
    this->saveImageStatus = false;

    this->allRunsPath = QDir::tempPath() + "/DataMatrixReaderRuns";

    QDir allRunsPath = QDir(this->allRunsPath);
    if (!allRunsPath.exists(this->allRunsPath))
    {
        allRunsPath.mkpath(this->allRunsPath);
    }
}

/*
 *  Dekonstruktor.
 */
DataMatrixReader::~DataMatrixReader()
{
    delete this->dmtxProcess;
    delete this->dmtxReadInputImage;
    delete this->dmtxReadOutputFile;
}

/*
 *  Löscht alle zwischenzeitlich gespeicherten Dateien aller Läufe.
 */
void DataMatrixReader::clearAllRuns()
{
    QDir allRunsPath = QDir(this->allRunsPath);
    if (allRunsPath.exists(this->allRunsPath))
    {
        QFileInfoList dirs = allRunsPath.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
        foreach (QFileInfo dir, dirs)
        {
            QDir currentDir = QDir(dir.absoluteFilePath());
            QFileInfoList files = currentDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files);

            foreach (QFileInfo currentFile, files)
            {
                 currentDir.remove(currentFile.fileName());
            }

            currentDir.rmdir(dir.absoluteFilePath());
        }
    }
}

/*
 *  Ermittelt die Anzahl der vorherigen Läufe.
 */
int DataMatrixReader::getPreviousRunCount()
{
    int runCount = 0;

    QDir allRunsPath = QDir(this->allRunsPath);
    if (allRunsPath.exists(this->allRunsPath))
    {
        QFileInfoList dirs = allRunsPath.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);

        runCount = dirs.count();
    }

    return runCount;
}

/*
 *  Erstellt den Pfad zum Speicherort eines bestimmten Laufs.
 */
QString DataMatrixReader::createRunPath(QString filePath)
{
    QFileInfo fileInfo = QFileInfo(filePath);
    QString dateTime = QDateTime::currentDateTime().toString("dd.MM.yyyy hh.mm.ss");

    QString path = this->allRunsPath + "/" + fileInfo.fileName() + " " + dateTime;
    QDir runPath = QDir(path);
    if (!runPath.exists(path))
    {
        runPath.mkpath(path);
    }

    return runPath.absolutePath();
}

/*
 *  Haupteinsprungspunkt für den DataMatrixReader.
 *  Hier beginnt die Analyse eines Bildes.
 */
void DataMatrixReader::processImage(QString fileName)
{
    this->saveImageCount = 0;

    // Sollen die Bilder gespeichert werden, wird ein spezieller Pfad dafür erstellt.
    if (this->saveImageStatus)
    {
        this->runPath = this->createRunPath(fileName);
    }

    // Das Quellbild in Farbe laden und daraus ein zusätzliches Graubild erstellen.
    IplImage* sourceColorImage = this->loadSourceImage(fileName);
    IplImage* sourceGrayImage = this->createGrayImage(sourceColorImage);

    QStringList sourceGrayImageData;
    this->saveAndSendImage(sourceGrayImage, "SourceGrayImage.jpg", "Source image (gray)",
                           sourceGrayImageData);

    // Geradenerkennung mittels Hough-Transformation im Graubild.
    HoughResultData *houghResultData = this->processHough(sourceGrayImage);

    CvPoint* dataMatrixEdges = houghResultData->getDataMatrixEdges();

    IplImage* currentWorkColorImage = sourceColorImage;

    // Muss das aktuelle Bild gedreht werden, um die Eckpunkte besser erkennen zu können?
    if (houghResultData->getHasToBeRotated())
    {
        double angle = 60;

        // Das Bild drehen, damit die Eckpunkte sicher gefunden werden können.
        IplImage* rotatedColorImage = this->rotateImage(sourceColorImage, houghResultData->getCenter(),
                                                        angle);

        QStringList colorImageData;
        colorImageData.append("Rotation center: " + this->cvPointToString(houghResultData->getCenter()));
        colorImageData.append("Rotation angle: " + QString::number(angle) + " degree");

        this->saveAndSendImage(rotatedColorImage, "RotatedImage.jpg", "Rotated image", colorImageData);

        // Nach dem Drehen können die Eckpunkte neu berechnet werden.
        dataMatrixEdges = this->calculateEdgesAfterRotation(houghResultData, sourceGrayImage,
                                                            sourceColorImage, rotatedColorImage, angle);

        currentWorkColorImage = rotatedColorImage;
    }

    IplImage* currentWorkThreshImage = this->processThresh(currentWorkColorImage,
                                                           "CurrentWorkThreshImage.jpg",
                                                           "Current work image with threshold");

    // Die Position (welche Ecke der DataMatrix) des Finder-Punktes ermitteln.
    int finderPos = this->detectFinderPosition(dataMatrixEdges, currentWorkThreshImage);
    CvPoint finder = dataMatrixEdges[finderPos];

    CvPoint max2 = dataMatrixEdges[(finderPos + 2) % 4];
    CvPoint max1 = this->detectSecondMaxPoint(houghResultData->getHoughLines(), max2, finder);

    CvPoint center = this->calculateCenterPoint(houghResultData->getDataMatrixEdges());

    this->paintCurrentPositions(currentWorkThreshImage, max1, max2, center,
                                finder, dataMatrixEdges[(finderPos + 1) % 4]);

    double angle = this->calculateRotationAngle(dataMatrixEdges, finderPos);

    IplImage* currentWorkRotatedImage = this->rotateImage(currentWorkThreshImage, center, angle);

    // Nach dem Rotieren müssten auch alle Punkte gedreht werden, die noch Verwendet werden.
    this->rotateDataMatrixEdges(dataMatrixEdges, center, angle);
    max1 = this->rotatePoint(center, max1, angle);
    finder = this->rotatePoint(center, finder, angle);

    this->calculateMissingPoint(dataMatrixEdges, finderPos, max1);

    this->paintDataMatrixEdges(currentWorkRotatedImage, dataMatrixEdges, finder);

    IplImage* currentWorkImage = this->removePerspective(currentWorkRotatedImage, dataMatrixEdges, finderPos);

    IplImage* currentROIImage = this->createROI(currentWorkImage, dataMatrixEdges, finderPos);

    IplImage* threshedROIImage = this->processThresh(currentROIImage,
                                                     "ThreshedROIImage.jpg",
                                                     "ROI image with binary threshold.");

    IplImage* currentSmoothedImage = this->smoothImage(threshedROIImage);

    this->processDataBlock(currentSmoothedImage);
}

/*
 *  Läd das Quellbild für die Analyse.
 */
IplImage* DataMatrixReader::loadSourceImage(QString fileName)
{
    QByteArray   bytes               = fileName.toAscii();
    const char * ptrSourceColorImage = bytes.data();

    IplImage* sourceColorImage = cvLoadImage(ptrSourceColorImage, 1);  // Als Farbbild laden

    QStringList sourceColorImageData;
    this->saveAndSendImage(sourceColorImage, "SourceColorImage.jpg", "Source image (color)",
                           sourceColorImageData);

    return sourceColorImage;
}

/*
 *  Wandelt das übergebene Farbild ein ein Graubild um.
 */
IplImage* DataMatrixReader::createGrayImage(IplImage* sourceImage)
{
    IplImage* sourceGrayImage = cvCreateImage(cvGetSize(sourceImage), IPL_DEPTH_8U, 1);  // 8 Bit, 1 Farbe
    cvCvtColor(sourceImage, sourceGrayImage, CV_BGR2GRAY); // in Graubild konvertieren

    return sourceGrayImage;
}

/*
 *  Wendet auf das übergebene Bild die Hough-Transformation an.
 */
HoughResultData* DataMatrixReader::processHough(IplImage *sourceGrayImage)
{
    IplImage* cannyResultImage = this->processCanny(sourceGrayImage);

    /*-----------------------------------------------------------------------------
     *  Die Hough- Transformation detektiert alle Geraden in dem Bild, die durch
     *  zwei Punkte beschrieben werden. Dabei wird der Winkel, in dem diese
     *  Geraden zum Ursprung liegen können in 0.5° Schritten (2*PI/720) durchgegangen.
     *  Dieser hohe Wert bedeutet eine hohe Rechenintensitaet.
     *-----------------------------------------------------------------------------*/
    CvSeq* lines = cvHoughLines2(cannyResultImage,
                                 cvCreateMemStorage(0),
                                 CV_HOUGH_PROBABILISTIC, 1, CV_PI/720, 35, 100, 250);

    HoughResultData *houghResultData = this->processHoughResult(lines,
                                                                cannyResultImage->width,
                                                                cannyResultImage->height);

    houghResultData->setHoughLines(lines);

    this->paintHoughLines(sourceGrayImage, houghResultData);

    return houghResultData;
}

/*
 *  Berechnet die Ecken der DataMatrix nach dem Drehen des Bildes erneut.
 */
CvPoint* DataMatrixReader::calculateEdgesAfterRotation(HoughResultData *houghResultData, IplImage* sourceGrayImage, IplImage* sourceColorImage, IplImage* rotatedColorImage, double angle)
{
    int x_max = 0;
    int y_max = 0;
    int x_min = sourceGrayImage->width;
    int y_min = sourceGrayImage->height;
    CvPoint *edge = new CvPoint[4];

    IplImage* rotatedHoughColorImageShowResult = cvCreateImage( cvGetSize(sourceColorImage), 8, 3 );
    rotatedHoughColorImageShowResult = cvCloneImage(rotatedColorImage);

    for( int i = 0; i < houghResultData->getHoughLines()->total; i++ )
    {
        CvPoint* line = (CvPoint*)cvGetSeqElem(houghResultData->getHoughLines(),i);

        int x_p0 = *(&line[0].x);
        int y_p0 = *(&line[0].y);
        int x_p1 = *(&line[1].x);
        int y_p1 = *(&line[1].y);

        CvPoint p0 = this->rotatePoint(houghResultData->getCenter(),
                                       this->setCvPoint(x_p0, y_p0),
                                       angle);

        CvPoint p1 = this->rotatePoint(houghResultData->getCenter(),
                                       this->setCvPoint(x_p1, y_p1),
                                       angle);

        x_p0 = p0.x;
        y_p0 = p0.y;
        x_p1 = p1.x;
        y_p1 = p1.y;

        *(&line[0].x) = x_p0;
        *(&line[0].y) = y_p0;
        *(&line[1].x) = x_p1;
        *(&line[1].y) = y_p1;

        cvLine(rotatedHoughColorImageShowResult, p0, p1, cvScalar(255, 0, 0));

        /*-----------------------------------------------------------------------------
         *  Berechnen der Eckpunkte der Matrix. Liegen Kanten der Matrix senkrecht oder
         *  waagerecht zum Koordinatenkreuz, so ist eine eindeutige Bestimmung im ersten
         *  Durchlauf noch nicht möglich. Die Matrix wird in diesem Fall um 60° gedreht.
         *-----------------------------------------------------------------------------*/
        x_max = max(x_max, max(x_p0, x_p1));
        if( x_max == x_p0)
            edge[1] = setCvPoint( x_p0, y_p0);
        if( x_max == x_p1)
            edge[1] = setCvPoint( x_p1, y_p1);

        x_min = min(x_min, min(x_p0, x_p1));
        if( x_min == x_p0)
            edge[3] = setCvPoint( x_p0, y_p0);
        if( x_min == x_p1)
            edge[3] = setCvPoint( x_p1, y_p1);

        y_max = max(y_max, max(y_p0, y_p1));
        if( y_max == y_p0)
            edge[2] = setCvPoint( x_p0, y_p0);
        if( y_max == y_p1)
            edge[2] = setCvPoint( x_p1, y_p1);

        y_min = min(y_min, min(y_p0, y_p1));
        if( y_min == y_p0)
            edge[0] = setCvPoint( x_p0, y_p0);
        if( y_min == y_p1)
            edge[0] = setCvPoint( x_p1, y_p1);
    }

    QStringList houghColorImageData;
    this->saveAndSendImage(rotatedHoughColorImageShowResult, "RotatedHoughColorImageShowResult.jpg",
                           "Rotated Hough result image", houghColorImageData);

    return edge;
}

/*
 *  Versucht im übergebenen Bild den DataMatrix Datenblock zu finden,
 *  die Spalten- und Zeilenanzahl zu bestimmen und aus dem Datenblock
 *  eine Bit-Matrix zu erstellen.
 */
void DataMatrixReader::processDataBlock(IplImage* currentImage)
{
    int offset = 4;

    CvPoint topLeft = this->setCvPoint(offset, offset);
    CvPoint topRight = this->setCvPoint(currentImage->width - offset, offset);
    CvPoint bottomRight = this->setCvPoint(currentImage->width - offset, currentImage->height - offset);

    BlockData blockDataTop = this->calculateAvgBlockSize(currentImage, topLeft, topRight);
    BlockData blockDataRight = this->calculateAvgBlockSize(currentImage, bottomRight, topRight);

    IplImage* imageBlockLines = cvCreateImage(cvGetSize(currentImage), IPL_DEPTH_8U, 3);
    cvCvtColor(currentImage, imageBlockLines, CV_GRAY2BGR);

    IplImage* imageTrueFields = cvCreateImage(cvGetSize(currentImage), IPL_DEPTH_8U, 3);
    cvCvtColor(currentImage, imageTrueFields, CV_GRAY2BGR);

    cvLine(imageBlockLines, topLeft, topRight, cvScalar(0, 0, 255));
    cvLine(imageBlockLines, bottomRight, topRight, cvScalar(0, 255, 0));

    double cvPointX = 0;
    double cvPointY = 0;
    bool** fieldValues = new bool*[blockDataTop.getBlockCount()];

    int trueFieldCount = 0;
    int falseFieldCount = 0;

    for (int i = 0; i < blockDataTop.getBlockCount(); i++)
    {
        fieldValues[i] = new bool[blockDataTop.getBlockCount()];
    }

    for (int x = 0; x < blockDataTop.getBlockCount(); x++)
    {
        for (int y = 0; y < blockDataRight.getBlockCount(); y++)
        {
            CvPoint center = this->setCvPoint(0, 0);
            cvPointX = topLeft.x
                       + blockDataTop.getLineStart()
                       + (blockDataTop.getAvgBlockSize() / 2)
                       + x * blockDataTop.getAvgBlockSize();

            center.x = cvPointX;

            cvPointY = bottomRight.y + 1
                       - blockDataRight.getLineStart()
                       - (blockDataRight.getAvgBlockSize() / 2)
                       - y * blockDataRight.getAvgBlockSize();

            center.y = cvPointY;

            CvScalar pixelValue = cvGet2D(currentImage, center.y, center.x);

            fieldValues[x][y] = pixelValue.val[0] == 0;

            cvLine(imageBlockLines, this->setCvPoint(center.x - 2, center.y),
                   this->setCvPoint(center.x + 2, center.y), cvScalar(0, 0, 255));
            cvLine(imageBlockLines, this->setCvPoint(center.x, center.y - 2),
                   this->setCvPoint(center.x, center.y + 2), cvScalar(0, 0, 255));

            if (pixelValue.val[0] == 0)
            {
                cvLine(imageTrueFields, this->setCvPoint(center.x - 2, center.y),
                       this->setCvPoint(center.x + 2, center.y), cvScalar(0, 0, 255));
                cvLine(imageTrueFields, this->setCvPoint(center.x, center.y - 2),
                       this->setCvPoint(center.x, center.y + 2), cvScalar(0, 0, 255));

                trueFieldCount++;
            }
            else
            {
                falseFieldCount++;
            }
        }
    }

    QStringList imageBlockLinesData;
    imageBlockLinesData.append("Horizontal & vertical block count: "
                               + QString::number(blockDataRight.getBlockCount()));

    this->saveAndSendImage(imageBlockLines, "ImageBlockLines.jpg", "Image with block lines.",
                           imageBlockLinesData);

    QStringList imageTrueFieldsData;
    imageTrueFieldsData.append("True field count: " + QString::number(trueFieldCount));
    imageTrueFieldsData.append("False field count: " + QString::number(falseFieldCount));

    this->saveAndSendImage(imageTrueFields, "ImageTrueFields.jpg", "Image with true field values.",
                           imageTrueFieldsData);

    this->drawNewMatrix(blockDataTop.getBlockCount(), fieldValues);
}

/*
 *  Erstellt aus der übergebenen Bit-Matrix ein neues Bild mit einer neu gezeichneten Matrix.
 */
void DataMatrixReader::drawNewMatrix(int blockCount, bool **fieldValues)
{
    int blockSize = 10;
    int offset = 5;
    int width = (blockSize * (blockCount + 2)) + offset * 2;
    int height = width;

    // Neues Bild erstellen und alle Pixel auf weiß setzen.
    IplImage *newMatrixImage = cvCreateImage(cvSize(width, height), 8, 1);
    cvAddS(newMatrixImage, cvScalar(255, 255, 255), newMatrixImage);

    // Datenbereich der DataMatrix.
    for (int x = 1; x <= blockCount; x++)
    {
        for (int y = 1; y <= blockCount; y++)
        {
            CvPoint bottomLeft;
            bottomLeft.x = x * blockSize + offset;
            bottomLeft.y = newMatrixImage->height - blockSize * y - offset;

            CvPoint topRight;
            topRight.x = bottomLeft.x + blockSize - 1;
            topRight.y = bottomLeft.y - blockSize + 1;

            if (fieldValues[x - 1][y - 1])
            {
                cvRectangle(newMatrixImage, bottomLeft, topRight, cvScalar(0, 0, 0), CV_FILLED);
            }
        }
    }

    for (int x = 0; x < blockCount + 2; x++)
    {
        // Untere Seite des Finder-Patterns.
        CvPoint bottomLeft;
        bottomLeft.x = x * blockSize + offset;
        bottomLeft.y = newMatrixImage->height - offset;

        CvPoint topRight;
        topRight.x = bottomLeft.x + blockSize - 1;
        topRight.y = bottomLeft.y - blockSize + 1;

        cvRectangle(newMatrixImage, bottomLeft, topRight, cvScalar(0, 0, 0), CV_FILLED);

        // Linke Seite des Finder-Patterns.
        bottomLeft.x = offset;
        bottomLeft.y = newMatrixImage->height - (x * blockSize) - offset;

        topRight.x = bottomLeft.x + blockSize - 1;
        topRight.y = bottomLeft.y - blockSize + 1;

        cvRectangle(newMatrixImage, bottomLeft, topRight, cvScalar(0, 0, 0), CV_FILLED);

        if (x % 2 == 0)
        {
            // Oberes alternating-Pattern.
            bottomLeft.x = x * blockSize + offset;
            bottomLeft.y = blockSize + offset;

            topRight.x = bottomLeft.x + blockSize - 1;
            topRight.y = bottomLeft.y - blockSize + 1;

            cvRectangle(newMatrixImage, bottomLeft, topRight, cvScalar(0, 0, 0), CV_FILLED);

            // Unteres alternating-Pattern.
            bottomLeft.x = newMatrixImage->width - blockSize - offset;
            bottomLeft.y = newMatrixImage->height - (x * blockSize) - offset;

            topRight.x = bottomLeft.x + blockSize - 1;
            topRight.y = bottomLeft.y - blockSize + 1;

            cvRectangle(newMatrixImage, bottomLeft, topRight, cvScalar(0, 0, 0), CV_FILLED);
        }
    }

    QStringList newMatrixImageData;
    this->saveAndSendImage(newMatrixImage, "NewMatrix.jpg", "Our new created matrix.",
                           newMatrixImageData);

    this->decodeDataMatrixBlock(newMatrixImage);

    emit imageProcessed();
}

/*
 *  Versucht das übergebene Bild an das Programm "dmtxread" zu übergeben,
 *  um die darin kodierten Daten zu erhalten.
 */
void DataMatrixReader::decodeDataMatrixBlock(IplImage* newMatrixImage)
{
    this->dmtxReadInputImage = new QTemporaryFile(QString(QDir::tempPath() + "/inputImage_XXXXXX.jpg"));
    this->dmtxReadOutputFile = new QTemporaryFile();

    if (this->dmtxReadInputImage->open() && this->dmtxReadOutputFile->open()) {
        this->saveImage(dmtxReadInputImage->fileName(), newMatrixImage);

        this->dmtxProcess = new QProcess(this);

        QString dmtxReadPath = QApplication::applicationDirPath() + "/dmtxread/dmtxread.exe";

        QStringList arguments;
        arguments.append(this->dmtxReadInputImage->fileName());

        this->dmtxProcess->setStandardOutputFile(this->dmtxReadOutputFile->fileName());

        this->dmtxProcess->start(dmtxReadPath, arguments);

        if (this->dmtxProcess->waitForFinished(3000))
        {
            QString decodedData;

            if (this->dmtxProcess->exitCode() == 0
                && this->dmtxProcess->exitStatus() == 0
                && this->dmtxReadOutputFile->isReadable())
            {
                this->dmtxReadOutputFile->seek(0);

                decodedData = QString(this->dmtxReadOutputFile->readAll());

                QByteArray   bytes          = decodedData.toAscii();
                const char * ptrDecodedData = bytes.data();

                int width = 650, height = 650;
                IplImage *decodedDataImage = cvCreateImage(cvSize(width, height), 8, 1);
                cvAddS(decodedDataImage, cvScalar(255, 255, 255), decodedDataImage);
                CvFont font;
                CvSize text_size;
                int ymin = 0;
                CvPoint ptR;

                cvInitFont(&font, CV_FONT_VECTOR0, 0.5, 0.5, 0.0, 2);
                cvGetTextSize(ptrDecodedData, &font, &text_size, &ymin);

                ptR.x = (width - text_size.width) / 2;
                ptR.y = (height + text_size.height) / 2;

                cvPutText(decodedDataImage, ptrDecodedData, ptR, &font, CV_RGB(0, 0, 0));

                QStringList decodedDataImageData;
                this->saveAndSendImage(decodedDataImage, "DecodedDataImage.jpg", "The decoded data!",
                                       decodedDataImageData);
            }

            this->dmtxProcess->closeReadChannel(QProcess::StandardOutput);
            this->dmtxProcess->closeWriteChannel();
            this->dmtxProcess->close();
            this->dmtxReadInputImage->close();
            this->dmtxReadOutputFile->close();
            this->dmtxReadInputImage->remove(this->dmtxReadInputImage->fileName());
            this->dmtxReadOutputFile->remove(this->dmtxReadOutputFile->fileName());
        }
    }
}

/*
 *  Dreht die übergebenen Punkte im Array um den center und um den angegebenen Winkel.
 */
void DataMatrixReader::rotateDataMatrixEdges(CvPoint* dataMatrixEdges, CvPoint center, double angle)
{
    /*-----------------------------------------------------------------------------
     * Auch die gefundenen Punkte der DataMatrix müssen um den entspechenden Winkel
     * gedreht werden.
     *-----------------------------------------------------------------------------*/
    for(int i = 0 ; i < 4 ; i++)
    {
        dataMatrixEdges[i] = rotatePoint( center, dataMatrixEdges[i], angle );
    }
}

/*
 *  Berechnet die durchschnittliche Blockbreite des DataMatrix Datenblocks.
 */
BlockData DataMatrixReader::calculateAvgBlockSize(IplImage* currentImage, CvPoint startPoint, CvPoint endPoint)
{
    double avgBlockSize = 0;
    int max_buffer;
    CvLineIterator iterator;

    int previousPixel = 0;
    int currentPixel = 0;
    int blackCount = 0;
    int whiteCount = 0;

    int lineStart = 0;
    int lineEnd = 0;

    int lineLength = 0;

    max_buffer = cvInitLineIterator(currentImage, startPoint, endPoint, &iterator, 8, 0);
    previousPixel = iterator.ptr[0];
    CV_NEXT_LINE_POINT(iterator);

    for(int i = 1 ; i < max_buffer; i++)
    {
        currentPixel = iterator.ptr[0];

        if (previousPixel == 255 && currentPixel == 0)
        {
            blackCount++;
        }
        else if (previousPixel == 0 && currentPixel == 255)
        {
            if (whiteCount == 0)
            {
                lineStart = i;
            }

            lineEnd = i;

            whiteCount++;
        }

        previousPixel = currentPixel;

        CV_NEXT_LINE_POINT(iterator);
    }

    whiteCount -= 1;

    lineLength = lineEnd - lineStart;

    avgBlockSize = (double)((double)lineLength / (whiteCount + blackCount));

    return BlockData(avgBlockSize, whiteCount + blackCount, lineStart);
}

/*
 *  Wendet auf das übergebene Bild einen binären Threshold an.
 */
IplImage* DataMatrixReader::processThresh(IplImage *currentImage, QString fileName, QString description)
{
    int threshold = 100;
    int maxValue = 255;
    int threshType = CV_THRESH_BINARY;

    // Der Thresh muss auf einem Graubild arbeiten. Wird ein Farbild übergeben,
    // muss es zuvor konvertiert werden.
    IplImage* currentWorkGrayImage;
    if (currentImage->nChannels == 3)
        currentWorkGrayImage = this->createGrayImage(currentImage);
    else
        currentWorkGrayImage = currentImage;

    IplImage* currentWorkThreshImage = cvCreateImage(cvGetSize(currentWorkGrayImage), IPL_DEPTH_8U, 1);
    cvThreshold(currentWorkGrayImage, currentWorkThreshImage, threshold, maxValue, threshType);

    QStringList threshImageData;
    threshImageData.append("Threshold value: " + QString::number(threshold));
    threshImageData.append("Threshold max value: " + QString::number(maxValue));
    threshImageData.append("Threshold type: CV_THRESH_BINARY");

    this->saveAndSendImage(currentWorkThreshImage, fileName, description, threshImageData);

    return currentWorkThreshImage;
}

/*
 *  Entfernt kleine Bildstörungen durch eine Dialatation, gefolgt von einer Erosion.
 */
IplImage* DataMatrixReader::smoothImage(IplImage* currentImage)
{
    IplImage* currentWorkImage = cvCreateImage(cvGetSize(currentImage),
                                               currentImage->depth, currentImage->nChannels);

    cvDilate(currentImage, currentWorkImage);
    cvErode(currentWorkImage, currentWorkImage);

    QStringList currentWorkImageData;
    this->saveAndSendImage(currentWorkImage, "SmoothedImage.jpg",
                           "Current smoothed (Median) work image.", currentWorkImageData);

    return currentWorkImage;
}

/*
 *  Hilfsmethode, um den Status zu verändern, ob die Bilder der Zwischenergebnisse
 *  im Dateisystem abgespeichert werden sollen.
 */
void DataMatrixReader::setSaveImagesStatus(bool saveImages)
{
    this->saveImageStatus = saveImages;
}

/*
 *  Speichert das übergebene Bild bei Bedarf ab und schickt ein Signal,
 *  dass ein weiteres Bild zum Anzeigen in der Oberfläche vorhanden ist.
 */
void DataMatrixReader::saveAndSendImage(IplImage* currentImage, QString imageName, QString description, QStringList additionalData)
{
    // Interessante Werte als zusätzliche Information anzeigen.
    // In umgekehrter Reihenfolge am Anfang einfügen, damit sie in der GUI in der korrekten
    // Reihenfolge angezeigt werden.
    additionalData.insert(0, "OpenCV color-model: " + QString(currentImage->colorModel));
    additionalData.insert(0, "Color-channels: " + QString::number(currentImage->nChannels));
    additionalData.insert(0, "Depth: " + QString::number(currentImage->depth));
    additionalData.insert(0, "Image-width/height: (" + QString::number(currentImage->width) + "x"
                          + QString::number(currentImage->height) + ")");

    // Wenn die Bilder zusätzlich noch im Dateisystem gespeichert werden sollen.
    if (this->saveImageStatus)
    {
        this->saveImageCount++;

        QString currentImagePath = this->runPath + "/" + QString::number(this->saveImageCount)
                                   + ". " + imageName;
        this->saveImage(currentImagePath, currentImage);
    }

    // Das openCV IplImage-Format in das Qt QImage-Format konvertieren.
    QImage &image = *IplImageConverter::IplImage2QImage(currentImage);
    QPixmap pixmap = QPixmap::fromImage(image);

    // Die "Außenwelt" über ein neues Bild benachrichtigen.
    emit stepProcessed(StepData(imageName, description, QIcon(pixmap), pixmap, additionalData));
}

/*
 *  Berechnet den Mittelpunkt von den im Array übergebenen Punkten.
 */
CvPoint DataMatrixReader::calculateCenterPoint(CvPoint* dataMatrixEdges)
{
    int x = 0;
    int y = 0;
    for (int i = 0; i < 4; i++)
    {
        x += dataMatrixEdges[i].x;
        y += dataMatrixEdges[i].y;
    }

    CvPoint center = this->setCvPoint(x / 4, y / 4);

    return center;
}

/*
 *  Wendet auf das übergebene Bild den Canny-Edge-Detector an.
 */
IplImage* DataMatrixReader::processCanny(IplImage *sourceGrayImage)
{
    int threshold1 = 100;
    int threshold2 = 150;
    int matrixSize = 3;

    IplImage *cannyResultImage = cvCreateImage(cvGetSize(sourceGrayImage), 8, 1);  // 1 Farbe
    cvCanny(sourceGrayImage, cannyResultImage, threshold1, threshold2, matrixSize);

    QStringList cannyResultImageData;
    cannyResultImageData.append("Threshold 1: " + QString::number(threshold1));
    cannyResultImageData.append("Threshold 1: " + QString::number(threshold2));
    cannyResultImageData.append("Matrix size (for sobel): " + QString::number(matrixSize));

    this->saveAndSendImage(cannyResultImage, "CannyResultImage.jpg", "Canny result image",
                           cannyResultImageData);

    return cannyResultImage;
}

/*
 *  Berechnet aus den übergebenen DataMatrix Eckpunkten eine Region of Interest
 *  und gibt ein neues Bild bestehend aus dieser ROI zurück.
 */
IplImage* DataMatrixReader::createROI(IplImage *currentWorkImage, CvPoint *dataMatrixEdges, int finderPos)
{
    int x_rect, y_rect, roi_width, roi_height;

    x_rect = dataMatrixEdges[(finderPos + 1) % 4].x;
    y_rect = dataMatrixEdges[(finderPos + 1) % 4].y;

    roi_width = dataMatrixEdges[finderPos].y - dataMatrixEdges[(finderPos + 1) % 4].y;
    roi_height = roi_width;

    CvRect roi_rect = cvRect(x_rect, y_rect, roi_width, roi_height);

    IplImage* currentROIImage = cvCreateImageHeader(cvSize(roi_width, roi_height),
                                                    currentWorkImage->depth,
                                                    currentWorkImage->nChannels);

    currentROIImage->origin = currentWorkImage->origin;
    currentROIImage->widthStep = currentWorkImage->widthStep;

    currentROIImage->imageData = currentWorkImage->imageData + roi_rect.y * currentWorkImage->widthStep +
                     roi_rect.x * currentWorkImage->nChannels;

    QStringList currentROIImageData;
    this->saveAndSendImage(currentROIImage, "CurrentROIImage.jpg", "Current work image with ROI",
                           currentROIImageData);

    return currentROIImage;
}

/*
 *  Entfernt die perspektivische Verzerrung des angegebenen Bildes.
 */
IplImage* DataMatrixReader::removePerspective(IplImage* currentWorkRotatedImage, CvPoint* edge, int finderPos)
{
    /*-----------------------------------------------------------------------------
     *  Jetz noch die Perspektive rausnehmen
     *-----------------------------------------------------------------------------*/
    IplImage* transformedImg = cvCloneImage(currentWorkRotatedImage);
    transformedImg->origin = currentWorkRotatedImage->origin;
    cvSet(transformedImg, cvScalarAll(0), 0);

    CvPoint2D32f srcQuad[4], dstQuad[4];
    CvMat* warp_matrix = cvCreateMat(3, 3, CV_32FC1);

    /*-----------------------------------------------------------------------------
     *  Zuweisung der verzerrten Ecken der Matrix
     *-----------------------------------------------------------------------------*/
    srcQuad[0].x = edge[(finderPos +1) % 4].x;
    srcQuad[0].y = edge[(finderPos +1) % 4].y;
    srcQuad[1].x = edge[(finderPos +2) % 4].x;
    srcQuad[1].y = edge[(finderPos +2) % 4].y;
    srcQuad[2].x = edge[finderPos].x;
    srcQuad[2].y = edge[finderPos].y;
    srcQuad[3].x = edge[(finderPos +3) % 4].x;
    srcQuad[3].y = edge[(finderPos +3) % 4].y;

    double kantenlaenge = edge[finderPos].y - edge[(finderPos + 1) % 4].y;

    /*-----------------------------------------------------------------------------
     *  Zuweisungspunkte der entzerrten Matrix. Der linke Schenkel des finders
     *  befindet sich bereits in der gewünschten Position und wird als Masstab
     *  genommen. Eine Vergrösserung oder Verkleinerung der Matrix wäre hier
     *  möglich.
     *-----------------------------------------------------------------------------*/
    dstQuad[0].x = edge[(finderPos +1) % 4].x;
    dstQuad[0].y = edge[(finderPos +1) % 4].y;
    dstQuad[1].x = edge[(finderPos +1) % 4].x + kantenlaenge;
    dstQuad[1].y = edge[(finderPos +1) % 4].y;
    dstQuad[2].x = edge[finderPos].x;
    dstQuad[2].y = edge[finderPos].y;
    dstQuad[3].x = edge[finderPos].x + kantenlaenge;
    dstQuad[3].y = edge[finderPos].y;

    cvGetPerspectiveTransform(srcQuad, dstQuad, warp_matrix);
    cvWarpPerspective(currentWorkRotatedImage, transformedImg, warp_matrix);

    QStringList transformedImgData;

    // Die Quellpunkte ausgeben:
    for (int i = 0; i < 4; i++)
    {
        transformedImgData.append("Source point transformation " + QString::number(i+1) + ": " +
                                  this->cvPointToString(srcQuad[i]));
    }

    // Die Zielpunkte ausgeben:
    for (int i = 0; i < 4; i++)
    {
        transformedImgData.append("Dest. point transformation " + QString::number(i+1) + ": " +
                                  this->cvPointToString(dstQuad[i]));
    }

    this->saveAndSendImage(transformedImg, "TransformedWorkImage.jpg",
                           "Current transformed work image", transformedImgData);

    return transformedImg;
}

/*
 *  Zeichnet die übergebenen Ecken der DataMatrix in der übergebene Bild ein.
 */
void DataMatrixReader::paintDataMatrixEdges(IplImage *currentWorkRotatedImage, CvPoint *edge, CvPoint finder)
{
    IplImage* currentRotatedImage = cvCreateImage( cvGetSize(currentWorkRotatedImage), 8, 3 );  // 3 Farben
    cvConvertImage(currentWorkRotatedImage, currentRotatedImage, CV_GRAY2BGR);

    for(int i = 0; i < 4; i++)
    {
        cvLine( currentRotatedImage, edge[i], edge[(i + 1) % 4], CV_RGB(255, 0, 0), 1, 8);
    }

    cvCircle(currentRotatedImage, edge[0], 8, CV_RGB(255, 0, 0), 2, 8, 0);
    cvCircle(currentRotatedImage, finder, 20, CV_RGB(0, 255, 0), 2, 8, 0);

    cvCircle(currentRotatedImage, edge[1], 8, CV_RGB(0, 255, 0), 2, 8, 0);
    cvCircle(currentRotatedImage, edge[2], 8, CV_RGB(0, 0, 255), 2, 8, 0);
    cvCircle(currentRotatedImage, edge[3], 8, CV_RGB(255, 188, 0), 2, 8, 0);

    QStringList currentRotatedImageData;
    for (int i = 0; i < 4; i++)
    {
        currentRotatedImageData.append("Data matrix edge " + QString::number(i+1) +
                                       ": " + this->cvPointToString(edge[i]));
    }

    this->saveAndSendImage(currentRotatedImage, "CurrentRotatedImageWithPoints.jpg",
                           "Current work image. Correct rotated and with edge points.",
                           currentRotatedImageData);
}

/*
 *  Berechnet den fehlenden vierten Punkt der DataMatrix.
 */
void DataMatrixReader::calculateMissingPoint(CvPoint* edge, int finderPos, CvPoint max)
{
    /*-----------------------------------------------------------------------------
     *  Berechnen des fehlenden Punktes aus edge[finderPos] und max
     *  pt_high = höher gelegener der beiden bekannten Punkte
     *  pt_low = der Andere. Siehe auch nächsten Kommentar.
     *-----------------------------------------------------------------------------*/

    CvPoint ursprung_regional, pt_low, pt_high;
    double m1, m2, diff_y1, diff_y2, diff_x, x_np, y_np;

    /*-----------------------------------------------------------------------------
     *  Die Punkte nach oben und unten zu sortieren scheint nicht so gut.
     *  Besser nach links und rechts. Meistens ist der Linke auch der höhere,
     *  aber nicht immer !
     *-----------------------------------------------------------------------------*/
    if( edge[(finderPos + 2) % 4].x < max.x) // Einen der beiden Punkte als oberen
    {
            pt_low  = max; // der rechte Punkt
            pt_high = edge[(finderPos +2 ) % 4];
    }
    else
    {
            pt_low  = edge[(finderPos + 2) % 4];
            pt_high = max;
    }
    int nenner;	// verhindert Division durch null

    /*-----------------------------------------------------------------------------
     *  m1: Steigung der Matrixkante von oben links nach oben rechts
     *-----------------------------------------------------------------------------*/
    nenner = pt_high.x - edge[(finderPos + 1) % 4].x;
    if (nenner == 0)
    {
        nenner++;
    }

    m1 = (double)(edge[(finderPos + 1) % 4].y - pt_high.y) / nenner;

    //cout << "Nenner : " << nenner << endl;
    /*-----------------------------------------------------------------------------
     *  m1: Steigung der Matrixkante von unten rechts nach oben rechts
     *-----------------------------------------------------------------------------*/
    nenner = pt_low.x - edge[(finderPos + 3) % 4].x;
    if (nenner == 0)
    {
        nenner++;
    }

    m2 = (double)(edge[(finderPos + 3) % 4].y -  pt_low.y) / nenner;

    /*-----------------------------------------------------------------------------
     *  Zu den zu untersuchenden Punkten wird ein Hilfkoordinatensystem kon-
     *  struiert. Die angegebene Differenzen beschreiben Dreiecke, die zur Be-
     *  stimmung des noch nötigen Punktes fehlen. Es kommen lediglich die
     *  Strahlensätze zum Einsatz.
     *  Es wird der Schnittpunkt folgender Geraden berechnet:
     *  	f1(x) = m1 * x + diff_y1
     *  	f2(x) = m2 * x - diff_y2
     *  diff_x dient als Hilfsvariable zur Berechnung von diff_y2
     *-----------------------------------------------------------------------------*/
    diff_y1 = pt_low.y - pt_high.y;
    diff_x  = pt_low.x - pt_high.x;
    diff_y2 = m2 * diff_x;
    ursprung_regional.x =  pt_low.x - diff_x;
    ursprung_regional.y = pt_high.y + diff_y1;

    /*-----------------------------------------------------------------------------
     *  Berechnen der Koordinaten zu Ursprung des Bildes und Zuweisung der Werte
     *  an die dem Finder sich befindende Position. Hiermit sind nun alle 4 Ecken
     *  der Matrix bestimmt.
     *-----------------------------------------------------------------------------*/
    x_np = (-diff_y2 - diff_y1) / ( m1 - m2 );
    y_np = m1 * x_np + diff_y1;

    // Den bisher falschen vierten Punkt der DataMatrix-Ecken mit dem Berechneten
    // Punkt überschreiben.
    edge[(finderPos + 2) % 4] = this->setCvPoint(ursprung_regional.x + x_np,
                                                 ursprung_regional.y - y_np);
}

/*
 *  Rotiert das angegebene Bild um den Mittelpunkt und um den angegebenen Winkel.
 */
IplImage* DataMatrixReader::rotateImage(IplImage* currentImage, CvPoint center, double angle)
{
    // Neue Referenz für das gedrehte Bild anlegen.
    // Die Angabe über Tiefe und Farbe kommt aus dem Quellbild.
    IplImage* currentWorkRotatedImage = cvCreateImage(cvGetSize(currentImage),
                                                      currentImage->depth, currentImage->nChannels);

    /*-----------------------------------------------------------------------------
     *  Berechnung der Rotationsmatrix und Durchführung der Rotation
     *-----------------------------------------------------------------------------*/
    CvMat* rot_mat = cvCreateMat(2, 3, CV_32FC1);
    cv2DRotationMatrix(cvPoint2D32f( center.x, center.y ), angle, 1.0, rot_mat );

    cvWarpAffine(currentImage, currentWorkRotatedImage, rot_mat);

    return currentWorkRotatedImage;
}

/*
 *  Rotiert das angegebene Bild um den Mittelpunkt und um den angegebenen Winkel.
 */
double DataMatrixReader::calculateRotationAngle(CvPoint *edge, int finderPos)
{
    /*-----------------------------------------------------------------------------
     *  Rotieren des Bilders mit dem Finder-Punkt.
     *-----------------------------------------------------------------------------*/
    double angle = 0.0;

    angle = -(atan((double)(edge[finderPos].x - edge[(finderPos + 1) % 4].x) /
                   ((double)edge[finderPos].y - edge[(finderPos + 1) % 4].y)) * 180 / M_PI);

    /*-----------------------------------------------------------------------------
     *  Fallunterscheidung. Zeigt der rechte Schenkel des Finders nach unten, so
     *  muß um weitere 180° gedreht werden.
     *-----------------------------------------------------------------------------*/
    if(			edge[(finderPos + 1) % 4].y > edge[finderPos].y  // von oben nach unten
                    && 	edge[(finderPos + 1) % 4].x > edge[finderPos].x)	// von links nach rechts
    {
            angle += 180;
    }

    if(			edge[(finderPos + 1) % 4].y > edge[finderPos].y	// von oben nach unten
                    && 	edge[(finderPos + 1) % 4].x < edge[finderPos].x)	// von rechts nach links
    {
            angle -= 180;
    }

    return angle;
}

/*
 *  Zeichnet diverse markante Punkte in das übergebene Bild ein.
 */
void DataMatrixReader::paintCurrentPositions(IplImage *currentWorkThreshImage, CvPoint max1, CvPoint max2, CvPoint center, CvPoint finder, CvPoint finderCorr)
{
    IplImage* currentWorkThreshColorImage = cvCreateImage( cvGetSize(currentWorkThreshImage), 8, 3 );  // 3 Farben
    cvConvertImage(currentWorkThreshImage, currentWorkThreshColorImage, CV_GRAY2BGR);

    cvLine(currentWorkThreshColorImage, finder, finderCorr, CV_RGB(0,255,0), 1, 8 );

    cvCircle(currentWorkThreshColorImage, finder, 20, CV_RGB(0, 255, 0), 2, 8, 0);

    cvCircle(currentWorkThreshColorImage, max1, 2, CV_RGB(0, 0, 255), 3, 8, 0);
    cvCircle(currentWorkThreshColorImage, max2, 2, CV_RGB(255, 0, 0), 2, 8, 0);

    cvCircle(currentWorkThreshColorImage, center, 10, CV_RGB(0, 255, 255), 2, 8, 0);

    QStringList currentWorkTreshData;
    currentWorkTreshData.append("Finder position: " + this->cvPointToString(finder));
    currentWorkTreshData.append("Max1 point: " + this->cvPointToString(max1));
    currentWorkTreshData.append("Max2 point: " + this->cvPointToString(max2));
    currentWorkTreshData.append("Current center point: " + this->cvPointToString(center));

    this->saveAndSendImage(currentWorkThreshColorImage, "CurrentWorkThreshColorImage.jpg",
                           "Current work image with threshold and positions",
                           currentWorkTreshData);
}

/*
 *  Ermittelt den Punkt mit dem größten Abstand zum Finder-Punkt.
 *  Das ist somit der Punkt gegenüber der Finder-Ecke.
 */
CvPoint DataMatrixReader::detectSecondMaxPoint(CvSeq *lines, CvPoint max2, CvPoint finder)
{
    /*-----------------------------------------------------------------------------
     *  Die Ecke gegenüber dem Finder.
     *
     *  Mit der gewählten Punktbestimmung ist zur Zeit noch nicht klar welcher der
     *  beiden in Frage kommenden Punkte gegen über dem finder gefunden wurde
     *-----------------------------------------------------------------------------*/

    CvPoint max = this->setCvPoint(0, 0);
    double maxAbstand = 0;

    /*-----------------------------------------------------------------------------
     *  Es werden alle Punkte, die die durch die Hugh- Transfomation gefundenen
     *  Linien beschreiben untersucht. Grundvoraussetzung ist ein Mindestabstand
     *  von 8 Pixeln zum bereits gefundenem Punkt und ein Maximalabstand von 50
     *  Pixeln um den Rechenaufwand zu reduzieren. Auch könnte durch Verzerrung
     *  bedingt ein anderer Eckpunkt noch weiter weg sein.
     *-----------------------------------------------------------------------------*/
    for(int i = 0; i < lines->total; i++ )
    {
            CvPoint* line = (CvPoint*)cvGetSeqElem(lines,i);
            double abstand = 0;
            double diff_x1, diff_y1;
            double x_pt, y_pt;
            double edge_diff_x, edge_diff_y;

            for (int y = 0; y < 2; y++)
            {
                    abstand = 0;

                    x_pt = *(&line[y].x);		// aktuell zu untersuchender Punkt
                    y_pt = *(&line[y].y);

                    edge_diff_x = x_pt - max2.x;
                    edge_diff_y = y_pt - max2.y;
                    double zwsp = sqrt(edge_diff_x*edge_diff_x + edge_diff_y*edge_diff_y);

                    /*-----------------------------------------------------------------------------
                     *  zwsp ist der Abstand, den ein weiter Punkt mindestens von dem bereits
                     *  bekannten entfernt sein muss, um als Fehlender in Betracht zu kommen.
                     *  Der Wert von 6 Pixeln wurde durch Experimente und Fehlersuche bestimmt.
                     *  Je nach Auflösung der Fotos ist eventuell eine Anpassung nötig.
                     *-----------------------------------------------------------------------------*/
                    //if (zwsp < 15) cout << " Kleiner Abstand = " << zwsp << endl;
                    if(zwsp > 8 && zwsp < 50) // je nach Verzerrung kann andere Ecke weiter weg sein !
                    {
                            diff_x1 = x_pt - finder.x;
                            diff_y1 = y_pt - finder.y;

                            abstand = sqrt(diff_x1*diff_x1 + diff_y1*diff_y1);
                            //cout << "abstand= " << abstand << endl;

                            if (abstand > maxAbstand)
                            {
                                    maxAbstand = abstand;
                                    max = line[y];
                            }
                    }
            }

    }

    return max;
}

/*
 *  Ermittelt die Position des L-förmigen Finders.
 */
int DataMatrixReader::detectFinderPosition(CvPoint* edge, IplImage* currentWorkThreshImage)
{
    IplImage* finderPosThreshImage = cvCreateImage(cvGetSize(currentWorkThreshImage), IPL_DEPTH_8U, 1 );
    finderPosThreshImage = cvCloneImage(currentWorkThreshImage);

    int density[4];
    int px_versatz = 5;
    int max_buffer;
    int finderPos = 0;
    CvLineIterator iterator;

    for(int i = 0; i < 4; i++)
    {
        density[i]=0;
    }

    edge[0].y += px_versatz;
    edge[1].x -= px_versatz;
    edge[2].y -= px_versatz;
    edge[3].x += px_versatz;

    /*-----------------------------------------------------------------------------
     *  Die Linien zwischen den gefunden Punkten werden auf schwarz oder weiss
     *  getestet. Ein sehr kleiner Wert bezeichnet eine schwarze Kante (Kante des
     *  Finders). Ein grösserer Wert bedeutet,  das auf der Linie auch weisse
     *  Stellen sind.
     *-----------------------------------------------------------------------------*/
    for(int i = 0 ; i<4 ; i++)
    {
        max_buffer = cvInitLineIterator(finderPosThreshImage, edge[i], edge[(i+1)%4], &iterator, 8, 0);
        for( int j =0 ; j<max_buffer ; j++ )
        {
                density[i] += iterator.ptr[0];
                iterator.ptr[0] = 255;
                CV_NEXT_LINE_POINT(iterator);
        }

        density[i] /= max_buffer;
    }

    for(int i = 0; i < 4; i++)
    {
        /*-----------------------------------------------------------------------------
         *  Folgen zwei Finder- Linien aufeinander, so ist der gesuchte Punkt gefunden
         *-----------------------------------------------------------------------------*/
        if ((density[i] / 10 == 0) && density[(i+1)%4] / 10 == 0)
        {
                finderPos = ((i+1) % 4);
        }
    }

    /*-----------------------------------------------------------------------------
     *  Der Versatz wurde benutzt um die Linien zum Mittelpunkt zu verschieben. Er
     *  wird nun wieder egalisiert.
     *-----------------------------------------------------------------------------*/
    edge[0].y -= px_versatz;
    edge[1].x += px_versatz;
    edge[2].y += px_versatz;
    edge[3].x -= px_versatz;

    QStringList finderPosData;
    finderPosData.append("Finder position: " + this->cvPointToString(edge[finderPos]));

    this->saveAndSendImage(finderPosThreshImage, "FinderPosThreshImage.jpg",
                           "Current image with threshold and the searched lines.",
                           finderPosData);

    return finderPos;
}

/*
 *  Zeichnet die durch den Hough-Algorithmus gefundenen Linien in das übergebene Bild ein.
 */
void DataMatrixReader::paintHoughLines(IplImage* sourceGrayImage, HoughResultData *houghResultData)
{
    IplImage* houghColorImageShowResult = cvCreateImage( cvGetSize(sourceGrayImage), 8, 3 );  // 3 Farben

    cvCvtColor( sourceGrayImage, houghColorImageShowResult, CV_GRAY2BGR );

    for(int i = 0; i < houghResultData->getHoughLines()->total; i++)
    {
        CvPoint* line = (CvPoint*)cvGetSeqElem(houghResultData->getHoughLines(), i);

        /*-----------------------------------------------------------------------------
         *  Zeichnen der Linie nur zum veranschaulichen
         *-----------------------------------------------------------------------------*/
        cvLine(houghColorImageShowResult, line[0], line[1], CV_RGB(255, 0, 0), 1, 8);
    }

    // Die Eckpunkte sind interessante Informationen:
    QStringList houghColorImageData;
    for (int i = 0; i < 4; i++)
    {
        houghColorImageData.append("Data matrix edge " + QString::number(i+1) + ": " +
                                   this->cvPointToString(houghResultData->getDataMatrixEdges()[i]));
    }

    houghColorImageData.append("Current center point: " + this->cvPointToString(houghResultData->getCenter()));
    houghColorImageData.append("Rotation needed: " + QString(houghResultData->getHasToBeRotated() ? "true" : "false"));

    this->saveAndSendImage(houghColorImageShowResult, "HoughColorImageShowResult.jpg", "Hough result image",
                           houghColorImageData);
}

/*
 *  Wandelt den übergebenen Punkt vom Typ CvPoint in ein Ausgabe freundliches Format um.
 */
QString DataMatrixReader::cvPointToString(CvPoint point)
{
    return "X=" + QString::number(point.x) + ";Y=" + QString::number(point.y);
}

/*
 *  Wandelt den übergebenen Punkt vom Typ CvPoint2D32f in ein Ausgabe freundliches Format um.
 */
QString DataMatrixReader::cvPointToString(CvPoint2D32f point)
{
    return "X=" + QString::number(point.x) + ";Y=" + QString::number(point.y);
}

/*
 *  Verarbeitet die vom Hough-Algorithmus gefundenen Linien und versucht die Ecken (Maxima) zu finden.
 */
HoughResultData* DataMatrixReader::processHoughResult(CvSeq *lines, int width, int height)
{
    int x_max = 0;
    int y_max = 0;
    int x_min = width;
    int y_min = height;
    int counterVertical = 0;
    int counterHorizontal = 0;
    double sumVertical = 0;
    double sumHorizontal = 0;
    CvPoint* edge = new CvPoint[4];
    CvPoint pt1, pt2;

    /*-----------------------------------------------------------------------------
     *  Die gefundenen Linien werden untersucht. Dabei werden die Endpunkte, die am
     *  weitesten oben, unten, links und recht sind herausgesucht.
     *-----------------------------------------------------------------------------*/
    for( int i = 0; i < lines->total; i++ )
    {
        CvPoint* line = (CvPoint*)cvGetSeqElem(lines,i);

        int x_p0 = *(&line[0].x);
        int y_p0 = *(&line[0].y);
        int x_p1 = *(&line[1].x);
        int y_p1 = *(&line[1].y);

        /*-----------------------------------------------------------------------------
         *  Um waagerecht liegende Matrixen erkennen zu können wird der durch-
         *  schnittliche Winkel, in dem die Linien zur waagerechten und senkrechten
         *  Achse liegen berechnet.
         *-----------------------------------------------------------------------------*/
        double y = abs((double)line[0].y - line[1].y);
        double x = abs((double)line[0].x - line[1].x);
        double alpha = atan(y / x) * 180/M_PI;

        if (alpha > 45)
        {
            sumVertical += alpha;
            counterVertical++;
        }
        else
        {
            sumHorizontal += alpha;
            counterHorizontal++;
        }

        /*-----------------------------------------------------------------------------
         *  Berechnen der Eckpunkte der Matrix. Liegen Kanten der Matrix senkrecht oder
         *  waagerecht zum Koordinatenkreuz, so ist eine eindeutige Bestimmung im ersten
         *  Durchlauf noch nicht möglich. Die Matrix wird in diesem Fall um 60° gedreht.
         *-----------------------------------------------------------------------------*/
        x_max = max(x_max, max(x_p0, x_p1));
        if( x_max == x_p0)
            edge[1] = setCvPoint( x_p0, y_p0);
        if( x_max == x_p1)
            edge[1] = setCvPoint( x_p1, y_p1);

        x_min = min(x_min, min(x_p0, x_p1));
        if( x_min == x_p0)
            edge[3] = setCvPoint( x_p0, y_p0);
        if( x_min == x_p1)
            edge[3] = setCvPoint( x_p1, y_p1);

        y_max = max(y_max, max(y_p0, y_p1));
        if( y_max == y_p0)
            edge[2] = setCvPoint( x_p0, y_p0);
        if( y_max == y_p1)
            edge[2] = setCvPoint( x_p1, y_p1);

        y_min = min(y_min, min(y_p0, y_p1));
        if( y_min == y_p0)
            edge[0] = setCvPoint( x_p0, y_p0);
        if( y_min == y_p1)
            edge[0] = setCvPoint( x_p1, y_p1);
    }

    bool hasToBeRotated = false;
    if ((sumVertical / counterVertical) > 80 || (sumHorizontal / counterHorizontal) < 10)
    {
        hasToBeRotated = true;
    }

    pt1.x = x_min;
    pt1.y = y_min;
    pt2.x = x_max;
    pt2.y = y_max;
    double centerY = (pt2.y - pt1.y) / 2 + pt1.y;
    double centerX = (pt2.x - pt1.x) / 2 + pt1.x;

    CvPoint center = this->setCvPoint(centerX, centerY);
    CvPoint2D32f center2D32f = cvPoint2D32f(centerX, centerY);

    HoughResultData *houghResultData = new HoughResultData(edge, hasToBeRotated, center, center2D32f);

    return houghResultData;
}

/*
 *  Erstellt einen CvPoint, setzt die x- und y-Komponente und gibt den neuen Punkt zurück.
 */
CvPoint DataMatrixReader::setCvPoint(int x, int y)
{
    CvPoint zwsp;
    zwsp.x = x;
    zwsp.y = y;
    return zwsp;
}

/*
 *  Rotiert den übergebenen Punkt um den Mittelpunkt und um den angegebenen Winkel.
 */
CvPoint DataMatrixReader::rotatePoint(CvPoint center, CvPoint toTurn, double alpha)
{
    CvPoint buff;
    alpha = -alpha * M_PI / 180;
    double x1 = toTurn.x - center.x;
    double x2 = toTurn.y - center.y;
    buff.x = ( cos( alpha ) * x1 - sin( alpha ) * x2) + center.x;
    buff.y = ( sin( alpha ) * x1 + cos( alpha ) * x2) + center.y;
    return buff;
}

/*
 *  Speichert das übergebene Bild im Dateisystem.
 */
void DataMatrixReader::saveImage(QString imagePath, IplImage* image)
{
    QByteArray   bytes  = imagePath.toAscii();
    const char * ptr    = bytes.data();

    cvSaveImage(ptr, image);
}
