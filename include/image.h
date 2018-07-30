
#ifndef IMAGE_H
#define IMAGE_H

#include <json/json.h>
#include <cstdint>
#include <map>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <queue>
#include <string>

#include "logger.h"
#include "taobao.h"
#include "threads.h"

void* OCRThread(void*);

namespace fc
{
    using cv::Mat;
    using std::string;

    class BaseImage;
    class Image;
    class WaterMarker;
    struct OcrResult;
    class OcrHandler;

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
        using scode = const int;

        scode IP_OK = 0;
        scode ERR_IO_NOT_EXIST = 1;

        const char* const err[48] = {
            "Image Processing Engine Start Successfully.",  // 0
            "Image Object does not exist in json file or doesnot appear to be an Object."
            "Please Read reference of configuration file.",  //1
        };
    }  // namespace img_error

    enum { ANCHOR_TOP = 1, ANCHOR_BOTTOM = 2, ANCHOR_LEFT = 4, ANCHOR_RIGHT = 8 };

    int ImageProcessingStartup(const Json::Value&);
    int ImageProcessingStartup(const string&, const string&, const string&);

    using tb::thread_ns::condition_variable;
    using tb::thread_ns::mutex;
    using tb::thread_ns::thread;

    class BaseImage;
    class Image;
    class WaterMarker;

    class BaseImage
    {
    protected:
        const string filename;
        Mat imageMat;
        tb::thread_ns::rwlock _l;

    public:
        void read()
        {
            _l.read();
        }
        void unlock()
        {
            _l.unlock();
        }
        BaseImage(const char* filename, unsigned int mask = cv::IMREAD_COLOR);
        bool valid() const;
        virtual ~BaseImage() {}
        const Mat& getMat() const
        {
            return imageMat;
        }
        void resize(double);
        void rotateScale(const cv::Point&, double, double);
    };

    class Image : public BaseImage
    {
        friend int ImageProcessingStartup(const Json::Value&);

        bool success;
        bool addWaterMarker(const WaterMarker&);

    public:
        Image(const char*);
        Image(const char*, size_t);
        Image();
        int OpenImageFile(const char*);
        int WriteToFile(const char* = nullptr);

        int getItemAccurateCode(string&, string&, int& price, int&, OcrResult&);
        int getItemCode(string&, string&, int& price, int&, OcrResult&, int* = nullptr);
        int AddWaterPrint();
    };

    class WaterMarker
    {
        friend int ImageProcessingStartup(const Json::Value&);
        friend int ImageProcessingDestroy();
        friend class Image;

        static std::vector<WaterMarker*> markers;

        mutable tb::thread_ns::rwlock _lock;

        string id;
        string waterMarkerPath;
        string position;
        double rotation;
        double transparent;
        double resize;
        int repeat;
        unsigned int anchor;
        int xOffset;
        int yOffset;

        BaseImage* waterMarker;
        Mat waterMarkerMask;
        bool CheckWaterMarker(char*, size_t);

        void buildMask();

    public:
        void read() const
        {
            _lock.read();
        }
        void unlock() const
        {
            _lock.unlock();
        }
        ~WaterMarker();
        WaterMarker();
        static const std::vector<WaterMarker*>& getMarkers();
        static WaterMarker* BuildWaterMarker(const Json::Value&, char*, size_t);
    };

    struct OcrResult {
    private:
        bool __find(string&, std::function<bool(const string&)>) const;

    public:
        uint64_t id;
        string errMessage;
        int errCode;
        string json;
        std::vector<string> words;

        const string& getJson()
        {
            return json;
        }
        void clear()
        {
            id = 0;
            errCode = 0;
            json = errMessage = "";
            words.clear();
        }
        const char* getError(int& e) const
        {
            e = errCode;
            return errMessage.c_str();
        };
        char* dumpJson() const;
        bool getFullCode(string&) const;
        bool getBarCode(string&) const;
        bool getPrice(int&) const;
        OcrResult()
        {
            id = 0;
            errMessage = "";
            errCode = 0;
            json = "";
        }
    };

    class OcrHandler
    {
        tb::thread_ns::condition_variable queueCond;
        tb::thread_ns::mutex queueMutex;

        std::queue<const char*> pathQueue;

        const string app_id;
        const string api_key;
        const string secret_key;

    public:
    };

    const std::map<int, const char*> ocrErrorTable = {
        {0, "Success"},
        {1, "curl error"},
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
        const string&, std::vector<string>&, uint64_t&, string&, int&, string&, int&, bool = false);

    int ProcessingOCR(const string&, OcrResult&, int&, bool = false);

    int ImageProcessingDestroy();
};  // namespace fc


#endif
