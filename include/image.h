
#ifndef IMAGE_H
#define IMAGE_H

#include <map>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

//#include "ocr.h"

#include "taobao.h"
#ifdef ENABLE_LOGGER
#include "logger.h"
#endif

void* OCRThread(void*);

namespace fc
{
    using cv::Mat;

    enum {
          ZBAR_OK = 1,
          TESSERACT_OK = 2,
          TESSERACT_FULL_BARCODE_VALID  = 4,
          TESSERACT_ZBAR_BARCODE_EQ = 8,
          ALL_CODE_VALIDATE_OK = 16
    };

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

    const size_t FULL_CODE_BUFFER_LENGTH = 16;
    const size_t BAR_CODE_BUFFER_LENGTH = 16;

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
        int getItemCode(std::string&, std::string&);
    };

    // using std::string;
    // using BaiduOCR =  aip::Ocr;
    // class OCR
    // {

    //     const std::string app_id;
    //     const std::string api_key;
    //     const std::string secret_key;

    //     BaiduOCR client;
    // public:
    //     OCR(string _id, string _key, string _skey)
    //         :app_id(_id), api_key(_key), secret_key(_skey), client(_id, _key, _skey)
    //     {
    //     }
    // };
    // int ImageProcessingStartup(std::map<std::string, std::string>&);
    int ImageProcessingStartup();
    int ImageProcessingDestroy();
};  // namespace fc


#endif
