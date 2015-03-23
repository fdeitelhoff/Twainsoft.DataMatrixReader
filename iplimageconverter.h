#ifndef IPLIMAGECONVERTER_H
#define IPLIMAGECONVERTER_H

#include <QImage>
#include <cv.h>

class IplImageConverter
{

public:
    IplImageConverter();

    static QImage* IplImage2QImage(IplImage *iplImg);
};

#endif // IPLIMAGECONVERTER_H
