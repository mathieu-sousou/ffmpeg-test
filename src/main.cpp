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

   namespace pt = boost::property_tree;

   //Json struture
   pt::ptree oroot;

   //Deprecated?
   //av_register_all();

   //Print avcodec version
   std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;

   //Path input
   std::string inputFilePath="/home/richard/Downloads/TestVidFFMPG/Video_011.mpg";
   std::string outputJsonPath="/home/richard/Downloads/TestVidFFMPG/PTSValues.json";

   //Check format
   if(inputFilePath.find_last_of('.') == std::string::npos)
   {
      std::cout << "Issue with input format" << std::endl;
      return -1;
   }

   //Init vars
   AVFormatContext* inputFormatContext = NULL;
   AVStream* inputStream = NULL;
   AVCodec* inputCodec = NULL;
   AVCodecContext* inputCodecContext = NULL;
   int inputVideoIndex = -1;

   //Open file
   int ret = avformat_open_input(&inputFormatContext, inputFilePath.c_str(), NULL, NULL);
   if(ret < 0)
   {
      std::cout << "Could not open input file: \"" << inputFilePath << "\"" << std::endl;
      return -1;
   }

   ret = avformat_find_stream_info(inputFormatContext, NULL);
   if(ret < 0)
   {
      std::cout << "Could not retrieve input stream information." << std::endl;
      return -1;
   }

   //Init packets and frames vars
   AVFrame* inputFrame = NULL;
   AVPacket* inputPacket = NULL;

   inputFrame = av_frame_alloc();
   if(!inputFrame)
   {
      return -1;
   }
   inputPacket = av_packet_alloc();
   if(!inputPacket)
   {
      return -1;
   }

   //Number of frames
   int numberOfFrames=0;

   //Debug vars
   int64_t last_dts = -1;
   std::vector<int64_t> dtsVector;

   //Check the video streams
   int videoStream=-1;
   //Loop over the streams declared in the file
   for(int i=0; i<inputFormatContext->nb_streams; i++){

      std::cout << "Index = " << i << std::endl;

      if(inputFormatContext->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {

         videoStream=i;

         //Get input stream
         inputStream = inputFormatContext->streams[videoStream];

         //Get decoder
         inputCodec = avcodec_find_decoder(inputStream->codecpar->codec_id);
         if(!inputCodec)
         {
            std::cout << "No decoder for the codec: " << inputStream->codecpar->codec_id << std::endl;
            return -1;
         }

         //Create codec context
         inputCodecContext = avcodec_alloc_context3(inputCodec);
         if(avcodec_parameters_to_context(inputCodecContext, inputStream->codecpar) < 0)
         {
            std::cout << "Failed to fill the codec context" << std::endl;
            return -1;
         }

         //Initialize codec context
         ret = avcodec_open2(inputCodecContext, inputCodec, NULL);
         if(ret < 0)
         {
            std::cout << "Failed to initialize the codec context." << std::endl;
            return -1;
         }

         //Initialize sample scaler
         int width = inputStream->codecpar->width;
         int height = inputStream->codecpar->height;

         //Buffer image
         cv::Mat bufferMatImage = cv::Mat::zeros(height,width,CV_8UC3);
         int cvLinesizes[1];
         cvLinesizes[0] = bufferMatImage.step1();

         //Conversion
         SwsContext* conversion = sws_getContext(width, height, (AVPixelFormat) inputStream->codecpar->format, width, height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

         while(av_read_frame(inputFormatContext, inputPacket) >= 0){

            ret = avcodec_send_packet(inputCodecContext, inputPacket);
            if(ret < 0)
            {
               std::cout << "Error supplying raw packet data to decoder. " << std::endl;
               return -1;
            }

            //Package info
            int64_t dts = inputPacket->dts;
            int64_t pts = inputPacket->pts;
            int64_t bpos = inputPacket->pos;
            int streamIndex = inputPacket->stream_index;

            while(ret >= 0)
            {
               ret = avcodec_receive_frame(inputCodecContext, inputFrame);

               if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
               {
                  break;
               }
               else if(ret < 0)
               {
                  std::cout << "Error receiving data from decoder." << std::endl;
                  return -1;
               }

               //Frame info
               int frame_dpn = inputFrame->display_picture_number;
               int frame_cpn = inputFrame->coded_picture_number;
               int frame_pts = inputFrame->pts;

               //Decode frame
               sws_scale(conversion, inputFrame->data, inputFrame->linesize, 0, height, &bufferMatImage.data, cvLinesizes);

               /*
                  std::cout <<"/////////////////////////" << std::endl;
                  std::cout <<"POS = " << bpos << std::endl;
                  std::cout <<"PTS = " << pts << std::endl;
                  std::cout <<"DTS = " << dts << std::endl;
                  std::cout <<"DPN = " << frame_dpn << std::endl;
                  std::cout <<"CPN = " << frame_cpn << std::endl;
                  std::cout <<"StreamIndex = " << streamIndex << std::endl;
                  */

               cv::imshow("DEBUG",bufferMatImage);
               if(last_dts>=dts){
                  std::cout << "Something is weird in the order" << std::endl;
                  //cv::waitKey(0);
                  //TODO : consider the case properly
                  return -1;
               } else {
                  //dtsVector.push_back(dts);
                  dtsVector.push_back(frame_pts);
                  cv::waitKey(10);
               }

               //Update last dts
               last_dts=dts;

               //Unref frame
               numberOfFrames++; 
               av_frame_unref(inputFrame);

            }

            //Unref packet
            av_packet_unref(inputPacket);

         }

         sws_freeContext(conversion);

      } else {
         std::cout <<"Audio or other format" << std::endl;
      }

   }

   //If no video were found, return an error
   if(videoStream==-1){
      std::cout << "No video stream were found" << std::endl;
      return -1; // Didn't find a video stream
   }

   std::cout << "Number of frames : " << numberOfFrames << std::endl;

   pt::ptree dtsNode;
   for(size_t k=0; k<dtsVector.size(); k++){

      // Create an unnamed node containing the value
      pt::ptree valueNode;
      valueNode.put("", dtsVector.at(k));

      // Add this node to the list.
      dtsNode.push_back(std::make_pair("", valueNode));
   }
   oroot.add_child("PTSvalues", dtsNode);

   pt::write_json(outputJsonPath, oroot );

   /*
      std::cout <<"Test seek frame : begin"<<std::endl;

   //Conversion
   SwsContext* conversionSeekFrame = sws_getContext(inputFormatContext->streams[0]->codecpar->width, inputFormatContext->streams[0]->codecpar->height, (AVPixelFormat) inputFormatContext->streams[0]->codecpar->format,
   inputFormatContext->streams[0]->codecpar->width, inputFormatContext->streams[0]->codecpar->height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

   //Buffer image
   cv::Mat bufferMatImage = cv::Mat::zeros(inputFormatContext->streams[0]->codecpar->height,inputFormatContext->streams[0]->codecpar->width,CV_8UC3);
   int cvLinesizes[1];
   cvLinesizes[0] = bufferMatImage.step1();

   std::cout << "SIZE VECTOR = " << dtsVector.size() << std::endl;
   for(size_t k=0; k<dtsVector.size(); k++){

   std::cout << "K=" << k << std::endl;

   if(av_seek_frame(inputFormatContext,0, dtsVector.at(k), AVSEEK_FLAG_BACKWARD)>=0){

   if(av_read_frame(inputFormatContext, inputPacket) >= 0){

   av_init_packet(inputPacket);

   ret = avcodec_send_packet(inputCodecContext, inputPacket);
   if(ret < 0)
   {
   std::cout << "Error supplying raw packet data to decoder. " << std::endl;
   return -1;
   }

   while(ret >= 0)
   {
   ret = avcodec_receive_frame(inputCodecContext, inputFrame);

   if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
   {
   std::cout << "WUT" << std::endl;
   break;
   }
   else if(ret < 0)
   {
   std::cout << "Error receiving data from decoder." << std::endl;
   return -1;
   }

   //Decode frame
   sws_scale(conversionSeekFrame, inputFrame->data, inputFrame->linesize, 0, inputFormatContext->streams[0]->codecpar->height, &bufferMatImage.data, cvLinesizes);


   std::cout << "DTS ASKED = " << dtsVector.at(k) << std::endl;
   std::cout << "DTS SEEK = " << inputPacket->dts << std::endl;
   std::cout << "KEYFRAME = " << inputFrame->key_frame << std::endl;

   cv::imshow("DEBUGBIS",bufferMatImage);
   cv::waitKey(0);

   }

   } else {
   std::cout << "Reading fail" << std::endl;
   }

   } else {

   std::cout << "Seeking fail" << std::endl;

   }

   }

   std::cout << "Ending OK" << std::endl;
   */

      return 0;

}
