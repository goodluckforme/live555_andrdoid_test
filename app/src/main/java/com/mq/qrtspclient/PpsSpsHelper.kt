package com.mq.qrtspclient

import android.util.Log

/**
 * @author 05878mq
 * @date 2025/3/27
 * @project QRtspClient
 * @description Pps头部拼接
 * @version 1.0
 * @copyright Copyright (c) 2025 guide. All rights reserved.
 */

object PpsSpsHelper {
    private var mPpsSps = ByteArray(0)
    fun makeSpsPps(frameData: ByteArray): ByteArray {
        Log.d("maqi", "frameData  " + bytesToHex(frameData))
//        Log.d("maqi", "frameData0 " + frameData[0] + " frameData[1]:" + frameData[1] + " frameData[2]:" + frameData[2] + " frameData[3]:" + frameData[3] + " frameData[4]:" + frameData[4]);
//        Log.d("maqi", "frameData5 " + frameData[5] + " frameData[6]:" + frameData[6] + " frameData[7]:" + frameData[7] + " frameData[8]:" + frameData[8]+ " frameData[9]:" + frameData[9]);
        Log.d("maqi", "frameData.length " + frameData.size)
        Log.d("maqi", "mPpsSps.length " + mPpsSps.size)
        // 记录pps和sps
        if (frameData[0].toInt() == 0 && frameData[1].toInt() == 0 && frameData[2].toInt() == 1 && frameData[3].toInt() and 0x1f == 7 || frameData[0].toInt() == 0 && frameData[1].toInt() == 0 && frameData[2].toInt() == 0 && frameData[3].toInt() == 1 && frameData[4].toInt() and 0x1f == 7) {
            if (mPpsSps.size != frameData.size) {
                mPpsSps = ByteArray(frameData.size)
            }
            System.arraycopy(frameData, 0, mPpsSps, 0, mPpsSps.size)
        } else if (frameData[0].toInt() == 0 && frameData[1].toInt() == 0 && frameData[2].toInt() == 1 && frameData[3].toInt() and 0x1f == 5 || frameData[0].toInt() == 0 && frameData[1].toInt() == 0 && frameData[2].toInt() == 0 && frameData[3].toInt() == 1 && frameData[4].toInt() and 0x1f == 5) {
            if (mPpsSps.size <= 0) {
                Log.d("maqi", "not  get ppssps")
                return frameData
            }
            // 在关键帧前面加上pps和sps数据
            val data = ByteArray(mPpsSps.size + frameData.size)
            val len = frameData.size + mPpsSps.size
            System.arraycopy(mPpsSps, 0, data, 0, mPpsSps.size)
            System.arraycopy(frameData, 0, data, mPpsSps.size, frameData.size)
            Log.d("maqi", "mPpsSps " + mPpsSps[0] + " mPpsSps[1]:" + mPpsSps[1] + " mPpsSps[2]:" + mPpsSps[2] + " mPpsSps[3]:" + mPpsSps[3])
            return data
        }
        return frameData
    }

    /**
     * 将 byte 数组转换为十六进制字符串（大写）
     * @param bytes 输入字节数组
     * @return 十六进制字符串，如 "1A2BFF"
     */
    fun bytesToHex(bytes: ByteArray?): String {
        if (bytes == null) {
            return ""
        }
        val hexString = StringBuilder()
        for (b in bytes) {
            // 将字节转换为无符号整数，并格式化为两位十六进制
            val hex = String.format("%02X", b.toInt() and 0xFF)
            hexString.append(hex)
        }
        return hexString.toString()
    }
}
