package com.mq.qrtspclient

interface FrameCallback {
    // 添加时间戳参数
    fun onFrameReceived(frameData: ByteArray, timestampMs: Long)
    fun infoCallBack(code: Int, message: String?)
}