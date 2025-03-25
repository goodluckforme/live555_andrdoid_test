//
// Created by 05878mq on 2025/3/25.
//

#ifndef QRTSPCLIENT_RTSPCLIENT_H
#define QRTSPCLIENT_RTSPCLIENT_H

#include <string>
#include <jni.h>
#include <android/log.h>
#include <android/log.h>

#define LOG_TAG "NativeLib"
#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

char *getLocalIP();

class RtspClient {

};


#endif //QRTSPCLIENT_RTSPCLIENT_H
