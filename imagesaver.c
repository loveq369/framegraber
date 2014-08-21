#include <stdio.h>
#include <errno.h>
#include <libavutil/pixfmt.h>

#define IMAGE_JPEG 1
#define IMAGE_PNG  2

int save_as_image(uint8_t **rgbdata, int *linesize, int w, int h,
		const char filename[], int fileformat)
{
	int out_finished = 0, ret = 0;
	FILE * fout;
	AVPacket dst_packet;
	AVCodecContext * out_codec_ctx;
	AVFrame * frame;
	enum AVCodecID codec_id;
	enum AVPixelFormat pixfmt;
	AVCodec * out_codec;
	char fnm[1024];
	switch (fileformat)
	{
	case IMAGE_JPEG:
		codec_id = AV_CODEC_ID_MJPEG;
		pixfmt = AV_PIX_FMT_YUVJ420P; // TODO: update
		sprintf(fnm, "%s.jpg", filename);
		break;
	case IMAGE_PNG:
		codec_id = AV_CODEC_ID_PNG;
		pixfmt = AV_PIX_FMT_RGB24;
		sprintf(fnm, "%s.png", filename);
		break;
	default:
		return 1; //TODO: valid errcode
	}
	out_codec = avcodec_find_encoder(codec_id);
	if (!out_codec)
		return errout("no encoder found", 1);
	if (!(out_codec_ctx = avcodec_alloc_context3(out_codec)))
	{
		avcodec_close(out_codec_ctx);
		return errout("codec can't be opened\n", 1);
	}

	uint8_t *yuv_data[4] =
	{ 0 };
	int dst_linesize[4] =
	{ 0 };
	struct SwsContext * sws_ctx = NULL;

	sws_ctx = sws_getCachedContext(sws_ctx, w, h, AV_PIX_FMT_RGB24, w, h,
			pixfmt, SWS_FAST_BILINEAR, 0, 0, 0);

	av_image_alloc(yuv_data, dst_linesize, w, h, pixfmt, 16);

	sws_scale(sws_ctx, (const uint8_t * const *) rgbdata, linesize, 0, h,
			yuv_data, dst_linesize);

	frame = av_frame_alloc();
	avpicture_fill((AVPicture*) frame, yuv_data[0], pixfmt, w, h);

	out_codec_ctx->width = w;
	out_codec_ctx->height = h;
	out_codec_ctx->pix_fmt = pixfmt;
	out_codec_ctx->time_base.num = 1;
	out_codec_ctx->time_base.den = 25;

	avcodec_open2(out_codec_ctx, out_codec, NULL);

	memset(&dst_packet, 0, sizeof(dst_packet));
	av_init_packet(&dst_packet);
	dst_packet.data = NULL;
	dst_packet.size = 0;

	avcodec_encode_video2(out_codec_ctx, &dst_packet, frame, &out_finished);

	if ((fout = fopen(fnm, "wb")))
	{
		fwrite(dst_packet.data, 1, dst_packet.size, fout);
		fclose(fout);
	}
	else
		ret = errno; // EROFS;

	av_freep(&yuv_data[0]);
	av_frame_free(&frame);
	avcodec_close(out_codec_ctx);
	av_free_packet(&dst_packet);
	av_free(out_codec_ctx);

	return ret;
}

__attribute((deprecated)) int save_as_jpg(uint8_t **rgbdata, int *linesize,
		int w, int h, const char filename[])
{
	int out_finished = 0, ret = 0;
	FILE * fout;
	AVPacket dst_packet;
	AVCodecContext * out_codec_ctx;
	AVFrame * frame;
	enum AVCodecID codec_id;
	AVCodec * out_codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);

	out_codec = avcodec_find_encoder(codec_id);
	// AV_CODEC_ID_MJPEG AV_CODEC_ID_MJPEGB AV_CODEC_ID_LJPEG AV_CODEC_ID_JPEG2000
	if (!out_codec)
		return errout("no encoder found", 1);
	if (!(out_codec_ctx = avcodec_alloc_context3(out_codec)))
	{
		avcodec_close(out_codec_ctx);
		return errout("codec can't be opened\n", 1);
	}

	uint8_t *yuv_data[4] =
	{ 0 };
	int dst_linesize[4] =
	{ 0 };
	struct SwsContext * swsctx_rgb2yuv = NULL;

	swsctx_rgb2yuv = sws_getCachedContext(swsctx_rgb2yuv, w, h,
			AV_PIX_FMT_RGB24, w, h, AV_PIX_FMT_YUVJ420P, // deprecated. alternative incompitable with jpeg
			SWS_FAST_BILINEAR, 0, 0, 0);

	av_image_alloc(yuv_data, dst_linesize, w, h, AV_PIX_FMT_YUVJ420P, 16);

	sws_scale(swsctx_rgb2yuv, (const uint8_t * const *) rgbdata, linesize, 0, h,
			yuv_data, dst_linesize);

	frame = av_frame_alloc();
	avpicture_fill((AVPicture*) frame, yuv_data[0], AV_PIX_FMT_YUVJ420P, w, h);

	out_codec_ctx->width = w;
	out_codec_ctx->height = h;
	out_codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
	out_codec_ctx->time_base.num = 1;
	out_codec_ctx->time_base.den = 25;

	avcodec_open2(out_codec_ctx, out_codec, NULL);

	memset(&dst_packet, 0, sizeof(dst_packet));
	av_init_packet(&dst_packet);
	dst_packet.data = NULL;
	dst_packet.size = 0;

	avcodec_encode_video2(out_codec_ctx, &dst_packet, frame, &out_finished);

	if ((fout = fopen(filename, "wb")))
	{
		fwrite(dst_packet.data, 1, dst_packet.size, fout);
		fclose(fout);
	}
	else
		ret = errno; // EROFS;

	av_freep(&yuv_data[0]);
	av_frame_free(&frame);
	avcodec_close(out_codec_ctx);
	av_free_packet(&dst_packet);
	av_free(out_codec_ctx);

	return ret;
}

__attribute((deprecated)) int save_as_png(uint8_t **rgbdata, int *linesize,
		int w, int h, const char filename[])
{
	int out_finished = 0, ret = 0;
	FILE * fout;
	AVPacket dst_packet;
	AVCodecContext * out_codec_ctx;
	AVFrame * frame;
	AVCodec * out_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
	// AV_CODEC_ID_MJPEG AV_CODEC_ID_MJPEGB AV_CODEC_ID_LJPEG AV_CODEC_ID_JPEG2000
	if (!out_codec)
		return errout("no encoder found", 1);
	if (!(out_codec_ctx = avcodec_alloc_context3(out_codec)))
	{
		avcodec_close(out_codec_ctx);
		return errout("codec can't be opened\n", 1);
	}

	uint8_t *yuv_data[4] =
	{ 0 };
	int dst_linesize[4] =
	{ 0 };
	struct SwsContext * swsctx_rgb2yuv = NULL;

	swsctx_rgb2yuv = sws_getCachedContext(swsctx_rgb2yuv, w, h,
			AV_PIX_FMT_RGB24, w, h, AV_PIX_FMT_RGB24, // deprecated. alternative incompitable with jpeg
			SWS_FAST_BILINEAR, 0, 0, 0);

	av_image_alloc(yuv_data, dst_linesize, w, h, AV_PIX_FMT_RGB24, 16);

	sws_scale(swsctx_rgb2yuv, (const uint8_t * const *) rgbdata, linesize, 0, h,
			yuv_data, dst_linesize);

	frame = av_frame_alloc();
	avpicture_fill((AVPicture*) frame, yuv_data[0], AV_PIX_FMT_RGB24, w, h);

	out_codec_ctx->width = w;
	out_codec_ctx->height = h;
	out_codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;
	out_codec_ctx->time_base.num = 1;
	out_codec_ctx->time_base.den = 25;

	avcodec_open2(out_codec_ctx, out_codec, NULL);

	memset(&dst_packet, 0, sizeof(dst_packet));
	av_init_packet(&dst_packet);
	dst_packet.data = NULL;
	dst_packet.size = 0;

	avcodec_encode_video2(out_codec_ctx, &dst_packet, frame, &out_finished);

	if ((fout = fopen(filename, "wb")))
	{
		fwrite(dst_packet.data, 1, dst_packet.size, fout);
		fclose(fout);
	}
	else
		ret = errno; // EROFS;

	av_freep(&yuv_data[0]);
	av_frame_free(&frame);
	avcodec_close(out_codec_ctx);
	av_free_packet(&dst_packet);
	av_free(out_codec_ctx);

	return ret;
}
