#include <chrono>
#include <thread>
#include <iostream>

#include <opencv2/opencv.hpp>
//#include <libyuv/video_common.h>
//#include <libyuv/convert_argb.h>


int main(int argc, char** argv )
{

    cv::Mat frame;  // variable frame of datatype Matrix
    cv::VideoCapture capture;
    //capture.open("/home/mathieu/surgar/surgeryProjectData/00000001A/intraopRaw/video/v001/leftVideo_0001.mkv");
    capture.open("/dev/video1");
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 2560);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
//    capture.set(cv::CAP_PROP_FORMAT);
//    capture.set(cv::CAP_PROP_MODE);
    std::cout << "f" << capture.get(cv::CAP_PROP_FORMAT) << std::endl;
    std::cout << "m" << capture.get(cv::CAP_PROP_MODE) << std::endl;
    capture.set(cv::CAP_PROP_FORMAT, -1);
    capture.set(cv::CAP_PROP_MODE, 3);
    std::cout << "f" << capture.get(cv::CAP_PROP_FORMAT) << std::endl;
    std::cout << "m" << capture.get(cv::CAP_PROP_MODE) << std::endl;
    capture.set(cv::CAP_PROP_CONVERT_RGB, 1);


    // 720p
    // no parallel 90%
    // fallback strip 4 205%
    // fallback strip 10 265%
    // fallback strip 100 300%
    // fallback strip n? 300%
    // tbb no parallel 70%
    // tbb strip 4 145%
    // tbb strip n? 230%
    // openmp WITH_PTHREADS_PF=OFF no parallel 254 % ??
    // openmp WITH_PTHREADS_PF=OFF strip 4 600% ??
    // openmp WITH_PTHREADS_PF=OFF strip n? 600%

    // no parallel 1.1 ms /frame 39%
    // tbb n release 0.6ms to 0.7ms /frame 75%
    // tbb 4 release 0.5ms to 0.6ms /frame 45%
    // tbb 2 release 0.6ms to 0.9ms /frame 45%


    for(;;){
        capture.read(frame);
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            break;
        }
//        int ConvertToARGB(const uint8_t* sample,
//                          size_t sample_size,
//                          uint8_t* dst_argb,
//                          int dst_stride_argb,
//                          int crop_x,
//                          int crop_y,
//                          int src_width,
//                          int src_height,
//                          int crop_width,
//                          int crop_height,
//                          enum RotationMode rotation,
//                          uint32_t fourcc);
        //std::cout << "size" << frame.rows * frame.cols << std::endl;

//      cv::Mat frameCopy{frame.size(), CV_8UC1};
//        int ret = libyuv::ConvertToARGB(frame.data, frame.cols * frame.rows,
//                                        frameCopy.data, frame.rows,
//                                        0, 0,
//                                        frame.cols, frame.rows,
//                                        frame.cols, frame.rows,
//                                        libyuv::RotationMode::kRotate0,
//                                        libyuv::FOURCC_YUY2);

        //std::cout << "ret: " << ret << std::endl;

//        LIBYUV_API
//        int U422ToARGB(const uint8_t* src_y,
//                       int src_stride_y,
//                       const uint8_t* src_u,
//                       int src_stride_u,
//                       const uint8_t* src_v,
//                       int src_stride_v,
//                       uint8_t* dst_argb,
//                       int dst_stride_argb,
//                       int width,
//                       int height);

        cv::imshow("Window", frame);

        //break;
        if(cv::waitKey(30)>=0)
            break;
    }
    return 0;
}