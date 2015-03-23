#include "houghresultdata.h"

HoughResultData::HoughResultData()
{
}

HoughResultData::HoughResultData(CvPoint *edges,
                                 bool rotated, CvPoint center, CvPoint2D32f center2D32f)
{
    this->dataMatrixEdges = edges;
    this->hasToBeRotated = rotated;
    this->center = center;
    this->center2D32f = center2D32f;
}

bool HoughResultData::getHasToBeRotated()
{
    return this->hasToBeRotated;
}

CvPoint HoughResultData::getCenter()
{
    return this->center;
}

CvPoint2D32f HoughResultData::getCenter2D32f()
{
    return this->center2D32f;
}

CvPoint* HoughResultData::getDataMatrixEdges()
{
    return this->dataMatrixEdges;
}

void HoughResultData::setHoughLines(CvSeq* lines)
{
    this->houghLines = lines;
}

CvSeq* HoughResultData::getHoughLines()
{
    return this->houghLines;
}
