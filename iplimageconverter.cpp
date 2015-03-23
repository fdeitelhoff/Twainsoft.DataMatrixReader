#include "iplimageconverter.h"

#include <QImage>

IplImageConverter::IplImageConverter() { }

/*
 *  Konvertiert ein openCV IplImage in ein Qt QImage.
 */
QImage* IplImageConverter::IplImage2QImage(IplImage *iplImg)
{
    int h = iplImg->height;
    int w = iplImg->width;
    int channels = iplImg->nChannels;
    char *data = iplImg->imageData;

    QImage *qimg = new QImage(w, h, QImage::Format_ARGB32);

    for (int y = 0; y < h; y++, data += iplImg->widthStep)
    {
        for (int x = 0; x < w; x++)
        {
            char r = 0, g = 0, b = 0, a = 0;

            if (channels == 1)
            {
                r = data[x * channels];
                g = data[x * channels];
                b = data[x * channels];
            }
            else if (channels == 3 || channels == 4)
            {
                r = data[x * channels + 2];
                g = data[x * channels + 1];
                b = data[x * channels];
            }

            if (channels == 4)
            {
                a = data[x * channels + 3];
                qimg->setPixel(x, y, qRgba(r, g, b, a));
            }
            else
            {
                qimg->setPixel(x, y, qRgb(r, g, b));
            }
        }
    }

    return qimg;
}
