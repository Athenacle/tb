
#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>
#include <map>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <queue>
#include <string>
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
        OCR_OK = 2,
        OCR_GET_FULL_CODE = 4,
        OCR_GET_BAR_CODE = 8,
        FULL_BAR_CODE_VALID = 16,
        ALL_CODE_VALIDATE_OK = 32,
        ZBAR_OCR_BAR_CODE_EQUAL = 64,
        OCR_GET_PRICE_OK = 128
    };

    namespace img_error
    {
        using scode = const unsigned int;

        scode IP_OK = 0;

        const char* const err[48] = {
            "Image Processing Engine Start Successfully."  // 0
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

        int getItemCode(std::string&, std::string&, int& price);
    };

    using std::string;

    struct OcrResult {
    private:
        bool __find(std::string&, std::function<bool(const std::string&)>) const;

    public:
        uint64_t id;
        std::string errMessage;
        int errCode;
        std::string json;
        std::vector<std::string> words;

        char* dumpJson() const;
        bool getFullCode(std::string&) const;
        bool getBarCode(std::string&) const;
        bool getPrice(int&) const;
    };

    class OcrHandler
    {
        tb::thread_ns::condition_variable queueCond;
        tb::thread_ns::mutex queueMutex;

        std::queue<const char*> pathQueue;

        const std::string app_id;
        const std::string api_key;
        const std::string secret_key;

    public:
    };

    const std::map<int, const char*> ocrErrorTable = {
        {4, "Open api request limit reached"},
        {14, "IAM Certification failed"},
        {17, "Open api daily request limit reached"},
        {18, "Open api qps request limit reached"},
        {19, "Open api total request limit reached"},
        {100, "Invalid parameter"},
        {110, "Access token invalid or no longer valid"},
        {111, "Access token expired"},
        {282000, "internal error"},
        {216100, "invalid param"},
        {216101, "not enough param"},
        {216102, "service not support"},
        {216103, "param too long"},
        {216110, "appid not exist"},
        {216200, "empty image"},
        {216201, "image format error"},
        {216202, "image size error"},
        {216630, "recognize error"},
        {216631, "recognize bank card error"},
        {216633, "recognize idcard error"},
        {216634, "detect error"},
        {282003, "missing parameters:"},
        {282005, "batch  processing error"},
        {282006, "batch task  limit reached"},
        {282110, "urls not exit"},
        {282111, "url format illegal"},
        {282112, "url download timeout"},
        {282113, "url response invalid"},
        {282114, "url size error"},
        {282808, "request id:"},
        {282809, "result type error"},
        {282810, "image recognize error"},
    };

    int ProcessingOCR(
        const char*, std::vector<std::string>&, uint64_t&, std::string&, int&, std::string&);

    int ProcessingOCR(const char*, OcrResult&);
    int ImageProcessingStartup(const string&, const string&, const string&);
    int ImageProcessingDestroy();
};  // namespace fc


#endif
