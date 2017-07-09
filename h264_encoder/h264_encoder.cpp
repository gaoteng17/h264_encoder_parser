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

/*����flush_encoder()����������ʣ�����Ƶ֡���*/
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
		printf("Flush Encoder: �ɹ����룡 ��ǰ֡��\t��С:%5d\n",enc_pkt.size);
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
	FILE *in_file = fopen("../ds_480x272.yuv", "rb");   //YUV�ļ�·��
	int in_w=480,in_h=272;                              //���������ļ������߶�
	int framenum=100;                                   //�������֡��
	const char* out_file = "ds.h264";	                //���h264�ļ� 

	/*ע��FFmpeg�������*/
	av_register_all();
	
	/*��ʼ�����������AVFormatContext*/
	pFormatCtx = avformat_alloc_context();

	/*���������ffmpeg��֧�ֵ�muxer�в������ļ����й�����Container����*/
	fmt = av_guess_format(NULL, out_file, NULL);
	pFormatCtx->oformat = fmt;


	/*������ļ�·��*/
	if (avio_open(&pFormatCtx->pb,out_file, AVIO_FLAG_READ_WRITE) < 0){
		printf("�޷�������ļ�! \n");
		return -1;
	}

	/*�������������AVStream*/
	video_st = avformat_new_stream(pFormatCtx, 0);
	//video_st->time_base.num = 1; 
	//video_st->time_base.den = 25;  

	if (video_st==NULL){
		return -1;
	}

	//�趨��ز���
	pCodecCtx = video_st->codec;                   //�������
	pCodecCtx->codec_id = fmt->video_codec;        //�������id
	pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;    //�����������
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;       //֡��ʽ
	pCodecCtx->width = in_w;                       //���
	pCodecCtx->height = in_h;                      //�߶�
	pCodecCtx->bit_rate = 400000;                  //������
	pCodecCtx->gop_size=250;                       //�����Ļ������С

	pCodecCtx->time_base.num = 1;                  //time_base����
	pCodecCtx->time_base.den = 25;                 //time_base��ĸ

	//H264
	//pCodecCtx->me_range = 16;                    //�˶����İ뾶
	//pCodecCtx->max_qdiff = 4;                    //����������ӱ仯��
	//pCodecCtx->qcompress = 0.6;                  //������ѹ������
	pCodecCtx->qmin = 10;                          //��С����
	pCodecCtx->qmax = 51;                          //�������

	//��ѡ����
	pCodecCtx->max_b_frames=3;                     //���b֡��

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

	//���Ժ���,����ļ���������Ƶ���Ļ�����Ϣ
	av_dump_format(pFormatCtx, 0, out_file, 1);

	pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
	if (!pCodec){
		printf("�޷��ҵ�������! \n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec,&param) < 0){
		printf("�޷��򿪱�����! \n");
		return -1;
	}

	//����AVFrame�ṹ��
	pFrame = av_frame_alloc();
	picture_size = avpicture_get_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
	picture_buf = (uint8_t *)av_malloc(picture_size);
	avpicture_fill((AVPicture *)pFrame, picture_buf, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);

	//д�ļ�ͷ
	avformat_write_header(pFormatCtx,NULL);

	av_new_packet(&pkt,picture_size);

	y_size = pCodecCtx->width * pCodecCtx->height;

	for (int i=0; i<framenum; i++){
		//��ԭʼYUV����
		if (fread(picture_buf, 1, y_size*3/2, in_file) <= 0){
			printf("��ȡԭʼ����ʧ��! \n");
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
		
		//���벿��
		int ret = avcodec_encode_video2(pCodecCtx, &pkt,pFrame, &got_picture);
		if(ret < 0){
			printf("����ʧ��! \n");
			return -1;
		}
		if (got_picture==1){
			printf("�ɹ����룡��ǰ֡: %5d\t��С:%5d\n",framecnt,pkt.size);
			framecnt++;
			pkt.stream_index = video_st->index;
			ret = av_write_frame(pFormatCtx, &pkt);
			av_free_packet(&pkt);
		}
	}

	//�����������ʣ���AVPacket
	int ret = flush_encoder(pFormatCtx,0);
	if (ret < 0) {
		printf("���±�����ʧ�ܣ�\n");
		return -1;
	}

	//д�ļ�β
	av_write_trailer(pFormatCtx);

	//�ͷ��ڴ�ռ�
	if (video_st){
		avcodec_close(video_st->codec);
		av_free(pFrame);
		av_free(picture_buf);
	}
	avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);

	//�ر��ļ�
	fclose(in_file);

	getchar();

	return 0;
}

