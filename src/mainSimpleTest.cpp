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

   std::cout << "Test FFMPEG : Reverse" << std::endl;

   //Print avcodec version
   std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;

   //Path input
   //std::string inputFilePath="/home/richard/Downloads/Video_011.mpg";
   std::string inputFilePath="/home/richard/data/Test_FFMPEG/split/0.mp4";

   /*
      std::string inputJsonPath="/home/richard/Downloads/PTSValues.json";

      namespace pt = boost::property_tree;

   // Create a root
   pt::ptree iroot;

   // Load the json file in this ptree
   pt::read_json(inputJsonPath, iroot);

   //Extract dts values
   std::vector<int64_t> ptsVector;
   for (pt::ptree::value_type &ptsValue : iroot.get_child("PTSvalues"))
   {
   ptsVector.push_back(std::stoi(ptsValue.second.data()));
   }
   */

   //Check format
   if(inputFilePath.find_last_of('.') == std::string::npos)
   {
      std::cout << "Issue with input format" << std::endl;
      return -1;
   }

   //Init vars
   AVFormatContext* m_formatContext = NULL;
   AVStream* m_stream = NULL;
   AVCodec* m_codec = NULL;
   AVCodecContext* m_codecContext = NULL;
   int videoStreamIndex = -1;

   //Open file
   int ret = avformat_open_input(&m_formatContext, inputFilePath.c_str(), NULL, NULL);
   if(ret < 0)
   {
      std::cout << "Could not open input file: \"" << inputFilePath << "\"" << std::endl;
      return -1;
   }
   ret = avformat_find_stream_info(m_formatContext, NULL);
   if(ret < 0)
   {
      std::cout << "Could not retrieve input stream information." << std::endl;
      return -1;
   }

   //Stream info
   for(int i=0; i<m_formatContext->nb_streams; i++){

      if(m_formatContext->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {

         std::cout << "Checked index " << i << " is a video index."  << std::endl;
         videoStreamIndex=i;
      } else {
         std::cout << "Checked index " << i << " is not a video index."  << std::endl;
      }

   }

   std::cout << "Video stream found = " << videoStreamIndex << std::endl;

   //Init stream
   m_stream = m_formatContext->streams[videoStreamIndex];

   //Init packets and frames vars
   AVFrame* m_frame = NULL;
   AVPacket* m_packet = NULL;

   m_frame = av_frame_alloc();
   if(!m_frame)
   {
      return -1;
   }
   m_packet = av_packet_alloc();
   if(!m_packet)
   {
      return -1;
   }

   //Get decoder
   m_codec = avcodec_find_decoder(m_stream->codecpar->codec_id);
   if(!m_codec)
   {
      std::cout << "No decoder for the codec: " << m_stream->codecpar->codec_id << std::endl;
      return -1;
   }

   //Create codec context
   m_codecContext = avcodec_alloc_context3(m_codec);
   if(avcodec_parameters_to_context(m_codecContext, m_stream->codecpar) < 0)
   {
      std::cout << "Failed to fill the codec context" << std::endl;
      return -1;
   }

   //Initialize codec context
   ret = avcodec_open2(m_codecContext, m_codec, NULL);
   if(ret < 0)
   {
      std::cout << "Failed to initialize the codec context." << std::endl;
      return -1;
   }

   //Conversion
   /*
      SwsContext* conversionSeekFrame = sws_getContext(m_stream->codecpar->width, m_stream->codecpar->height,
      (AVPixelFormat) m_stream->codecpar->format,
      m_stream->codecpar->width, m_stream->codecpar->height,
      AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
      */

   //Buffer image
   cv::Mat bufferMatImage = cv::Mat::zeros(m_stream->codecpar->height, m_stream->codecpar->width,CV_8UC3);
   int cvLinesizes[1];
   cvLinesizes[0] = bufferMatImage.step1();

   //std::cout << "SIZE VECTOR PTS= " << ptsVector.size() << std::endl;

   int64_t pts_packet = -1;
   int64_t dts_packet = -1;

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   /*
   //TODO here debug starting from one !!!! remove and zero instead
   for(size_t k=0; k<ptsVector.size(); k++){

   int64_t pts_target = ptsVector.at(k);
   std::cout << "Seeking frame n = " << pts_target << std::endl;

   //AVSEEK
   ret = av_seek_frame(m_formatContext, -1, pts_target, AVFMT_SEEK_TO_PTS | AVSEEK_FLAG_BACKWARD);
   if (ret < 0){
   std::cout<<"Something went wrong with seeking" << std::endl;
   return -1;
   }
   avcodec_flush_buffers(m_codecContext);


   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
   std::cout<<"Something went wrong with reading packet" << std::endl;
   return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
   std::cout << "OK : video packet" << std::endl;
   } else {
   std::cout << "Not a video packet" << std::endl;
   av_packet_unref(m_packet);
   }

   */

   //AVSEND
   ret = avcodec_send_packet(m_codecContext, m_packet);
   if(ret < 0)
   {
   std::cout << "Error supplying raw packet data to decoder. " << std::endl;
   return -1;
   }
   pts_packet = m_packet->pts;
   dts_packet = m_packet->dts;
   std::cout << "PTS packet = " << pts_packet << std::endl;
   std::cout << "DTS packet = " << dts_packet << std::endl;

   /*
   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
   std::cout << "Not enough data : continuing decoding" << std::endl;
   av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
   std::cout << "EOF" << std::endl;
   } else {
   if(ret<0){
   std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
   return -1;
   } else {
   std::cout << "OK image found" << std::endl;
   //Print PTS
   std::cout << "Frame PTS = " << m_frame->pts << std::endl;
   }
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
   std::cout<<"Something went wrong with reading packet" << std::endl;
   return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
   std::cout << "OK : video packet" << std::endl;
   } else {
   std::cout << "Not a video packet" << std::endl;
   av_packet_unref(m_packet);
   }

   //AVSEND
   ret = avcodec_send_packet(m_codecContext, m_packet);
   if(ret < 0)
   {
      std::cout << "Error supplying raw packet data to decoder. " << std::endl;
      return -1;
   }
   pts_packet = m_packet->pts;
   dts_packet = m_packet->dts;
   std::cout << "PTS packet = " << pts_packet << std::endl;
   std::cout << "DTS packet = " << dts_packet << std::endl;

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVSEND
   ret = avcodec_send_packet(m_codecContext, m_packet);
   if(ret < 0)
   {
      std::cout << "Error supplying raw packet data to decoder. " << std::endl;
      return -1;
   }
   pts_packet = m_packet->pts;
   dts_packet = m_packet->dts;
   std::cout << "PTS packet = " << pts_packet << std::endl;
   std::cout << "DTS packet = " << dts_packet << std::endl;

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVSEND
   ret = avcodec_send_packet(m_codecContext, m_packet);
   if(ret < 0)
   {
      std::cout << "Error supplying raw packet data to decoder. " << std::endl;
      return -1;
   }
   pts_packet = m_packet->pts;
   dts_packet = m_packet->dts;
   std::cout << "PTS packet = " << pts_packet << std::endl;
   std::cout << "DTS packet = " << dts_packet << std::endl;

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVSEND
   ret = avcodec_send_packet(m_codecContext, m_packet);
   if(ret < 0)
   {
      std::cout << "Error supplying raw packet data to decoder. " << std::endl;
      return -1;
   }
   pts_packet = m_packet->pts;
   dts_packet = m_packet->dts;
   std::cout << "PTS packet = " << pts_packet << std::endl;
   std::cout << "DTS packet = " << dts_packet << std::endl;

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVSEND
   ret = avcodec_send_packet(m_codecContext, m_packet);
   if(ret < 0)
   {
      std::cout << "Error supplying raw packet data to decoder. " << std::endl;
      return -1;
   }
   pts_packet = m_packet->pts;
   std::cout << "PTS packet = " << pts_packet << std::endl;

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVSEND
   ret = avcodec_send_packet(m_codecContext, m_packet);
   if(ret < 0)
   {
      std::cout << "Error supplying raw packet data to decoder. " << std::endl;
      return -1;
   }
   pts_packet = m_packet->pts;
   std::cout << "PTS packet = " << pts_packet << std::endl;

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVREAD
   ret = av_read_frame(m_formatContext, m_packet);
   if (ret<0){
      std::cout<<"Something went wrong with reading packet" << std::endl;
      return -1;
   }
   if(m_packet->stream_index==videoStreamIndex){
      std::cout << "OK : video packet" << std::endl;
   } else {
      std::cout << "Not a video packet" << std::endl;
      av_packet_unref(m_packet);
   }

   //AVSEND
   ret = avcodec_send_packet(m_codecContext, m_packet);
   if(ret < 0)
   {
      std::cout << "Error supplying raw packet data to decoder. " << std::endl;
      return -1;
   }
   pts_packet = m_packet->pts;
   std::cout << "PTS packet = " << pts_packet << std::endl;

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //AVRECEIVE
   ret = avcodec_receive_frame(m_codecContext, m_frame);
   if (ret == AVERROR(EAGAIN)) {
      std::cout << "Not enough data : continuing decoding" << std::endl;
      av_frame_unref(m_frame);
   } else if (ret == AVERROR_EOF) {
      std::cout << "EOF" << std::endl;
   } else {
      if(ret<0){
         std::cout << "Forbiden return in av_receive_frame : BUG" << std::endl;
         return -1;
      } else {
         std::cout << "OK image found" << std::endl;
         //Print PTS
         std::cout << "Frame PTS = " << m_frame->pts << std::endl;
      }
   }

   //Decode frame
   sws_scale(conversionSeekFrame,
         m_frame->data, 
         m_frame->linesize, 0, m_stream->codecpar->height, 
         &bufferMatImage.data, cvLinesizes);

   //Unref
   //av_frame_unref(m_frame);
   //av_packet_unref(m_packet);

   //Imshow debug
   //cv::imshow("DEBUG",bufferMatImage);
   //cv::waitKey(0);

   break;

}

*/

std::cout << "Ending Test" << std::endl;

return 0;

}
