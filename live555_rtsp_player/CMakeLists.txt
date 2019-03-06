# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DSOCKLEN_T=socklen_t)
add_definitions(-DNO_SSTREAM=1)
add_definitions(-DNEWLOCALE_NOT_USED)
#add_definitions(-D_LARGEFILE_SOURCE=1)
#add_definitions(-D_FILE_OFFSET_BITS=64)
add_definitions(-DBSD=1)

# 头文件引入
include_directories(src/main/cpp/live/UsageEnvironment/include)
include_directories(src/main/cpp/live/BasicUsageEnvironment/include)
include_directories(src/main/cpp/live/groupsock/include)
include_directories(src/main/cpp/live/liveMedia/include)

#设置源码连接BasicUsageEnvironment
file(GLOB BasicUsageEnvironmentFiles
        src/main/cpp/live/BasicUsageEnvironment/*.cpp)
#添加临时库,Add a library to the project using the specified source files.
#If no type is given explicitly the type is STATIC or SHARED based on whether the current value of the variable BUILD_SHARED_LIBS is ON.
add_library(BasicUsageEnvironment
        ${BasicUsageEnvironmentFiles})

#设置源码连接groupsock
file(GLOB groupsockfiles
        src/main/cpp/live/groupsock/*.cpp src/main/cpp/live/groupsock/*.c)
add_library(groupsock
        ${groupsockfiles})

#设置源码连接liveMedia
file(GLOB liveMediaFiles
        src/main/cpp/live/liveMedia/*.hh src/main/cpp/live/liveMedia/*.cpp src/main/cpp/live/liveMedia/*.c)
add_library(liveMedia ${liveMediaFiles})

#设置源码连接UsageEnvironment
file(GLOB UsageEnvironmentFiles
        src/main/cpp/live/UsageEnvironment/*.cpp)
add_library(UsageEnvironment
        ${UsageEnvironmentFiles})

####################################################################################################
#base64
# 头文件引入
include_directories(src/main/cpp/base64)
file(GLOB base64 src/main/cpp/base64/*.cpp)
add_library(base64_decode ${base64})


####################################################################################################

#应用类源码.
add_library( rtsp-lib
        SHARED
        src/main/cpp/RTSPClient.cpp )
#src/main/cpp/testRTSPClient.cpp )

find_library(log-lib
        log )

target_link_libraries( rtsp-lib
        ${log-lib}
        liveMedia
        groupsock
        BasicUsageEnvironment
        UsageEnvironment
        base64_decode )
