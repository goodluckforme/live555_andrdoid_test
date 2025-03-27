//
// Created by 05878mq on 2025/3/25.
//

#include "RtspClient.h"


// JNI 启动解码函数
extern "C" JNIEXPORT void JNICALL
Java_com_mq_qrtspclient_RTSPClient_startStream(
        JNIEnv *env, jclass thiz, jstring j_rtsp_url, jobject callback) {
    REQUEST_STREAMING_OVER_TCP = true;//初始化为tcp

    isrunning = true;
    has_sps = false;
    has_pps = false;
    // 获取 JavaVM
    env->GetJavaVM(&g_jvm);
    LOGE("startStream :======================================1");
    // 保存解码器对象引用
    g_callback_obj = env->NewGlobalRef(callback);
    LOGE("startStream :======================================2");

    const char *rtsp_url = env->GetStringUTFChars(j_rtsp_url, 0);
    LOGE("startStream :======================================3");

    LOGE("rtsp_url  : %s ", rtsp_url);
    // 创建任务调度器和环境
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *envir = BasicUsageEnvironment::createNew(*scheduler);
    LOGE("startStream :======================================4");
    // 创建RTSP客户端
    rtspClient = ourRTSPClient::createNew(*envir, rtsp_url, RTSP_CLIENT_VERBOSITY_LEVEL, NULL);
    LOGE("startStream :======================================5");

    // 设置回调到客户端状态
    dynamic_cast<ourRTSPClient *>(rtspClient)->scs.callback = g_callback_obj;
    LOGE("startStream :======================================6");

    // 发送DESCRIBE命令
    rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
    LOGE("startStream :======================================7");
    // 进入事件循环（需要在后台线程运行）
    envir->taskScheduler().doEventLoop();
}


// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString) {
    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias
        LOGE("startStream :======================================11  resultString:%s",
             resultString);
        LOGE("startStream :======================================11  resultCode:%d", resultCode);
        if (resultCode != 0) {
//            env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
            delete[] resultString;
            break;
        }
        LOGE("startStream :======================================12");
        char *const sdpDescription = resultString;
//        env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

        std::string tmp = "Got a SDP description:\n" + std::string(resultString);
        sps_pps_size = get_sps_pps_from_sdp(resultString, sps_pps_from_sdp);
        LOGE("SDP description： =============================== sps_pps_size:%d", sps_pps_size);
//        JNIEnv *pEnv = getJNIEnv();
//        infoCallBack(102, charToJstring(pEnv, tmp.c_str()));

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription; // because we don't need it anymore
        if (scs.session == NULL) {
//            env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
            LOGE("startStream :======================================13_1");
            break;
        } else if (!scs.session->hasSubsessions()) {
//            env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
            LOGE("startStream :======================================13_2");
            break;
        }
        LOGE("startStream :======================================13");
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


void continueAfterPLAY(RTSPClient *rtspClient, int resultCode, char *resultString) {
    Boolean success = False;

    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias
        LOGE("startStream :======================================133_1  resultCode %d ", resultCode);
        LOGE("RTSP PLAY resultCode = %d, error = %s", resultCode, resultString);
        if (resultCode != 0) {
//            env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
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
        LOGE("startStream :======================================133_2  scs.duration %d ", scs.duration);

//        env << *rtspClient << "Started playing session";
        if (scs.duration > 0) {
//            env << " (for up to " << scs.duration << " seconds)";
        }
//        env << "...\n";

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

void openURL(UsageEnvironment &env, char const *progName, char const *rtspURL) {
    // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
    // to receive (even if more than stream uses the same "rtsp://" URL).
    RTSPClient *rtspClient = ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
    if (rtspClient == NULL) {
//        env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
        return;
    }

    ++rtspClientCount;

    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
    // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
    rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
}

void subsessionByeHandler(void *clientData, char const *reason) {
    MediaSubsession *subsession = (MediaSubsession *) clientData;
    RTSPClient *rtspClient = (RTSPClient *) subsession->miscPtr;
    UsageEnvironment &env = rtspClient->envir(); // alias

//    env << *rtspClient << "Received RTCP \"BYE\"";
    if (reason != NULL) {
//        env << " (reason:\"" << reason << "\")";
        delete[] (char *) reason;
    }
//    env << " on \"" << *subsession << "\" subsession\n";

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



// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
        : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0),
          callback(NULL) {
}

StreamClientState::~StreamClientState() {
    delete iter;
    if (session != NULL) {
        // We also need to delete "session", and unschedule "streamTimerTask" (if set)
        UsageEnvironment &env = session->envir(); // alias

        env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
        Medium::close(session);
    }
    // 释放全局引用
    if (callback != NULL) {
        JNIEnv *env = getJNIEnv(); // 需要实现getJNIEnv()
        env->DeleteGlobalRef(callback);
        callback = NULL;
    }
}

// Implementation of the RTSP 'response handlers':

void setupNextSubsession(RTSPClient *rtspClient) {
    UsageEnvironment &env = rtspClient->envir(); // alias
    StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias
    LOGE("startStream :======================================111");
    scs.subsession = scs.iter->next();
    if (scs.subsession != NULL) {
        if (!scs.subsession->initiate()) {
//            env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
            setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
        } else {
//            env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
//            if (scs.subsession->rtcpIsMuxed()) {
//                env << "client port " << scs.subsession->clientPortNum();
//            } else {
//                env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
//            }
//            env << ")\n";
            LOGE("startStream :======================================112");
            // Continue setting up this subsession, by sending a RTSP "SETUP" command:
            rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False,
                                         REQUEST_STREAMING_OVER_TCP);
        }
        return;
    }

    LOGE("startStream :======================================113_0 scs.session->absStartTime() %d", scs.session->absStartTime() != NULL);

    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != NULL) {
        LOGE("startStream :======================================113");
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(),
                                    scs.session->absEndTime());
    } else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        LOGE("startStream :======================================114 scs.duratio %d  scs.session ", scs.duration,scs.session->name());
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
    }
}

void continueAfterSETUP(RTSPClient *rtspClient, int resultCode, char *resultString) {
    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((ourRTSPClient *) rtspClient)->scs; // alias

        if (resultCode != 0) {
//            env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << resultString << "\n";
            break;
        }

//        env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
//        if (scs.subsession->rtcpIsMuxed()) {
//            env << "client port " << scs.subsession->clientPortNum();
//        } else {
//            env << "client ports " << scs.subsession->clientPortNum() << "-"
//                << scs.subsession->clientPortNum() + 1;
//        }
//        env << ")\n";

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        scs.subsession->sink = DummySink::createNew(env, *scs.subsession, scs.callback);
        // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subsession->sink == NULL) {
//            env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
            break;
        }

        //env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
        scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
        scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
                                           subsessionAfterPlaying, scs.subsession);
        // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
        if (scs.subsession->rtcpInstance() != NULL) {
            scs.subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler,
                                                                    scs.subsession);
        }
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession(rtspClient);
}

// JNI 停止解码函数
extern "C" JNIEXPORT void JNICALL
Java_com_mq_qrtspclient_RTSPClient_stopStream(JNIEnv *env, jclass thiz) {
    if (isrunning) {
        runningFlag = 1;
        isrunning = false;
    }
    LOGE("stopStream: Streaming stopped successfully");
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

    // env << *rtspClient << "Closing the stream.\n";
    Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.
    LOGE("startStream :======================================21 rtspClientCount  %d",
         rtspClientCount);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_mq_qrtspclient_RTSPClient_getVersion(JNIEnv *env, jclass thiz) {
    LOGI("版本信息: %s", LIVEMEDIA_LIBRARY_VERSION_STRING);
}

char *url;

extern "C"
JNIEXPORT jstring JNICALL
Java_com_mq_qrtspclient_RTSPClient_getUrl(JNIEnv *env, jclass thiz) {
    if (!url) {
        return NULL;
    }
    return env->NewStringUTF(url);
}

extern "C" JNIEXPORT int JNICALL Java_com_mq_qrtspclient_RTSPClient_start
        (JNIEnv *env, jclass clazz, jstring fileName_) {
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
    ServerMediaSession *sms = ServerMediaSession::createNew(*env_, streamName, streamName,
                                                            descriptionString);
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
    LOGE("----------------------url  ：  %s  ", url);
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
                            (family == AF_INET) ? sizeof(struct sockaddr_in)
                                                : sizeof(struct sockaddr_in6),
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
 