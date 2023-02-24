#include <iostream>
#include <string>

#include <opencv2/opencv.hpp>
/*
 /home/mathieu/bosse/test/ffmpeg/install;
 /home/mathieu/bosse/test/opencv-4.5.5/install
 ../configure --prefix=../install --enable-shared --disable-stripping --enable-debug=1
*/

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}

void checkFfmpegError(int rc, char * msg = "")
{
    if (rc < 0) {
        char str[256];
        av_strerror(rc, str, 256);
        std::cout << "error: " << msg
                  << " (rc: " << rc << ", ffmpeg msg: " << str << ")" << std::endl;
        exit(-1);
    }
}

int main(int argc, const char* argv[])
{
    std::cout << "Test FFMPEG" << std::endl;
    int ret = -1;
    std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;
    //std::string inputFilePath = "/home/mathieu/Downloads/test_video.mp4";
    std::string inputFilePath="../video/charuco.mp4"; // to give for test

    AVFormatContext* inputFormatContext = NULL;

    // avformat_open_input
    //   Open an input stream and read the header.
    //   The codecs are not opened. The stream must be closed with avformat_close_input().
    checkFfmpegError(avformat_open_input(&inputFormatContext, inputFilePath.c_str(), NULL, NULL),
                     "open file failed");

    // avformat_find_stream_info
    //   Read packets of a media file to get stream information.
    //   This is useful for file formats with no headers such as MPEG. This function also computes
    //     the real framerate in case of MPEG-2 repeat frame mode. The logical file position is not
    //     changed by this function; examined packets may be buffered for later processing.
    checkFfmpegError(avformat_find_stream_info(inputFormatContext, NULL),
                     "could not retrieve input stream information.");

    // Check the video streams
    int inputVideoIndex = -1;
    // Loop over the streams declared in the file
    for(int i = 0; i < inputFormatContext->nb_streams; i++){
        if(inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            inputVideoIndex = i;
            std::cout << "Stream "<< i << " is a video stream." << std::endl;
        } else if(inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            std::cout << "Stream "<< i << " is an audio stream." << std::endl;
        } else {
            std::cout << "Stream "<< i << " is of unknown type" << std::endl;
        }
    }
    checkFfmpegError(inputVideoIndex != -1 ? 0 : -1, "no video stream found");

    // Get input video streams
    AVStream * inputVideoStream = inputFormatContext->streams[inputVideoIndex];
    std::cout << "start time: " << inputVideoStream->start_time << std::endl;

    // Get decoders of video stream
    // avcodec_find_decoder
    //   Find a registered decoder with a matching codec ID.
    AVCodec * inputCodec = avcodec_find_decoder(inputVideoStream->codecpar->codec_id);
    if(!inputCodec) {
        std::cout << "No decoder for the codec: " << inputVideoStream->codecpar->codec_id << std::endl;
        return -1;
    }

    // Create codec contexts
    AVCodecContext * ctx = avcodec_alloc_context3(inputCodec);
    if (!ctx) {
        std::cout << "Can't allocate decoder context" << std::endl;
        return AVERROR(ENOMEM);
    }

    // avcodec_parameters_to_context
    //   Fill the codec context based on the values from the supplied codec parameters.
    if(avcodec_parameters_to_context(ctx, inputVideoStream->codecpar) < 0)
    {
        std::cout << "Failed to fill the codec context" << std::endl;
        return -1;
    }

    // Initialize codec contexts
    // avcodec_open2
    //   Initialize the AVCodecContext to use the given AVCodec.
    //   Prior to using this function the context has to be allocated with avcodec_alloc_context3().
    ret = avcodec_open2(ctx, inputCodec, NULL);
    if(ret < 0) {
        std::cout << "can't open decoder." << std::endl;
        return -1;
    }

    // Initialize sample scaler
    int width = inputVideoStream->codecpar->width;
    int height = inputVideoStream->codecpar->height;
    std::cout << "width: " << width << " height: " << height << std::endl;

    // init image buffer
    cv::Mat bufferMatImage = cv::Mat::zeros(height, width, CV_8UC3);
    int cvLinesizes[1];
    cvLinesizes[0] = bufferMatImage.step1();

    // Conversion
    // sws_getContext
    //   Allocate and return an SwsContext. You need it to perform
    //   scaling/conversion operations using sws_scale().
    SwsContext* conversion = sws_getContext(width, height, static_cast<AVPixelFormat>(inputVideoStream->codecpar->format),
                                            width, height, AV_PIX_FMT_BGR24,
                                            SWS_FAST_BILINEAR, NULL, NULL, NULL);

    // Demuxers read a media file and split it into chunks of data (packets).
    // A packet contains one or more encoded frames which belongs to a single elementary stream.
    // In the lavf API this process is represented by the avformat_open_input()
    // function for opening a file, av_read_frame() for reading a single packet
    // and finally avformat_close_input(), which does the cleanup.
    AVFrame* inputFrame = NULL;
    inputFrame = av_frame_alloc();
    if(!inputFrame)
        return AVERROR(ENOMEM);

    AVPacket* inputPacket = NULL;
    inputPacket = av_packet_alloc();
    if(!inputPacket)
        return AVERROR(ENOMEM);

    size_t readFrameNum = 0;
    int numFrame = 0;
    ret = 0;
    bool stopIt = false;
    while(ret >= 0){
        // av_read_frame
        //   Return the next frame of a stream.
        //   This function returns what is stored in the file, and does not validate that what
        //   is there are valid frames for the decoder. It will split what is stored in the file
        //   into frames and return one for each call. It will not omit invalid data between valid
        //   frames so as to give the decoder the maximum information possible for decoding.
        ret = av_read_frame(inputFormatContext, inputPacket);
        if (ret >= 0 && inputPacket->stream_index != inputVideoIndex) {
            av_packet_unref(inputPacket);
            continue;
        }
        if (ret < 0)
            ret = avcodec_send_packet(ctx, NULL);
        else {
            if (inputPacket->pts == AV_NOPTS_VALUE)
                inputPacket->pts = inputPacket->dts = numFrame;
            ret = avcodec_send_packet(ctx, inputPacket);
        }
        av_packet_unref(inputPacket);

        if (ret < 0) {
            std::cout << "error submitting a packet for decoding" << std::endl;
            return ret;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(ctx, inputFrame);
            if (ret == AVERROR_EOF) {
                stopIt = true;
                break;
            }
            else if (ret == AVERROR(EAGAIN)) {
                ret = 0;
                break;
            } else if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                return ret;
            }

            // decode frame to openCV
            sws_scale(conversion, inputFrame->data, inputFrame->linesize, 0, height, &bufferMatImage.data, cvLinesizes);

            std::cout << "showing frame num: " << numFrame << " timestamp: " << inputFrame->pts << std::endl;

            // show it
            cv::imshow("Mat", bufferMatImage);
            if(cv::waitKey(10) >= 0)
                stopIt = true;

            av_frame_unref(inputFrame);
         }
         numFrame++;

         if (stopIt)
            break;
    }

    av_packet_free(&inputPacket);
    av_frame_free(&inputFrame);
    avformat_close_input(&inputFormatContext);
    avcodec_free_context(&ctx);

    return 0;
}
