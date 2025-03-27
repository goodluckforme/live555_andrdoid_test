//
// Created by 05878mq on 2025/3/25.
//

#ifndef QRTSPCLIENT_RTSPCLIENT_H
#define QRTSPCLIENT_RTSPCLIENT_H

#include <string>
#include <jni.h>
#include <android/log.h>
#include <UsageEnvironment.hh>
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include <ifaddrs.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 100000
#define REQUEST_STREAMING_OVER_TCP False

#define LOG_TAG "NativeLib"
#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


unsigned char runningFlag = 1;
unsigned char sps_pps[21] = { 0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x1E,
                              0x95, 0xA8, 0x28, 0x0F, 0x64, 0x00, 0x00, 0x00, 0x01, 0x68, 0xEE, 0x3C,
                              0x80 };
int size = 0;
unsigned char *buffer = NULL;

int len = 0;
unsigned char *temp_sps = NULL;
unsigned char *temp_sps_pps = NULL;


// 全局 JNI 环境和对象引用
JavaVM *g_jvm = nullptr;
jobject g_callback_obj = nullptr;
RTSPClient *rtspClient = nullptr;

// 添加JNIEnv获取函数
JNIEnv *getJNIEnv() {
    JNIEnv *env;
    if (g_jvm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        g_jvm->AttachCurrentThread(&env, NULL);
    }
    return env;
}


#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"
static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString);

void continueAfterSETUP(RTSPClient *rtspClient, int resultCode, char *resultString);

void continueAfterPLAY(RTSPClient *rtspClient, int resultCode, char *resultString);

// Other event handler functions:
void subsessionAfterPlaying(
        void *clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void *clientData, char const *reason);

// called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void *clientData);
// called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// The main streaming routine (for each "rtsp://" URL):
void openURL(UsageEnvironment &env, char const *progName, char const *rtspURL);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient *rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient *rtspClient, int exitCode = 1);


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
    jobject callback;
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

class DummySink : public MediaSink {
public:
    static DummySink *createNew(UsageEnvironment &env,
                                MediaSubsession &subsession,
                                jobject callback) {
        return new DummySink(env, subsession, callback);
    }

private:
    DummySink(UsageEnvironment &env, MediaSubsession &subsession, jobject callback)
            : MediaSink(env), fSubsession(subsession) {
        // 创建全局引用
        JNIEnv *jniEnv = getJNIEnv();
        m_callback = jniEnv->NewGlobalRef(callback);
        fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
        env << "Failed to get a SDP description: " << "\n";
    }

    virtual ~DummySink() {
        delete[] fReceiveBuffer;
        JNIEnv *jniEnv = getJNIEnv();
        jniEnv->DeleteGlobalRef(m_callback);
    }

    static void afterGettingFrame(void *clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds) {
        DummySink *sink = (DummySink *) clientData;
        sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime,
                                durationInMicroseconds);
    }

    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime, unsigned durationInMicroseconds) {
        JNIEnv *jniEnv = getJNIEnv();
        // 获取回调类和方法ID
        jclass clazz = jniEnv->GetObjectClass(m_callback);
        jmethodID method = jniEnv->GetMethodID(clazz, "onFrameReceived", "([BJ)V");

        LOGE("mediumName :%s",fSubsession.mediumName());
        // only get the video data, exclude audio data.
        if (strcmp(fSubsession.mediumName(), "video") == 0) {
            if (runningFlag == 1) {
                // sps
                if (fReceiveBuffer[0] == 0x67) {
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

                if (fReceiveBuffer[0] == 0x65) {
                    runningFlag = 0;

                    size = frameSize + 4;
                    buffer = (unsigned char *) malloc(size);
                    buffer[0] = 0x00;
                    buffer[1] = 0x00;
                    buffer[2] = 0x00;
                    buffer[3] = 0x01;
                    memcpy(buffer + 4, fReceiveBuffer, frameSize);

                    if (runningFlag == 0 && method != NULL) {
                        jbyteArray jbarray = jniEnv->NewByteArray(size);
                        jniEnv->SetByteArrayRegion(jbarray, 0, size, (jbyte *) buffer);
                        jniEnv->CallVoidMethod(m_callback, method, jbarray, size);
                        jniEnv->DeleteLocalRef(jbarray);
                    }
                }
                size = frameSize + 4;
                buffer = (unsigned char *) malloc(size);
                buffer[0] = 0x00;
                buffer[1] = 0x00;
                buffer[2] = 0x00;
                buffer[3] = 0x01;
                memcpy(buffer + 4, fReceiveBuffer, frameSize);

                if (runningFlag == 0 && method != NULL) {
                    jbyteArray jbarray = jniEnv->NewByteArray(size);
                    jniEnv->SetByteArrayRegion(jbarray, 0, size, (jbyte*) buffer);

                    jniEnv->CallVoidMethod(m_callback, method, jbarray, size);
                    jniEnv->DeleteLocalRef(jbarray);
                }

                /*
                 常用Nalu_type:
                 0x67 (0 11 00111) SPS          非常重要     type = 7
                 0x68 (0 11 01000) PPS          非常重要     type = 8
                 0x65 (0 11 00101) IDR  关键帧      非常重要      type = 5
                 0x61 (0 10 00001) I帧      重要            type = 1
                 0x41 (0 10 00001) P帧      重要            type = 1
                 0x01 (0 00 00001) B帧      不重要       type = 1
                 0x06 (0 00 00110) SEI  不重要       type = 6
                 */
            } else {
                if (fReceiveBuffer[0] != 0x67 && fReceiveBuffer[0] != 0x68) {
                    size = frameSize + 4;
                    buffer = (unsigned char *) malloc(size);
                    buffer[0] = 0x00;
                    buffer[1] = 0x00;
                    buffer[2] = 0x00;
                    buffer[3] = 0x01;
                    memcpy(buffer + 4, fReceiveBuffer, frameSize);

                    if (runningFlag == 0 && method != NULL) {
                        jbyteArray jbarray = jniEnv->NewByteArray(size);
                        jniEnv->SetByteArrayRegion(jbarray, 0, size, (jbyte*) buffer);

                        jniEnv->CallVoidMethod(m_callback, method, jbarray, size);
                        jniEnv->DeleteLocalRef(jbarray);
                    }
                }
            }

            free(buffer);
            size = 0;
            buffer = NULL;
        }
        continuePlaying();
    }

    Boolean continuePlaying() {
        if (fSource == NULL) return False;
        fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                              afterGettingFrame, this,
                              onSourceClosure, this);
        return True;
    }

private:
    u_int8_t *fReceiveBuffer;
    MediaSubsession &fSubsession;
    jobject m_callback;
};


char *getLocalIP();

class RtspClient {

};


#endif //QRTSPCLIENT_RTSPCLIENT_H
