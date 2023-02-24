#include <iostream>
#include <string>

#include <opencv2/opencv.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}


int main(int argc, const char* argv[])
{

   std::cout << "Test FFMPEG" << std::endl;

   //Return value
   int ret = -1;

   //Print avcodec version
   std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;

   //Path input
   std::string inputFilePath1="/home/mathieu/bosse/salt/src/salt/calibration/tests/data/charuco.mp4";
   std::string inputFilePath2="/home/mathieu/bosse/salt/src/salt/calibration/tests/data/chessboard.mp4";
   //std::string inputFilePath1="/home/mathieu/surgar/surgeryProjectData/00000001A/intraopRaw/video/v001/leftVideo_0001.mkv";
   //std::string inputFilePath2="/home/mathieu/surgar/surgeryProjectData/00000001A/intraopRaw/video/v001/rightVideo_0001.mkv";
   //std::string inputFilePath2="/home/richard/data/Test_FFMPEG/split2/1.mp4";
   //std::string inputFilePath2="/home/richard/data/Test_FFMPEG/test_output.mp4";
   //std::string inputFilePath2="/home/richard/data/debug.mp4";

   //Init vars
   AVFormatContext* inputFormatContext1 = NULL;
   AVFormatContext* inputFormatContext2 = NULL;
   AVStream* inputStream1 = NULL;
   AVStream* inputStream2 = NULL;
   AVCodec* inputCodec1 = NULL;
   AVCodec* inputCodec2 = NULL;
   AVCodecContext* inputCodecContext1 = NULL;
   AVCodecContext* inputCodecContext2 = NULL;

   int inputVideoIndex1 = -1;
   int inputVideoIndex2 = -1;

   
   //avdevice_register_all();
   avcodec_register_all();
   ///////////////
   //Open files
   /////
   //File 1
   ret = avformat_open_input(&inputFormatContext1, inputFilePath1.c_str(), NULL, NULL);
   if(ret < 0)
   {
      std::cout << "Could not open input file: \"" << inputFilePath1 << "\" ret : " << ret << std::endl;
      return -1;
   }
   ret = avformat_find_stream_info(inputFormatContext1, NULL);
   if(ret < 0)
   {
      std::cout << "Could not retrieve input stream information." << std::endl;
      return -1;
   }
   /////
   //File 2
   ret = avformat_open_input(&inputFormatContext2, inputFilePath2.c_str(), NULL, NULL);
   if(ret < 0)
   {
      std::cout << "Could not open input file: \"" << inputFilePath2 << "\"" << std::endl;
      return -1;
   }
   ret = avformat_find_stream_info(inputFormatContext2, NULL);
   if(ret < 0)
   {
      std::cout << "Could not retrieve input stream information." << std::endl;
      return -1;
   }


   //////////////////
   //Init Frames
   /////
   //Frame 1
   AVFrame* inputFrame1 = NULL;
   AVFrame* inputFrame2 = NULL;
   inputFrame1 = av_frame_alloc();
   if(!inputFrame1)
   {
      return -1;
   }
   /////
   //Frame 2
   inputFrame2 = av_frame_alloc();
   if(!inputFrame2)
   {
      return -1;
   }

   /////////////////
   //Init packets
   //Packet 1
   AVPacket* inputPacket1 = NULL;
   inputPacket1 = av_packet_alloc();
   if(!inputPacket1)
   {
      return -1;
   }
   AVPacket* inputPacket2 = NULL;
   inputPacket2 = av_packet_alloc();
   if(!inputPacket2)
   {
      return -1;
   }


   //Check the video streams

   std::cout << std::endl;
   std::cout << "Video 1" << std::endl;
   //Loop over the streams declared in the file
   for(int i=0; i<inputFormatContext1->nb_streams; i++){

      if(inputFormatContext1->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {

         inputVideoIndex1=i;
         std::cout << "Stream "<< i << " is a video stream." << std::endl;

      } else if (inputFormatContext1->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {

         std::cout << "Stream "<< i << " is an audio stream." << std::endl;

      } else {

         std::cout << "Stream "<< i << " is of unknown type" << std::endl;

      }
   }

   std::cout << std::endl;
   std::cout << "Video 2" << std::endl;
   //Loop over the streams declared in the file
   for(int i=0; i<inputFormatContext2->nb_streams; i++){

      if(inputFormatContext2->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {

         inputVideoIndex2=i;
         std::cout << "Stream "<< i << " is a video stream." << std::endl;

      } else if (inputFormatContext2->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {

         std::cout << "Stream "<< i << " is an audio stream." << std::endl;

      } else {

         std::cout << "Stream "<< i << " is of unknown type" << std::endl;

      }
   }
   std::cout << std::endl;

   //Get input streams
   inputStream1 = inputFormatContext1->streams[inputVideoIndex1];
   inputStream2 = inputFormatContext2->streams[inputVideoIndex2];

   std::cout << "DEBUG" << std::endl;
   std::cout << inputStream2->pts_wrap_behavior << std::endl;
   std::cout << inputStream2->mux_ts_offset << std::endl;
   std::cout << inputStream2->start_time << std::endl;
   //return -1;

   //Get decoders
   inputCodec1 = avcodec_find_decoder(inputStream1->codecpar->codec_id);
   if(!inputCodec1)
   {
      std::cout << "No decoder for the codec: " << inputStream1->codecpar->codec_id << std::endl;
      return -1;
   }
   inputCodec2 = avcodec_find_decoder(inputStream2->codecpar->codec_id);
   if(!inputCodec2)
   {
      std::cout << "No decoder for the codec: " << inputStream2->codecpar->codec_id << std::endl;
      return -1;
   }

   //Create codec contexts
   inputCodecContext1 = avcodec_alloc_context3(inputCodec1);
   if(avcodec_parameters_to_context(inputCodecContext1, inputStream1->codecpar) < 0)
   {
      std::cout << "Failed to fill the codec context" << std::endl;
      return -1;
   }
   inputCodecContext2 = avcodec_alloc_context3(inputCodec2);
   if(avcodec_parameters_to_context(inputCodecContext2, inputStream2->codecpar) < 0)
   {
      std::cout << "Failed to fill the codec context" << std::endl;
      return -1;
   }


   //Initialize codec contexts
   ret = avcodec_open2(inputCodecContext1, inputCodec1, NULL);
   if(ret < 0)
   {
      std::cout << "Failed to initialize the codec context." << std::endl;
      return -1;
   }
   ret = avcodec_open2(inputCodecContext2, inputCodec2, NULL);
   if(ret < 0)
   {
      std::cout << "Failed to initialize the codec context." << std::endl;
      return -1;
   }

   //Initialize sample scaler
   int width = -1;
   int height = -1;
   if(inputStream1->codecpar->width==inputStream2->codecpar->width){

      width=inputStream1->codecpar->width;

   } else {

      std::cout << "Size mismatch !!" << std::endl;
      return 1;

   }
   if(inputStream1->codecpar->height==inputStream2->codecpar->height){

      height=inputStream1->codecpar->height;

   } else {

      std::cout << "Size mismatch !!" << std::endl;
      return 1;

   }


   //Buffer image
   cv::Mat bufferMatImage = cv::Mat::zeros(height,width,CV_8UC3);
   int cvLinesizes[1];
   cvLinesizes[0] = bufferMatImage.step1();

   //Check pixel format
   if(inputStream1->codecpar->format!=inputStream2->codecpar->format){

      std::cout << "Pixel format mismatch !!" << std::endl;
      return 1;

   }

   //Conversion
   SwsContext* conversion = sws_getContext(width, height, (AVPixelFormat) inputStream1->codecpar->format, width, height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

   bool continueValue=true;

   int ret1=-1;
   int ret2=-1;
   size_t nbFrames=0;
   std::vector< std::pair<int64_t,cv::Mat> > frameVector1;
   std::vector< std::pair<int64_t,cv::Mat> > frameVector2;

   //TODO remove
   size_t ID_test=0;

   while(true){

      ret1 = av_read_frame(inputFormatContext1, inputPacket1);
      ret2 = av_read_frame(inputFormatContext2, inputPacket2);

      if(ret1!=ret2){

         std::cout << "Missing outputs" << std::endl;
         return 1;

      }

      //ret1==ret2
      if(ret1<0){
         break;
      }

      ret = avcodec_send_packet(inputCodecContext1, inputPacket1);
      if(ret < 0)
      {
         std::cout << "Error supplying raw packet data to decoder. " << std::endl;
         return -1;
      }

      ret = avcodec_send_packet(inputCodecContext2, inputPacket2);
      if(ret < 0)
      {
         std::cout << "Error supplying raw packet data to decoder. " << std::endl;
         return -1;
      }


      while(ret1 >= 0)
      {
         ret1 = avcodec_receive_frame(inputCodecContext1, inputFrame1);

         if(ret1 == AVERROR(EAGAIN) || ret1 == AVERROR_EOF)
         {
            break;
         }
         else if(ret1 < 0)
         {
            std::cout << "Error receiving data from decoder." << std::endl;
            return -1;
         }

         //std::cout << "PTS1 found = " << inputFrame1->pts << std::endl;
         
         //Extract pts
         int64_t frame_pts1 = inputFrame1->pts;


         //Decode frame
         sws_scale(conversion, inputFrame1->data, inputFrame1->linesize, 0, height, &bufferMatImage.data, cvLinesizes);

         //Clone mat
         cv::Mat mat1 = bufferMatImage.clone();

         //Push values
         frameVector1.push_back(std::make_pair(frame_pts1,mat1));

      }

      while(ret2 >= 0)
      {
         ret2 = avcodec_receive_frame(inputCodecContext2, inputFrame2);

         if(ret2 == AVERROR(EAGAIN) || ret2 == AVERROR_EOF)
         {
            break;
         }
         else if(ret2 < 0)
         {
            std::cout << "Error receiving data from decoder." << std::endl;
            return -1;
         }

         //std::cout << "PTS2 found = " << inputFrame2->pts << std::endl;
         
         //Extract pts
         int64_t frame_pts2 = inputFrame2->pts;

         
         //Decode frame
         sws_scale(conversion, inputFrame2->data, inputFrame2->linesize, 0, height, &bufferMatImage.data, cvLinesizes);

         //Clone mat
         cv::Mat mat2 = bufferMatImage.clone();

         //Push values
         frameVector2.push_back(std::make_pair(frame_pts2,mat2));

      }


      //Shall check the frame in the vector
      size_t nbEltVect = std::min(frameVector1.size(),frameVector2.size());

      for(size_t k=0; k<nbEltVect; k++){

         std::cout << "Checking number : " << nbFrames << std::endl;
         nbFrames++;
         
         //Always take first values of the elements as we erase the vector little by little
         int64_t pts1 = frameVector1.at(0).first;
         int64_t pts2 = frameVector2.at(0).first;
         cv::Mat mat1 = frameVector1.at(0).second;
         cv::Mat mat2 = frameVector2.at(0).second;

         //TODO remove
         std::cout << "DEBUG Index = " << ID_test << std::endl;
         std::cout << "PTS " << pts1 << " and " << pts2 << "." <<std::endl;
         ID_test++;
         
         if(pts1!=pts2){
            std::cout << "PTS does not correspond " << pts1 << " and " << pts2 << "." <<std::endl;
            return 1;
         }

         //Split channels
         cv::Mat mat1Split[3];
         cv::Mat mat2Split[3];
         cv::split(mat1,mat1Split);
         cv::split(mat2,mat2Split);

         for(size_t nchan=0; nchan<3; nchan++){

            // Equal if no elements disagree
            cv::Mat diff = mat1Split[nchan] != mat2Split[nchan];
            if(cv::countNonZero(diff) != 0){

               std::cout << "Mat does not correspond" << std::endl;
               cv::imshow("Mat1", mat1);
               cv::imshow("Mat2", mat2);
               cv::waitKey(0);
               return 1;

            } 

         }

         //TODO remove
         cv::imshow("Mat1", mat1);
         cv::imshow("Mat2", mat2);
         cv::waitKey(10);

         //Remove first element
         frameVector1.erase(frameVector1.begin());
         frameVector2.erase(frameVector2.begin());

      }



   }


   return 0;

}
