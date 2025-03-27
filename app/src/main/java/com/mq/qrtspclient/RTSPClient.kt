package com.mq.qrtspclient

object RTSPClient {

    // 获取版本相关信息
    @JvmStatic
    external fun getVersion()

    @JvmStatic
    external fun start(filename: String): Int

    @JvmStatic
    external fun getUrl(): String


    // Native方法声明
    @JvmStatic
    external fun startStream(url: String, callback: FrameCallback)

    @JvmStatic
    external fun stopStream()


    init {
        System.loadLibrary("rtspclient")  // 加载生成的 .so 库
    }

}
