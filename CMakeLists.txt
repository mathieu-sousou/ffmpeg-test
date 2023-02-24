cmake_minimum_required(VERSION 3.12.2)

project(cleanTestFFMPEG LANGUAGES C CXX VERSION 0.1)

find_package(OpenCV REQUIRED COMPONENTS opencv_core opencv_imgproc opencv_videoio opencv_dnn opencv_highgui)
#find_package(Boost REQUIRED)

#add_library(libyuv STATIC IMPORTED)
#set_target_properties( libyuv PROPERTIES IMPORTED_LOCATION /home/mathieu/bosse/test/libyuv/libyuv.a )

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(AVFILTER_LIBRARY avfilter)
find_library(SWSCALE_LIBRARY swscale)
find_library(SWRESAMPLE_LIBRARY swresample)
#find_package(PkgConfig REQUIRED)
#pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
#        libavdevice
#        libavfilter
#        libavformat
#        libavcodec
#        libswresample
#        libswscale
#        libavutil
#        )

find_package(ZLIB)
find_package(BZip2)



find_package(Threads REQUIRED)


#READ VIDEO AND EXTRACT DTS
add_executable(CVDisplayVideo
        src/CVDisplayVideo.cpp
        )
target_include_directories(CVDisplayVideo PUBLIC /home/mathieu/bosse/test/libyuv/include)
target_include_directories(CVDisplayVideo PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(CVDisplayVideo PRIVATE libyuv ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

#READ VIDEO AND EXTRACT DTS
add_executable(testDecoding
    src/main.cpp
)
target_include_directories(testDecoding PUBLIC ${Boost_INCLUDE_DIRS}) 
target_link_libraries(testDecoding PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

#TEST SEEK
add_executable(testSelectFrame
    src/mainCheckFrame.cpp
)
target_include_directories(testSelectFrame PUBLIC ${Boost_INCLUDE_DIRS}) 
target_link_libraries(testSelectFrame PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

#SMALL TEMPORARY TESTS
add_executable(testSimple
    src/mainSimpleTest.cpp
)
target_include_directories(testSimple PUBLIC ${Boost_INCLUDE_DIRS}) 
target_link_libraries(testSimple PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

#TRANCODE
add_executable(testTranscode
    src/mainTrancodeVideo.cpp
)
target_include_directories(testTranscode PUBLIC ${Boost_INCLUDE_DIRS}) 
target_link_libraries(testTranscode PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})


#SPLIT
add_executable(testSplitvideos
    src/mainSplitVideo.cpp
)
target_include_directories(testSplitvideos PUBLIC ${Boost_INCLUDE_DIRS}) 
target_link_libraries(testSplitvideos PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

#CONCAT
add_executable(testConcatVideos
    src/mainConcatenateVideo.cpp
        src/CVDisplayVideo.cpp)
target_include_directories(testConcatVideos PUBLIC ${Boost_INCLUDE_DIRS}) 
target_link_libraries(testConcatVideos PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

#CHECK SAME VIDEOS
add_executable(testSameVideos
    src/mainSameVideos.cpp
)
target_include_directories(testSameVideos PUBLIC ${Boost_INCLUDE_DIRS}) 
target_link_libraries(testSameVideos PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

#CHECK SAME VIDEOS
add_executable(testTimestampVideos
    src/mainDebugVideoTimestamp.cpp
)
target_include_directories(testTimestampVideos PUBLIC ${Boost_INCLUDE_DIRS}) 
target_link_libraries(testTimestampVideos PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})


#CHECK SAME VIDEOS
add_executable(mainSameVideosNoOpenCV
        src/mainSameVideosNoOpenCV.cpp
        )
target_include_directories(mainSameVideosNoOpenCV PUBLIC ${Boost_INCLUDE_DIRS})
#target_link_libraries(mainSameVideosNoOpenCV PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})
target_link_libraries(mainSameVideosNoOpenCV PRIVATE
        ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${OpenCV_LIBS}
        ${OpenCV_LIBS} ${Boost_LIBRARIES} Threads::Threads ZLIB::ZLIB ${BZIP2_LIBRARIES} dl)
        #PkgConfig::LIBAV ${Boost_LIBRARIES})# Threads::Threads ZLIB::ZLIB ${BZIP2_LIBRARIES})
# /home/mathieu/bosse/test/ffmpeg/install;