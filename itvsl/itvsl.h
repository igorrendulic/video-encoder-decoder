#ifndef _ITVSLLIB_H_
#define _ITVSLLIB_H_

#include <memory>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include "itvslprotocol.pb.h"
#include "video_log.h"

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavdevice/avdevice.h>
#include <inttypes.h>
#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <hiredis/hiredis.h>
#ifdef __cplusplus
}
#endif

using namespace itvsl::protocol::v1beta1;

namespace itvsl
{

    namespace media
    {
        typedef enum FileInputType
        {
            RTSP,
            FILE,
            V4L2
        } FileInputType;

        typedef struct StreamingParams
        {
            /*
             * H264 -> H265
             * Audio -> remuxed (untouched)
             * MP4 - MP4
             */
            //   StreamingParams sp = {0};
            //   sp.copy_audio = 1;
            //   sp.copy_video = 0;
            //   sp.video_codec = "libx265";
            //   sp.codec_priv_key = "x265-params";
            //   sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0";

            /*
             * H264 -> H264 (fixed gop)
             * Audio -> remuxed (untouched)
             * MP4 - MP4
             */
            // StreamingParams sp = {0};
            // sp.copy_audio = 1;
            // sp.copy_video = 0;
            // sp.video_codec = "libx264";
            // sp.codec_priv_key = "x264-params";
            // sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";

            /*
             * H264 -> H264 (fixed gop)
             * Audio -> remuxed (untouched)
             * MP4 - fragmented MP4
             */
            // StreamingParams sp = {0};
            // sp.copy_audio = 1;
            // sp.copy_video = 0;
            // sp.video_codec = "libx264";
            // sp.codec_priv_key = "x264-params";
            // sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
            // sp.muxer_opt_key = "movflags";
            // sp.muxer_opt_value = "frag_keyframe+empty_moov+default_base_moof";

            /*
             * H264 -> H264 (fixed gop)
             * Audio -> AAC
             * MP4 - MPEG-TS
             */
            // StreamingParams sp = {0};
            // sp.copy_audio = 0;
            // sp.copy_video = 0;
            // sp.video_codec = "libx264";
            // sp.codec_priv_key = "x264-params";
            // sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
            // sp.audio_codec = "aac";
            // sp.output_extension = ".ts";

            char copy_video;
            char copy_audio;
            char *video_codec;
            char *audio_codec;
            char *output_extension;
            char *muxer_opt_key;
            char *muxer_opt_value;
            char *codec_priv_key;
            char *codec_priv_value;
        } StreamingParams;

        typedef struct StreamingContext
        {
            AVFormatContext *pFormatContext;
            AVCodec *video_codec;
            AVCodec *audio_codec;
            AVStream *video_stream;
            AVStream *audio_stream;
            AVCodecContext *video_codec_context;
            AVCodecContext *audio_codec_context;
            int video_index = -1;
            int audio_index = -1;
            char *inputFile;

            // PPS and SPS info (so no need to add to stream itself)
            uint8_t *extradata;
            size_t extradatasize;
        } StreamingContext;

        // main Class
        class LibItvsl
        {
            StreamingContext *decodeStreamContext;
            StreamingContext *encodeStreamContext;
            redisContext *redisConn;
            redisReply *redReply;

        public:
            LibItvsl()
            {

                decodeStreamContext = (StreamingContext *)calloc(1, sizeof(StreamingContext));
                decodeStreamContext->audio_index = -1;
                decodeStreamContext->video_index = -1;

                encodeStreamContext = (StreamingContext *)calloc(1, sizeof(StreamingContext));
                encodeStreamContext->audio_index = -1;
                encodeStreamContext->video_index = -1;

                // TODO: instead of redis open fifo file for writing to it: check this; https://www.youtube.com/watch?v=2hba3etpoJg

                redisConn = redisConnect("localhost", 6379);
                if (redisConn->err)
                {
                    printf("Redis connection error: %s\n", redisConn->errstr);
                    exit(1);
                }
            }

            StreamingContext *getDecodingContext();
            StreamingContext *getEncodingContext();

            // openInput populates AVFormatContext
            int openInput(FileInputType it, const char *fileLocation, AVFormatContext **avfc);

            // decode method
            int decode(AVCodecContext *pCodecContext, AVFrame *frame, AVPacket *pkt, SwsContext *sws_ctx, AVFrame *pFrameBGR);

            // fills in the information about the stream
            int fillStreamInfo(AVStream *avs, AVCodec **avc, AVCodecContext **avcc);

            // setting up audio, video codecs, stream indexes, ...
            int prepareDecoder(StreamingContext *sc);

            // preparing copy params
            int prepareCopy(AVFormatContext *avfc, AVStream **avs, AVCodecParameters *decoder_par);

            // converting packets to protobuf format (proto/itvslprotocol.proto)
            CPacket *itvslPacketize(AVPacket *packet);

            VideoFrame *bgrToVideoFrame(AVPacket *pPacket, AVFrame *pFrame, AVFrame *bgrFrame, const char *deviceID);

            // deserialized protobuf packet to AVPAcket (must be deallocated with )
            static AVPacket *deserializeAVPacket(CPacket packet);

            // remuxing packets
            int remux(AVPacket **pkt, AVFormatContext **avfc, AVRational decoder_tb, AVRational encoder_tb);

            int encodeVideo(StreamingContext *decoder, StreamingContext *encoder, AVFrame *input_frame);

            ~LibItvsl()
            {

                avformat_close_input(&decodeStreamContext->pFormatContext);
                avformat_network_deinit();

                avformat_free_context(decodeStreamContext->pFormatContext);

                if (decodeStreamContext->video_codec_context != NULL)
                {
                    avcodec_free_context(&decodeStreamContext->video_codec_context);
                    decodeStreamContext->video_codec_context = NULL;
                }
                if (decodeStreamContext->audio_codec_context != NULL)
                {
                    avcodec_free_context(&decodeStreamContext->audio_codec_context);
                    decodeStreamContext->audio_codec_context = NULL;
                }
                if (encodeStreamContext->video_codec_context != NULL)
                {
                    avcodec_free_context(&encodeStreamContext->video_codec_context);
                    encodeStreamContext->video_codec_context = NULL;
                }
                if (encodeStreamContext->audio_codec_context != NULL)
                {
                    avcodec_free_context(&encodeStreamContext->audio_codec_context);
                    encodeStreamContext->audio_codec_context = NULL;
                }

                free(decodeStreamContext);
                decodeStreamContext = NULL;

                free(encodeStreamContext);
                encodeStreamContext = NULL;

                redisFree(redisConn);
            }
        };
    } // namespace media
};    // namespace itvsl

#endif

// fix temporary array error in c++1x
#ifdef av_err2str
#undef av_err2str
av_always_inline char *av_err2str(int errnum)
{
    static char str[AV_ERROR_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#endif
