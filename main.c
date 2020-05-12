#include <libavutil/imgutils.h>
#include <libavformat/rtsp.h>
#include <stdio.h>



double ntp_timestamp(AVFormatContext *pFormatCtx, uint32_t *last_rtcp_ts, double *base_time) {
	RTSPState* rtsp_state = (RTSPState*) pFormatCtx->priv_data;
	RTSPStream* rtsp_stream = rtsp_state->rtsp_streams[0];
	RTPDemuxContext* rtp_demux_context = (RTPDemuxContext*) rtsp_stream->transport_priv;

	/*
	printf("timestamp:                %d\n", rtp_demux_context->timestamp);
	printf("base_timestamp:           %d\n", rtp_demux_context->base_timestamp);
	printf("cur_timestamp:            %d\n", rtp_demux_context->cur_timestamp);
	printf("last_rtcp_ntp_time:       %ld\n", rtp_demux_context->last_rtcp_ntp_time);
	printf("last_rtcp_reception_time: %ld\n", rtp_demux_context->last_rtcp_reception_time);
	printf("first_rtcp_ntp_time:      %ld\n", rtp_demux_context->first_rtcp_ntp_time);
	printf("last_rtcp_timestamp:      %d\n", rtp_demux_context->last_rtcp_timestamp);

	printf("diff: %d\n",(rtp_demux_context->timestamp-rtp_demux_context->base_timestamp));
	printf("====================================\n");*/


	//ntp time extraction for DAHUA Cameras

	uint32_t new_rtcp_ts = rtp_demux_context->last_rtcp_timestamp;
	uint64_t last_ntp_time = 0;

	if(new_rtcp_ts != *last_rtcp_ts){
		*last_rtcp_ts=new_rtcp_ts;
		last_ntp_time = rtp_demux_context->last_rtcp_ntp_time;
		uint32_t seconds = ((last_ntp_time >> 32) & 0xffffffff)-2208988800;
		uint32_t fraction  = (last_ntp_time & 0xffffffff);
		double useconds = ((double) fraction / 0xffffffff);
		*base_time = seconds+useconds;
	}

	int32_t d_ts = rtp_demux_context->timestamp-*last_rtcp_ts;
	return *base_time+d_ts/90000.0;
}


int decode(int* got_frame, AVFrame* pFrame, AVCodecContext* pCodecCtx, AVPacket* packet) {
	*got_frame = 0;

	int ret = avcodec_send_packet(pCodecCtx, packet);

	if (ret < 0)
		return ret == AVERROR_EOF ? 0 : ret;

	ret = avcodec_receive_frame(pCodecCtx, pFrame);

	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
		return ret;

	if (ret >= 0)
		*got_frame = 1;

	return 0;
}



int main(int argc, char *argv[]) {
	//av_log_set_level(AV_LOG_DEBUG);
	av_log_set_level(0);

	AVCodecParameters * origin_par = NULL;
	AVCodec           * pCodec = NULL;
	AVFormatContext   * pFormatCtx = NULL;
	AVCodecContext    * pCodecCtx = NULL;
	AVFrame           * pFrame = NULL;
	AVPacket          packet;
	int               videoStream = -1;
	uint32_t					frame_size = 1280*720*1.5;

	uint8_t network_mode = 0;
	int result;
	char* rtsp_source = "rtsp://admin:12345@10.50.0.59:554/Streaming/Channels/1";

	avformat_network_init();

	av_init_packet(&packet);

	AVDictionary* opts = NULL;
	av_dict_set(&opts, "stimeout", "5000000", 0);

	if (network_mode == 0) {
		av_log(NULL, AV_LOG_DEBUG, "Opening UDP stream\n");
		result = avformat_open_input(&pFormatCtx, rtsp_source, NULL, &opts);
	} else {
		av_dict_set(&opts, "rtsp_transport", "tcp", 0);

		av_log(NULL, AV_LOG_DEBUG, "Opening TCP stream\n");
		result = avformat_open_input(&pFormatCtx, rtsp_source, NULL, &opts);
	}

	if (result < 0) {
		av_log(NULL, AV_LOG_ERROR, "Couldn't open stream\n");
		return 0;
	}

	av_log(NULL, AV_LOG_DEBUG, "Opened stream\n");

	result = avformat_find_stream_info(pFormatCtx, NULL);
	if (result < 0) {
		av_log(NULL, AV_LOG_ERROR, "Couldn't find stream information\n");
		return 0;
	}

	videoStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (videoStream == -1) {
		av_log(NULL, AV_LOG_ERROR, "Couldn't find video stream\n");
		return 0;
	}

	origin_par = pFormatCtx->streams[videoStream]->codecpar;
	pCodec = avcodec_find_decoder(origin_par->codec_id);
	//pCodec = avcodec_find_decoder_by_name("h264_cuvid");

	if (pCodec == NULL) {
		av_log(NULL, AV_LOG_ERROR, "Unsupported codec\n");
		return 0;
	}

	pCodecCtx = avcodec_alloc_context3(pCodec);

	if (pCodecCtx == NULL) {
		av_log(NULL, AV_LOG_ERROR, "Couldn't allocate codec context\n");
		return 0;
	}

	result = avcodec_parameters_to_context(pCodecCtx, origin_par);
	if (result) {
		av_log(NULL, AV_LOG_ERROR, "Couldn't copy decoder context\n");
		return 0;
	}

	result = avcodec_open2(pCodecCtx, pCodec, NULL);
	if (result < 0) {
		av_log(NULL, AV_LOG_ERROR, "Couldn't open decoder\n");
		return 0;
	}

	pFrame = av_frame_alloc();
	if (pFrame == NULL) {
		av_log(NULL, AV_LOG_ERROR, "Couldn't allocate frame\n");
		return 0;
	}

	int byte_buffer_size = av_image_get_buffer_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, 16);
	byte_buffer_size = byte_buffer_size < frame_size ? byte_buffer_size : frame_size;

	int number_of_written_bytes;
	int got_frame;
	uint32_t last_rtcp_ts = 0;
	double base_time = 0;

	uint8_t* frame_data = NULL;

	FILE *output_file = NULL;

	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		if (decode(&got_frame, pFrame, pCodecCtx, &packet) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Decoding error\n");
			break;
		}

		if (got_frame) {
			double ts = ntp_timestamp(pFormatCtx, &last_rtcp_ts, &base_time);

			// frame
			frame_data = av_malloc(byte_buffer_size);

			if (!frame_data) {
				av_log(NULL, AV_LOG_ERROR, "Couldn't allocate buffer\n");
				break;
			}

			number_of_written_bytes = av_image_copy_to_buffer(
				frame_data, byte_buffer_size,
				(const uint8_t* const *) pFrame->data,
				(const int*) pFrame->linesize,
				pCodecCtx->pix_fmt,
				pCodecCtx->width, pCodecCtx->height, 1);

			if (number_of_written_bytes < 0) {
				av_log(NULL, AV_LOG_ERROR, "Couldn't copy to buffer\n");
				break;
			}

			//write YUV frames to files
			/*char file_name_buf[30];
			snprintf(file_name_buf, 30, "output/ts_%f_yuv", ts);

			output_file = fopen(file_name_buf, "w+");

			if (fwrite(frame_data, 1, byte_buffer_size, output_file) < 0) {
				fprintf(stderr, "Failed to dump raw data.\n");
      } else {
				fclose(output_file);
			}*/

			free(frame_data);

			av_packet_unref(&packet);
			av_init_packet(&packet);
		}
	}
	return 0;
}
