
#include "image.h"
#include <zbar.h>
#include <iostream>
#include <vector>

#define SHOW(m)                                 \
    do {                                        \
        cv::namedWindow(#m, cv::WINDOW_NORMAL); \
        cv::imshow(#m, m);                      \
        cv::waitKey(0);                         \
    } while (0);

namespace
{
    int opencvFindBarCodeROI(const cv::Mat mat, cv::Rect& roi, unsigned dilateTimes = 4)
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
        Mat element = getStructuringElement(0, cv::Size(7, 7));
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
        if (rect.height < 10) {
            return -1;
        } else {
            roi = Rect(rect.x, rect.y + cuttedHeight, rect.width, rect.height);
            return 0;
        }
    }

    // identify bar code via zbar
    int zbarCodeIdentify(const cv::Mat& mat, char* code, size_t length)
    {
        using namespace cv;
        using namespace zbar;

        ImageScanner scanner;
        Mat gray;
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
            const char* str = begin->get_data().c_str();
            if (length <= strlen(str)) {
                return -2;
            } else {
                strcpy(code, str);
                std::cout << code;
                return 1;
            }
        } else if (n < 0) {
            return -1;
        } else {
            return 0;
        }
    }


    int tesseractCodeIdentify(cv::Mat& mat, char* code, size_t length)
    {
        using namespace cv;
        using namespace tesseract;
        TessBaseAPI* tess = fc::tessEngine;
        tess->SetImage(reinterpret_cast<unsigned char*>(mat.data),
                       mat.size().width,
                       mat.size().height,
                       mat.channels(),
                       mat.step1());
        tess->Recognize(0);
        const char* out = tess->GetUTF8Text();
        std::cout << out;
        return 0;
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

    int Image::getBarCode(char* code, size_t length)
    {
        *code = 0;
        cv::Rect rect;
        int ret = opencvFindBarCodeROI(imageMat, rect);
        if (ret == -1) {
            return 0;
        }
        Mat tmp = imageMat(rect);
        int status = -1;
        status = zbarCodeIdentify(tmp, code, length);
        if (status == 1) {
            //zbarCodeIdentify get result successfully.
            return 1;
        } else {
            //try tesseract to get result.
            status = tesseractCodeIdentify(tmp, code, length);
            return 0;
        }
    }

    int Image::WriteToFile(const char* filename)
    {
        if (filename == nullptr)
            filename = this->filename;
        return imwrite(filename, imageMat);
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
