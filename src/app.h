#pragma once

#include "base.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <pthread.h>

#define MAX_URL_LENGTH 256
#define MAX_PATH_LENGTH 256

#define ARENA_DEFAULT_RESERVE_SIZE MiB(64)
#define ARENA_DEFAULT_COMMIT_SIZE MiB(64)
#define ARENA_DEFAULT_FLAGS ARENA_FLAG__NONE

#define ARENA_HEADER_SIZE 64

enum Arena_Flags {
  ARENA_FLAG__NONE = 0,
  ARENA_FLAG__NO_CHAIN = (1 << 0),
  ARENA_FLAG__LARGE_PAGES = (1 << 1),
};

struct Arena_Params {
  Arena_Flags flags;
  u64 reserve_size;
  u64 commit_size;
  void *optional_backing_buffer;
};

struct Arena {
  Arena *prev;
  Arena *current;
  Arena_Flags flags;
  u32 cmt_size;
  u64 res_size;
  u64 base_pos;

  u64 pos;
  u64 cmt;
  u64 res;

  // freelist?
};

struct Video_Frame_YUV {
  u32 width, height;

  u8 *y_data;
  u8 *u_data; // U and V have half width and height
  u8 *v_data;
};

struct Video {
  // TODO: Look into width and height, maybe have more fields
  // source and dest?
  s32 width, height;

  AVRational time_base;

  AVFormatContext *fmt_ctx;
  s32 stream_index;
  AVCodecContext *codec_ctx;

  AVPacket *packet;
  AVFrame *frame;
  SwsContext *sws_ctx;
};

struct Video_Fetcher {
  pthread_t thread;
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  bool is_fetching;
  char url[MAX_URL_LENGTH];
  char progress[256];
};

struct Video_Source_Dir {
  char path[MAX_PATH_LENGTH];
  Video_Source_Dir *next;
};

struct Video_Source {
  char id[32];
  char video_file[MAX_PATH_LENGTH];
  char info_json_file[MAX_PATH_LENGTH];
};

struct Video_Lister {
  Video_Source_Dir root_video_dir;

  u32 num_sources;
  Video_Source sources[1024];
};

struct Sequencer_Theme {
  ImColor text;
  ImColor background;
  ImColor primary;
  ImColor secondary;
  ImColor accent;
};

enum Sequencer_Playback_State {
  PLAYBACK_STATE__PLAY,
  PLAYBACK_STATE__PAUSE,
};

struct Sequencer {
  Sequencer_Theme theme;
  
  f32 zoom;
  f32 stride;
  f32 pan;

  f32 timeline_height;
  f32 lister_width;
  f32 border_width;

  Sequencer_Playback_State state;
  f64 playback_time;
  f64 max_time;
};

struct YUV_Texture {
  u32 width, height;
  u32 ids[3];
};

struct Render_Target {
  u32 width, height;
  u32 fbo;
  u32 tex;
};

struct Renderer {
  u32 shader_program;
  Render_Target target;

  u32 vao, vbo, ibo;

  float draw_color[3];
};

struct App {
  bool is_open;

  Video_Fetcher vid_fetcher;
  Video_Lister vid_lister;

  Arena *video_arena;
  Video video;

  Sequencer sequencer;

  YUV_Texture video_texture;
  Renderer renderer;
};