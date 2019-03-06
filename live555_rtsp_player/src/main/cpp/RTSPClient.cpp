/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2018, Live Networks, Inc.  All rights reserved
// A demo application, showing how to create and run a RTSP client (that can potentially receive multiple streams concurrently).
//
// NOTE: This code - although it builds a running application - is intended only to illustrate how to develop your own RTSP
// client application.  For a full-featured RTSP client application - with much more functionality, and many options - see
// "openRTSP": http://www.live555.com/openRTSP/
//Poe 参考实现1：https://blog.csdn.net/abc_1234d/article/details/80229423
//Poe 参考实现2： https://gitee.com/zhongyuxin1011/live555Demo/blob/master/src/com/live555/ViedioUtils.java


#include <jni.h>
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "base64.h"

#include <string>
#include <sstream>
#include <stdio.h>
#include <android/log.h>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_TAG "rtsplive555"
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Forward function definitions:

char eventLoopWatchVariable = 0;
RTSPClient *_rtspClient;

JNIEnv *_env;

unsigned char runningFlag = 1;

/** 随意指定的 sps_pps 一般都不能用  */
unsigned char sps_pps[21] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x1E,
                             0x95, 0xA8, 0x28, 0x0F, 0x64, 0x00, 0x00, 0x00, 0x01, 0x68, 0xEE, 0x3C,
                             0x80};

/** 从 sdp 中解析出来的 sps_pps 的大小 */
size_t sps_pps_size = 0;
/** 从 sdp 中解析出来的 sps_pps */
unsigned char sps_pps_2[128];

jclass _clazz;
jobject callback_object;
jmethodID callback_frame_method;
jmethodID callback_info_method;

Boolean isrunning = false;
string tmp;

#pragma mark - 函数声明

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString);
void continueAfterSETUP(RTSPClient *rtspClient, int resultCode, char *resultString);
void continueAfterPLAY(RTSPClient *rtspClient, int resultCode, char *resultString);

// Other event handler functions:
void subsessionAfterPlaying(
        void *clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void
subsessionByeHandler(void *clientData); // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void *clientData);
// called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// The main streaming routine (for each "rtsp://" URL):
void openURL(UsageEnvironment &env, char const *progName, char const *rtspURL);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient *rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient *rtspClient, int exitCode = 1);


/**
 * c/c++ string turn to java jstring
 */
jstring charToJstring(JNIEnv *env, const char *pat) {
    if (env == NULL) {
        return NULL;
    }
    //LOGE("%s", pat);
    jclass _strclass = (env)->FindClass("java/lang/String");
    jstring _encode = (env)->NewStringUTF("GB2312");
    jmethodID ctorID = env->GetMethodID(_strclass, "<init>",
                                        "([BLjava/lang/String;)V");
    jbyteArray bytes = env->NewByteArray(strlen(pat));
    env->SetByteArrayRegion(bytes, 0, strlen(pat), (jbyte *) pat);
    jstring jstr = (jstring) env->NewObject(_strclass, ctorID, bytes, _encode);
//	env->ReleaseByteArrayElements(bytes, NULL, 0);
    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(_strclass);
    env->DeleteLocalRef(_encode);
    return jstr;
}

/**
 * info call back
 */
void infoCallBack(jstring jstr) {
    if (!isrunning) {
        return;
    }

    if (callback_object == NULL && _clazz != NULL) {
        callback_object = _env->AllocObject(_clazz);
    }

    if (callback_object != NULL && callback_info_method != NULL) {
        _env->CallVoidMethod(callback_object, callback_info_method, jstr);
        _env->DeleteLocalRef(jstr);
    }

    tmp = "";

    const char *sdp_title = _env->GetStringUTFChars(jstr, NULL);
    LOGE("%s: %s", __FUNCTION__, sdp_title);
    _env->ReleaseStringUTFChars(jstr, sdp_title);

}

int start(const char *path) {
    // Begin by setting up our usage environment:
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *env = BasicUsageEnvironment::createNew(*scheduler);

    // We need at least one "rtsp://" URL argument:
    if (path == NULL) {
//		LOGE("URL is not allow NULL!");
        tmp = "URL is not allow NULL!";
        infoCallBack(charToJstring(_env, tmp.c_str()));
        return 1;
    }

    eventLoopWatchVariable = 0;

    // There are argc-1 URLs: argv[1] through argv[argc-1].  Open and start streaming each one:
    openURL(*env, NULL, path);

    // All subsequent activity takes place within the event loop:
    env->taskScheduler().doEventLoop(&eventLoopWatchVariable);
    // This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.

//  return 0;

    // If you choose to continue the application past this point (i.e., if you comment out the "return 0;" statement above),
    // and if you don't intend to do anything more with the "TaskScheduler" and "UsageEnvironment" objects,
    // then you can also reclaim the (small) memory used by these objects by uncommenting the following code:

    env->reclaim();
    env = NULL;
    delete scheduler;
    scheduler = NULL;

    return 0;
}

/**
 * 从 sdp 中解析出 sps_pps，放在 sps_pps 中，返回长度
 * @param sdp sdp字串
 * @param sps_pps 返回sps_pps，结构是 [分隔符]sps[分隔符]pps，分隔符是{0x00, 0x00, 0x00, 0x01}
 * @return sps_pps 的长度，失败返回 -1
 */
size_t get_sps_pps_from_sdp(string sdp, unsigned char *sps_pps)
{
    if (sps_pps == nullptr) return -1;

    /*
     *  参考
     *  https://www.cnblogs.com/wangqiguo/p/4556391.html
        RTSP 响应的SDP的内容中sprop-parameter-sets键值：
        sprop-parameter-sets=Z2QAKq2wpDBSAgFxQWKQPQRWFIYKQEAuKCxSB6CKwpDBSAgFxQWKQPQRTDoUKQNC4oJHMGIemHQoUgaFxQSOYMQ9MOhQpA0LigkcwYh6xEQmIVilsQRWUURJsogxOU4QITKUIEVlCCTYQVhBMJQhMIjGggWQJFaIGBJZBAaEnaMIDwsSWQQKCwsrRBQYOWQweO0YEBZASNAogszlAUAW7/wcFBwMQAABdwAAr8g4AAADAL68IAAAdzWU//+MAAADAF9eEAAAO5rKf//CgA==,aP48sA==;
        其中逗号前面的内容是sps的二进制数据被base64之后的结果
        而逗号后面的内容(不要分号,分号是sdp中键值对的分隔符),是pps的内容
        使用live555中的base64Decode函数分别对这两部分进行反base64解码得到的二进制数据就是h264中的sps pps 的二进制内容
        分别是以67 和 68 开头
    */

    // 从sdp中解析出加密的sps 和 pps，sdp 结构如下
    // ...[prefix][sps][suffix][pps][suffix2|suffix3]...
    // 前缀
    string prefix("sprop-parameter-sets=");
    size_t prefix_head_index = sdp.find(prefix);
    if (prefix_head_index == string::npos) {
        LOGE("%s: invalid sdp", __FUNCTION__);
        return -1;
    }
    // sps头下表
    size_t sps_head_index = prefix_head_index + prefix.size();
    // 后缀
    string suffix(",");
    size_t suffix_head_index = sdp.find(suffix.c_str(), sps_head_index);
    if (suffix_head_index == string::npos) {
        LOGE("%s: invalid sdp, can't parse sps", __FUNCTION__);
        return -1;
    }
    // sps长度
    size_t sps_size = suffix_head_index - sps_head_index;
    const char * sps_sdp = sdp.substr(sps_head_index, sps_size).c_str();
    LOGE("%s: sps %s", __FUNCTION__, sps_sdp);
    // 后缀2
    string suffix2(";");
    string suffix3("\r");
    size_t pps_head_index = suffix_head_index + suffix.size();
    // pps头下表
    size_t suffix2_head_index = sdp.find(suffix2.c_str(), pps_head_index);
    if (suffix2_head_index == string::npos)
        suffix2_head_index = sdp.find(suffix3.c_str(), pps_head_index);
    if (suffix2_head_index == string::npos) {
        LOGE("%s: invalid sdp, can't parse pps", __FUNCTION__);
        return -1;
    }
    // pps长度
    size_t pps_size = suffix2_head_index - pps_head_index;
    const char* pps_sdp = sdp.substr(pps_head_index, pps_size).c_str();
    LOGE("%s: pps %s", __FUNCTION__, pps_sdp);



    // 解密 sps 和 pps，并且拼接其阿里
    // 1. 帧分隔符
    char separate[4] = {0x00, 0x00, 0x00, 0x01};
    int sps_pps_size = 0;
    memcpy(sps_pps + sps_pps_size, separate, 4);
    sps_pps_size += 4;
    // 2. sps
    unsigned int result_size = 0;
    unsigned char * p = base64Decode(sps_sdp, result_size);
    memcpy(sps_pps + sps_pps_size, p, result_size);
    sps_pps_size += result_size;
    // 3. 帧分隔符
    memcpy(sps_pps + sps_pps_size, separate, 4);
    sps_pps_size += 4;
    // 4. pps
    result_size = 0;
    p = base64Decode(pps_sdp, result_size);
    memcpy(sps_pps + sps_pps_size, p, result_size);
    sps_pps_size += result_size;

//    for (int i = 0 ; i <  sps_pps_size ; i ++)
//        LOGE("%s: sps_pps %X", __FUNCTION__, sps_pps[i]);

    return sps_pps_size;
}

#pragma mark - native 方法实现

extern "C"
JNIEXPORT jint JNICALL
Java_com_rtsp_poe_rtsplive555_RTSPClient_start(JNIEnv *env, jclass type, jstring path_) {
//  const char *path = env->GetStringUTFChars(path_, 0);
    // Begin by setting up our usage environment:
//  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
//  UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
    if (!isrunning) {
        const char *sdp_title = env->GetStringUTFChars(path_, NULL);

        _env = env;
        _clazz = type;
        runningFlag = 1;
        isrunning = true;

//        callback_frame_method = env->GetMethodID(type, "onNativeInfo", "(Ljava/lang/String;)V");
//        if (callback_frame_method == NULL) {
//            LOGE("can't find method: public void onNativeInfo(java.lang.String)");
//        }
//     LOGE(charToJstring(env,sdp_title));
        LOGE("%s", __FUNCTION__);
        // block function
        start(sdp_title);

        env->ReleaseStringUTFChars(path_, sdp_title);
        env->DeleteLocalRef(path_);

        if (_clazz != NULL && callback_object != NULL) {
            _env->DeleteLocalRef(_clazz);
            _env->DeleteLocalRef(callback_object);
        }

        callback_frame_method = NULL;
        callback_info_method = NULL;

        _clazz = NULL;
        callback_object = NULL;

        _env = NULL;
        _rtspClient = NULL;

        return 0;
    } else {
        return -1;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_rtsp_poe_rtsplive555_RTSPClient_stop(JNIEnv *env, jclass type) {

    if (isrunning) {
        isrunning = false;

//        if (_clazz != NULL && callback_object != NULL && _env != NULL) {
//            _env->DeleteLocalRef(_clazz);
//            _env->DeleteLocalRef(callback_object);
//        }
//
//        callback_frame_method = NULL;
//        callback_info_method = NULL;
//
//        _clazz = NULL;
//        callback_object = NULL;
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_rtsp_poe_rtsplive555_RTSPClient_setCallback(JNIEnv *env, jclass type, jobject callback) {

    if (callback == NULL) return;

    // 保存回调对象
    callback_object = env->NewGlobalRef(callback);
    jclass callbackClass = (jclass)env->GetObjectClass(callback_object);

    // 帧数据回调
    callback_frame_method = env->GetMethodID(callbackClass, "onFrame","([BI)V");
    if (callback_frame_method == NULL) {
        LOGE("%s: GetMethodID onFrame return NULL", __FUNCTION__);
        env->DeleteLocalRef(callbackClass);
        return;
    }

    // 信息回调
//    callback_info_method = env->GetMethodID(callbackClass, "onInfo","(Ljava/lang/Object;)V");
//    if (callback_info_method == NULL) {
//        LOGE("%s: GetMethodID onInfo return NULL", __FUNCTION__);
//        env->DeleteLocalRef(callbackClass);
//        return;
//    }

    env->DeleteLocalRef(callbackClass);
}

#pragma mark - 类声明

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class StreamClientState {
public:
    StreamClientState();

    virtual ~StreamClientState();

public:
    MediaSubsessionIterator *iter;
    MediaSession *session;
    MediaSubsession *subsession;
    TaskToken streamTimerTask;
    double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient : public RTSPClient {
public:
    static ourRTSPClient *createNew(UsageEnvironment &env, char const *rtspURL,
                                    int verbosityLevel = 0,
                                    char const *applicationName = NULL,
                                    portNumBits tunnelOverHTTPPortNum = 0);

protected:
    ourRTSPClient(UsageEnvironment &env, char const *rtspURL,
                  int verbosityLevel, char const *applicationName,
                  portNumBits tunnelOverHTTPPortNum);

    // called only by createNew();
    virtual ~ourRTSPClient();

public:
    StreamClientState scs;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class DummySink : public MediaSink {
public:
    static DummySink *createNew(UsageEnvironment &env,
                                MediaSubsession &subsession, // identifies the kind of data that's being received
                                char const *streamId = NULL); // identifies the stream itself (optional)

private:
    DummySink(UsageEnvironment &env, MediaSubsession &subsession, char const *streamId);

    // called only by "createNew()"
    virtual ~DummySink();

    static void afterGettingFrame(void *clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds);

    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime, unsigned durationInMicroseconds);

private:
    // redefined virtual functions:
    virtual Boolean continuePlaying();

private:
    /** 接收帧数据的缓冲区 */
    u_int8_t *fReceiveBuffer;
    MediaSubsession &fSubsession;
    char *fStreamId;
};

#pragma mark - 函数实现

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

//static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.

void openURL(UsageEnvironment &env, char const *progName, char const *rtspURL) {
    // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
    // to receive (even if more than stream uses the same "rtsp://" URL).

    tmp = "to create a RTSP client for URL \"" + string(rtspURL) + "\":" +
          string(env.getResultMsg());
    infoCallBack(charToJstring(_env, tmp.c_str()));

    RTSPClient *rtspClient = ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL,
                                                      progName);
    if (rtspClient == NULL) {
//    env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
        tmp = "Failed to create a RTSP client for URL \"" + string(rtspURL) + "\":" +
              string(env.getResultMsg());
        infoCallBack(charToJstring(_env, tmp.c_str()));
        return;
    }
    _rtspClient = rtspClient;
//  ++rtspClientCount;

    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
    // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
    rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
}


// Implementation of the RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString) {
    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias

        if (resultCode != 0) {
//      env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
            tmp = "Failed to get a SDP description: " + string(resultString);
            infoCallBack(charToJstring(_env, tmp.c_str()));
            delete[] resultString;
            break;
        }

        char *const sdpDescription = resultString;
//    env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";
        tmp = "Got a SDP description:\n" + string(resultString);
        sps_pps_size = get_sps_pps_from_sdp(resultString, sps_pps_2);
        LOGE("SDP description： =============================================================");
        infoCallBack(charToJstring(_env, tmp.c_str()));
        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription; // because we don't need it anymore
        if (scs.session == NULL) {
//      env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
            tmp = "Failed to create a MediaSession object from the SDP description: " +
                  string(env.getResultMsg());
            infoCallBack(charToJstring(_env, tmp.c_str()));
            break;
        } else if (!scs.session->hasSubsessions()) {
//      env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
            tmp = "This session has no media subsessions (i.e., no \"m=\" lines)";
            infoCallBack(charToJstring(_env, tmp.c_str()));
            break;
        }

        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession(rtspClient);
        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP True

void setupNextSubsession(RTSPClient *rtspClient) {
    UsageEnvironment &env = rtspClient->envir(); // alias
    StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias

    scs.subsession = scs.iter->next();
    if (scs.subsession != NULL) {
        if (!scs.subsession->initiate()) {
//      env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
            tmp = "Failed to initiate the subsession: " + string(env.getResultMsg());
            infoCallBack(charToJstring(_env, tmp.c_str()));
            setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
        } else {
//      env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
            tmp = "Initiated the subsession (";
            ostringstream oss;
            if (scs.subsession->rtcpIsMuxed()) {
//	env << "client port " << scs.subsession->clientPortNum();
                oss << tmp << "client port " << scs.subsession->clientPortNum() << ")";
                tmp = oss.str();
            } else {
//	env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
                oss << tmp << "client port " << scs.subsession->clientPortNum()
                    << "-" << scs.subsession->clientPortNum() + 1 << ")";
                tmp = oss.str();
            }
//      env << ")\n";
            infoCallBack(charToJstring(_env, tmp.c_str()));
            // Continue setting up this subsession, by sending a RTSP "SETUP" command:
            rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False,
                                         REQUEST_STREAMING_OVER_TCP);
        }
        return;
    }

    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != NULL) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(),
                                    scs.session->absEndTime());
    } else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
    }
}

void continueAfterSETUP(RTSPClient *rtspClient, int resultCode, char *resultString) {
    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias

        if (resultCode != 0) {
//      env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << resultString << "\n";
            tmp = "Failed to set up the subsession: " + string(resultString);
            infoCallBack(charToJstring(_env, tmp.c_str()));
            break;
        }

//    env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
        tmp = "Set up the subsession (";
        ostringstream oss;
        if (scs.subsession->rtcpIsMuxed()) {
//      env << "client port " << scs.subsession->clientPortNum();
            oss << tmp << "client port " << scs.subsession->clientPortNum()
                << ")";
            tmp = oss.str();
        } else {
//      env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
            oss << tmp << "client port " << scs.subsession->clientPortNum()
                << "-" << scs.subsession->clientPortNum() + 1 << ")";
            tmp = oss.str();
        }
//    env << ")\n";
        infoCallBack(charToJstring(_env, tmp.c_str()));

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        scs.subsession->sink = DummySink::createNew(env, *scs.subsession, rtspClient->url());
        // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subsession->sink == NULL) {
            /*env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
            << "\" subsession: " << env.getResultMsg() << "\n";*/
            tmp = "Failed to create a data sink for the subsession: "
                  + string(env.getResultMsg());
            infoCallBack(charToJstring(_env, tmp.c_str()));
            break;
        }

//    env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
        tmp = "Created a data sink for the subsession";
        infoCallBack(charToJstring(_env, tmp.c_str()));
        scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
        scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
                                           subsessionAfterPlaying, scs.subsession);
        // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
        if (scs.subsession->rtcpInstance() != NULL) {
            scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
        }
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient *rtspClient, int resultCode, char *resultString) {
    Boolean success = False;
    ostringstream oss;

    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias

        if (resultCode != 0) {
//      env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
            tmp = "Failed to start playing session: " + string(resultString);
            infoCallBack(charToJstring(_env, tmp.c_str()));
            break;
        }

        // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
        // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
        // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
        // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
        if (scs.duration > 0) {
            unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
            scs.duration += delaySlop;
            unsigned uSecsToDelay = (unsigned) (scs.duration * 1000000);
            scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                                          (TaskFunc *) streamTimerHandler,
                                                                          rtspClient);
        }

//    env << *rtspClient << "Started playing session";
        tmp = "Started playing session";
        infoCallBack(charToJstring(_env, tmp.c_str()));
        if (scs.duration > 0) {
//      env << " (for up to " << scs.duration << " seconds)";
            oss << "(for up to " << scs.duration << " seconds)...";
            tmp = oss.str();
            infoCallBack(charToJstring(_env, tmp.c_str()));
        }
//    env << "...\n";

        success = True;
    } while (0);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
        shutdownStream(rtspClient);
    }
}


// Implementation of the other event handlers:

void subsessionAfterPlaying(void *clientData) {
    MediaSubsession *subsession = (MediaSubsession *) clientData;
    RTSPClient *rtspClient = (RTSPClient *) (subsession->miscPtr);

    // Begin by closing this subsession's stream:
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession &session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != NULL) {
        if (subsession->sink != NULL) return; // this subsession is still active
    }

    // All subsessions' streams have now been closed, so shutdown the client:
    shutdownStream(rtspClient);
}

void subsessionByeHandler(void *clientData) {
    MediaSubsession *subsession = (MediaSubsession *) clientData;
    RTSPClient *rtspClient = (RTSPClient *) subsession->miscPtr;
    UsageEnvironment &env = rtspClient->envir(); // alias

//  env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";
    tmp = "Received RTCP \"BYE\" on subsession";
    infoCallBack(charToJstring(_env, tmp.c_str()));
    // Now act as if the subsession had closed:
    subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void *clientData) {
    ourRTSPClient *rtspClient = (ourRTSPClient *) clientData;
    StreamClientState &scs = rtspClient->scs; // alias

    scs.streamTimerTask = NULL;

    // Shut down the stream:
    shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient *rtspClient, int exitCode) {
    UsageEnvironment &env = rtspClient->envir(); // alias
    StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias

    // First, check whether any subsessions have still to be closed:
    if (scs.session != NULL) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession *subsession;

        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                Medium::close(subsession->sink);
                subsession->sink = NULL;

                if (subsession->rtcpInstance() != NULL) {
                    subsession->rtcpInstance()->setByeHandler(NULL,
                                                              NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
                }

                someSubsessionsWereActive = True;
            }
        }

        if (someSubsessionsWereActive) {
            // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
            // Don't bother handling the response to the "TEARDOWN".
            rtspClient->sendTeardownCommand(*scs.session, NULL);
        }
    }

//  env << *rtspClient << "Closing the stream.\n";
    tmp = "Closing the stream.";
    infoCallBack(charToJstring(_env, tmp.c_str()));
    Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

    /* if (--rtspClientCount == 0) {
       // The final stream has ended, so exit the application now.
       // (Of course, if you're embedding this code into your own application, you might want to comment this out,
       // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
       exit(exitCode);
     }*/
}

#pragma mark - 类 ourRTSPClient

// Implementation of "ourRTSPClient":

ourRTSPClient *ourRTSPClient::createNew(UsageEnvironment &env, char const *rtspURL,
                                        int verbosityLevel, char const *applicationName,
                                        portNumBits tunnelOverHTTPPortNum) {
    return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment &env, char const *rtspURL,
                             int verbosityLevel, char const *applicationName,
                             portNumBits tunnelOverHTTPPortNum)
        : RTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
}

ourRTSPClient::~ourRTSPClient() {
}

#pragma mark - 类 StreamClientState
// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
        : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

StreamClientState::~StreamClientState() {
    delete iter;
    if (session != NULL) {
        // We also need to delete "session", and unschedule "streamTimerTask" (if set)
        UsageEnvironment &env = session->envir(); // alias

        env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
        Medium::close(session);
    }
}

#pragma mark - 类 DummySink
// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
/** 帧缓冲区的大小，如果太小，会出现视频下方马赛克的问题 */
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 500000

DummySink *
DummySink::createNew(UsageEnvironment &env, MediaSubsession &subsession, char const *streamId) {
    return new DummySink(env, subsession, streamId);
}

DummySink::DummySink(UsageEnvironment &env, MediaSubsession &subsession, char const *streamId)
        : MediaSink(env),
          fSubsession(subsession) {
    fStreamId = strDup(streamId);
    fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
}

DummySink::~DummySink() {
    delete[] fReceiveBuffer;
    delete[] fStreamId;
}

void DummySink::afterGettingFrame(void *clientData, unsigned frameSize, unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds) {
    //LOGE("%s678", __FUNCTION__);
    DummySink *sink = (DummySink *) clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

int size = 0;
unsigned char *buffer = NULL;

/** 接收到的sps_pps的数据长度，0 则是没接收到sps_pps */
int len = 0;
unsigned char *temp_sps = NULL;
unsigned char *temp_sps_pps = NULL;

void DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned /*durationInMicroseconds*/) {
    //LOGE("%s123", __FUNCTION__);


    // We've just received a frame of data.  (Optionally) print out information about it:
    //将解码后的数据回调到android.
    // only get the video data, exclude audio data.
    if (strcmp(fSubsession.mediumName(), "video") == 0) {

        //if (fReceiveBuffer != NULL) LOGE("FrameType： %x", fReceiveBuffer[0]/* & 0x0f*/);

        /*
         常用Nalu_type:
         0x67 (0 11 00111) SPS          非常重要     type = 7
         0x68 (0 11 01000) PPS          非常重要     type = 8
         0x65 (0 11 00101) IDR  关键帧  非常重要     type = 5
         0x61 (0 10 00001) I帧      重要             type = 1
         0x41 (0 10 00001) P帧      重要             type = 1
         0x01 (0 00 00001) B帧      不重要           type = 1
         0x06 (0 00 00110) SEI      不重要           type = 6
         */
        if (runningFlag == 1) {
            // sps
            if (fReceiveBuffer[0] == 0x67) {

//                for (int i = 0 ; i <  frameSize ; i ++)
//                    LOGE("%s: sps %X", __FUNCTION__, fReceiveBuffer[i]);

                len = frameSize + 4;
                temp_sps = (unsigned char *) malloc(len);
                temp_sps[0] = 0x00;
                temp_sps[1] = 0x00;
                temp_sps[2] = 0x00;
                temp_sps[3] = 0x01;
                memcpy(temp_sps + 4, fReceiveBuffer, frameSize);

            }

            // sps + pps
            if (fReceiveBuffer[0] == 0x68) {
//
//                for (int i = 0 ; i <  frameSize ; i ++)
//                    LOGE("%s: pps %X", __FUNCTION__, fReceiveBuffer[i]);

                int l = len;
                len = len + frameSize + 4;

                temp_sps_pps = (unsigned char *) malloc(len);
                memcpy(temp_sps_pps, temp_sps, l);

                temp_sps_pps[l + 0] = 0x00;
                temp_sps_pps[l + 1] = 0x00;
                temp_sps_pps[l + 2] = 0x00;
                temp_sps_pps[l + 3] = 0x01;
                memcpy(temp_sps_pps + l + 4, fReceiveBuffer, frameSize);


                l = 0;
                free(temp_sps);
                temp_sps = NULL;
            }

            //IDR
            if (fReceiveBuffer[0] == 0x65) {
                runningFlag = 0;

                // 先发送SPS_PPS
                if (len == 0) {
                    jbyteArray jbarray = _env->NewByteArray(sps_pps_size);
                    _env->SetByteArrayRegion(jbarray, 0, sps_pps_size,
                                             (jbyte *) sps_pps_2);
                    _env->CallVoidMethod(callback_object, callback_frame_method, jbarray, sps_pps_size);
                    _env->DeleteLocalRef(jbarray);
                } else {
                    jbyteArray jbarray = _env->NewByteArray(len);
                    _env->SetByteArrayRegion(jbarray, 0, len,
                                             (jbyte *) temp_sps_pps);
                    _env->CallVoidMethod(callback_object, callback_frame_method, jbarray,
                                         len);
                    _env->DeleteLocalRef(jbarray);

                    len = 0;
                    free(temp_sps_pps);
                    temp_sps_pps = NULL;
                }

                // IDR 前面加上 {00, 00, 00, 01}
                size = frameSize + 4;
                buffer = (unsigned char *) malloc(size);
                buffer[0] = 0x00;
                buffer[1] = 0x00;
                buffer[2] = 0x00;
                buffer[3] = 0x01;
                memcpy(buffer + 4, fReceiveBuffer, frameSize);

                // 再发送IDR帧
                if (runningFlag == 0 && callback_frame_method != NULL) {
                    // 把帧数据处理掉
                    jbyteArray jbarray = _env->NewByteArray(size);
                    _env->SetByteArrayRegion(jbarray, 0, size, (jbyte *) buffer);

                    _env->CallVoidMethod(callback_object, callback_frame_method, jbarray, size);
                    _env->DeleteLocalRef(jbarray);
                }
            }
        } else {
            if (fReceiveBuffer[0] != 0x67 && fReceiveBuffer[0] != 0x68) {
                size = frameSize + 4;
                buffer = (unsigned char *) malloc(size);
                buffer[0] = 0x00;
                buffer[1] = 0x00;
                buffer[2] = 0x00;
                buffer[3] = 0x01;
                memcpy(buffer + 4, fReceiveBuffer, frameSize);

                if (runningFlag == 0 && callback_frame_method != NULL) {
                    // 把帧数据处理掉
                    jbyteArray jbarray = _env->NewByteArray(size);
                    _env->SetByteArrayRegion(jbarray, 0, size, (jbyte *) buffer);

                    _env->CallVoidMethod(callback_object, callback_frame_method, jbarray, size);
                    _env->DeleteLocalRef(jbarray);
                }
                //if (fReceiveBuffer != NULL) LOGE("FrameType： %x", fReceiveBuffer[0]/* & 0x0f*/);
            }
        }


        free(buffer);
        size = 0;
        buffer = NULL;
    }

    // Then continue, to request the next frame of data:
    if (!isrunning) {
        shutdownStream(_rtspClient);
    } else {
        continuePlaying();
    }

}

/** 获取帧数据 */
Boolean DummySink::continuePlaying() {
    //LOGE("%s", __FUNCTION__);

    if (fSource == NULL) return False; // sanity check (should not happen)

    // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
    fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                          afterGettingFrame, this,
                          onSourceClosure, this);
    return True;
}

#ifdef __cplusplus
}
#endif