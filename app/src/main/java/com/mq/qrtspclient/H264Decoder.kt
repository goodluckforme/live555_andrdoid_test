package com.mq.qrtspclient

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.util.Base64
import android.view.Surface
import java.nio.ByteBuffer

class H264Decoder(val surface: Surface, val width: Int, val height: Int) {
    private var decoder: MediaCodec? = null

    fun startDecoder() {
        decoder = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
        val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height)
//        val format = MediaFormat.createVideoFormat(mimeType, width, height)
        format.setInteger(MediaFormat.KEY_WIDTH, width)
        format.setInteger(MediaFormat.KEY_HEIGHT, height)

//        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
//        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible)
        format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, width * height) // 预防解码失败

        decoder?.configure(format, surface, null, 0)
        decoder?.start()
    }


    fun decodeFrame(data: ByteArray, offset: Int, length: Int, presentationTimeUs: Long) {
        val inputIndex = decoder?.dequeueInputBuffer(10000) ?: -1
        if (inputIndex >= 0) {
            val inputBuffer = decoder?.getInputBuffer(inputIndex)
            inputBuffer?.clear()
            inputBuffer?.put(data, offset, length)
            decoder?.queueInputBuffer(inputIndex, 0, length, presentationTimeUs, 0)
        }

        val bufferInfo = MediaCodec.BufferInfo()
        val outputIndex = decoder?.dequeueOutputBuffer(bufferInfo, 10000) ?: -1
        if (outputIndex >= 0) {
            decoder?.releaseOutputBuffer(outputIndex, true)
        }
    }

    fun stopDecoder() {
        decoder?.stop()
        decoder?.release()
        decoder = null
    }
}
