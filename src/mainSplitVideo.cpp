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

int initializeOutputFile(AVFormatContext ** f_outputFormatContext, 
      AVStream ** f_outputStream,
      const AVCodecContext* f_outputCodecContext, 
      AVPacket ** f_outputPacket, const std::string & f_outputFilePath){

   int ret= -1;

   *f_outputPacket = NULL;
   *f_outputPacket= av_packet_alloc();
   if(!*f_outputPacket)
   {
      std::cout << "Cannot allocate output packet" << std::endl;
      return -1;
   }

   //Context
   *f_outputFormatContext = NULL;
   avformat_alloc_output_context2(f_outputFormatContext, NULL, NULL, f_outputFilePath.c_str());
   if (!*f_outputFormatContext) {
      std::cout << "Could not deduce outputformat" << std::endl;
      return -1;
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
   ret = avio_open(&((*f_outputFormatContext)->pb), f_outputFilePath.c_str(), AVIO_FLAG_WRITE);
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

int addFrameToOutput(AVFormatContext * f_outputFormatContext,
      AVCodecContext* f_outputCodecContext,
      AVPacket* f_outputPacket, 
      const AVRational & f_requestedFrameRate, const AVRational & f_streamUsedTimeBase,
      const AVStream* f_outputStream,
      AVFrame* f_frame){

   int ret = -1;

   ret = avcodec_send_frame(f_outputCodecContext, f_frame);
   
   while(ret >= 0)
   {

      ret = avcodec_receive_packet(f_outputCodecContext, f_outputPacket);
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
      f_outputPacket->stream_index = 0;

      //TODO shall check packet duration
      f_outputPacket->duration = (f_outputStream->time_base.den * f_requestedFrameRate.den) / (f_outputStream->time_base.num * f_requestedFrameRate.num );

      //Update TS for the output packet
      av_packet_rescale_ts(f_outputPacket, f_streamUsedTimeBase, f_outputStream->time_base);

      if(f_outputFormatContext==NULL){
         std::cout << "Format Context is null" << std::endl;
      }

      //Write packet
      ret = av_interleaved_write_frame(f_outputFormatContext, f_outputPacket);
      if(ret < 0)
      {
         std::cout << "Failed to write a packet to the output file." << std::endl;
         return -1;
      }

   }

   return 0;

}

int finalizeOutPutWriting(AVFormatContext * f_outputFormatContext,
      AVCodecContext* f_outputCodecContext,
      const AVRational & f_requestedFrameRate, const AVRational & f_streamUsedTimeBase,
      const AVStream* f_outputStream,
      AVPacket* f_outputPacket){

   //TODO flush ??
   std::cout << "BEFORE FLUSH" << std::endl;
   addFrameToOutput(f_outputFormatContext,
         f_outputCodecContext,
         f_outputPacket,
         f_requestedFrameRate,f_streamUsedTimeBase,
         f_outputStream,
         NULL);

   std::cout << "AFTER FLUSH" << std::endl;

   //Write trailer of the video
   av_write_trailer(f_outputFormatContext);

   //Free packet and frame
   av_packet_free(&f_outputPacket);

   //Free context and all stream associated
   avformat_free_context(f_outputFormatContext);

   return 0;

}

int initializeOutputCodec(const AVCodec * f_codecOutput,
      AVCodecContext** f_outputCodecContext,
      const AVCodecContext* f_inputCodecContext,
      const AVRational & f_requestedFrameRate, const AVRational & f_streamUsedTimeBase){

   *f_outputCodecContext = avcodec_alloc_context3(f_codecOutput);
   if(!f_outputCodecContext)
   {
      std::cout << "Cannot initialise codec context for the output" << std::endl;
      return -1;
   }

   //Set vars from input codec
   (*f_outputCodecContext)->height = f_inputCodecContext->height;
   (*f_outputCodecContext)->width = f_inputCodecContext->width;
   (*f_outputCodecContext)->sample_aspect_ratio = f_inputCodecContext->sample_aspect_ratio;
   (*f_outputCodecContext)->bit_rate = f_inputCodecContext->bit_rate;
   (*f_outputCodecContext)->rc_buffer_size = f_inputCodecContext->rc_buffer_size;
   (*f_outputCodecContext)->rc_max_rate = f_inputCodecContext->rc_max_rate;
   (*f_outputCodecContext)->rc_min_rate = f_inputCodecContext->rc_min_rate;
   (*f_outputCodecContext)->gop_size = 10;//Gap between KF
   (*f_outputCodecContext)->max_b_frames = 1;//Nb of B frames in the interval
   (*f_outputCodecContext)->time_base = f_streamUsedTimeBase;
   (*f_outputCodecContext)->pix_fmt = f_inputCodecContext->pix_fmt;
   (*f_outputCodecContext)->framerate = f_requestedFrameRate;

   /*
   std::cout << "height = " << f_inputCodecContext->height << std::endl;
   std::cout << "width = " << f_inputCodecContext->width << std::endl;
   std::cout << "aspect ration = " << f_inputCodecContext->sample_aspect_ratio.num << " / " << f_inputCodecContext->sample_aspect_ratio.den << std::endl;
   std::cout << "br = " << f_inputCodecContext->bit_rate << std::endl;
   std::cout << "buffer size = " << f_inputCodecContext->rc_buffer_size << std::endl;
   std::cout << "rc_max_rate = " << f_inputCodecContext->rc_max_rate << std::endl;
   std::cout << "rc_min_rate = " << f_inputCodecContext->rc_min_rate << std::endl;
   std::cout << "time base = " << f_streamUsedTimeBase.num << " / " << f_streamUsedTimeBase.den << std::endl;
   std::cout << "pixfmt = " << f_inputCodecContext->pix_fmt << std::endl;
   if(f_inputCodecContext->pix_fmt == AV_PIX_FMT_YUV420P){
      std::cout << "OK format = AV_PIX_FMT_YUV420P" <<std::endl;
   } else {
      std::cout << "ERROR pixfmt is not correct" << std::endl;
      exit(0);
   }
   */

   if(f_codecOutput->pix_fmts)
   {
      if(f_codecOutput->pix_fmts[0] == (*f_outputCodecContext)->pix_fmt){
         std::cout << "OK pixmft initialized" << std::endl;
      } else {
         std::cout << "Issue while initializing pixmft : error." << std::endl;
         return -1;
      }
   } else {
      std::cout <<"WTFFF"<< std::endl;
   }
   AVDictionary *f_encoderOpts = NULL;           // "create" an empty dictionary
   av_dict_set(&f_encoderOpts, "x265-params", "lossless=1", 0);

   int ret = avcodec_open2(*f_outputCodecContext, f_codecOutput, &f_encoderOpts);
   if(ret < 0)
   {
      std::cout << "Could not open the request codec." << std::endl;
      return -1;
   }

   //Dictionnary is not used anymore
   av_dict_free(&f_encoderOpts);

   return 0;

}



int addFrameToFolderOutputs(int & f_numFrame, int f_maxFrame, int & f_currentbatch,
      const std::string & f_folderOutput, 
      const AVCodecContext* f_inputCodecContext,
      AVFormatContext ** f_outputFormatContext,
      const AVCodec* f_codecOutput,
      AVCodecContext** f_outputCodecContext,
      AVPacket** f_outputPacket,
      const AVRational & f_requestedFrameRate, const AVRational & f_streamUsedTimeBase,
      AVStream** f_outputStream,
      AVFrame* f_frame){

   if(f_numFrame>= f_maxFrame){

      if(finalizeOutPutWriting(*f_outputFormatContext, 
               *f_outputCodecContext,
               f_requestedFrameRate, f_streamUsedTimeBase,
               *f_outputStream,
               *f_outputPacket)<0){

         std::cout << "Something went wrong when closing output file" << std::endl;
         return -1;

      }

      //Free codec context
      avcodec_free_context(f_outputCodecContext);

      f_numFrame=0;
      f_currentbatch++;

   }

   if(f_numFrame==0){

      std::string outputPath = f_folderOutput+std::to_string(f_currentbatch)+".mp4";

      if(initializeOutputCodec(f_codecOutput,
               f_outputCodecContext,
               f_inputCodecContext,
               f_requestedFrameRate, f_streamUsedTimeBase)<0){
         std::cout << "Something went wrong with output codec initialization" << std::endl;
         return -1;
      }

      if(initializeOutputFile(f_outputFormatContext, 
               f_outputStream, 
               *f_outputCodecContext, 
               f_outputPacket, 
               outputPath)<0){
         std::cout << "Something went wrong while initializing outputFile" << std::endl;
         return -1;
      }

   }

   if(addFrameToOutput(*f_outputFormatContext,
            *f_outputCodecContext,
            *f_outputPacket,
            f_requestedFrameRate, f_streamUsedTimeBase,
            *f_outputStream,
            f_frame)<0){

      std::cout << "Something went wrong while adding frame" << std::endl;
      return -1;

   }

   //Increase the number of frame
   f_numFrame++;

   return 0;

}

int main(int argc, const char* argv[])
{

   std::cout << "Splitting video" << std::endl;

   namespace pt = boost::property_tree;

   //Output settings
   std::string codecOutputName = "libx265";

   //Json struture
   pt::ptree oroot;

   //Print avcodec version
   std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;

   //Path input
   std::string inputFilePath="/home/richard/data/Test_FFMPEG/test_video.mp4";
   std::string outputJsonPath="/home/richard/data/Test_FFMPEG/recap.json";
   std::string outputVideoFolderPath="/home/richard/data/Test_FFMPEG/split2/";
   AVRational m_usedTimeBasedPreEncoding = (AVRational){1, 90};

   //Check format
   if(inputFilePath.find_last_of('.') == std::string::npos)
   {
      std::cout << "Issue with input format" << std::endl;
      return -1;
   }

   //Init vars
   AVFormatContext* inputFormatContext = NULL;
   AVStream* inputStream = NULL;
   AVCodec* m_codecInput = NULL;
   AVCodecContext* m_codecContext = NULL;
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
   int64_t last_pts = -1;

   //Check the video streams
   int videoStreamIndex=-1;
   //Loop over the streams declared in the file
   for(int i=0; i<inputFormatContext->nb_streams; i++){

      std::cout << "Index = " << i << std::endl;

      if(inputFormatContext->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {
         videoStreamIndex=i;
         break;
      }

   }

   //Get input stream
   inputStream = inputFormatContext->streams[videoStreamIndex];

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

   //Initialize sample scaler
   int width = inputStream->codecpar->width;
   int height = inputStream->codecpar->height;

   //TODO MOVE
   AVCodec* m_codecOutput = avcodec_find_encoder_by_name(codecOutputName.c_str());
   if(!m_codecOutput){
      std::cout << "Cannot find codec for the output" << std::endl;
      return -1;
   }
   AVCodecContext* m_outputCodecContext;

   //Frame and packets
   AVPacket* m_outputPacket;
   AVFrame* m_outputFrame;
   AVFormatContext * m_outputFormatContext;
   AVStream* m_outputStream;

   //Buffer image
   cv::Mat bufferMatImage = cv::Mat::zeros(height,width,CV_8UC3);
   int cvLinesizes[1];
   cvLinesizes[0] = bufferMatImage.step1();

   //Conversion
   SwsContext* conversion = sws_getContext(width, height, (AVPixelFormat) inputStream->codecpar->format, width, height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

   bool noisyDebug=true;

   int maxFramesOutputVideo = 100;
   int indexFrame = 0;
   int indexVideo = 0;

   size_t ID_FramesConsidered = 0;

   //Read frames
   while(av_read_frame(inputFormatContext, inputPacket) >= 0){

      //get packet
      ret = avcodec_send_packet(m_codecContext, inputPacket);
      if(ret < 0)
      {
         std::cout << "Error supplying raw packet data to decoder. " << std::endl;
         return -1;
      }

      //Check stream type
      if(inputPacket->stream_index==videoStreamIndex){

         //OK

      } else {
         if(noisyDebug){
            std::cout << "Not a video packet" << std::endl;
         }
         av_packet_unref(inputPacket);
         continue;
      }

      while(ret >= 0)
      {
         ret = avcodec_receive_frame(m_codecContext, inputFrame);

         if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
         {
            break;
         }
         else if(ret < 0)
         {
            std::cout << "Error receiving data from decoder." << std::endl;
            return -1;
         }

         int64_t pts_frame = inputFrame->pts;

         //Decode frame
         sws_scale(conversion, inputFrame->data, inputFrame->linesize, 0, height, &bufferMatImage.data, cvLinesizes);

         //Add frame to output 
          addFrameToFolderOutputs(indexFrame, maxFramesOutputVideo, indexVideo, outputVideoFolderPath,
               m_codecContext,
               &m_outputFormatContext,
               m_codecOutput,
               &m_outputCodecContext,
               &m_outputPacket,
               inputStream->avg_frame_rate, m_usedTimeBasedPreEncoding,
               &m_outputStream,
               inputFrame);
         /*
         addFrameToFolderOutputs(indexFrame, maxFramesOutputVideo, indexVideo, outputVideoFolderPath,
               m_codecContext,
               &m_outputFormatContext,
               m_codecOutput,
               &m_outputCodecContext,
               &m_outputPacket,
               inputStream->avg_frame_rate, inputStream->time_base,
               &m_outputStream,
               inputFrame);
               */

         cv::imshow("DEBUG",bufferMatImage);
         if(last_pts>=pts_frame){
            std::cout << "Something is weird in the order" << std::endl;
            return -1;
         } else {
            //cv::waitKey(10);
            cv::waitKey(10);
         }

         //Update last dts
         last_pts=pts_frame;
         ID_FramesConsidered++;

         av_frame_unref(inputFrame);

      }

      //Unref packet
      av_packet_unref(inputPacket);

   }

   //Finalize the last sequence of video
   finalizeOutPutWriting(m_outputFormatContext,
         m_outputCodecContext,
         inputStream->avg_frame_rate, m_usedTimeBasedPreEncoding,
         m_outputStream,
         m_outputPacket);

   sws_freeContext(conversion);

   return 0;

}
