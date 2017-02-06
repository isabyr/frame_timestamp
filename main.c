#include <libavutil/adler32.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavformat/rtsp.h>

int decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt)
{
	int ret;
	*got_frame = 0;
	
	if (pkt) {
		ret = avcodec_send_packet(avctx, pkt);
		if (ret < 0)
			return ret == AVERROR_EOF ? 0 : ret;
	}
	
	ret = avcodec_receive_frame(avctx, frame);
	
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
		return ret;
	
	if (ret >= 0)
		*got_frame = 1;

	return 0;
}

int main(int argc, char *argv[]) {
	AVFormatContext   *pFormatCtx = NULL;
	int               videoStream;
	//AVCodecContext    *pCodecCtxOrig = NULL;
	AVCodecParameters *origin_par = NULL;
	AVCodecContext    *pCodecCtx = NULL;
	AVCodec           *pCodec = NULL;
	//AVFrame           *pFrame = NULL;
	AVFrame           *pFrameRGB = NULL;
	AVPacket          packet;
	int               numBytes;
	int               result;
	uint8_t           *buffer = NULL;

	if(argc < 2) {
		printf("Please provide a URI for RTSP source\n");
		return -1;
	}
	av_register_all();

	avformat_network_init();

	result = avformat_open_input(&pFormatCtx, argv[1], NULL, NULL);
	if(result < 0)
		return -1; // Couldn't open URI

	result = avformat_find_stream_info(pFormatCtx, NULL);
	if(result < 0)
		return -1; // Couldn't find stream information

	videoStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if(videoStream==-1)
		return -1; // Didn't find a video stream

	/*pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;
	pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);*/

	origin_par = pFormatCtx->streams[videoStream]->codecpar;
	pCodec = avcodec_find_decoder(origin_par->codec_id);
	
	if(pCodec==NULL)
		return -1; // Unsupported codec

	pCodecCtx = avcodec_alloc_context3(pCodec);

	if(pCodecCtx==NULL)
		return -1; //Can't allocate decoder context

	result = avcodec_parameters_to_context(pCodecCtx, origin_par);
	if(result)
		return -1; //Can't copy decoder context
	
	/*if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -1; // Error copying codec context
	}*/

	result = avcodec_open2(pCodecCtx, pCodec, NULL);
	if(result < 0)
		return -1; // Could not open decoder

	//pFrame=av_frame_alloc();

	pFrameRGB=av_frame_alloc();
	if(pFrameRGB==NULL)
		return -1; //Can't allocate frame

	/*numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));*/
	numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 16);
	buffer = av_malloc(numBytes);

	if(!buffer)
		return -1; //Can't allocate buffer

	//avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
	//pCodecCtx->width, pCodecCtx->height);
	av_init_packet(&packet);

	uint32_t last_rtcp_ts = 0;
	uint64_t last_ntp_time = 0;
	double base_time = 0;

	while(av_read_frame(pFormatCtx, &packet)>=0) {
		// Is this a packet from the video stream?
		if(packet.stream_index==videoStream) {
			// Decode video frame
			int got_frame;
			//decode(pCodecCtx, pFrame, &got_frame, &packet);
			decode(pCodecCtx, pFrameRGB, &got_frame, &packet);

			if(got_frame){
				
				RTSPState* rtsp_state = (RTSPState*) pFormatCtx->priv_data;
				RTSPStream* rtsp_stream = rtsp_state->rtsp_streams[0];
				RTPDemuxContext* rtp_demux_context = (RTPDemuxContext*) rtsp_stream->transport_priv;

				uint32_t new_rtcp_ts = rtp_demux_context->last_rtcp_timestamp;

				uint32_t timestamp = rtp_demux_context->timestamp;
				if(new_rtcp_ts != last_rtcp_ts){
					last_rtcp_ts=new_rtcp_ts;
					last_ntp_time = rtp_demux_context->last_rtcp_ntp_time;
					uint32_t seconds = (uint32_t) ((last_ntp_time >> 32) & 0xffffffff)-2208988800;
					uint32_t fraction  = (uint32_t) (last_ntp_time & 0xffffffff);
					double useconds = ((double) fraction / 0xffffffff);
					base_time = seconds+useconds;
					printf("baseline changed\n");
				}
				printf("frame id: %d\ttimestamp: %.6f \n", (timestamp/3600), (base_time+(timestamp-last_rtcp_ts)/3600*0.04));

			} else{
				printf("no frame!!!\n");
			}
		}

		av_packet_unref(&packet);
		av_init_packet(&packet);
	}

	av_packet_unref(&packet);
	av_frame_free(&pFrameRGB);

	//av_frame_free(&pFrame);

	avcodec_close(pCodecCtx);
	//avcodec_close(pCodecCtxOrig);
	avformat_close_input(&pFormatCtx);
	avcodec_free_context(&pCodecCtx);
	av_freep(buffer);
	return 0;
}
