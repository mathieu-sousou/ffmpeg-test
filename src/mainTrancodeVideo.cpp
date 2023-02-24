#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <numeric>

#include <opencv2/opencv.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <math.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}

int getDecoderAndContextFromStream(AVStream* f_stream,
      AVCodec** f_codec, AVCodecContext** f_codecContext){

   //Get decoder
   *f_codec = avcodec_find_decoder(f_stream->codecpar->codec_id);
   if(!*f_codec)
   {
      std::cout << "No decoder for the codec: " << f_stream->codecpar->codec_id << std::endl;
      return -1;
   }

   //Create codec context
   *f_codecContext = avcodec_alloc_context3(*f_codec);
   if(avcodec_parameters_to_context(*f_codecContext, f_stream->codecpar) < 0)
   {
      std::cout << "Failed to fill the codec context" << std::endl;
      return -1;
   }

   //Initialize codec context
   if(avcodec_open2(*f_codecContext, *f_codec, NULL) < 0)
   {
      std::cout << "Failed to initialize the codec context." << std::endl;
      return -1;
   } else {
      return 0;
   }

}

int getResearchedPacket(AVFormatContext* f_formatContext, AVCodecContext* f_codecContext, 
      int videoStreamIndex, 
      int64_t seekDts,
      AVPacket* f_packet, AVFrame* bufferFrame,
      size_t & nbDataLoaded, size_t & nbVideoDataLoaded,
      bool noisyDebug){

   int dts_packet = -1;
   int ret = -1;
   bool haveKeyFrame=false;

   while(dts_packet<seekDts){

      //AVREAD
      ret = av_read_frame(f_formatContext, f_packet);
      nbDataLoaded++;
      //Issue while calling av_read_frame : several reason possible
      if (ret<0){
         if(noisyDebug){
            std::cout<<"Something went wrong while reading packet (av_read_frame)" << std::endl;
         }
         return -3;
      }

      //Check stream type
      if(f_packet->stream_index==videoStreamIndex){
         if(noisyDebug){
            std::cout << "OK : video packet" << std::endl;
         }
         nbVideoDataLoaded++;
      } else {
         if(noisyDebug){
            std::cout << "Not a video packet" << std::endl;
         }
         av_packet_unref(f_packet);
         continue;
      }

      //AVSEND
      while(true){

         //Send packet
         ret = avcodec_send_packet(f_codecContext, f_packet);

         //If the packet cannot receive more data
         if(ret == AVERROR(EAGAIN)){

            //Frame should be extracted from the packet before sending new data
            ret = avcodec_receive_frame(f_codecContext, bufferFrame);

            //A frame have been correctly extracted (decoded or not)
            if ((ret == AVERROR(EAGAIN)) || (ret>=0)){
               if(noisyDebug){
                  std::cout << "OK : One frame have been removed" << std::endl;
               }
               //Unref the frame : we do not need it
               av_frame_unref(bufferFrame);
               //Try again from the begining to send packet
               continue;
            } else if (ret == AVERROR_EOF) //End of file reached (should not happen)
            {
               std::cout << "End of file detected : Error" << std::endl;
               return -1;
            } else //Undeterminded error with receive_frame
            {
               std::cout << "Undetermined error in getResearchedPacket (receive_frame)" << std::endl;
               return -1;
            }

         } else if (ret< 0) //Undetermined error in send_packet
         {
            std::cout << "Undetermined error supplying raw packet data to decoder. " << std::endl;
            return -1;
         }

         //Update packet dts
         dts_packet = f_packet->dts;
         if(noisyDebug){
            std::cout << "DTS packet = " << dts_packet << std::endl;
         }

         //If there is no video keyframe, this cannot work
         if(f_packet->flags == AV_PKT_FLAG_KEY){
            haveKeyFrame=true;
            if(noisyDebug){
               std::cout << "Video keyframe Detected" << std::endl;
            }
         } else {
            if(noisyDebug){
               std::cout << "No keyframe in the packet" << std::endl;
            }

         }

         break;

      }

   }

   //If there is a keyframe: OK, if not
   if(haveKeyFrame){
      return 0;
   } else {
      return -2;
   }

}

int getResearchedFrame(AVCodecContext* f_codecContext,
      int64_t seekPts,
      AVFrame* f_frame,
      bool noisyDebug){

   int ret = -1;
   int pts_frame = -1;

   while(pts_frame<seekPts){

      //AVRECEIVE
      ret = avcodec_receive_frame(f_codecContext, f_frame);
      if(ret<0) //Error in receive frame
      {
         if(ret == AVERROR(EAGAIN)) //Addtional data required
         {
            if(noisyDebug){
               std::cout << "Need additionnal KF" << std::endl;
            }
            return -2;
         } else //Undetermined error
         {
            if(noisyDebug){
               std::cout << "Undetermined error in getResearchedFrame (receive frame)" << std::endl;
            }
            return -1;
         }
      } else  //Return OK
      {
         //Check PTS
         pts_frame = f_frame->pts;
         if(noisyDebug){
            std::cout << "Frame PTS = " << pts_frame << std::endl;
         }
         if(pts_frame<seekPts)//Pts does not correspond (greater)
         {
            //Unref the ref not required
            av_frame_unref(f_frame);
         }
      }

   }

   if(pts_frame==seekPts){
      if(noisyDebug){
         std::cout << "OK frame FOUND" << std::endl;
      }
      return 0;
   } else {
      if(noisyDebug){
         std::cout << "Frame NOT FOUND : Error" << std::endl;
      }
      return -1;
   }

}

int main(int argc, const char* argv[])
{

   std::cout << "Test FFMPEG : Reverse" << std::endl;

   //Print avcodec version
   std::cout << "Using avcodec version : " << LIBAVCODEC_VERSION_MAJOR << std::endl;

   //Path input
   std::string inputFilePath="/home/richard/Downloads/TestVidFFMPG/Video_011.mpg";
   std::string outputFilePath="/home/richard/Downloads/TestVidFFMPG/Video_011_output.mp4";
   std::string inputJsonPath="/home/richard/Downloads/TestVidFFMPG/PTSValues.json";

   //Output settings
   std::string codecOutputName = "libx265";

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

   //Get decoder and context
   if(getDecoderAndContextFromStream(m_stream, &m_codec, &m_codecContext)<0){
      return -1;
   }

   //Conversion
   SwsContext* conversionSeekFrame = sws_getContext(m_stream->codecpar->width, m_stream->codecpar->height,
         (AVPixelFormat) m_stream->codecpar->format,
         m_stream->codecpar->width, m_stream->codecpar->height,
         AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

   //Buffer image
   cv::Mat bufferMatImage = cv::Mat::zeros(m_stream->codecpar->height, m_stream->codecpar->width,CV_8UC3);
   int cvLinesizes[1];
   cvLinesizes[0] = bufferMatImage.step1();

   //Encoding part
   //TODO begin

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
   m_outputCodecContext->height = m_codecContext->height;
   m_outputCodecContext->width = m_codecContext->width;
   m_outputCodecContext->sample_aspect_ratio = m_codecContext->sample_aspect_ratio;
   m_outputCodecContext->bit_rate = m_codecContext->bit_rate;
   m_outputCodecContext->rc_buffer_size = m_codecContext->rc_buffer_size;
   m_outputCodecContext->rc_max_rate = m_codecContext->rc_max_rate;
   m_outputCodecContext->rc_min_rate = m_codecContext->rc_min_rate;
   m_outputCodecContext->gop_size = 10;//Gap between KF
   m_outputCodecContext->max_b_frames = 1;//Nb of B frames in the interval
   m_outputCodecContext->time_base = (AVRational){m_stream->time_base.num, m_stream->time_base.den};
   m_outputCodecContext->pix_fmt = m_codecContext->pix_fmt;
   m_outputCodecContext->framerate = m_stream->avg_frame_rate;
   if(m_codecOutput->pix_fmts)
   {
      if(m_codecOutput->pix_fmts[0] == m_codecContext->pix_fmt){
         std::cout << "OK pixmft initialized" << std::endl;
      } else {
         std::cout << "Issue while initializing pixmft : error." << std::endl;
         return -1;
      }
   } //TODO add missing vars

   AVDictionary *m_encoderOpts = NULL;           // "create" an empty dictionary
   av_dict_set(&m_encoderOpts, "x265-params", "lossless=1", 0);

   ret = avcodec_open2(m_outputCodecContext, m_codecOutput, &m_encoderOpts);
   if(ret < 0)
   {
      std::cout << "Could not open the request codec." << std::endl;
      return -1;
   }

   //Frame and packets
   AVPacket* m_outputPacket = NULL;
   AVFrame* m_outputFrame = NULL;
   m_outputFrame = av_frame_alloc();
   if(!m_outputFrame)
   {
      std::cout << "Cannot allocate output frame" << std::endl;
      return -1;
   }
   m_outputPacket= av_packet_alloc();
   if(!m_outputPacket)
   {
      std::cout << "Cannot allocate output packet" << std::endl;
      return -1;
   }

   //Context
   AVFormatContext * m_outputFormatContext;

   avformat_alloc_output_context2(&m_outputFormatContext, NULL, NULL, outputFilePath.c_str());
   if (!m_outputFormatContext) {
      std::cout << "Could not deduce outputformat" << std::endl;
      return -1;
   }
   if (!m_outputFormatContext) {
      return-1;
   }

   //Stream
   AVStream* m_outputStream = NULL;
   m_outputStream = avformat_new_stream(m_outputFormatContext, NULL);
   if(!m_outputStream)
   {
      std::cout << "Failed to add new stream to output." << std::endl;
      return -1;
   }

   //Fill the codec parameter of the stream with the one of the codec we use
   avcodec_parameters_from_context(m_outputStream->codecpar, m_outputCodecContext);

   //open output
   ret = avio_open(&m_outputFormatContext->pb, outputFilePath.c_str(), AVIO_FLAG_WRITE);
   if(ret < 0)
   {
      std::cout << "Could not open output file: \"" << outputFilePath << std::endl;
      return -1;
   }

   //Write output header
   ret = avformat_write_header(m_outputFormatContext, NULL);
   if(ret < 0)
   {
      std::cout << "Failed to write header to the output file. Error: " << std::endl;
      return -1;
   }

   //TODO end encoding part

   int64_t dts_packet;
   int64_t pts_frame ;

   //Target
   int64_t pts_target;
   int64_t largePts_target;
   int64_t initialMargin = 40000;
   int64_t marginStep = 5000;
   int64_t numberMargins;

   //Debug output
   bool noisyDebug = false;
   bool visualDebug = true;

   //Stats
   std::vector<size_t> numberPacketsUsedForDecodingVector;
   std::vector<size_t> numberVideoPacketsUsedForDecodingVector;
   std::vector<double> accessTimeVector;

   //Chrono
   std::chrono::high_resolution_clock::time_point t1,t2;

   std::cout << "START (Context) = " << double(m_formatContext->start_time)/double(AV_TIME_BASE) << std::endl;
   std::cout << "UNIT USED (Context) = " << 1.0/double(AV_TIME_BASE) << std::endl;
   std::cout << "START (stream) = " << double(m_stream->start_time)*double(m_stream->time_base.num)/double(m_stream->time_base.den) <<std::endl;
   std::cout << "UNIT USED = " << double(m_stream->time_base.num)/double(m_stream->time_base.den) <<std::endl;
   std::cout << std::endl;

   //TODO here debug starting from custom index !!!! remove and zero instead
   for(size_t k=0; k<ptsVector.size(); k++){

      std::cout << std::endl;
      std::cout << "/////////////////////////////" << std::endl;
      std::cout << "K = " << k << std::endl;
      std::cout << "Seeking frame n = " << pts_target << std::endl;

      if(!noisyDebug){
         t1 = std::chrono::high_resolution_clock::now();
      }

      //Update target
      pts_target = ptsVector.at(k);

      //Init number of margins
      numberMargins = 0;

      size_t nbMovePacket;
      size_t nbMovePacketVideo;

      while(true){

         if(noisyDebug){
            std::cout << "/////////////////////////////" << std::endl;
            std::cout << "Margins = " << (numberMargins*marginStep) << std::endl;
         }

         nbMovePacket = 0;
         nbMovePacketVideo = 0;

         //Seek frame return a keyframe but most of the time an audio keyframe and not a video keyframe
         //Shall check a little bit before
         largePts_target = pts_target-(initialMargin+numberMargins*marginStep);

         //AVSEEK
         ret = av_seek_frame(m_formatContext, videoStreamIndex, largePts_target,
               AVFMT_SEEK_TO_PTS | AVSEEK_FLAG_BACKWARD);
         if (ret < 0){
            std::cout<<"Something went wrong with seeking" << std::endl;
            return -1;
         }
         avcodec_flush_buffers(m_codecContext);

         //Packet initialisation
         int returnGRP = getResearchedPacket(m_formatContext, m_codecContext,
               videoStreamIndex, pts_target,
               m_packet, m_frame,
               nbMovePacket, nbMovePacketVideo,
               noisyDebug);

         //TODO handle issue reading (-3)
         if(returnGRP<0){
            if(returnGRP==-2){
               //Try with more KFs
               if(noisyDebug){
                  std::cout << "Adding KFs" << std::endl;
               }
               numberMargins++;
               continue;
            } else {
               std::cout << "Error in GRP" << std::endl;
               return -1;
            }
         }

         //Frame extraction
         int returnGRF = getResearchedFrame(m_codecContext,
               pts_target,
               m_frame,
               noisyDebug);

         if(returnGRF<0){
            if(returnGRF==-2){
               //Try with more KFs
               if(noisyDebug){
                  std::cout << "Adding KFs" << std::endl;
               }
               numberMargins++;
               continue;
            } else {
               std::cout << "Error in GRF" << std::endl;
               return -1;
            }
         }

         break;

      }

      //Encoding TODO (maybe use output frame instead)
      ret = avcodec_send_frame(m_outputCodecContext, m_frame);
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
         m_outputPacket->duration = (m_outputStream->time_base.den * m_stream->avg_frame_rate.den) / (m_outputStream->time_base.num * m_stream->avg_frame_rate.num );

         //Update TS for the output packet
         av_packet_rescale_ts(m_outputPacket, m_stream->time_base, m_outputStream->time_base);

         //Write packet
         ret = av_interleaved_write_frame(m_outputFormatContext, m_outputPacket);
         if(ret < 0)
         {
            std::cout << "Failed to write a packet to the output file." << std::endl;
         }
         
      }
      //End encoding part (TODO)

      //Decode frame
      sws_scale(conversionSeekFrame,
            m_frame->data,
            m_frame->linesize, 0, m_stream->codecpar->height,
            &bufferMatImage.data, cvLinesizes);

      if(!noisyDebug){
         t2 = std::chrono::high_resolution_clock::now();
         std::chrono::duration<double, std::milli> accessTimeMs = t2 - t1;
         accessTimeVector.push_back(accessTimeMs.count());
      }


      //Time computation
      double timeFrame = double((m_frame->pts-m_stream->start_time)*m_stream->time_base.num)/double(m_stream->time_base.den);
      int fullSec = floor(timeFrame);
      int m_ms = floor(1000*(timeFrame-fullSec));
      int fullMin = fullSec/60;
      int m_sec = fullSec % 60;
      int m_hour = fullMin/60;
      int m_min = fullMin % 60;
      std::cout << std::endl;
      std::cout << "/////////////////////////" << std::endl;
      std::cout << "// TIME = " << m_min << " : " << m_hour << " : " << m_sec << " : " << m_ms << std::endl; 
      std::cout << "/////////////////////////" << std::endl;

      //Unref
      av_frame_unref(m_frame);
      av_packet_unref(m_packet);
      av_packet_unref(m_outputPacket);//TODO : check that (encoding)

      std::cout << "Packets loaded = " << nbMovePacket << std::endl;
      std::cout << "Video Packets loaded = " << nbMovePacketVideo << std::endl;

      //Update stats vectors
      numberPacketsUsedForDecodingVector.push_back(nbMovePacket);
      numberVideoPacketsUsedForDecodingVector.push_back(nbMovePacketVideo);

      //Imshow debug
      if(visualDebug){
         cv::imshow("DEBUG",bufferMatImage);
         cv::waitKey(10);
      }

      //break;

   }

   //Encoding : finalize video (TODO)
   av_write_trailer(m_outputFormatContext);

   //Stats computation
   size_t nbStatsValues = numberPacketsUsedForDecodingVector.size();
   double averageNumberPackets = std::accumulate(numberPacketsUsedForDecodingVector.begin(),
         numberPacketsUsedForDecodingVector.end(), 0.0) / numberPacketsUsedForDecodingVector.size();
   double averageNumberVideoPackets = std::accumulate(numberVideoPacketsUsedForDecodingVector.begin(),
         numberVideoPacketsUsedForDecodingVector.end(), 0.0) / numberVideoPacketsUsedForDecodingVector.size();
   double averageAccessTime;
   if(!noisyDebug){
      averageAccessTime=std::accumulate(accessTimeVector.begin(),
            accessTimeVector.end(), 0.0) / accessTimeVector.size();
   }
   std::sort(numberPacketsUsedForDecodingVector.begin(), numberPacketsUsedForDecodingVector.end());
   std::sort(numberVideoPacketsUsedForDecodingVector.begin(), numberVideoPacketsUsedForDecodingVector.end());
   if(!noisyDebug){
      std::sort(accessTimeVector.begin(), accessTimeVector.end());
   }
   double medianNumberPackets=0; 
   double medianNumberVideoPackets=0; 
   double medianAccessTime=0; 
   if (nbStatsValues % 2 == 0)
   {
      medianNumberPackets = (numberPacketsUsedForDecodingVector[nbStatsValues / 2 - 1] + 
            numberPacketsUsedForDecodingVector[nbStatsValues / 2]) / 2;
      medianNumberVideoPackets = (numberVideoPacketsUsedForDecodingVector[nbStatsValues / 2 - 1] + 
            numberVideoPacketsUsedForDecodingVector[nbStatsValues / 2]) / 2;
      if(!noisyDebug){
         medianAccessTime=(accessTimeVector[nbStatsValues / 2 - 1] +
               accessTimeVector[nbStatsValues / 2]) / 2;
      }
   }
   else
   {
      medianNumberPackets = numberPacketsUsedForDecodingVector[nbStatsValues / 2];
      medianNumberVideoPackets = numberVideoPacketsUsedForDecodingVector[nbStatsValues / 2];
      if(!noisyDebug){
         medianAccessTime = accessTimeVector[nbStatsValues / 2];
      }
   }

   //Print stats
   std::cout << std::endl;
   std::cout << "//////////////////////////////////////////" << std::endl;
   std::cout << "Number of packets used for decoding" << std::endl;
   std::cout << "Average = " << averageNumberPackets << std::endl;
   std::cout << "Median = " << medianNumberPackets << std::endl;

   std::cout << "//////////////////////////////////////////" << std::endl;
   std::cout << "Number of videos packets used for decoding" << std::endl;
   std::cout << "Average = " << averageNumberVideoPackets << std::endl;
   std::cout << "Median = " << medianNumberVideoPackets << std::endl;

   if(!noisyDebug){
      std::cout << "//////////////////////////////////////////" << std::endl;
      std::cout << "Access Time" << std::endl;
      std::cout << "Average = " << averageAccessTime << std::endl;
      std::cout << "Median = " << medianAccessTime << std::endl;
   }



   std::cout << "Ending Test" << std::endl;

   return 0;

}
