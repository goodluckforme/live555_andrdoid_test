package com.mq.qrtspclient

interface FrameCallback {
    // 添加时间戳参数
    fun onFrameReceived(data: ByteArray?, timestampMs: Long)
    fun onError(code: Int, message: String?)
}