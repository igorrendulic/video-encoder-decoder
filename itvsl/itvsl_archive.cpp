#include <chrono>

#include "itvsl_archive.h"
#include "video_log.h"

using namespace itvsl::archive;
using namespace itvsl::log;

using namespace std::chrono_literals; // ns, us, ms, s, h, etc.
using namespace std::chrono;

// created filename from current ts
static std::string createFilename(AVPacket *pkt)
{
    auto cur_ts = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    std::string filename = std::to_string(cur_ts.count());
    filename += ".mp4";
    return filename;
}

static int segmentStart(LibItvsl *itvsl, StreamingContext *encoder, StreamingContext *decoder, StreamingParams params, std::string filename)
{

    avformat_alloc_output_context2(&encoder->pFormatContext, NULL, NULL, filename.c_str());
    encoder->pFormatContext->debug;

    if (decoder->video_index > -1)
    {
        if (itvsl->prepareCopy(encoder->pFormatContext, &encoder->video_stream, decoder->video_stream->codecpar))
        {
            logging("failed to prepare video copy");
            return -1;
        }
    }
    if (decoder->audio_index > -1)
    {
        if (itvsl->prepareCopy(encoder->pFormatContext, &encoder->audio_stream, decoder->audio_stream->codecpar))
        {
            logging("failed to prepare audio copy");
            return -1;
        }
    }

    if (encoder->pFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
    {
        // Place global headers in extradata instead of every keyframe.
        encoder->pFormatContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if (!(encoder->pFormatContext->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&encoder->pFormatContext->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0)
        {
            logging("could not open the output file: ", filename);
            return -1;
        }
    }

    AVDictionary *muxer_opts = NULL;

    if (params.muxer_opt_key && params.muxer_opt_value)
    {
        av_dict_set(&muxer_opts, params.muxer_opt_key, params.muxer_opt_value, 0);
        av_dict_set(&muxer_opts, "fflags", "-autobsf", 0);
    }

    if (avformat_write_header(encoder->pFormatContext, &muxer_opts) < 0)
    {
        logging("an error occurred when opening output file");
        return -1;
    }
    av_dict_free(&muxer_opts);

    return 0;
}

// end the segment (write_trailer)
static int segmentEnd(StreamingContext *encoder)
{
    // flushing video encoder (NULL frame)
    // writer trailer and close avio
    av_write_frame(encoder->pFormatContext, NULL); /* Flush any buffered data (fragmented mp4) */
    av_write_trailer(encoder->pFormatContext);
    avio_close(encoder->pFormatContext->pb);

    return 0;
}

// write a single packet into a segment (remux)
static int segmentWritePacket(LibItvsl *itvsl, StreamingContext *encoder, StreamingContext *decoder, AVPacket *pkt, int64_t lastPts)
{

    // calculate frame duration
    AVRational time_base = decoder->pFormatContext->streams[pkt->stream_index]->time_base;
    AVRational fps = decoder->pFormatContext->streams[pkt->stream_index]->r_frame_rate;

    int64_t frameDuration = time_base.den / fps.num;
    pkt->duration = frameDuration;

    if (decoder->pFormatContext->streams[pkt->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        // if (!params.copy_video)
        // {
        //     // TODO: refactor to be generic for audio and video (receiving a function pointer to the differences)
        //     if (transcode_video(decoder, encoder, input_packet, input_frame))
        //         return -1;
        //     av_packet_unref(input_packet);
        // }
        // else
        // {

        if (itvsl->remux(&pkt, &encoder->pFormatContext, decoder->video_stream->time_base, encoder->video_stream->time_base))
        {
            return -1;
        }
    }
    else if (decoder->pFormatContext->streams[pkt->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        // if (!sp.copy_audio)
        // {
        //     if (transcode_audio(decoder, encoder, input_packet, input_frame))
        //         return -1;
        //     av_packet_unref(input_packet);
        // }
        // else
        // {
        if (itvsl->remux(&pkt, &encoder->pFormatContext, decoder->audio_stream->time_base, encoder->audio_stream->time_base))
        {
            return -1;
        }
        // }
    }
    return 0;
}

// void ItvslArchive::processMedia(Queue<std::string> &q, LibItvsl &chLib) {
void ItvslArchive::processMedia(Queue<AVPacket> &q, LibItvsl &chLib, std::future<void> future)
{

    LibItvsl *itvsl = (&chLib);
    StreamingContext *decoder = itvsl->getDecodingContext();                             // input stream
    StreamingContext *encoder = (StreamingContext *)calloc(1, sizeof(StreamingContext)); // output stream (segmented files)

    int64_t previous_pts = 0;
    int num_packets = 0;

    StreamingParams params = {0};
    params.copy_audio = 1;
    params.copy_video = 1;

    //     params.video_codec = (char*)"libx264";
    //     params.codec_priv_key = (char*)"x264-params";;
    //     params.codec_priv_value = (char *) "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
    params.muxer_opt_key = (char *)"movflags";
    ;
    params.muxer_opt_value = (char *)"frag_keyframe+empty_moov+default_base_moof";

    while (future.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout)
    {
        // logging("running loop");
        AVPacket obj = q.pop();
        AVPacket *pkt = &obj;

        if (pkt->stream_index > 12345678)
        {
            break;
        }

        bool isKeyFrame = false;

        if (pkt->flags & AV_PKT_FLAG_KEY)
        {
            isKeyFrame = true;
        }

        if (!hasFirstKeyFrame && isKeyFrame)
        {
            hasFirstKeyFrame = true;
        }
        if (!hasFirstKeyFrame)
        {
            // logging("skipped frame, not a kreyframe %d", isKeyFrame);
            av_packet_unref(pkt);
            continue;
        }

        // init everything
        if (isKeyFrame)
        {

            // don't finalize the segment until currentFilanem exists (basically skipping first run, until first segment created, since there is no segment yet to end)
            if (hasFirstFilename)
            {
                logging("segment end: %d", hasFirstFilename);
                num_packets = 0;
                segmentEnd(encoder);
            }

            std::string filename = createFilename(pkt);

            segmentStart(itvsl, encoder, decoder, params, filename);

            hasFirstFilename = true;
        }

        segmentWritePacket(itvsl, encoder, decoder, pkt, last_pts);

        last_pts = pkt->pts;

        av_packet_unref(pkt);
        // av_packet_free(&pkt); // free cloned packet
    }
    logging("exiting archiving thread.clean up ahread");

    if (encoder->video_codec_context != NULL)
    {
        avcodec_free_context(&encoder->video_codec_context);
        encoder->video_codec_context = NULL;
    }
    if (encoder->audio_codec_context != NULL)
    {
        avcodec_free_context(&encoder->audio_codec_context);
        encoder->audio_codec_context = NULL;
    }

    free(encoder);
    encoder = NULL;
}