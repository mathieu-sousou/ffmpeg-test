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

   std::cout << "Debug timestamp video" << std::endl;

   int ret = -1;

   //Output settings
   std::string codecOutputName = "libx265";

   //Print avcodec version
   std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;

   //Path input
   std::string outputFilePath="/home/richard/data/Test_FFMPEG/test_output.mp4";

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

   //Set vars from input codec
   m_outputCodecContext->height = 1080;
   m_outputCodecContext->width = 1920;
   m_outputCodecContext->sample_aspect_ratio = (AVRational){1, 1};
   m_outputCodecContext->bit_rate = 222305996;
   m_outputCodecContext->rc_buffer_size = 0;
   m_outputCodecContext->rc_max_rate = 0;
   m_outputCodecContext->rc_min_rate = 0;
   m_outputCodecContext->gop_size = 10;//Gap between KF
   m_outputCodecContext->max_b_frames = 1;//Nb of B frames in the interval
   m_outputCodecContext->time_base = (AVRational){1, 90000};
   m_outputCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
   m_outputCodecContext->framerate = (AVRational){548730000, 21928171};

   AVDictionary *m_encoderOpts = NULL;           // "create" an empty dictionary
   av_dict_set(&m_encoderOpts, "x265-params", "lossless=1", 0);

   ret = avcodec_open2(m_outputCodecContext, m_codecOutput, &m_encoderOpts);
   if(ret < 0)
   {
      std::cout << "Could not open the request codec." << std::endl;
      return -1;
   }

   //Dictionnary is not used anymore
   av_dict_free(&m_encoderOpts);

   AVPacket * m_outputPacket = NULL;
   m_outputPacket= av_packet_alloc();
   if(!m_outputPacket)
   {
      std::cout << "Cannot allocate output packet" << std::endl;
      return -1;
   }

   AVFormatContext * m_outputFormatContext = NULL;
   avformat_alloc_output_context2(&m_outputFormatContext, NULL, NULL, outputFilePath.c_str());
   if (!m_outputFormatContext) {
      std::cout << "Could not deduce outputformat" << std::endl;
      return -1;
   }

   AVStream * m_outputStream = NULL;
   m_outputStream = avformat_new_stream(m_outputFormatContext, NULL);
   if(!m_outputStream)
   {
      std::cout << "Failed to add new stream to output." << std::endl;
      return -1;
   }

   avcodec_parameters_from_context(m_outputStream->codecpar, m_outputCodecContext);

   ret = avio_open(&(m_outputFormatContext->pb), outputFilePath.c_str(), AVIO_FLAG_WRITE);
   if(ret < 0)
   {
      std::cout << "Could not open output file: " << outputFilePath << std::endl;
      return -1;
   }

   ret = avformat_write_header(m_outputFormatContext, NULL);
   if(ret < 0)
   {
      std::cout << "Failed to write header to the output file." << std::endl;
      return -1;
   }

   //End initialization

   //TODO frames

   int64_t ptsTest = 401594;

   for(size_t k=0; k<100; k++){

      std::cout << "K = " << k << std::endl;

      AVFrame *frameToEncode = NULL;
      frameToEncode = av_frame_alloc();
      if (!frameToEncode) {
         //Exit? TODO
         std::cout<<"Could not allocate video frame"<< std::endl;
         exit(1);
      }
      frameToEncode->format = AV_PIX_FMT_YUV420P;
      frameToEncode->width  = 1920;
      frameToEncode->height = 1080;

      frameToEncode->pts = ptsTest;

      ret = av_frame_get_buffer(frameToEncode, 0);
      if (ret < 0) {
         fprintf(stderr, "Could not allocate the video frame data\n");
         exit(1);
      }

      ret = avcodec_send_frame(m_outputCodecContext, frameToEncode);
      while(ret >= 0)
      {

         //std::cout << "OK?" << std::endl;

         ret = avcodec_receive_packet(m_outputCodecContext, m_outputPacket);
         if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
         {
            break;
         }
         else if(ret < 0)
         {
            std::cout << "Error receiving packet from encoder." << std::endl;
            return -1;
         }

         //Maybe not hard coded : shall be equal to the one of the output video stream created
         m_outputPacket->stream_index = 0;

         //TODO shall check packet duration
         m_outputPacket->duration = (m_outputStream->time_base.den * 21928171) / (m_outputStream->time_base.num * 548730000 );

         //Update TS for the output packet TODO continue
         av_packet_rescale_ts(m_outputPacket, (AVRational){1, 90000}, m_outputStream->time_base);

         //Write packet
         ret = av_interleaved_write_frame(m_outputFormatContext, m_outputPacket);
         if(ret < 0)
         {
            std::cout << "Failed to write a packet to the output file." << std::endl;
            return -1;
         }

      }

      ptsTest+=17;

   }

   //Flush
   ret = avcodec_send_frame(m_outputCodecContext, NULL);
   while(ret >= 0)
   {

      ret = avcodec_receive_packet(m_outputCodecContext, m_outputPacket);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      {
         break;
      }
      else if(ret < 0)
      {
         std::cout << "Error receiving packet from encoder." << std::endl;
         return -1;
      }

      //Maybe not hard coded : shall be equal to the one of the output video stream created
      m_outputPacket->stream_index = 0;

      //TODO shall check packet duration
      m_outputPacket->duration = (m_outputStream->time_base.den * 21928171) / (m_outputStream->time_base.num * 548730000 );

      //Update TS for the output packet TODO continue
      av_packet_rescale_ts(m_outputPacket, (AVRational){1, 90000}, m_outputStream->time_base);

      //Write packet
      ret = av_interleaved_write_frame(m_outputFormatContext, m_outputPacket);
      //ret = av_write_frame(f_outputFormatContext, f_outputPacket);
      if(ret < 0)
      {
         std::cout << "Failed to write a packet to the output file." << std::endl;
         return -1;
      }

   }

   //Close file

   //Write trailer of the video
   av_write_trailer(m_outputFormatContext);

   //Free packet and frame
   av_packet_free(&m_outputPacket);

   //Free context and all stream associated
   avformat_free_context(m_outputFormatContext);


   return 0;

}
