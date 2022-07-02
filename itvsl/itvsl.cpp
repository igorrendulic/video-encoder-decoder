#include "itvsl.h"
#include <fstream>

using namespace itvsl::media;
using namespace itvsl::log;

StreamingContext *LibItvsl::getDecodingContext()
{
    return decodeStreamContext;
}

StreamingContext *LibItvsl::getEncodingContext()
{
    return encodeStreamContext;
}

// openInput opens up a file (rtsp, v4l2, file)
int LibItvsl::openInput(FileInputType it, const char *in_filename, AVFormatContext **avfc)
{
    // TODO: remove this until more supported

    AVDictionary *options = NULL;
    switch (it)
    {
    case RTSP:
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        break;
    case V4L2:
        // framerate needs to set before opening the v4l2 device
        av_dict_set(&options, "framerate", "30", 0);
        // This will not work if the camera does not support h264. In that case
        // remove this line. I wrote this for Raspberry Pi where the camera driver
        // can stream h264.
        // av_dict_set(&options, "input_format", "h264", 0);
        av_dict_set(&options, "video_size", "640x480", 0);

        break;
    case FILE:
        break;
    default:
        break;
    };

    avformat_network_init();
    avdevice_register_all();
    // avcodec_register_all();

    auto ifmt = av_find_input_format("v4l2");
    if (!ifmt)
    {
        logging("Cannot find input format");
        exit(1);
    }

    *avfc = avformat_alloc_context();
    if (!*avfc)
    {
        logging("failed to alloc memory for format");
        return -1;
    }

    if (avformat_open_input(avfc, in_filename, NULL, &options) != 0)
    {
        logging("failed to open input file %s", in_filename);
        return -1;
    }

    if (avformat_find_stream_info(*avfc, NULL) < 0)
    {
        logging("failed to get stream info");
        return -1;
    }

    return 0;
}

int LibItvsl::fillStreamInfo(AVStream *avs, AVCodec **avc, AVCodecContext **avcc)
{
    *avc = avcodec_find_decoder(avs->codecpar->codec_id);
    if (!*avc)
    {
        logging("failed to find the codec");
        return -1;
    }

    *avcc = avcodec_alloc_context3(*avc);
    if (!*avcc)
    {
        logging("failed to alloc memory for codec context");
        return -1;
    }

    if (avcodec_parameters_to_context(*avcc, avs->codecpar) < 0)
    {
        logging("failed to fill codec context");
        return -1;
    }

    if (avcodec_open2(*avcc, *avc, NULL) < 0)
    {
        logging("failed to open codec");
        return -1;
    }

    return 0;
}

int LibItvsl::prepareDecoder(StreamingContext *sc)
{
    uint8_t *videoextradata = NULL;
    size_t videoextradatasize = 0;

    uint8_t *audioextradata = NULL;
    size_t audioextradatasize = 0;

    for (int i = 0; i < sc->pFormatContext->nb_streams; i++)
    {
        if (sc->pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoextradata = sc->pFormatContext->streams[i]->codecpar->extradata;

            videoextradatasize = sc->pFormatContext->streams[i]->codecpar->extradata_size;

            sc->video_stream = sc->pFormatContext->streams[i];
            sc->video_index = i;

            if (fillStreamInfo(sc->video_stream, &sc->video_codec, &sc->video_codec_context))
            {
                return -1;
            }
        }
        else if (sc->pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioextradata = sc->pFormatContext->streams[i]->codecpar->extradata;

            audioextradatasize = sc->pFormatContext->streams[i]->codecpar->extradata_size;

            sc->audio_stream = sc->pFormatContext->streams[i];
            sc->audio_index = i;

            if (fillStreamInfo(sc->audio_stream, &sc->audio_codec, &sc->audio_codec_context))
            {
                return -1;
            }
        }
        else
        {
            logging("skipping streams other than audio and video");
        }
    }

    return 0;
}

int LibItvsl::prepareCopy(AVFormatContext *avfc, AVStream **avs, AVCodecParameters *decoder_par)
{
    *avs = avformat_new_stream(avfc, NULL);
    avcodec_parameters_copy((*avs)->codecpar, decoder_par);
    return 0;
}

int LibItvsl::remux(AVPacket **pkt, AVFormatContext **avfc, AVRational decoder_tb, AVRational encoder_tb)
{
    av_packet_rescale_ts(*pkt, decoder_tb, encoder_tb);
    if (av_interleaved_write_frame(*avfc, *pkt) < 0)
    {
        logging("error while copying stream packet");
        return -1;
    }
    return 0;
}

// re-creating the original AVPacket
// check this: https://github.com/GPUOpen-LibrariesAndSDKs/AMF/issues/113
static AVPacket *deserializeAVPAcket(CPacket packet)
{
    std::vector<uint8_t> pktD(packet.data().begin(), packet.data().end());
    uint8_t *pktData = pktD.data();
    size_t pktDataSize = sizeof(pktData);

    AVPacket *pkt = av_packet_alloc();

    uint8_t *side_data = (uint8_t *)packet.side_data().c_str();
    size_t side_data_size = sizeof(*side_data);

    // AV_MALLOC is important here!
    pkt->data = (uint8_t *)av_malloc(pktD.size());
    memcpy(pkt->data, pktD.data(), pktD.size());

    pkt->size = pktD.size();
    pkt->dts = packet.dts();
    pkt->pts = packet.pts();
    pkt->pos = packet.pos();
    pkt->stream_index = packet.stream_id();
    pkt->flags = packet.flags();
    pkt->duration = packet.duration();

    if (pkt->pts == AV_NOPTS_VALUE)
    {
        pkt->pts = 0;
        pkt->dts = pkt->pts;
    }

    return pkt;
}

// itvslPacketize - converting a packet to protobuf packet to be transfered or shared
CPacket *LibItvsl::itvslPacketize(AVPacket *packet)
{
    CPacket *dstPacket = new CPacket();

    int side_size = 0;
    uint8_t *side = av_packet_get_side_data(packet, AV_PKT_DATA_NEW_EXTRADATA, &side_size);
    // logging("side data size: %d", sizeof(side));

    // making copy of packet data
    char *cptr = reinterpret_cast<char *>(new uint8_t[(packet->size + AV_INPUT_BUFFER_PADDING_SIZE)]);
    memcpy(cptr, packet->data, packet->size + AV_INPUT_BUFFER_PADDING_SIZE);

    dstPacket->set_stream_id(0);
    dstPacket->set_data(cptr, packet->size + AV_INPUT_BUFFER_PADDING_SIZE);
    dstPacket->set_duration(packet->duration);
    dstPacket->set_pts(packet->pts);
    dstPacket->set_dts(packet->dts);
    dstPacket->set_size(packet->size);
    dstPacket->set_flags(packet->flags);
    dstPacket->set_side_data(side, side_size);

    free(side);
    free(cptr);

    return dstPacket;
}

VideoFrame *LibItvsl::bgrToVideoFrame(AVPacket *pPacket, AVFrame *pFrame, AVFrame *bgrFrame, const char *deviceID)
{
    VideoFrame *dstFrame = new VideoFrame();

    ShapeProto *shape = new ShapeProto();

    ShapeProto_Dim *dimHeight = shape->add_dim();
    dimHeight->set_name("height");
    dimHeight->set_size(bgrFrame->height);

    ShapeProto_Dim *dimWidth = shape->add_dim();
    dimWidth->set_name("width");
    dimWidth->set_size(bgrFrame->width);

    ShapeProto_Dim *dimdepth = shape->add_dim();
    dimdepth->set_size(3);

    // char filebuf[256];
    // snprintf(filebuf, sizeof filebuf, "%s%d%s", "out_", pPacket->dts, ".rgb");
    // std::FILE *output=fopen(filebuf,"wb+");

    // fwrite(bgrFrame->data[0],(pFrame->width)*(pFrame->height)*3,1,output);
    // std::fclose(output);

    size_t rgb_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, bgrFrame->width, bgrFrame->height, 1);
    uint8_t *dst_data = (uint8_t *)(av_malloc(rgb_size));
    av_image_copy_to_buffer(dst_data, rgb_size, (const uint8_t **)bgrFrame->data, bgrFrame->linesize, AV_PIX_FMT_BGR24, bgrFrame->width, bgrFrame->height, 1);

    // for (int i=0; i<rgb_size-1; i++) {
    //     uint8_t c = (uint8_t)(rand() % 100 + 1);
    //     dst_data[i] = c;
    // }

    dstFrame->set_dts(pPacket->dts);
    dstFrame->set_pts(pPacket->pts);
    dstFrame->set_width(bgrFrame->width);
    dstFrame->set_height(bgrFrame->height);
    dstFrame->set_is_keyframe(1);
    dstFrame->set_device_id(deviceID);
    dstFrame->set_allocated_shape(shape);
    dstFrame->set_data(dst_data, rgb_size);

    // dstFrame.set_data(cptr, sizeof(bgrFrame->data));
    // dstFrame->set_pix_fmt(pt);

    int len = dstFrame->ByteSize();
    char *buf = new char[len];
    dstFrame->SerializeToArray(buf, len);

    printf("DTS frame: %d:%d\n", dstFrame->width(), dstFrame->height());

    // TODO: store to fifo pipe serialized protobuf file!
    // redReply = (redisReply *)redisCommand(redisConn, "xadd %s MAXLEN ~ 10 * %s %b", deviceID, "data", buf, len);
    // printf("KEY: %s\n", redReply->str);
    // freeReplyObject(redReply);

    free(buf);
    free(dst_data);
    delete dstFrame;

    return 0;
}

int LibItvsl::decode(AVCodecContext *pCodecContext, AVFrame *pFrame, AVPacket *pPacket, SwsContext *sws_ctx, AVFrame *pFrameBGR)
{
    int response = avcodec_send_packet(pCodecContext, pPacket);
    if (response < 0)
    {
        logging("Error while sending a packet to decoder: %s", av_err2str(response));
        return response;
    }

    while (response >= 0)
    {
        response = avcodec_receive_frame(pCodecContext, pFrame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
        {
            break;
        }
        else if (response < 0)
        {
            logging("Error while receiving a frame from the decoder: %s", av_err2str(response));
            return response;
        }
        if (response >= 0)
        {

            sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data, pFrame->linesize, 0, pCodecContext->height, pFrameBGR->data, pFrameBGR->linesize);
            logging("Frame %d (type=%c, size=%d bytes) pts %d key_frame %d [DTS %d]",
                    pCodecContext->frame_number,
                    av_get_picture_type_char(pFrameBGR->pict_type),
                    pFrameBGR->pkt_size,
                    pFrameBGR->pts,
                    pFrameBGR->key_frame,
                    pFrameBGR->coded_picture_number,
                    pFrameBGR->pkt_dts);

            // tb = pCodecContext->time_base.num
            bgrToVideoFrame(pPacket, pFrame, pFrameBGR, "test");

            // char frame_filename[1024];
            // snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", pCodecContext->frame_number);
            // std::FILE *f;

            // int i;
            // f = fopen(frame_filename, "w");

            // fprintf(f, "P5\n%d %d\n%d\n", pFrame->width, pFrame->height, 255);
            // for (i=0; i<pFrame->height; i++) {
            //     fwrite(pFrame->data[0] + i * pFrame->linesize[0], 1, pFrame->width, f);
            // }
            // fclose(f);
        }
    }

    return 0;
}

int LibItvsl::encodeVideo(StreamingContext *decoder, StreamingContext *encoder, AVFrame *input_frame)
{
    if (input_frame)
        input_frame->pict_type = AV_PICTURE_TYPE_NONE;

    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet)
    {
        logging("could not allocate memory for output packet");
        return -1;
    }

    int response = avcodec_send_frame(encoder->video_codec_context, input_frame);
    while (response >= 0)
    {
        response = avcodec_receive_packet(encoder->video_codec_context, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
        {
            break;
        }
        else if (response < 0)
        {
            logging("Error while receiving packet from encoder: %s", av_err2str(response));
            return -1;
        }
        output_packet->stream_index = decoder->video_index;
        output_packet->duration = encoder->video_stream->time_base.den / encoder->video_stream->time_base.num / decoder->video_stream->avg_frame_rate.num * decoder->video_stream->avg_frame_rate.den;
        av_packet_rescale_ts(output_packet, decoder->video_stream->time_base, encoder->video_stream->time_base);
        response = av_interleaved_write_frame(encoder->pFormatContext, output_packet);
        if (response != 0)
        {
            logging("Error %d while receiving packet from decoder: %s", response, av_err2str(response));
            return -1;
        }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}