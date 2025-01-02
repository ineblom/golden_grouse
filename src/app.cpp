#include "app.h"
#include <stdlib.h>

static App *app;

#include "arena.cpp"
#include "video_fetcher.cpp"
#include "video_lister.cpp"
#include "renderer.cpp"
#include "video.cpp"
#include "sequencer.cpp"

static void app_init() {
  ProfileFuncBegin();

  app = (App *)malloc(sizeof(App));
  memset(app, 0, sizeof(App));
  app->is_open = true;

  video_fetcher_init(&app->vid_fetcher);
  video_lister_init(&app->vid_lister);
  renderer_init(&app->renderer);

  app->video_arena = arena_alloc((Arena_Params){
    .reserve_size = MiB(64),
    .commit_size = MiB(64),
  });

  video_open(&app->video, "./videos/jackal.mp4");

  app->video_texture = create_yuv_texture(video_width(&app->video), video_height(&app->video));

  app->sequencer.theme = (Sequencer_Theme){
    .text = ImColor(232, 236, 244),
    .background = ImColor(12, 16, 25),
    .primary = ImColor(159, 177, 214),
    .secondary = ImColor(49, 73, 122),
    .accent = ImColor(103, 134, 199),
  };

  app->sequencer.zoom = 250.0f;
  app->sequencer.stride = 2.0f;
  app->sequencer.pan = 8.0f;

  app->sequencer.timeline_height = 80.0f;
  app->sequencer.lister_width = 160.0f;
  app->sequencer.border_width = 1.0f;

  app->sequencer.max_time = video_duration(&app->video);

  ProfileEnd();
}

static void app_shutdown() {
  ProfileFuncBegin();

  video_close(&app->video);
  renderer_shutdown(&app->renderer);
  video_fetcher_shutdown(&app->vid_fetcher);
  video_lister_shutdown(&app->vid_lister);

  ProfileEnd();
}

static void app_update_ui() {
  ProfileFuncBegin();

  ImGui::DockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode);

  video_fetcher_window(&app->vid_fetcher);
  video_lister_window(&app->vid_lister);

  renderer_draw_debug_ui(&app->renderer);

  if (ImGui::Begin("Video Player")) {
    Video *video = &app->video;

    static f32 t = 0.0f;
    ImGui::SliderFloat("Timestamp", &t, 0.0f,
                      video->fmt_ctx->duration / AV_TIME_BASE);
    if (ImGui::Button("Seek")) {
      video_seek(video, t);
    }
    f64 sec = pts_to_sec(video->time_base, video->frame->pts);
    s32 min = (s32)(sec / 60.0);
    sec -= min * 60.0;
    s32 hours = min / 60;
    min -= hours * 60;
    ImGui::Text("%dh %dm %.2fs", hours, min, sec);

    f32 aspect = (f32)app->video_texture.width / (f32)app->video_texture.height;
    f32 height = 60.0f;
    ImGui::Image(app->video_texture.ids[0], ImVec2(height * aspect, height));
    ImGui::SameLine();
    ImGui::Image(app->video_texture.ids[1], ImVec2(height * aspect, height));
    ImGui::Image(app->video_texture.ids[2], ImVec2(height * aspect, height));
  }
  ImGui::End();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  if (ImGui::Begin("Preview")) {
    f32 aspect = (f32)app->renderer.target.width / (f32)app->renderer.target.height;
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImVec2 image_size = ImVec2(size.x, size.x / aspect);
    if (image_size.y > size.y) {
      image_size = ImVec2(size.y * aspect, size.y);
    }
    ImGui::SetCursorPos((ImGui::GetWindowSize() - image_size) * 0.5f);
    ImGui::Image(app->renderer.target.tex, image_size);
  }
  ImGui::End();
  ImGui::PopStyleVar(1);

  draw_sequencer(&app->sequencer);

  ProfileEnd();
}

static void app_update(f64 delta_time) {
  ProfileFuncBegin();
  
  update_sequencer(&app->sequencer, delta_time);

  arena_pop_to(app->video_arena, 0);
  Video_Frame_YUV frame = video_read_frame(&app->video, app->video_arena);
  upload_frame_to_texture(app->video_texture, frame);

  app_update_ui();

  ProfileEnd();
}

static void app_draw() {
  ProfileFuncBegin();

  renderer_draw(&app->renderer, app->video_texture);

  ProfileEnd();
}