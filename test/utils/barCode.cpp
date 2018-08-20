
#include <iostream>
#include <string>
#include "image.h"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("usage: barCode <image file>");
        exit(-1);
    }
    fc::Image i(argv[1]);
    string bc;
    int roi[4];
    i.getBarCode(bc, roi);
    std::cout << bc << std::endl;
    if (roi[0] > 0) {
        cv::Mat m(i.getMat(), cv::Rect(roi[0], roi[1], roi[2], roi[3]));
        cv::imshow("test", m);
        cv::waitKey(0);
    }
    return 0;
}
