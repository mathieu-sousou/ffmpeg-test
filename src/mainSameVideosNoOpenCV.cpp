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

void checkError(int ret, char * msg = "")
{
    if (ret < 0) {
        char str[256];
        av_strerror(ret, str, 256);
        std::cout << "error at line: " << __LINE__ << "msg: " << msg
            << " ret: " << ret << " reason: " << str << std::endl;
        exit(-1);
    }
}

int main(int argc, const char* argv[])
{
    std::cout << "Test FFMPEG" << std::endl;
    int ret = -1;
    std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;
    //std::string inputFilePath1 = "/home/mathieu/Downloads/test_video.mp4";
    std::string inputFilePath1="/home/mathieu/bosse/salt/src/salt/calibration/tests/data/chessboard.mp4"; // to give for test

    AVFormatContext* inputFormatContext = NULL;

    // avformat_open_input
    //   Open an input stream and read the header.
    //   The codecs are not opened. The stream must be closed with avformat_close_input().
    checkError(avformat_open_input(&inputFormatContext, inputFilePath1.c_str(), NULL, NULL));

    // avformat_find_stream_info
    //   Read packets of a media file to get stream information.
    //   This is useful for file formats with no headers such as MPEG. This function also computes
    //     the real framerate in case of MPEG-2 repeat frame mode. The logical file position is not
    //     changed by this function; examined packets may be buffered for later processing.
    checkError(avformat_find_stream_info(inputFormatContext, NULL), "Could not retrieve input stream information.");

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
    checkError(inputVideoIndex != -1 ? 0 : -1, "no video stream found");

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
    AVCodecContext * inputCodecContext = avcodec_alloc_context3(inputCodec);
    // avcodec_parameters_to_context
    //   Fill the codec context based on the values from the supplied codec parameters.
    if(avcodec_parameters_to_context(inputCodecContext, inputVideoStream->codecpar) < 0)
    {
        std::cout << "Failed to fill the codec context" << std::endl;
        return -1;
    }

    // Initialize codec contexts
    // avcodec_open2
    //   Initialize the AVCodecContext to use the given AVCodec.
    //   Prior to using this function the context has to be allocated with avcodec_alloc_context3().
    ret = avcodec_open2(inputCodecContext, inputCodec, NULL);
    if(ret < 0) {
        std::cout << "Failed to initialize the codec context." << std::endl;
        return -1;
    }

    /////////////// opencv ///////////////
    // Initialize sample scaler
    int width = inputVideoStream->codecpar->width;
    int height = inputVideoStream->codecpar->height;

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
        return -1;

    AVPacket* inputPacket = NULL;
    inputPacket = av_packet_alloc();
    if(!inputPacket)
        return -1;

    size_t readFrameNum = 0;
    while(true){
        // av_read_frame
        //   Return the next frame of a stream.
        //   This function returns what is stored in the file, and does not validate that what
        //   is there are valid frames for the decoder. It will split what is stored in the file
        //   into frames and return one for each call. It will not omit invalid data between valid
        //   frames so as to give the decoder the maximum information possible for decoding.
        int ret = av_read_frame(inputFormatContext, inputPacket);
        if(ret < 0)
            break;

        // supply raw packet data to decoder
        checkError(avcodec_send_packet(inputCodecContext, inputPacket),
                   "Error supplying raw packet data to decoder. ");

        std::vector< std::pair<int64_t, cv::Mat> > frameVector;
        while(ret >= 0)
        {
            // avcodec_receive_frame
            //   Return decoded output data from a decoder or encoder (when the AV_CODEC_FLAG_RECON_FRAME flag is used).
            ret = avcodec_receive_frame(inputCodecContext, inputFrame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            else if(ret < 0) {
                std::cout << "Error receiving data from decoder." << std::endl;
                return -1;
            }
            // Extract pts (timestamp)
            int64_t frameTimestamp = inputFrame->pts;

            // decode frame to openCV
            sws_scale(conversion, inputFrame->data, inputFrame->linesize, 0, height, &bufferMatImage.data, cvLinesizes);

            // Push values
            frameVector.push_back(std::make_pair(frameTimestamp, bufferMatImage.clone()));
        }

        for(auto i = 0; i < frameVector.size(); i++)
        {
            auto timestamp = frameVector.at(i).first;
            cv::Mat mat = frameVector.at(i).second;

            std::cout << "frame num = " << readFrameNum << std::endl;
            std::cout << "  timestamp " << timestamp << std::endl;

            cv::imshow("Mat", mat);
            cv::waitKey(10);
        }
        std::cout << "  nb received frame from decoder: " << frameVector.size() << std::endl;
        readFrameNum++;
    }

   return 0;
}
