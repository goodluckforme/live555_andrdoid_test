package com.mq.qrtspclient;

public class PpsSps {
    private static byte[] mPpsSps = new byte[0];

    public static byte[] makeSpsPps(byte[] outData) {
        // 记录pps和sps
        if ((outData[0] == 0 && outData[1] == 0 && outData[2] == 1 && (outData[3] & 0x1f) == 7) || (outData[0] == 0
                && outData[1] == 0 && outData[2] == 0 && outData[3] == 1 && (outData[4] & 0x1f) == 7)) {
            mPpsSps = outData;
        } else if ((outData[0] == 0 && outData[1] == 0 && outData[2] == 1 && (outData[3] & 0x1f) == 5)
                || (outData[0] == 0 && outData[1] == 0 && outData[2] == 0 && outData[3] == 1
                && (outData[4] & 0x1f) == 5)) {
            // 在关键帧前面加上pps和sps数据
            byte[] data = new byte[mPpsSps.length + outData.length];
            System.arraycopy(mPpsSps, 0, data, 0, mPpsSps.length);
            System.arraycopy(outData, 0, data, mPpsSps.length, outData.length);
            return data;
        }
        return outData;
    }
}
