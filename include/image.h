
#ifndef IMAGE_H
#define IMAGE_H

#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#ifdef ENABLE_LOGGER
#include "logger.h"
#endif

namespace fc
{
    using cv::Mat;

    extern tesseract::TessBaseAPI* tessEngine;

    namespace img_error
    {
        using scode = const unsigned int;
        scode IP_OK = 0;
        scode IP_OCR_ERROR = 1;

        const char* const err[48] = {
            "Image Processing Engine Start Successfully."  // 0
            "Trsseract OCR Engine Start Failed."           // 1
        };
    }  // namespace img_error

    class Image
    {
        const char* filename;
        Mat imageMat;

    public:
        Image(const char*);
        Image(const char*, size_t);
        Image();
        int OpenImageFile(const char*);
        int WriteToFile(const char* = nullptr);

        //int getBarCode(char* destBuffer, int bufferSize)
        //destBuffer must been allocated with bufferSize length including terminating null
        //ret val:  1  -> success
        //          0  -> no code recognized
        //         -1  -> zbar internal error
        //         -2  -> buffer too small.
        int getBarCode(char*, size_t);
    };

    int ImageProcessingStartup();
    int ImageProcessingDestroy();
};  // namespace fc


#endif
