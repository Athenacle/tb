
#include "image.h"
#include "id.h"
#include "ocr.h"
#include "taobao.h"

#include <json/json.h>
#include <unistd.h>
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
    aip::Ocr* client;

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
    ImageProcessingHandler* ImageProcessingHandler::instance = nullptr;

    ImageProcessingHandler& ImageProcessingHandler::getImageProcessingHandler()
    {
        if (instance == nullptr) {
            instance = new ImageProcessingHandler();
        }
        return *instance;
    }

    void ImageProcessingHandler::AddWaterMarker(const WaterMarker* wm)
    {
        auto h = getImageProcessingHandler().waters;
        waters.emplace_back(wm);
    }

    BaseImage::BaseImage(const char* _path, unsigned int mask) : filename(_path)
    {
        imageMat = cv::imread(filename, mask);
    }

    Image::Image(const char* _fname) : BaseImage(_fname) {}


    int Image::getItemCode(std::string& fcode, std::string& bcode, int& price)
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
        OcrResult result;
        zstatus = zbarCodeIdentify(imageMat, rect, bcode);
        tstatus = ProcessingOCR(this->filename, result);
        if (zstatus == 1
            && only::checkBarCodeValidate(bcode)) {  // zbar success. Bar Code MUST be TRUE.
            ret = ret | ZBAR_OK;                     // 1
        }
        std::cerr << "tstatus : " << tstatus;
        if (tstatus >= 1) {
            ret = ret | OCR_OK;  // 2
            if (result.getFullCode(teFCode)) {
                ret = ret | OCR_GET_FULL_CODE;
                fcode = teFCode;
            }
            if (result.getBarCode(teBCode)) {
                ret = ret | OCR_GET_BAR_CODE;
            }
            if (bcode.length() == 0) {
                //zbar failed to get bar code, set it to OCR-gotten BarCode
                bcode = teBCode;
            }
            if (only::checkFullBarCode(teFCode, teBCode)) {
                ret = ret | FULL_BAR_CODE_VALID;  // 4
            }
            if (result.getPrice(price)) {
                ret = ret | OCR_GET_PRICE_OK;
            }
        }
        if (zstatus == 1 && tstatus == 1) {
            if (bcode == teBCode) {
                ret = ret | ZBAR_OCR_BAR_CODE_EQUAL;
                if (only::checkFullBarCode(teFCode, bcode)) {
                    ret = ret | ALL_CODE_VALIDATE_OK;  // 16
                }
            }
        }
        return ret;
    }

    int Image::WriteToFile(const char* filename)
    {
        if (filename == nullptr)
            filename = this->filename.c_str();
        return imwrite(filename, imageMat);
    }

    char* OcrResult::dumpJson() const
    {
        unsigned char* gzCompressed;
        unsigned char* _json =
            const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(this->json.c_str()));
        int ret = tb::utils::gzCompress(const_cast<unsigned char*>(_json),
                                        strlen(reinterpret_cast<char*>(_json)),
                                        &gzCompressed);
        if (ret == -1) {
            return nullptr;
        }
        char* base64;
        ret = tb::utils::base64Encode(gzCompressed, ret, &base64, false);
        tb::utils::releaseMemory(gzCompressed);
        if (ret > 0) {
            return base64;
        } else {
            return nullptr;
        }
    }

    bool OcrResult::__find(std::string& _code, std::function<bool(const std::string&)> fn) const
    {
        auto c = std::find_if(words.cbegin(), words.cend(), fn);
        bool ret;
        if (c != words.end()) {
            _code = *c;
            ret = true;
        } else {
            _code = "";
            ret = false;
        }
        return ret;
    }

    bool OcrResult::getFullCode(std::string& _code) const
    {
        return __find(_code, only::checkFullCode);
    }

    bool OcrResult::getBarCode(std::string& _code) const
    {
        return __find(_code, only::checkBarCodeValidate);
    }

    bool OcrResult::getPrice(int& p) const
    {
        constexpr auto cp = only::checkPrice;
        auto c = std::find_if(words.cbegin(), words.cend(), [&](auto& p) { return cp(p) != -1; });
        if (c != words.cend()) {
            p = cp(*c);
            return true;
        } else {
            return false;
        }
    }

    WaterMarker::WaterMarker()
    {
        id = "";
        rotation = 0;
        transparent = 1;
        repeat = 1;
        anchor = ANCHOR_TOP | ANCHOR_LEFT;
        xOffset = 0;
        yOffset = 1;
        waterMarker = waterMarkerMask = nullptr;
    }

#define WATER_GETVALUE(destValue, parent, key, Type)            \
    do {                                                        \
        if (parent.isMember(#key) && parent[#key].is##Type()) { \
            destValue = parent[#key].as##Type();                \
        }                                                       \
    } while (0);

    bool WaterMarker::CheckWaterMarker(char* buffer, size_t bsize)
    {
        auto t = transparent;
        auto r = rotation;
        auto re = repeat;
        if (waterMarkerPath == "") {
            snprintf(buffer,
                     bsize,
                     "WaterMarker %s does not contain a image path.",
                     id == "" ? "" : id.c_str());
            log_ERROR(buffer);
            return false;
        } else {
            auto s = tb::utils::checkFileCanRead(waterMarkerPath.c_str(), buffer, bsize);
            if (s == -1u) {
                log_ERROR(buffer);
                return false;
            }
            waterMarker = new BaseImage(waterMarkerPath.c_str());
            waterMarker = new BaseImage(waterMarkerPath.c_str(), cv::IMREAD_GRAYSCALE);
        }
        if (r < 0) {
            double rr = 360 - std::fmod(rotation, 360);
            snprintf(buffer, bsize, "Image ROTATION Get negative value %lf, assume as %lf.", r, rr);
            rotation = rr;
            log_WARNING(buffer);
        }
        if (r > 360) {
            double rr = std::fmod(rotation, 360);
            snprintf(buffer,
                     bsize,
                     "Image ROTATION Get value %lf greater than 360, assume as %lf.",
                     r,
                     rr);
            rotation = rr;
            log_WARNING(buffer);
        }
        if (t < 0) {
            double tt = std::fmod(std::abs(t), 1.0);
            snprintf(
                buffer, bsize, "Image TRANSPARENT Get negative value %lf , assume as %lf.", t, tt);
            transparent = tt;
            log_WARNING(buffer);
        }
        if (t - 1.0 > 1e-5) {
            double tt = std::fmod(t, 1.0);
            snprintf(buffer,
                     bsize,
                     "Image TRANSPARENT Get value %lf greater than 1.0 , assume as %lf.",
                     t,
                     tt);
            transparent = tt;
            log_WARNING(buffer);
        }
        if (re < 0) {
            snprintf(buffer, bsize, "Image repeat Get negative value %d, assume as 1.", re);
            repeat = 1;
            log_WARNING(buffer);
        }
        snprintf(buffer,
                 bsize,
                 "WaterMarker %s image %s load successfully.",
                 id == "" ? "" : id.c_str(),
                 waterMarkerPath.c_str());
        log_INFO(buffer);
        snprintf(
            buffer,
            bsize,
            "Rotation: %lf, Transparent: %lf, Repeat: %d, position: %s, xOffset: %d, yOffset: %d.",
            rotation,
            transparent,
            repeat,
            position.c_str(),
            xOffset,
            yOffset);
        log_INFO(buffer);
        return true;
    }

    WaterMarker* WaterMarker::BuildWaterMarker(const Json::Value& v, char* buffer, size_t bsize)
    {
        auto* ret = new WaterMarker();
        WATER_GETVALUE(ret->id, v, id, String);
        WATER_GETVALUE(ret->waterMarkerPath, v, image, String);
        WATER_GETVALUE(ret->rotation, v, rotation, Double);

        WATER_GETVALUE(ret->transparent, v, transparent, Double);
        WATER_GETVALUE(ret->repeat, v, repeat, Double);
        auto pos = v["position"];
        if (pos.isObject()) {
            WATER_GETVALUE(ret->position, pos, anchor, String);
            WATER_GETVALUE(ret->xOffset, pos, x, Double);
            WATER_GETVALUE(ret->yOffset, pos, y, Double);
        }
        bool status = ret->CheckWaterMarker(buffer, bsize);
        return status ? ret : nullptr;
    }

    int ImageProcessingDestroy()
    {
        return 0;
    }


    int ImageProcessingStartup(const Json::Value& v)
    {
        using namespace img_error;
        const int bufferSize = 1024;
        char* buffer = tb::utils::requestMemory(bufferSize);

        if (!v.isMember("image")) {
            log_ERROR(err[ERR_IO_NOT_EXIST]);
            return ERR_IO_NOT_EXIST;
        }
        auto image = v["image"];
        if (!v.isObject()) {
            log_ERROR(err[ERR_IO_NOT_EXIST]);
            return ERR_IO_NOT_EXIST;
        }

        if (image.isMember("waterMark")) {
            auto waterMark = image["waterMark"];
            auto handler = ImageProcessingHandler::getImageProcessingHandler();
            if (waterMark.isArray()) {
                for (Json::ArrayIndex i = 0; i < waterMark.size(); i++) {
                    auto w = waterMark[i];
                    if (w.isObject()) {
                        handler.AddWaterMarker(
                            WaterMarker::BuildWaterMarker(w, buffer, bufferSize));
                    }
                }
            } else if (waterMark.isObject()) {
                handler.AddWaterMarker(
                    WaterMarker::BuildWaterMarker(waterMark, buffer, bufferSize));
            }
        }
        if (image.isMember("ocr")) {
            auto ocr = image["ocr"];
            if (ocr.isObject() && ocr.isMember("BaiduOCR")) {
                auto baiduOCR = ocr["ocr"];
                bool enable = false;
                string aid, akey, skey;
                aid = akey = skey;
                if (baiduOCR.isObject()) {
                    if (baiduOCR.isMember("enable")) {
                        enable = baiduOCR["enable"].asBool();
                    }
                    aid = baiduOCR["app-id"].asString();
                    akey = baiduOCR["app-key"].asString();
                    skey = baiduOCR["secret-key"].asString();
                    if (enable && aid != "" && akey != "" && skey != "") {
                        ImageProcessingStartup(aid, akey, skey);
                    }
                }
            }
        }
        return 0;
    }

    int ImageProcessingStartup(const string& _aid, const string& _akey, const string& _skey)
    {
        if (client == nullptr) {
            client = new aip::Ocr(_aid, _akey, _skey);
        }
        return 0;
    }

    int ProcessingOCR(const string& path, OcrResult& result)
    {
        return ProcessingOCR(
            path, result.words, result.id, result.errMessage, result.errCode, result.json);
    }

    int ProcessingOCR(const string& _path,
                      std::vector<std::string>& vstring,
                      uint64_t& _id,
                      std::string& _errmessage,
                      int& _errcode,
                      std::string& json)
    {
        auto path = _path.c_str();
        Json::Value result;
        std::string image;
        if (access(path, R_OK) != 0) {
            return -1;
        }
        aip::get_file_content(path, &image);
        std::map<std::string, std::string> options;
        options["language_type"] = "CHN_ENG";
        options["detect_direction"] = "true";
        options["detect_language"] = "true";
        options["probability"] = "true";
        result = client->general_basic(image, options);
        vstring.clear();
        if (result.isMember("error_code")) {
            _errcode = result["error_code"].asInt();
            if (result.isMember("error_msg")) {
                _errmessage = result["error_msg"].asString();
            } else {
                _errmessage = ocrErrorTable.at(_errcode);
            }
            return 0;
        }

        if (result.isMember("log_id")) {
            _id = result["log_id"].asUInt64();
        }
        if (result.isMember("words_result_num")) {
            vstring.reserve(result["words_result_num"].asUInt());
        }
        if (result.isMember("words_result") && result["words_result"].isArray()) {
            auto results = result["words_result"];
            for (unsigned int i = 0; i < results.size(); i++) {
                vstring.emplace_back(results[i]["words"].asString());
            }
        }
        Json::StreamWriterBuilder fwriter;
        fwriter.settings_["indentation"] = "";
        json = Json::writeString(fwriter, result);
        std::cout << json;
        return vstring.size();
    }
}  // namespace fc
