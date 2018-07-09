
#include "image.h"
#include "id.h"

#include <zbar.h>
#include <iostream>
#include <vector>

#define ENABLE_SHOW

#ifdef ENABLE_SHOW
#define SHOW(m)                                 \
    do {                                        \
        cv::namedWindow(#m, cv::WINDOW_NORMAL); \
        cv::imshow(#m, m);                      \
        cv::waitKey(0);                         \
    } while (0);
#else
#define SHOW(m)
#endif

namespace
{
    int opencvFindBarCodeROI(const cv::Mat mat,
                             cv::Rect& roi,
                             unsigned dilateTimes = 4,
                             cv::Size elmentSize = cv::Size(7, 7))
    {
        using namespace cv;
        using std::vector;

        int height = mat.rows;
        int cuttedHeight = height * 0.4;
        Mat imageGussian;
        Mat imageSobelX, imageSobelY, imageSobelOut;
        Mat raw;
        mat.copyTo(raw);
        Mat img = raw(cv::Range(cuttedHeight, height), cv::Range::all());
        cvtColor(img, img, CV_RGB2GRAY);
        GaussianBlur(img, imageGussian, cv::Size(3, 3), 0);
        Mat imageX16S, imageY16S;
        Sobel(imageGussian, imageX16S, CV_16S, 1, 0, 3, 1, 0, 4);
        Sobel(imageGussian, imageY16S, CV_16S, 0, 1, 3, 1, 0, 4);
        convertScaleAbs(imageX16S, imageSobelX, 1, 0);
        convertScaleAbs(imageY16S, imageSobelY, 1, 0);
        imageSobelOut = imageSobelX - imageSobelY;
        blur(imageSobelOut, imageSobelOut, cv::Size(3, 3));
        Mat imageSobleOutThreshold;
        threshold(imageSobelOut, imageSobleOutThreshold, 170, 255, CV_THRESH_BINARY);
        Mat element = getStructuringElement(0, elmentSize);
        morphologyEx(imageSobleOutThreshold, imageSobleOutThreshold, cv::MORPH_CLOSE, element);
        erode(imageSobleOutThreshold, imageSobleOutThreshold, element);
        for (unsigned i = 0; i < dilateTimes; i++) {
            dilate(imageSobleOutThreshold, imageSobleOutThreshold, element);
        }
        vector<vector<cv::Point>> contours;
        vector<cv::Vec4i> hiera;
        findContours(
            imageSobleOutThreshold, contours, hiera, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
        Rect&& rect = cv::Rect(0, 0, 0, 0);
        for (auto& r : contours) {
            auto t = cv::boundingRect((Mat)r);
            if (rect.area() < t.area()) {
                rect = t;
            }
        }
        if (rect.height > rect.width) {
            return -1;
        } else {
            roi = Rect(rect.x, rect.y + cuttedHeight, rect.width, rect.height);
            return 0;
        }
    }

    // identify bar code via zbar
    int zbarCodeIdentify(const cv::Mat& raw, const cv::Rect& roi, std::string& bcode)
    {
        using namespace cv;
        using namespace zbar;

        ImageScanner scanner;
        Mat gray;
        Mat mat = raw(roi);
        Mat barImg = mat(Range(0.1 * mat.rows, mat.rows), Range::all());
        resize(barImg, barImg, Size(barImg.cols, 3 * barImg.rows));
        cvtColor(barImg, gray, COLOR_RGB2GRAY);
        scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
        zbar::Image zbimg(gray.cols, gray.rows, "Y800", gray.data, gray.rows * gray.cols);
        int n = scanner.scan(zbimg);
        if (n > 0) {
            auto results = scanner.get_results();
            assert(results.get_size() == 1);
            auto begin = results.symbol_begin();
            bcode = begin->get_data();
            return 1;
        } else if (n < 0) {
            return -1;
        } else {
            return 0;
        }
    }


    int tesseractCodeIdentify(cv::Mat& raw,
                              const cv::Rect& roi,
                              std::string& fcode,
                              std::string& bcode)
    {
        using namespace cv;
        using namespace tesseract;
        using namespace only;

        if (roi.x < 100 || roi.y < 50) {
            return 0;
        }


        Rect large(roi.x - 80, roi.y - 40, roi.width + 110, roi.height + 80);
        rectangle(raw, large, Scalar(255, 0), 2);
        Rect small;
        opencvFindBarCodeROI(raw, small, 3, Size(6, 5));
        rectangle(raw, small, Scalar(255, 255, 255), -1);
        Mat ocrRegion;
        resize(raw(large), ocrRegion, Size(0,0),2,2);
        cvtColor(ocrRegion, ocrRegion, COLOR_RGB2GRAY);
        SHOW(ocrRegion)
        Mat resultMat;
        threshold(ocrRegion, resultMat, 180, 255, CV_THRESH_BINARY);
        SHOW(resultMat);

        TessBaseAPI* tess = fc::tessEngine;
        tess->Clear();
        tess->SetImage(reinterpret_cast<unsigned char*>(resultMat.data),
                       resultMat.size().width,
                       resultMat.size().height,
                       resultMat.channels(),
                       resultMat.step1());
        tess->GetUTF8Text();
        const char* out = tess->GetUTF8Text();
        int ret = only::checkOcrOutput(out, fcode, bcode) ? 1 : 0;
        delete[] out;
        return ret;
    }

}  // namespace

#define DRAW                                                                                     \
    do {                                                                                         \
        std::string text = (*code == 0 ? "unknown" : code);                                      \
        int font_face = cv::FONT_HERSHEY_COMPLEX;                                                \
        double font_scale = 2;                                                                   \
        int thickness = 2;                                                                       \
        int baseline;                                                                            \
        cv::Size text_size = cv::getTextSize(text, font_face, font_scale, thickness, &baseline); \
        cv::Point origin;                                                                        \
        origin.x = imageMat.cols / 2 - text_size.width / 2;                                      \
        origin.y = imageMat.rows / 2 + text_size.height / 2;                                     \
        cv::putText(imageMat,                                                                    \
                    text,                                                                        \
                    origin,                                                                      \
                    font_face,                                                                   \
                    font_scale,                                                                  \
                    cv::Scalar(0, 0, 255),                                                       \
                    thickness,                                                                   \
                    8,                                                                           \
                    0);                                                                          \
    } while (0);

namespace fc
{
    tesseract::TessBaseAPI* tessEngine = nullptr;

    Image::Image(const char* _fname) : filename(_fname)
    {
        imageMat = cv::imread(filename, cv::IMREAD_COLOR);
    }

    int Image::getItemCode(std::string& fcode, std::string& bcode)
    {
        cv::Rect rect;
        int ret = opencvFindBarCodeROI(imageMat, rect);
        if (ret == -1) {
            return 0;
        }
        Mat tmp = imageMat(rect);
        int zstatus = -1;
        int tstatus = -1;

        ret = 0;
        std::string teFCode, teBCode;
        teFCode = teBCode = "";
        zstatus = zbarCodeIdentify(imageMat, rect, bcode);
        tstatus = tesseractCodeIdentify(imageMat, rect, teFCode, teBCode);
        if (zstatus == 1
            && only::checkBarCodeValidate(bcode)) {  // zbar success. Bar Code MUST be TRUE.
            ret = ret | ZBAR_OK;                     // 1
        }
        if (tstatus == 1) {            //teseract success
            ret = ret | TESSERACT_OK;  // 2
            fcode = teFCode;
            if (bcode.length() == 0) {
                bcode = teBCode;
            }
            if (only::checkFullBarCode(teFCode, teBCode)) {
                ret = ret | TESSERACT_FULL_BARCODE_VALID;  // 4
            } else {
                std::string result;
                if (only::restoreFullCode(teFCode, teBCode, result)) {
                    fcode = result;
                    ret = ret | TESSERACT_FULL_BARCODE_VALID;
                }
            }
        }
        if (zstatus == 1 && tstatus == 1) {
            if (bcode == teBCode) {
                ret = ret | TESSERACT_ZBAR_BARCODE_EQ;  // 8
            }
        }
        if ((ret & TESSERACT_ZBAR_BARCODE_EQ) == TESSERACT_ZBAR_BARCODE_EQ) {
            if (only::checkFullBarCode(teFCode, bcode)) {
                ret = ret | ALL_CODE_VALIDATE_OK;  // 16
            }
        }
        return ret;
    }

    int Image::WriteToFile(const char* filename)
    {
        if (filename == nullptr)
            filename = this->filename;
        return imwrite(filename, imageMat);
    }

    int ImageProcessingDestroy()
    {
        if (tessEngine != nullptr) {
            tessEngine->Clear();
            tessEngine->End();
            delete tessEngine;
            tessEngine = nullptr;
        }
        return 0;
    }

    int ImageProcessingStartup()
    {
        using namespace img_error;
        if (tessEngine == nullptr) {
            tessEngine = new tesseract::TessBaseAPI();

            if (tessEngine->Init(nullptr, "eng")) {
#ifdef ENABLE_LOGGER
                log_ERROR(err[IP_OCR_ERROR]);
#endif
                return IP_OCR_ERROR;
            }
        }
        return IP_OCR_ERROR;
    }

}  // namespace fc
