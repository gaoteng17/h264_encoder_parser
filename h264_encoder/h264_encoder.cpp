/*
 * h.264 video encoder
 * based on FFmpeg
 * learned from https://github.com/leixiaohua1020/
 * It can encode YUV data to H.264 bitstream. 
 */

#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif
#endif

/*调用flush_encoder()将编码器中剩余的视频帧输出*/
int flush_encoder(AVFormatContext *fmt_ctx,unsigned int stream_index){
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_video2 (fmt_ctx->streams[stream_index]->codec, &enc_pkt,
			NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame){
			ret=0;
			break;
		}
		printf("Flush Encoder: 成功编码！ 当前帧：\t大小:%5d\n",enc_pkt.size);
		ret = av_write_frame(fmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}

int main(int argc, char* argv[])
{
	AVFormatContext* pFormatCtx;
	AVOutputFormat* fmt;
	AVStream* video_st;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket pkt;
	uint8_t* picture_buf;
	AVFrame* pFrame;
	int picture_size;
	int y_size;
	int framecnt=0;
	FILE *in_file = fopen("../ds_480x272.yuv", "rb");   //YUV文件路径
	int in_w=480,in_h=272;                              //定义输入文件宽度与高度
	int framenum=100;                                   //定义编码帧数
	const char* out_file = "ds.h264";	                //输出h264文件 

	/*注册FFmpeg编解码器*/
	av_register_all();
	
	/*初始化输出码流的AVFormatContext*/
	pFormatCtx = avformat_alloc_context();

	/*从所编译的ffmpeg库支持的muxer中查找与文件名有关联的Container类型*/
	fmt = av_guess_format(NULL, out_file, NULL);
	pFormatCtx->oformat = fmt;


	/*打开输出文件路径*/
	if (avio_open(&pFormatCtx->pb,out_file, AVIO_FLAG_READ_WRITE) < 0){
		printf("无法打开输出文件! \n");
		return -1;
	}

	/*创建输出码流的AVStream*/
	video_st = avformat_new_stream(pFormatCtx, 0);
	//video_st->time_base.num = 1; 
	//video_st->time_base.den = 25;  

	if (video_st==NULL){
		return -1;
	}

	//设定相关参数
	pCodecCtx = video_st->codec;                   //编解码器
	pCodecCtx->codec_id = fmt->video_codec;        //编解码器id
	pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;    //编解码器类型
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;       //帧格式
	pCodecCtx->width = in_w;                       //宽度
	pCodecCtx->height = in_h;                      //高度
	pCodecCtx->bit_rate = 400000;                  //比特率
	pCodecCtx->gop_size=250;                       //连续的画面组大小

	pCodecCtx->time_base.num = 1;                  //time_base分子
	pCodecCtx->time_base.den = 25;                 //time_base分母

	//H264
	//pCodecCtx->me_range = 16;                    //运动侦测的半径
	//pCodecCtx->max_qdiff = 4;                    //最大量化因子变化量
	//pCodecCtx->qcompress = 0.6;                  //量化器压缩比率
	pCodecCtx->qmin = 10;                          //最小质量
	pCodecCtx->qmax = 51;                          //最大质量

	//可选参数
	pCodecCtx->max_b_frames=3;                     //最大b帧数

	AVDictionary *param = 0;
	//H.264
	if(pCodecCtx->codec_id == AV_CODEC_ID_H264) {
		av_dict_set(&param, "preset", "slow", 0);
		av_dict_set(&param, "tune", "zerolatency", 0);
		//av_dict_set(&param, "profile", "main", 0);
	}
	//H.265
	if(pCodecCtx->codec_id == AV_CODEC_ID_H265){
		av_dict_set(&param, "preset", "ultrafast", 0);
		av_dict_set(&param, "tune", "zero-latency", 0);
	}

	//调试函数,输出文件的音、视频流的基本信息
	av_dump_format(pFormatCtx, 0, out_file, 1);

	pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
	if (!pCodec){
		printf("无法找到编码器! \n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec,&param) < 0){
		printf("无法打开编码器! \n");
		return -1;
	}

	//分配AVFrame结构体
	pFrame = av_frame_alloc();
	picture_size = avpicture_get_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
	picture_buf = (uint8_t *)av_malloc(picture_size);
	avpicture_fill((AVPicture *)pFrame, picture_buf, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);

	//写文件头
	avformat_write_header(pFormatCtx,NULL);

	av_new_packet(&pkt,picture_size);

	y_size = pCodecCtx->width * pCodecCtx->height;

	for (int i=0; i<framenum; i++){
		//读原始YUV数据
		if (fread(picture_buf, 1, y_size*3/2, in_file) <= 0){
			printf("读取原始数据失败! \n");
			return -1;
		}else if(feof(in_file)){
			break;
		}
		pFrame->data[0] = picture_buf;              // Y
		pFrame->data[1] = picture_buf+ y_size;      // U 
		pFrame->data[2] = picture_buf+ y_size*5/4;  // V
		
		//PTS
		pFrame->pts=i*(video_st->time_base.den)/((video_st->time_base.num)*25);
		int got_picture=0;
		
		//编码部分
		int ret = avcodec_encode_video2(pCodecCtx, &pkt,pFrame, &got_picture);
		if(ret < 0){
			printf("编码失败! \n");
			return -1;
		}
		if (got_picture==1){
			printf("成功编码！当前帧: %5d\t大小:%5d\n",framecnt,pkt.size);
			framecnt++;
			pkt.stream_index = video_st->index;
			ret = av_write_frame(pFormatCtx, &pkt);
			av_free_packet(&pkt);
		}
	}

	//输出编码器中剩余的AVPacket
	int ret = flush_encoder(pFormatCtx,0);
	if (ret < 0) {
		printf("更新编码器失败！\n");
		return -1;
	}

	//写文件尾
	av_write_trailer(pFormatCtx);

	//释放内存空间
	if (video_st){
		avcodec_close(video_st->codec);
		av_free(pFrame);
		av_free(picture_buf);
	}
	avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);

	//关闭文件
	fclose(in_file);

	getchar();

	return 0;
}

