#include "video_log.h"

void itvsl::log::logging(const char *fmt, ...)
{
    va_list args;
    fprintf(stderr, "\e[93;3;5LOG: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\e[0m\n");
}

void itvsl::log::log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    char buf[AV_TS_MAX_STRING_SIZE];

    itvsl::log::logging("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d",
                        av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
                        av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
                        av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
                        pkt->stream_index);
}

void itvsl::log::print_timing(AVFormatContext *avf, AVCodecContext *avc, AVStream *avs)
{
    itvsl::log::logging("=================================================");
    // itvsl::log::logging("%s", name);

    itvsl::log::logging("\tAVFormatContext");
    if (avf != NULL)
    {
        itvsl::log::logging("\t\tstart_time=%d duration=%d bit_rate=%d start_time_realtime=%d", avf->start_time, avf->duration, avf->bit_rate, avf->start_time_realtime);
    }
    else
    {
        itvsl::log::logging("\t\t->NULL");
    }

    itvsl::log::logging("\tAVCodecContext");
    if (avc != NULL)
    {
        itvsl::log::logging("\t\tbit_rate=%d ticks_per_frame=%d width=%d height=%d gop_size=%d keyint_min=%d sample_rate=%d profile=%d level=%d ",
                            avc->bit_rate, avc->ticks_per_frame, avc->width, avc->height, avc->gop_size, avc->keyint_min, avc->sample_rate, avc->profile, avc->level);
        itvsl::log::logging("\t\tavc->time_base=num/den %d/%d", avc->time_base.num, avc->time_base.den);
        itvsl::log::logging("\t\tavc->framerate=num/den %d/%d", avc->framerate.num, avc->framerate.den);
        itvsl::log::logging("\t\tavc->pkt_timebase=num/den %d/%d", avc->pkt_timebase.num, avc->pkt_timebase.den);
    }
    else
    {
        itvsl::log::logging("\t\t->NULL");
    }

    itvsl::log::logging("\tAVStream");
    if (avs != NULL)
    {
        itvsl::log::logging("\t\tindex=%d start_time=%d duration=%d ", avs->index, avs->start_time, avs->duration);
        itvsl::log::logging("\t\tavs->time_base=num/den %d/%d", avs->time_base.num, avs->time_base.den);
        itvsl::log::logging("\t\tavs->sample_aspect_ratio=num/den %d/%d", avs->sample_aspect_ratio.num, avs->sample_aspect_ratio.den);
        itvsl::log::logging("\t\tavs->avg_frame_rate=num/den %d/%d", avs->avg_frame_rate.num, avs->avg_frame_rate.den);
        itvsl::log::logging("\t\tavs->r_frame_rate=num/den %d/%d", avs->r_frame_rate.num, avs->r_frame_rate.den);
    }
    else
    {
        itvsl::log::logging("\t\t->NULL");
    }

    itvsl::log::logging("=================================================");
}