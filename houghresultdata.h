#ifndef HOUGHRESULTDATA_H
#define HOUGHRESULTDATA_H

#include <cv.h>

class HoughResultData
{
public:
    HoughResultData();

    HoughResultData(CvPoint *dataMatrixEdges, bool hasTobeRotated,
                    CvPoint center, CvPoint2D32f center2D32f);

    bool getHasToBeRotated();

    CvPoint getCenter();

    CvPoint2D32f getCenter2D32f();

    CvPoint* getDataMatrixEdges();

    CvSeq* getHoughLines();

    void setHoughLines(CvSeq* lines);

private:
    CvPoint* dataMatrixEdges;

    CvSeq* houghLines;

    bool hasToBeRotated;

    CvPoint center;

    CvPoint2D32f center2D32f;
};

#endif // HOUGHRESULTDATA_H
