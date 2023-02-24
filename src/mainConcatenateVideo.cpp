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

int initializeOutputFileNoPacket(AVFormatContext ** f_outputFormatContext, 
      AVStream ** f_outputStream,
      const AVCodecContext* f_outputCodecContext, 
      const std::string & f_outputFilePath){

   int ret= -1;

   //Context
   avformat_alloc_output_context2(f_outputFormatContext, NULL, NULL, f_outputFilePath.c_str());
   if (!*f_outputFormatContext) {
      std::cout << "Could not deduce outputformat" << std::endl;
      return -1;
   }
   if (!*f_outputFormatContext) {
      return-1;
   }

   //Stream
   *f_outputStream = NULL;
   *f_outputStream = avformat_new_stream(*f_outputFormatContext, NULL);
   if(!*f_outputStream)
   {
      std::cout << "Failed to add new stream to output." << std::endl;
      return -1;
   }

   //Fill the codec parameter of the stream with the one of the codec we use
   avcodec_parameters_from_context((*f_outputStream)->codecpar, f_outputCodecContext);

   //Open output
   ret = avio_open(&(*f_outputFormatContext)->pb, f_outputFilePath.c_str(), AVIO_FLAG_WRITE);
   if(ret < 0)
   {
      std::cout << "Could not open output file: " << f_outputFilePath << std::endl;
      return -1;
   }

   //Write output header
   ret = avformat_write_header(*f_outputFormatContext, NULL);
   if(ret < 0)
   {
      std::cout << "Failed to write header to the output file." << std::endl;
      return -1;
   }

   return 0;

}

int finalizeOutPutWriting(AVFormatContext * f_outputFormatContext,
      AVPacket* f_packet){

   //Write trailer of the video
   av_write_trailer(f_outputFormatContext);

   //Free packet and frame
   av_packet_free(&f_packet);

   //Free context and all stream associated
   avformat_free_context(f_outputFormatContext);

   return 0;

}

int main(int argc, const char* argv[])
{

   std::cout << "Concat video" << std::endl;

   //Output settings
   std::string codecOutputName = "libx265";

   //Print avcodec version
   std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;

   //Path input
   std::string inputVideoFolderPath="/home/richard/data/Test_FFMPEG/split2/";
   std::string outputVideoPath="/home/richard/data/Test_FFMPEG/concat.mp4";
   size_t numberVideos = 59;
   //size_t numberVideos = 61;
   //size_t numberVideos = 2;

   //Init vars
   AVFormatContext* inputFormatContext = NULL;
   AVStream* inputStream = NULL;
   AVCodec* m_codecInput = NULL;
   AVCodecContext* m_codecContext = NULL;
   int inputVideoIndex = -1;

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

   int videoStreamIndex=0;

   int ret = -1;

   AVCodec* m_codecOutput = avcodec_find_encoder_by_name(codecOutputName.c_str());
   if(!m_codecOutput){
      std::cout << "Cannot find codec for the output" << std::endl;
      return -1;
   }

   AVCodecContext* m_outputCodecContext = avcodec_alloc_context3(m_codecOutput);
   if(!m_outputCodecContext)
   {
      std::cout << "Cannot initialise codec context for the output" << std::endl;
      return -1;
   }

   AVFormatContext * m_outputFormatContext;
   AVStream* m_outputStream;

   for(size_t nbVideo=0; nbVideo<numberVideos; nbVideo++){

      //Input Path
      std::string inputFilePath = inputVideoFolderPath+std::to_string(nbVideo)+".mp4";

      std::cout << "Opening = " << inputFilePath << std::endl;

      //Open file
      ret = avformat_open_input(&inputFormatContext, inputFilePath.c_str(), NULL, NULL);
      if(ret < 0)
      {
         std::cout << "Could not open input file: \"" << inputFilePath << "\"" << std::endl;
         return -1;
      }

      //std::cout << "After open input " << std::endl;

      ret = avformat_find_stream_info(inputFormatContext, NULL);
      if(ret < 0)
      {
         std::cout << "Could not retrieve input stream information." << std::endl;
         return -1;
      }

      //std::cout << "After find stream info " << std::endl;

      if(!inputFormatContext->streams[videoStreamIndex]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
         std::cout << "Main stream is not a video stream : error" << std::endl;
         return -1;
      }

      //Get input stream
      inputStream = inputFormatContext->streams[videoStreamIndex];

      //std::cout << "After get stream " << std::endl;

      //TODO carefull here 1 instead of 1
      if(nbVideo==0){

         //Get decoder
         m_codecInput = avcodec_find_decoder(inputStream->codecpar->codec_id);
         if(!m_codecInput)
         {
            std::cout << "No decoder for the codec: " << inputStream->codecpar->codec_id << std::endl;
            return -1;
         }

         //Create codec context
         m_codecContext = avcodec_alloc_context3(m_codecInput);
         if(avcodec_parameters_to_context(m_codecContext, inputStream->codecpar) < 0)
         {
            std::cout << "Failed to fill the codec context" << std::endl;
            return -1;
         }

         //Initialize codec context
         ret = avcodec_open2(m_codecContext, m_codecInput, NULL);
         if(ret < 0)
         {
            std::cout << "Failed to initialize the codec context." << std::endl;
            return -1;
         }

         //Set vars from input codec
         m_outputCodecContext->height = m_codecContext->height;
         m_outputCodecContext->width = m_codecContext->width;
         m_outputCodecContext->sample_aspect_ratio = m_codecContext->sample_aspect_ratio;
         m_outputCodecContext->bit_rate = m_codecContext->bit_rate;
         m_outputCodecContext->rc_buffer_size = m_codecContext->rc_buffer_size;
         m_outputCodecContext->rc_max_rate = m_codecContext->rc_max_rate;
         m_outputCodecContext->rc_min_rate = m_codecContext->rc_min_rate;
         m_outputCodecContext->gop_size = 10;//Gap between KF
         m_outputCodecContext->max_b_frames = 1;//Nb of B frames in the interval
         //m_outputCodecContext->time_base = (AVRational){inputStream->time_base.num, inputStream->time_base.den};
         m_outputCodecContext->time_base = (AVRational){1, 90000};
         m_outputCodecContext->pix_fmt = m_codecContext->pix_fmt;
         m_outputCodecContext->framerate = inputStream->avg_frame_rate;
         if(m_codecOutput->pix_fmts)
         {
            if(m_codecOutput->pix_fmts[0] == m_codecContext->pix_fmt){
               std::cout << "OK pixmft initialized" << std::endl;
            } else {
               std::cout << "Issue while initializing pixmft : error." << std::endl;
               return -1;
            }
         }

         AVDictionary *m_encoderOpts = NULL;           // "create" an empty dictionary
         av_dict_set(&m_encoderOpts, "x265-params", "lossless=1", 0);

         ret = avcodec_open2(m_outputCodecContext, m_codecOutput, &m_encoderOpts);
         if(ret < 0)
         {
            std::cout << "Could not open the request codec." << std::endl;
            return -1;
         }

         if(initializeOutputFileNoPacket(&m_outputFormatContext,
                  &m_outputStream,
                  m_outputCodecContext,
                  outputVideoPath
                  )<0){
            std::cout << "Issue while initializing : error" << std::endl;
            return -1;
         }

      }


      //Read frames
      while(av_read_frame(inputFormatContext, inputPacket) >= 0){

         //Rescale TS packet
         av_packet_rescale_ts(inputPacket, (AVRational){1, 90000}, (AVRational){1, 90});

         //Write packet to output
         ret = av_interleaved_write_frame(m_outputFormatContext, inputPacket);
         if(ret < 0)
         {
            std::cout << "Failed to write a packet to the output file." << std::endl;
            return -1;
         }

         //Unref packet
         av_packet_unref(inputPacket);

      }

      //Close input
      avformat_close_input(&inputFormatContext);

   }

   //Missing flush TODO??

   //Finalize the last sequence of video
   finalizeOutPutWriting(m_outputFormatContext, inputPacket);


   return 0;

}
