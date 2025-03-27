//
// Created by 05878mq on 2025/3/25.
//

#ifndef QRTSPCLIENT_RTSPCLIENT_H
#define QRTSPCLIENT_RTSPCLIENT_H

#include <string>
#include <jni.h>
#include <android/log.h>
#include <UsageEnvironment.hh>
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "Base64.hh"
#include <ifaddrs.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 100000
#define REQUEST_STREAMING_OVER_TCP False

#define LOG_TAG "NativeLib"
#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef struct {
    unsigned int profile_idc;
    unsigned int level_idc;

    unsigned int width;
    unsigned int height;
    unsigned int fps;       //SPS中可能不包含FPS信息
} sps_info_struct;


typedef unsigned char BYTE;
typedef int INT;
typedef unsigned int UINT;

typedef struct {
    const BYTE *data;   //sps数据
    UINT size;          //sps数据大小
    UINT index;         //当前计算位所在的位置标记
} sps_bit_stream;

/**
 移除H264的NAL防竞争码(0x03)

 @param data sps数据
 @param dataSize sps数据大小
 */
static void del_emulation_prevention(BYTE *data, UINT *dataSize) {
    UINT dataSizeTemp = *dataSize;
    for (UINT i = 0, j = 0; i < (dataSizeTemp - 2); i++) {
        int val = (data[i] ^ 0x0) + (data[i + 1] ^ 0x0) + (data[i + 2] ^ 0x3);    //检测是否是竞争码
        if (val == 0) {
            for (j = i + 2; j < dataSizeTemp - 1; j++) {    //移除竞争码
                data[j] = data[j + 1];
            }

            (*dataSize)--;      //data size 减1
        }
    }
}

static void sps_bs_init(sps_bit_stream *bs, const BYTE *data, UINT size) {
    if (bs) {
        bs->data = data;
        bs->size = size;
        bs->index = 0;
    }
}


/**
 是否已经到数据流最后

 @param bs sps_bit_stream数据
 @return 1：yes，0：no
 */
static INT eof(sps_bit_stream *bs) {
    return (bs->index >= bs->size * 8);    //位偏移已经超出数据
}


/**
 读取从起始位开始的BitCount个位所表示的值

 @param bs sps_bit_stream数据
 @param bitCount bit位个数(从低到高)
 @return value
 */
static UINT u(sps_bit_stream *bs, BYTE bitCount) {
    UINT val = 0;
    for (BYTE i = 0; i < bitCount; i++) {
        val <<= 1;
        if (eof(bs)) {
            val = 0;
            break;
        } else if (bs->data[bs->index / 8] & (0x80 >> (bs->index % 8))) {     //计算index所在的位是否为1
            val |= 1;
        }
        bs->index++;  //递增当前起始位(表示该位已经被计算，在后面的计算过程中不需要再次去计算所在的起始位索引，缺点：后面每个bit位都需要去位移)
    }

    return val;
}


/**
 读取无符号哥伦布编码值(UE)
 #2^LeadingZeroBits - 1 + (xxx)

 @param bs sps_bit_stream数据
 @return value
 */
static UINT ue(sps_bit_stream *bs) {
    UINT zeroNum = 0;
    while (u(bs, 1) == 0 && !eof(bs) && zeroNum < 32) {
        zeroNum++;
    }

    return (UINT) ((1 << zeroNum) - 1 + u(bs, zeroNum));
}


/**
 读取有符号哥伦布编码值(SE)
 #(-1)^(k+1) * Ceil(k/2)

 @param bs sps_bit_stream数据
 @return value
 */
INT se(sps_bit_stream *bs) {
    INT ueVal = (INT) ue(bs);
    double k = ueVal;

    INT seVal = (INT) ceil(k / 2);     //ceil:返回大于或者等于指定表达式的最小整数
    if (ueVal % 2 == 0) {       //偶数取反，即(-1)^(k+1)
        seVal = -seVal;
    }

    return seVal;
}


/**
 视频可用性信息(Video usability information)解析

 @param bs sps_bit_stream数据
 @param info sps解析之后的信息数据及结构体
 @see E.1.1 VUI parameters syntax
 */
void vui_para_parse(sps_bit_stream *bs, sps_info_struct *info) {
    UINT aspect_ratio_info_present_flag = u(bs, 1);
    if (aspect_ratio_info_present_flag) {
        UINT aspect_ratio_idc = u(bs, 8);
        if (aspect_ratio_idc == 255) {      //Extended_SAR
            u(bs, 16);      //sar_width
            u(bs, 16);      //sar_height
        }
    }

    UINT overscan_info_present_flag = u(bs, 1);
    if (overscan_info_present_flag) {
        u(bs, 1);       //overscan_appropriate_flag
    }

    UINT video_signal_type_present_flag = u(bs, 1);
    if (video_signal_type_present_flag) {
        u(bs, 3);       //video_format
        u(bs, 1);       //video_full_range_flag
        UINT colour_description_present_flag = u(bs, 1);
        if (colour_description_present_flag) {
            u(bs, 8);       //colour_primaries
            u(bs, 8);       //transfer_characteristics
            u(bs, 8);       //matrix_coefficients
        }
    }

    UINT chroma_loc_info_present_flag = u(bs, 1);
    if (chroma_loc_info_present_flag) {
        ue(bs);     //chroma_sample_loc_type_top_field
        ue(bs);     //chroma_sample_loc_type_bottom_field
    }

    UINT timing_info_present_flag = u(bs, 1);
    if (timing_info_present_flag) {
        UINT num_units_in_tick = u(bs, 32);
        UINT time_scale = u(bs, 32);
        UINT fixed_frame_rate_flag = u(bs, 1);

        info->fps = (UINT) ((float) time_scale / (float) num_units_in_tick);
        if (fixed_frame_rate_flag) {
            info->fps = info->fps / 2;
        }
    }

    UINT nal_hrd_parameters_present_flag = u(bs, 1);
    if (nal_hrd_parameters_present_flag) {
        //hrd_parameters()  //see E.1.2 HRD parameters syntax
    }

    //后面代码需要hrd_parameters()函数接口实现才有用
    UINT vcl_hrd_parameters_present_flag = u(bs, 1);
    if (vcl_hrd_parameters_present_flag) {
        //hrd_parameters()
    }
    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
        u(bs, 1);   //low_delay_hrd_flag
    }

    u(bs, 1);       //pic_struct_present_flag
    UINT bitstream_restriction_flag = u(bs, 1);
    if (bitstream_restriction_flag) {
        u(bs, 1);   //motion_vectors_over_pic_boundaries_flag
        ue(bs);     //max_bytes_per_pic_denom
        ue(bs);     //max_bits_per_mb_denom
        ue(bs);     //log2_max_mv_length_horizontal
        ue(bs);     //log2_max_mv_length_vertical
        ue(bs);     //max_num_reorder_frames
        ue(bs);     //max_dec_frame_buffering
    }
}

//See 7.3.1 NAL unit syntax
//See 7.3.2.1.1 Sequence parameter set data syntax
INT h264_parse_sps(const BYTE *data, UINT dataSize, sps_info_struct *info) {
    if (!data || dataSize <= 0 || !info) return 0;
    INT ret = 0;

    BYTE *dataBuf = static_cast<BYTE *>(malloc(dataSize));
    memcpy(dataBuf, data, dataSize);        //重新拷贝一份数据，防止移除竞争码时对原数据造成影响
    del_emulation_prevention(dataBuf, &dataSize);

    sps_bit_stream bs = {0};
    sps_bs_init(&bs, dataBuf, dataSize);   //初始化SPS数据流结构体

    u(&bs, 1);      //forbidden_zero_bit
    u(&bs, 2);      //nal_ref_idc
    UINT nal_unit_type = u(&bs, 5);
    LOGE("h264_parse_sps  nal_unit_type=%d", nal_unit_type);
    if (nal_unit_type == 0x7) {     //Nal SPS Flag
        info->profile_idc = u(&bs, 8);
        u(&bs, 1);      //constraint_set0_flag
        u(&bs, 1);      //constraint_set1_flag
        u(&bs, 1);      //constraint_set2_flag
        u(&bs, 1);      //constraint_set3_flag
        u(&bs, 1);      //constraint_set4_flag
        u(&bs, 1);      //constraint_set4_flag
        u(&bs, 2);      //reserved_zero_2bits
        info->level_idc = u(&bs, 8);

        ue(&bs);    //seq_parameter_set_id

        UINT chroma_format_idc = 1;     //摄像机出图大部分格式是4:2:0
        if (info->profile_idc == 100 || info->profile_idc == 110 || info->profile_idc == 122 ||
            info->profile_idc == 244 || info->profile_idc == 44 || info->profile_idc == 83 ||
            info->profile_idc == 86 || info->profile_idc == 118 || info->profile_idc == 128 ||
            info->profile_idc == 138 || info->profile_idc == 139 || info->profile_idc == 134 || info->profile_idc == 135) {
            chroma_format_idc = ue(&bs);
            if (chroma_format_idc == 3) {
                u(&bs, 1);      //separate_colour_plane_flag
            }

            ue(&bs);        //bit_depth_luma_minus8
            ue(&bs);        //bit_depth_chroma_minus8
            u(&bs, 1);      //qpprime_y_zero_transform_bypass_flag
            UINT seq_scaling_matrix_present_flag = u(&bs, 1);
            if (seq_scaling_matrix_present_flag) {
                UINT seq_scaling_list_present_flag[8] = {0};
                for (INT i = 0; i < ((chroma_format_idc != 3) ? 8 : 12); i++) {
                    seq_scaling_list_present_flag[i] = u(&bs, 1);
                    if (seq_scaling_list_present_flag[i]) {
                        if (i < 6) {    //scaling_list(ScalingList4x4[i], 16, UseDefaultScalingMatrix4x4Flag[I])
                        } else {    //scaling_list(ScalingList8x8[i − 6], 64, UseDefaultScalingMatrix8x8Flag[i − 6] )
                        }
                    }
                }
            }
        }

        ue(&bs);        //log2_max_frame_num_minus4
        UINT pic_order_cnt_type = ue(&bs);
        if (pic_order_cnt_type == 0) {
            ue(&bs);        //log2_max_pic_order_cnt_lsb_minus4
        } else if (pic_order_cnt_type == 1) {
            u(&bs, 1);      //delta_pic_order_always_zero_flag
            se(&bs);        //offset_for_non_ref_pic
            se(&bs);        //offset_for_top_to_bottom_field

            UINT num_ref_frames_in_pic_order_cnt_cycle = ue(&bs);
            INT *offset_for_ref_frame = (INT *) malloc((UINT) num_ref_frames_in_pic_order_cnt_cycle * sizeof(INT));
            for (INT i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
                offset_for_ref_frame[i] = se(&bs);
            }
            free(offset_for_ref_frame);
        }

        ue(&bs);      //max_num_ref_frames
        u(&bs, 1);      //gaps_in_frame_num_value_allowed_flag

        UINT pic_width_in_mbs_minus1 = ue(&bs);     //第36位开始
        UINT pic_height_in_map_units_minus1 = ue(&bs);      //47
        UINT frame_mbs_only_flag = u(&bs, 1);

        info->width = (INT) (pic_width_in_mbs_minus1 + 1) * 16;
        info->height = (INT) (pic_height_in_map_units_minus1 + 1) * 16;
//        info->height = (INT)(2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16;

        if (!frame_mbs_only_flag) {
            u(&bs, 1);      //mb_adaptive_frame_field_flag
        }

        u(&bs, 1);     //direct_8x8_inference_flag
        UINT frame_cropping_flag = u(&bs, 1);
        if (frame_cropping_flag) {
            UINT frame_crop_left_offset = ue(&bs);
            UINT frame_crop_right_offset = ue(&bs);
            UINT frame_crop_top_offset = ue(&bs);
            UINT frame_crop_bottom_offset = ue(&bs);

            //See 6.2 Source, decoded, and output picture formats
            INT crop_unit_x = 1;
            INT crop_unit_y = 2 - frame_mbs_only_flag;      //monochrome or 4:4:4
            if (chroma_format_idc == 1) {   //4:2:0
                crop_unit_x = 2;
                crop_unit_y = 2 * (2 - frame_mbs_only_flag);
            } else if (chroma_format_idc == 2) {    //4:2:2
                crop_unit_x = 2;
                crop_unit_y = 2 - frame_mbs_only_flag;
            }

            info->width -= crop_unit_x * (frame_crop_left_offset + frame_crop_right_offset);
            info->height -= crop_unit_y * (frame_crop_top_offset + frame_crop_bottom_offset);
//            info->height = (pic_height_in_map_units_minus1 + 1) * 16 - (frame_crop_top_offset * 2) - (frame_crop_bottom_offset * 2);
        }

        UINT vui_parameters_present_flag = u(&bs, 1);
        if (vui_parameters_present_flag) {
            vui_para_parse(&bs, info);
        }

        ret = 1;
    }
    free(dataBuf);

    return ret;
}

/**
 * c/c++ string turn to java jstring
 */
jstring charToJstring(JNIEnv *env, const char *pat) {
    if (env == NULL) {
        return NULL;
    }

    jclass _strclass = (env)->FindClass("java/lang/String");
    jstring _encode = (env)->NewStringUTF("GB2312");
    jmethodID ctorID = env->GetMethodID(_strclass, "<init>",
                                        "([BLjava/lang/String;)V");
    jbyteArray bytes = env->NewByteArray(strlen(pat));
    env->SetByteArrayRegion(bytes, 0, strlen(pat), (jbyte *) pat);
    jstring jstr = (jstring) env->NewObject(_strclass, ctorID, bytes, _encode);
    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(_strclass);
    env->DeleteLocalRef(_encode);
    return jstr;
}

unsigned char runningFlag = 1;
unsigned char sps_pps[21] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x1E,
                             0x95, 0xA8, 0x28, 0x0F, 0x64, 0x00, 0x00, 0x00, 0x01, 0x68, 0xEE, 0x3C,
                             0x80};

/** 从 sdp 中解析出来的 sps_pps 的大小 */
size_t sps_pps_size = 0;
/** 从 sdp 中解析出来的 sps_pps */
unsigned char sps_pps_from_sdp[128];

int size = 0;
unsigned char *buffer = NULL;

int len = 0;
unsigned char *temp_sps = NULL;
unsigned char *temp_sps_pps = NULL;


// 全局 JNI 环境和对象引用
JavaVM *g_jvm = nullptr;
jobject g_callback_obj = nullptr;
RTSPClient *rtspClient = nullptr;

Boolean isrunning = false;
Boolean force_tcp = true;
Boolean has_sps = true;
Boolean has_pps = true;


/**
 * 从 sdp 中解析出 sps_pps，放在 sps_pps 中，返回长度
 * @param sdp sdp字串
 * @param sps_pps 返回sps_pps，结构是 [分隔符]sps[分隔符]pps，分隔符是{0x00, 0x00, 0x00, 0x01}
 * @return sps_pps 的长度，失败返回 -1
 */
size_t get_sps_pps_from_sdp(std::string sdp, unsigned char *sps_pps) {
    if (sps_pps == nullptr) return -1;
    std::string sdp_prefix = "sprop-parameter-sets";
    /*
     *  参考
     *  https://www.cnblogs.com/wangqiguo/p/4556391.html
        RTSP 响应的SDP的内容中sprop-parameter-sets键值：
        sprop-parameter-sets=Z2QAKq2wpDBSAgFxQWKQPQRWFIYKQEAuKCxSB6CKwpDBSAgFxQWKQPQRTDoUKQNC4oJHMGIemHQoUgaFxQSOYMQ9MOhQpA0LigkcwYh6xEQmIVilsQRWUURJsogxOU4QITKUIEVlCCTYQVhBMJQhMIjGggWQJFaIGBJZBAaEnaMIDwsSWQQKCwsrRBQYOWQweO0YEBZASNAogszlAUAW7/wcFBwMQAABdwAAr8g4AAADAL68IAAAdzWU//+MAAADAF9eEAAAO5rKf//CgA==,aP48sA==;
        其中逗号前面的内容是sps的二进制数据被base64之后的结果
        而逗号后面的内容(不要分号,分号是sdp中键值对的分隔符),是pps的内容
        使用live555中的base64Decode函数分别对这两部分进行反base64解码得到的二进制数据就是h264中的sps pps 的二进制内容
        分别是以67 和 68 开头
    */

    // 从sdp中解析出加密的sps 和 pps，sdp 结构如下
    // ...[prefix][sps][suffix][pps][suffix2|suffix3]...
    // 前缀
    std::string prefix("sprop-parameter-sets=");
    std::string singleSpsPrefix("sprop-sps=");
    std::string singlePpsPrefix("sprop-pps=");
    size_t prefix_head_index = sdp.find(prefix);
    size_t sps_head_index = sdp.find(singleSpsPrefix);
    size_t pps_head_index = sdp.find(singlePpsPrefix);
    LOGE("%s: get_sps_pps_from_sdp sdp=\n%s", __FUNCTION__, sdp.c_str());
    if (prefix_head_index == std::string::npos && sps_head_index == std::string::npos) {
        LOGE("%s: not sprop-parameter-sets >>> invalid sdp prefix_head_index=%d", __FUNCTION__,
             prefix_head_index);
//        return -1;
    } else if (sdp.find(singleSpsPrefix) != std::string::npos &&
               sdp.find(singlePpsPrefix) != std::string::npos) {
        LOGE("%s: not sprop-pps >>>> invalid sdp sps_head_index=%d, pps_head_index=%d",
             __FUNCTION__, sps_head_index, pps_head_index);
        // sps头下表
        sps_head_index = sps_head_index + singleSpsPrefix.size();
        // 后缀
        std::string suffix("; sprop-pps=");
        size_t suffix_head_index = sdp.find(suffix.c_str(), sps_head_index);
        if (suffix_head_index == std::string::npos) {
            // a=fmtp:108 sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB7oAPAgBDlja5JMvTcBAQEEAAAAwAQAAADAZxA; sprop-pps=RAHA8vA8kA==
            //H265 可能头不一样
            LOGE("%s: not sprop-pps >>> invalid sdp, can't parse sps2", __FUNCTION__);
            REQUEST_STREAMING_OVER_TCP = false || force_tcp;
            return -1;
        }
        // sps长度
        size_t sps_size = suffix_head_index - sps_head_index;
        const char *sps_sdp = sdp.substr(sps_head_index, sps_size).c_str();
        LOGE("%s: sps2 %s", __FUNCTION__, sps_sdp);
        // 后缀2
        std::string suffix3("\r");
        pps_head_index = suffix_head_index + suffix.size();
        // pps头下表
        size_t suffix2_head_index = sdp.find(suffix3.c_str(), pps_head_index);
        if (suffix2_head_index == std::string::npos) {
            LOGE("%s: invalid sdp, can't parse pps2", __FUNCTION__);
            REQUEST_STREAMING_OVER_TCP = false || force_tcp;
            return -1;
        }
        // pps长度
        size_t pps_size = suffix2_head_index - pps_head_index;
        const char *pps_sdp = sdp.substr(pps_head_index, pps_size).c_str();
        LOGE("%s: pps2 %s", __FUNCTION__, pps_sdp);

        // 解密 sps 和 pps，并且拼接其阿里
        // 1. 帧分隔符
        char separate[4] = {0x00, 0x00, 0x00, 0x01};
        int sps_pps_size = 0;
        memcpy(sps_pps + sps_pps_size, separate, 4);
        sps_pps_size += 4;
        // 2. sps
        unsigned int result_size = 0;
        unsigned char *p = base64Decode(sps_sdp, result_size);
        memcpy(sps_pps + sps_pps_size, p, result_size);
        sps_pps_size += result_size;
        // 3. 帧分隔符
        memcpy(sps_pps + sps_pps_size, separate, 4);
        sps_pps_size += 4;
        // 4. pps
        result_size = 0;
        p = base64Decode(pps_sdp, result_size);
        memcpy(sps_pps + sps_pps_size, p, result_size);
        sps_pps_size += result_size;

        return sps_pps_size;
    } else {

    }
    // sps头下表
    sps_head_index = prefix_head_index + prefix.size();
    // 后缀
    std::string suffix(",");
    size_t suffix_head_index = sdp.find(suffix.c_str(), sps_head_index);
    if (suffix_head_index == std::string::npos) {
        // a=fmtp:108 sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB7oAPAgBDlja5JMvTcBAQEEAAAAwAQAAADAZxA; sprop-pps=RAHA8vA8kA==
        //H265 可能头不一样
        LOGE("%s: invalid sdp, can't parse sps  sps_pps=%s", __FUNCTION__, sps_pps);
        REQUEST_STREAMING_OVER_TCP = false || force_tcp;
        return -1;
    }
    // sps长度
    size_t sps_size = suffix_head_index - sps_head_index;
    const char *sps_sdp = sdp.substr(sps_head_index, sps_size).c_str();
    LOGE("%s: sps %s", __FUNCTION__, sps_sdp);
    // 后缀2
    std::string suffix2(";");
    std::string suffix3("\r");
    pps_head_index = suffix_head_index + suffix.size();
    // pps头下表
    size_t suffix2_head_index = sdp.find(suffix2.c_str(), pps_head_index);
    if (suffix2_head_index == std::string::npos)
        suffix2_head_index = sdp.find(suffix3.c_str(), pps_head_index);
    if (suffix2_head_index == std::string::npos) {
        LOGE("%s: invalid sdp, can't parse pps", __FUNCTION__);
        REQUEST_STREAMING_OVER_TCP = false || force_tcp;
        return -1;
    }
    // pps长度
    size_t pps_size = suffix2_head_index - pps_head_index;
    const char *pps_sdp = sdp.substr(pps_head_index, pps_size).c_str();
    LOGE("%s: pps %s", __FUNCTION__, pps_sdp);


    // 解密 sps 和 pps，并且拼接其阿里
    // 1. 帧分隔符
    char separate[4] = {0x00, 0x00, 0x00, 0x01};
    int sps_pps_size = 0;
    memcpy(sps_pps + sps_pps_size, separate, 4);
    sps_pps_size += 4;
    // 2. sps
    unsigned int result_size = 0;
    unsigned char *p = base64Decode(sps_sdp, result_size);
    memcpy(sps_pps + sps_pps_size, p, result_size);
    sps_pps_size += result_size;
    // 3. 帧分隔符
    memcpy(sps_pps + sps_pps_size, separate, 4);
    sps_pps_size += 4;
    // 4. pps
    result_size = 0;
    p = base64Decode(pps_sdp, result_size);
    memcpy(sps_pps + sps_pps_size, p, result_size);
    sps_pps_size += result_size;

//    for (int i = 0 ; i <  sps_pps_size ; i ++)
//        LOGE("%s: sps_pps %X", __FUNCTION__, sps_pps[i]);

    return sps_pps_size;
}


// 添加JNIEnv获取函数
JNIEnv *getJNIEnv() {
    JNIEnv *env;
    if (g_jvm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        g_jvm->AttachCurrentThread(&env, NULL);
    }
    return env;
}


#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"
static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString);

void continueAfterSETUP(RTSPClient *rtspClient, int resultCode, char *resultString);

void continueAfterPLAY(RTSPClient *rtspClient, int resultCode, char *resultString);

// Other event handler functions:
void subsessionAfterPlaying(
        void *clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void *clientData, char const *reason);

// called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void *clientData);
// called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// The main streaming routine (for each "rtsp://" URL):
void openURL(UsageEnvironment &env, char const *progName, char const *rtspURL);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient *rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient *rtspClient, int exitCode = 1);


// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class StreamClientState {
public:
    StreamClientState();

    virtual ~StreamClientState();

public:
    MediaSubsessionIterator *iter;
    MediaSession *session;
    MediaSubsession *subsession;
    TaskToken streamTimerTask;
    double duration;
    jobject callback;
};


// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient : public RTSPClient {
public:
    static ourRTSPClient *createNew(UsageEnvironment &env, char const *rtspURL,
                                    int verbosityLevel = 0,
                                    char const *applicationName = NULL,
                                    portNumBits tunnelOverHTTPPortNum = 0);

protected:
    ourRTSPClient(UsageEnvironment &env, char const *rtspURL,
                  int verbosityLevel, char const *applicationName,
                  portNumBits tunnelOverHTTPPortNum);

    // called only by createNew();
    virtual ~ourRTSPClient();

public:
    StreamClientState scs;
};

class DummySink : public MediaSink {
public:
    static DummySink *createNew(UsageEnvironment &env,
                                MediaSubsession &subsession,
                                jobject callback) {
        return new DummySink(env, subsession, callback);
    }

private:
    DummySink(UsageEnvironment &env, MediaSubsession &subsession, jobject callback)
            : MediaSink(env), fSubsession(subsession) {
        // 创建全局引用
        JNIEnv *jniEnv = getJNIEnv();
        m_callback = jniEnv->NewGlobalRef(callback);
        fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
        env << "Failed to get a SDP description: " << "\n";
    }

    virtual ~DummySink() {
        delete[] fReceiveBuffer;
        JNIEnv *jniEnv = getJNIEnv();
        jniEnv->DeleteGlobalRef(m_callback);
    }

    static void afterGettingFrame(void *clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds) {
        DummySink *sink = (DummySink *) clientData;
        sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime,
                                durationInMicroseconds);
    }

    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime, unsigned durationInMicroseconds) {
        JNIEnv *jniEnv = getJNIEnv();
        // 获取回调类和方法ID
        jclass _clazz = jniEnv->GetObjectClass(m_callback);
        jmethodID methodid = jniEnv->GetMethodID(_clazz, "onFrameReceived", "([BJ)V");

//        LOGE("mediumName :%s", fSubsession.mediumName());
        LOGE("fReceiveBuffer[0] :  0x%02X", fReceiveBuffer[0]);
        // only get the video data, exclude audio data.
        if (strcmp(fSubsession.mediumName(), "video") == 0) {
            if (runningFlag == 1) {
                // sps
                if (fReceiveBuffer[0] == 0x67) {
                    LOGE("--------------------------1000------------------------");
                    len = frameSize + 4;
                    temp_sps = (unsigned char *) malloc(len);
                    temp_sps[0] = 0x00;
                    temp_sps[1] = 0x00;
                    temp_sps[2] = 0x00;
                    temp_sps[3] = 0x01;
                    memcpy(temp_sps + 4, fReceiveBuffer, frameSize);

                    sps_info_struct *self = (sps_info_struct *) malloc(sizeof(sps_info_struct));
                    h264_parse_sps(fReceiveBuffer, frameSize, self);
                    LOGE("%s: h264_parse_sps>>>>>>>>>>>>>>>>>>>>>>> w h=[%d,%d], baseline profile,level=[%d,%d], fps=%d",
                         __FUNCTION__, self->width, self->height, self->profile_idc, self->level_idc, self->fps);
                    std::string tmp = "width:" + std::to_string(self->width) + " height: " + std::to_string(self->height);
                    LOGE("--------------------------100003------------------------%s",tmp.c_str());
                    jstring jstr = charToJstring(jniEnv, tmp.c_str());

                    jmethodID methodId = jniEnv->GetMethodID(_clazz, "infoCallBack", "(ILjava/lang/String;)V");
                    jniEnv->CallVoidMethod(m_callback, methodId, 103,jstr);
                    jniEnv->DeleteLocalRef(jstr);

                    free(self);
                    has_sps = true;
                }

                // sps + pps
                if (fReceiveBuffer[0] == 0x68) {
                    LOGE("--------------------------1001------------------------");
                    for (int i = 0; i < frameSize; i++) {
                        LOGE("%s: pps %X", __FUNCTION__, fReceiveBuffer[i]);
                    }
                    int l = len;
                    len = len + frameSize + 4;

                    temp_sps_pps = (unsigned char *) malloc(len);
                    memcpy(temp_sps_pps, temp_sps, l);
                    temp_sps_pps[l + 0] = 0x00;
                    temp_sps_pps[l + 1] = 0x00;
                    temp_sps_pps[l + 2] = 0x00;
                    temp_sps_pps[l + 3] = 0x01;
                    memcpy(temp_sps_pps + l + 4, fReceiveBuffer, frameSize);
                    l = 0;
                    free(temp_sps);
                    temp_sps = NULL;
                    for (int i = 0; i < frameSize; i++) {
                        LOGE("%s: pps 2 %X", __FUNCTION__, temp_sps_pps[i]);
                    }
                    has_pps = true;
                }

                if (fReceiveBuffer[0] == 0x65) {
                    for (int i = 0; i < frameSize; i++) {
                        LOGE("%s: idr %X", __FUNCTION__, fReceiveBuffer[i]);
                    }
                    LOGE("--------------------------1002------------------------");
                    runningFlag = 0;
                    if (has_sps && has_pps) {
                        if (methodid == NULL) {
                            std::string tmp = "can't find method: public void onFrameReceived(byte[], int)";
//                            infoCallBack(jniEnv ,-1, charToJstring(jniEnv, tmp.c_str()));
                            jstring jstr = charToJstring(jniEnv, tmp.c_str());
                            jmethodID methodId = jniEnv->GetMethodID(_clazz, "infoCallBack", "(ILjava/lang/String;)V");
                            jniEnv->CallVoidMethod(m_callback, methodId, -1,jstr);
                            jniEnv->DeleteLocalRef(jstr);
                            LOGE("--------------------------1003------------------------");
                        } else {
                            if (len == 0) {
                                LOGE("--------------------------1003_1------------------------");
                                jbyteArray jbarray = jniEnv->NewByteArray(21);
                                jniEnv->SetByteArrayRegion(jbarray, 0, 21,
                                                           (jbyte *) sps_pps);
                                jniEnv->CallVoidMethod(m_callback, methodid, jbarray, 21);
                                jniEnv->DeleteLocalRef(jbarray);
                            } else {
                                LOGE("--------------------------1003_2------------------------%d", len);
                                jbyteArray jbarray = jniEnv->NewByteArray(len);
                                jniEnv->SetByteArrayRegion(jbarray, 0, len,
                                                           (jbyte *) temp_sps_pps);
                                jniEnv->CallVoidMethod(m_callback, methodid, jbarray,
                                                       len);
                                jniEnv->DeleteLocalRef(jbarray);

                                len = 0;
                                free(temp_sps_pps);
                                temp_sps_pps = NULL;
                                LOGE("--------------------------1004------------------------");
                            }
                        }
                    } else {
                        jbyteArray jbarray = jniEnv->NewByteArray(sps_pps_size);
                        jniEnv->SetByteArrayRegion(jbarray, 0, sps_pps_size, (jbyte *) sps_pps_from_sdp);
                        jniEnv->CallVoidMethod(m_callback, methodid, jbarray, sps_pps_size);
                        jniEnv->DeleteLocalRef(jbarray);
                        LOGE("%s: pps_sps from sdp", __FUNCTION__);
                    }
                    size = frameSize + 4;
                    buffer = (unsigned char *) malloc(size);
                    buffer[0] = 0x00;
                    buffer[1] = 0x00;
                    buffer[2] = 0x00;
                    buffer[3] = 0x01;
                    memcpy(buffer + 4, fReceiveBuffer, frameSize);

                    if (runningFlag == 0 && methodid != NULL) {
                        LOGE("--------------------------1005------------------------");
                        jbyteArray jbarray = jniEnv->NewByteArray(size);
                        jniEnv->SetByteArrayRegion(jbarray, 0, size, (jbyte *) buffer);

                        jniEnv->CallVoidMethod(m_callback, methodid, jbarray, size);
                        jniEnv->DeleteLocalRef(jbarray);
                    }

                    /*
                     常用Nalu_type:
                     0x67 (0 11 00111) SPS          非常重要     type = 7
                     0x68 (0 11 01000) PPS          非常重要     type = 8
                     0x65 (0 11 00101) IDR  关键帧      非常重要      type = 5
                     0x61 (0 10 00001) I帧      重要            type = 1
                     0x41 (0 10 00001) P帧      重要            type = 1
                     0x01 (0 00 00001) B帧      不重要       type = 1
                     0x06 (0 00 00110) SEI  不重要       type = 6
                     */
                }

            } else {
                if (fReceiveBuffer[0] != 0x67 && fReceiveBuffer[0] != 0x68) {
                    LOGE("--------------------------1006------------------------");
                    size = frameSize + 4;
                    buffer = (unsigned char *) malloc(size);
                    buffer[0] = 0x00;
                    buffer[1] = 0x00;
                    buffer[2] = 0x00;
                    buffer[3] = 0x01;
                    memcpy(buffer + 4, fReceiveBuffer, frameSize);

                    if (runningFlag == 0 && methodid != NULL) {
                        LOGE("--------------------------1007------------------------");
                        jbyteArray jbarray = jniEnv->NewByteArray(size);
                        jniEnv->SetByteArrayRegion(jbarray, 0, size, (jbyte *) buffer);

                        jniEnv->CallVoidMethod(m_callback, methodid, jbarray, size);
                        jniEnv->DeleteLocalRef(jbarray);
                    }
                }
            }

            free(buffer);
            size = 0;
            buffer = NULL;
        }

        // Then continue, to request the next frame of data:
        if (!isrunning) {
            shutdownStream(rtspClient);
        } else {
            continuePlaying();
        }
    }

    Boolean continuePlaying() {
        if (fSource == NULL) return False;
        fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                              afterGettingFrame, this,
                              onSourceClosure, this);
        return True;
    }

private:
    u_int8_t *fReceiveBuffer;
    MediaSubsession &fSubsession;
    jobject m_callback;
};


char *getLocalIP();

class RtspClient {

};


#endif //QRTSPCLIENT_RTSPCLIENT_H
