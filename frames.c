#include <stdio.h>
#include <errno.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "imagesaver.c"

#ifdef VERBOSE
int errout(char*str, int err_num)
{
	fprintf(stderr, "%s\n", str);
	return err_num;
}
#else
#define errout(a,b) b
#endif


void dump_text_to_rgb(uint8_t *buff, int width, int height, char* str, int x,
		int y, FT_Face face)
{
	uint8_t gray;
	int charnum, bmpx, bmpy, rgbnum;
	FT_UInt glyph_index;
	for (charnum = 0; str[charnum]; charnum++)
	{
		glyph_index = FT_Get_Char_Index(face, str[charnum]);
		FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
		for (bmpy = 0; bmpy < face->glyph->bitmap.rows; bmpy++)
			for (bmpx = 0; bmpx < face->glyph->bitmap.width; bmpx++) // blend
			{
				gray = face->glyph->bitmap.buffer[bmpx
						+ bmpy * face->glyph->bitmap.pitch];
				rgbnum = (x + bmpx) * 3 + (y + bmpy) * width * 3;
				/*buff[rgbnum]   +=  (255-buff[rgbnum])  *(gray/255.0);
				 buff[rgbnum+1] +=  (255-buff[rgbnum+1])*(gray/255.0);
				 buff[rgbnum+2] +=  (255-buff[rgbnum+2])*(gray/255.0);*/
				/// with background
				buff[rgbnum] = buff[rgbnum + 1] = buff[rgbnum + 2] = gray;
			}
		x += face->glyph->bitmap.width;
	}
}

void cut_to_name(char * str)
{
	int i = -1, slash = -1, dot = -1;

	while (str[++i])
		if (str[i] == '/')
			slash = i;
		else if (str[i] == '.')
			dot = i;

	if (dot != -1)
		str[dot] = 0;
	if (slash != -1 && slash < dot)
		memmove(str, &str[slash + 1], dot - slash);
}

int main(int argc, char**argv)
{
	AVFormatContext * format = 0;
	AVCodecContext * codec_ctx = 0;
	AVCodec * codec = 0;
	AVPacket packet;
	struct SwsContext * swsctx_scale2rgb = NULL;

	FT_Library library;
	FT_Face face;

	if (argc < 2)
	{
		printf("usage: %s input_file [output directory]\n", argv[0]);
		return EINVAL;
	}

	FT_Init_FreeType(&library);
	if (FT_New_Face(library, "DejaVuSans.ttf", 0, &face))
		return errout("can't load ./DejaVuSans.ttf", 1);
	FT_Set_Char_Size(face, 0, 16 * 64, 120, 120);

	av_register_all();

	if (0 != avformat_open_input(&format, argv[1], NULL, NULL)) //< open video file
		return errout("can't open file", ENOENT);

	if (0 > avformat_find_stream_info(format, NULL)) //< check if any stream availaible
		return errout("no video stream vound", 1);

	int stream_number = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1,
			NULL, 0);
	if (AVERROR_STREAM_NOT_FOUND == stream_number)
		return errout("format not supported\n", 1);

	else
		codec_ctx = format->streams[stream_number]->codec;

	codec = avcodec_find_decoder(codec_ctx->codec_id);
	if (!codec)
		return errout("decoder not found\n", 1);

	if (avcodec_open2(codec_ctx, codec, NULL))
		return errout("codec not supported\n", 1);

	int frame_count = 0, sec = 0;
	int64_t target_pts;
	AVFrame *frame = av_frame_alloc();
	memset(&packet, 0, sizeof(packet));
	av_init_packet(&packet);

	// prepare filename prefix
	cut_to_name(argv[1]);

	while (0 <= av_read_frame(format, &packet))
	{
		if (packet.stream_index == stream_number)
		{
			int frame_finished;
			if (0
					>= avcodec_decode_video2(codec_ctx, frame, &frame_finished,
							&packet))
				fprintf(stderr, "can't decode\n");
			if (frame_finished)
			{
				char filename[255];
				uint8_t *dst_data[4];
				int dst_linesize[4];

				swsctx_scale2rgb = sws_getCachedContext(swsctx_scale2rgb,
						codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
						codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
						SWS_FAST_BILINEAR, 0, 0, 0);

				av_image_alloc(dst_data, dst_linesize, codec_ctx->width,
						codec_ctx->height, AV_PIX_FMT_RGB24, 1);

				sws_scale(swsctx_scale2rgb,
						(const uint8_t * const *) frame->data, frame->linesize,
						0, codec_ctx->height, dst_data, dst_linesize);

				sprintf(filename, "%02d:%02d:%02d", sec / 60 / 60, sec / 60,
						sec % 60);
				dump_text_to_rgb(dst_data[0], frame->width, frame->height,
						filename, frame->width - 150, 20, face);

				sprintf(filename, "%s/%s_%d.png", argc == 3 ? argv[2] : ".",
						argv[1], ++frame_count);
				/*save_as_jpg(dst_data, dst_linesize, frame->width, frame->height,
						filename);*/
				save_as_image(dst_data, dst_linesize, frame->width, frame->height,
										filename, IMAGE_JPEG); // IMAGE_PNG can be used

				/// seek +10 sec
				sec += 10;
				target_pts = av_rescale_q(AV_TIME_BASE * sec,
				AV_TIME_BASE_Q
				, format->streams[stream_number]->time_base);
				av_seek_frame(format, stream_number, target_pts, 0);
			}
		}
		av_free_packet(&packet);
	}
	/// EOF reached
	sws_freeContext(swsctx_scale2rgb);
	av_frame_free(&frame);
	avformat_close_input(&format);
	printf("tottal frames %d\n", frame_count);
	return 0;
}

