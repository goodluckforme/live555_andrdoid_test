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

    private var mPpsSps = ByteArray(0)
    fun makeSpsPps(outData: ByteArray) {
        // 记录pps和sps
        var outData = outData
        if (outData[0].toInt() == 0 && outData[1].toInt() == 0 && outData[2].toInt() == 1 && outData[3].toInt() and 0x1f == 7 || outData[0].toInt() == 0 && outData[1].toInt() == 0 && outData[2].toInt() == 0 && outData[3].toInt() == 1 && outData[4].toInt() and 0x1f == 7) {
            mPpsSps = outData
        } else if (outData[0].toInt() == 0 && outData[1].toInt() == 0 && outData[2].toInt() == 1 && outData[3].toInt() and 0x1f == 5 || outData[0].toInt() == 0 && outData[1].toInt() == 0 && outData[2].toInt() == 0 && outData[3].toInt() == 1 && outData[4].toInt() and 0x1f == 5) {
            // 在关键帧前面加上pps和sps数据
            val data = ByteArray(mPpsSps.size + outData.size)
            System.arraycopy(mPpsSps, 0, data, 0, mPpsSps.size)
            System.arraycopy(outData, 0, data, mPpsSps.size, outData.size)
            outData = data
        }
    }
}
