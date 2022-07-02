#ifndef _ITVSLLIB_LOG_H_
#define _ITVSLLIB_LOG_H_

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <inttypes.h>
#include <libavutil/timestamp.h>
#ifdef __cplusplus
}
#endif

namespace itvsl
{
    namespace log
    {
        void logging(const char *fmt, ...);
        void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt);
        void print_timing(AVFormatContext *avf, AVCodecContext *avc, AVStream *avs);

#undef av_err2str
#define av_err2str(errnum) \
    av_make_error_string((char *)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)
#undef av_ts2timestr
#define av_ts2timestr(ts, tb) \
    av_ts_make_time_string((char *)__builtin_alloca(AV_TS_MAX_STRING_SIZE), ts, tb)
#undef av_ts2str
#define av_ts2str(ts) \
    av_ts_make_string((char *)__builtin_alloca(AV_TS_MAX_STRING_SIZE), ts)
    }; // namespace log
};     // namespace itvsl

#endif