#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h> // pixfmt
#include <errno.h>

#ifdef VERBOSE
int errout(char*str,int err_num)
{
    fprintf(stderr,"%s\n",str);
    return err_num;
}
#else
#define errout(a,b) b
#endif

int save_as_jpg(AVFrame *frame, const char filename[])
{
    int out_finished=0, ret=0;
    FILE * fout;
    AVPacket dst_packet;
    AVCodecContext * out_codec_ctx;
    AVCodec * out_codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    // AV_CODEC_ID_MJPEG AV_CODEC_ID_MJPEGB AV_CODEC_ID_LJPEG AV_CODEC_ID_JPEG2000
    if(!out_codec) return errout("no encoder found",1);
    out_codec_ctx = avcodec_alloc_context3(out_codec);
    if(!out_codec_ctx) fprintf(stderr,"no out codec ctx\n"); // FIXME:
    out_codec_ctx->width = frame->width;
    out_codec_ctx->height = frame->height;
    out_codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    out_codec_ctx->time_base.num = 1;
    out_codec_ctx->time_base.den = 25;

    avcodec_open2(out_codec_ctx,out_codec,NULL);

    memset(&dst_packet,0,sizeof(dst_packet));
    av_init_packet(&dst_packet);
    dst_packet.data = NULL;
    dst_packet.size = 0;

    avcodec_encode_video2(out_codec_ctx,&dst_packet,frame,&out_finished);

    if((fout = fopen(filename,"wb")))
    {
        fwrite(dst_packet.data,1,dst_packet.size,fout);
        fclose(fout);
    }
    else ret = errno; // EROFS;

    avcodec_close(out_codec_ctx);
    av_free_packet(&dst_packet);
    av_free(out_codec_ctx);

    return ret;
}

int main(int argc, char**argv)
{
    AVFormatContext * format = 0;
    AVCodecContext * codec_ctx = 0;
    AVCodec * codec=0;
    AVPacket packet;

    if(argc!=2)
    {
        printf("usage: %s filename\n",argv[0]);
        return EINVAL;
    }

    av_register_all();

    if (0 != avformat_open_input(&format, argv[1], NULL, NULL))//< open video file
        return errout("can't open file",ENOENT);

    if(0>avformat_find_stream_info(format,NULL))  //< check if any stream availaible
        return errout("no video stream vound",1);


    int stream_number = av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    if(AVERROR_STREAM_NOT_FOUND == stream_number) codec_ctx = NULL; // FIXME: further decoding imposible
    else codec_ctx = format->streams[stream_number]->codec;

    codec = avcodec_find_decoder(codec_ctx->codec_id);
    if(!codec)
        return errout("decoder not found\n",1);

    if(avcodec_open2(codec_ctx,codec,NULL)) return 1; // TODO: errlog


    int tottal_frames=0, sec=0;
    int64_t target_pts;
    AVFrame *frame = av_frame_alloc();
    memset(&packet,0,sizeof(packet));
    av_init_packet(&packet);

    while(0 <= av_read_frame(format,&packet))
    {
        if(packet.stream_index==stream_number)
        {
            int frame_finished;
            if(0>=avcodec_decode_video2(codec_ctx,frame,&frame_finished,&packet)) fprintf(stderr,"can't decode\n");
            if(frame_finished)
            {
                char filename[255];

                sprintf(filename,"%s_%d.jpg",argv[1],++tottal_frames); // TODO: truncate name only
                save_as_jpg(frame,filename);
                /// seek +10 sec
                sec += 10;
                target_pts = av_rescale_q(AV_TIME_BASE * sec,
                                      AV_TIME_BASE_Q,
                                      format->streams[stream_number]->time_base);
                av_seek_frame(format,stream_number,target_pts,0);
            }
        }
        av_free_packet(&packet);
    }
    /// EOF reached
    av_frame_free(&frame);
    avformat_close_input(&format);
    printf("tottal frames %d\n",tottal_frames);
    return 0;
}
