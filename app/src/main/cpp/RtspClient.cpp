//
// Created by 05878mq on 2025/3/25.
//

#include "RtspClient.h"
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <ifaddrs.h>

extern "C"
JNIEXPORT void JNICALL
Java_com_mq_qrtspclient_MainActivity_00024Companion_getVersion(JNIEnv *env, jobject thiz) {
    LOGI("版本信息: %s", LIVEMEDIA_LIBRARY_VERSION_STRING);
}

char *url;

extern "C"
JNIEXPORT jstring JNICALL
Java_com_mq_qrtspclient_MainActivity_00024Companion_getUrl(JNIEnv *env, jobject thiz) {
    if (!url) {
        return NULL;
    }
    return env->NewStringUTF(url);
}

extern "C" JNIEXPORT int JNICALL Java_com_mq_qrtspclient_MainActivity_00024Companion_start
        (JNIEnv *env, jobject clazz, jstring fileName_) {
    const char *inputFilename = env->GetStringUTFChars(fileName_, 0);
    FILE *file = fopen(inputFilename, "rb");
    if (!file) {
        LOGE("couldn't open %s", inputFilename);
        return -1;
    }
    fclose(file);

    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *env_ = BasicUsageEnvironment::createNew(*scheduler);
    UserAuthenticationDatabase *authDB = NULL;
    // Create the RTSP server:
    RTSPServer *rtspServer = RTSPServer::createNew(*env_, 8554, authDB);
    if (rtspServer == NULL) {
        LOGE("Failed to create RTSP server: %s", env_->getResultMsg());
        return -1;
    }
    char const *descriptionString = "Session streamed by \"testOnDemandRTSPServer\"";
    char const *streamName = "mystream";
    ServerMediaSession *sms = ServerMediaSession::createNew(*env_, streamName, streamName, descriptionString);
    sms->addSubsession(H264VideoFileServerMediaSubsession::createNew(*env_, inputFilename, True));
    rtspServer->addServerMediaSession(sms);

    url = rtspServer->rtspURL(sms);

    const char *localIP = getLocalIP();
    if (localIP) {
        char urls[100];
        snprintf(url, sizeof(urls), "rtsp://%s:8554/%s", localIP, streamName);
        LOGE("%s stream, from the file %s ", streamName, inputFilename);
        LOGE("Play this stream using the URL: %s\n", urls);
    } else {
        LOGE("Failed to get local IP address");
    }
    LOGE("%s url ",url);
//    delete[] url;

    env->ReleaseStringUTFChars(fileName_, inputFilename);
    env_->taskScheduler().doEventLoop(); // does not return
}


// 获取本地非回环 IPv4 地址
char *getLocalIP() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        LOGE("getifaddrs() failed: %s", strerror(errno));
        return nullptr;
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr,
                            (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST,
                            nullptr, 0, NI_NUMERICHOST);
            if (s != 0) {
                LOGE("getnameinfo() failed: %s", gai_strerror(s));
                continue;
            }
            // 过滤掉回环地址
            if (strcmp(host, "127.0.0.1") != 0) {
                static char ip[INET_ADDRSTRLEN];
                strcpy(ip, host);
                freeifaddrs(ifaddr);
                return ip;
            }
        }
    }

    freeifaddrs(ifaddr);
    return nullptr;
}