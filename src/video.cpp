#define chk_err(code) if ((code) < 0) { print_err(code, __FILE__, __LINE__); }

static void print_err(int code, const char *filename, int line) {
  char desc[256] = {0};
  av_strerror(code, desc, 256);
  fprintf(stderr, "ffmpeg error [%d] (%s:%d): %s \n", code, filename, line, desc);
}

static void video_open(Video *video, const char *path) {
  ProfileFuncBegin();

  video->fmt_ctx = avformat_alloc_context();

  chk_err(avformat_open_input(&video->fmt_ctx, path, NULL, NULL));

  chk_err(avformat_find_stream_info(video->fmt_ctx, NULL));

  printf("%s, %lld seconds, %lld bit/s\n", video->fmt_ctx->iformat->long_name,
      video->fmt_ctx->duration / AV_TIME_BASE,
      video->fmt_ctx->bit_rate);

  video->stream_index = -1;
  const AVCodec *codec = NULL;
  AVCodecParameters *codec_params = NULL;

  for (int i = 0; i < video->fmt_ctx->nb_streams; ++i) {
    AVCodecParameters *local_codec_params = video->fmt_ctx->streams[i]->codecpar;
    const AVCodec *local_codec = avcodec_find_decoder(local_codec_params->codec_id);

    if (local_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video->stream_index == -1) {
        video->stream_index = i;
        codec = local_codec;
        codec_params = local_codec_params;
        video->time_base = video->fmt_ctx->streams[i]->time_base;
        printf("time base: %d/%d\n", video->time_base.num, video->time_base.den);
      }
      printf("video codec:\n  resolution: %d x %d\n", local_codec_params->width, local_codec_params->height);
    } else if (local_codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
      printf("audio codec:\n  %d channels, sample rate: %d\n",
        local_codec_params->ch_layout.nb_channels, local_codec_params->sample_rate);
    }

    printf("  '%s', ID: %d, bitrate: %lld\n", local_codec->name, local_codec->id, local_codec_params->bit_rate);
  }

  if (video->stream_index == -1) {
    fprintf(stderr, "No video stream found.\n");
    // TODO: Error
  }

  video->codec_ctx = avcodec_alloc_context3(codec);
  chk_err(avcodec_parameters_to_context(video->codec_ctx, codec_params));

  // configure codec for faster decoding
  video->codec_ctx->skip_frame = AVDISCARD_NONREF;
  video->codec_ctx->skip_loop_filter = AVDISCARD_NONKEY;
  video->codec_ctx->skip_idct = AVDISCARD_NONKEY;
  video->codec_ctx->thread_count = 4;
  video->codec_ctx->thread_type = FF_THREAD_FRAME;

  chk_err(avcodec_open2(video->codec_ctx, codec, NULL));

  video->width = video->codec_ctx->width;
  video->height = video->codec_ctx->height;

  video->sws_ctx = sws_getContext(video->width, video->height, video->codec_ctx->pix_fmt,
                                   video->width, video->height, AV_PIX_FMT_YUV420P,
                                   SWS_BILINEAR, NULL, NULL, NULL);

  video->packet = av_packet_alloc();
  video->frame = av_frame_alloc();

  ProfileEnd();
}

static void video_close(Video *video) {
  ProfileFuncBegin();

  av_frame_free(&video->frame);
  av_packet_free(&video->packet);
  avcodec_free_context(&video->codec_ctx);
  avformat_close_input(&video->fmt_ctx);

  ProfileEnd();
}

static Video_Frame_YUV video_read_frame(Video *video, Arena *arena) {
  ProfileFuncBegin();

  av_frame_unref(video->frame);

  Video_Frame_YUV result = {0};

  while (av_read_frame(video->fmt_ctx, video->packet) >= 0) {
    // discard packets that are not from the video stream
    if (video->packet->stream_index != video->stream_index) {
      av_packet_unref(video->packet);
      continue;
    }

    // send the packet to the decoder
    chk_err(avcodec_send_packet(video->codec_ctx, video->packet));

    // receive the decoded frame
    s32 response = avcodec_receive_frame(video->codec_ctx, video->frame);
    if (response == AVERROR(EAGAIN) || response == AVERROR(AVERROR_EOF)) {
      av_packet_unref(video->packet);
      continue;
    } else {
      chk_err(response);
    }

    result.width = video->width;
    result.height = video->height;
    result.y_data = push_array(arena, u8, video->width * video->height);
    result.u_data = push_array(arena, u8, video->width * video->height / 4);
    result.v_data = push_array(arena, u8, video->width * video->height / 4);
    
    u8 *dest[4] = { result.y_data, result.u_data, result.v_data, NULL };
    s32 dest_linesize[4] = { video->width, video->width / 2, video->width / 2, 0 };

    chk_err(sws_scale(video->sws_ctx, video->frame->data, video->frame->linesize,
        0, video->height, dest, dest_linesize));

    av_packet_unref(video->packet);
    break;
  }

  ProfileEnd();

  return result;
}

static f64 pts_to_sec(AVRational time_base, s64 pts) {
  f64 sec = av_q2d(time_base) * pts;
  return sec;
}

static s64 sec_to_pts(AVRational time_base, f64 sec) {
  s64 pts = av_rescale_q(sec * AV_TIME_BASE, AV_TIME_BASE_Q, time_base);
  return pts;
}

static void video_seek(Video *video, f32 sec) {
  ProfileFuncBegin();

  s64 pts = sec_to_pts(video->time_base, sec);

  avformat_seek_file(video->fmt_ctx, video->stream_index,
                     INT64_MIN, pts, INT64_MAX, AVSEEK_FLAG_BACKWARD);
  avcodec_flush_buffers(video->codec_ctx);

  av_frame_unref(video->frame);
  while (av_read_frame(video->fmt_ctx, video->packet) >= 0) {
    if (video->packet->stream_index != video->stream_index) {
      av_packet_unref(video->packet);
      continue;
    }

    chk_err(avcodec_send_packet(video->codec_ctx, video->packet));

    s32 response = avcodec_receive_frame(video->codec_ctx, video->frame);
    if (response == AVERROR(EAGAIN) || response == AVERROR(AVERROR_EOF)) {
      av_packet_unref(video->packet);
      continue;
    } else {
      chk_err(response);
    }

    s64 off = pts - video->frame->pts;
    if (off > 0) continue;

    av_packet_unref(video->packet);
    break;
  }

  ProfileEnd();
}

static inline u32 video_width(Video *video) {
  return video->width;
}

static inline u32 video_height(Video *video) {
  return video->height;
}

static inline f64 video_duration(Video *video) {
  return pts_to_sec(video->time_base, video->fmt_ctx->duration);
}