


static void time_indicator(Sequencer *seq, ImDrawList *painter,
                          ImVec2 window_pos, ImVec2 window_size,
                          ImVec2 timeline_pos) {
  ProfileFuncBegin();

  f32 xmin = (seq->playback_time - seq->pan) * seq->zoom;
  f32 ymin = seq->timeline_height;
  f32 ymax = window_size.y;

  xmin += timeline_pos.x; 
  ymin += timeline_pos.y; 
  ymax += timeline_pos.y; 

  ImVec2 cursor_size = {10.0f, 20.0f};
  ImVec2 cursor_pos = {xmin, ymin};

  if (xmin + cursor_size.x < window_pos.x + seq->lister_width || xmin > window_pos.x + window_size.x) {
    return;
  }

  painter->AddLine(ImVec2{xmin, ymin}, ImVec2{xmin, ymax}, seq->theme.primary);

  ImGui::PushID("time indicator");
  ImGui::SetCursorScreenPos(cursor_pos - cursor_size);
  ImGui::SetNextItemAllowOverlap();
  ImGui::InvisibleButton("##indicator", cursor_size * 2.0f);
  ImGui::PopID();

  static f32 initial_time = 0.0f;
  if (ImGui::IsItemActivated()) {
    initial_time = seq->playback_time;
  }

  ImColor color = seq->theme.primary;
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem) || ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    color = color * 1.2f;
  }

  if (ImGui::IsItemActive()) {
    //*time += ImGui::GetIO().MouseDelta.x / (zoom / stride);
    seq->playback_time = initial_time + (ImGui::GetMouseDragDelta().x / seq->zoom);
    if (seq->playback_time < 0.0f) seq->playback_time = 0.0f;
  }

  if (ImGui::IsItemDeactivated()) {
  }

  ImVec2 points[5] = {
    cursor_pos,
    cursor_pos - ImVec2{ -cursor_size.x, cursor_size.y / 2.0f },
    cursor_pos - ImVec2{ -cursor_size.x, cursor_size.y },
    cursor_pos - ImVec2{ cursor_size.x, cursor_size.y },
    cursor_pos - ImVec2{ cursor_size.x, cursor_size.y / 2.0f }
  };

  painter->AddConvexPolyFilled(points, 5, ImColor(color));
  painter->AddPolyline(points, 5, ImColor(color * 1.25f), true, 1.0f);

  ProfileEnd();
}

static void update_sequencer(Sequencer *seq, f32 delta_time) {
  ProfileFuncBegin();

  if (seq->state == PLAYBACK_STATE__PLAY) {
    seq->playback_time += delta_time;
  }

  ProfileEnd();
}

static void draw_sequencer(Sequencer *seq) {
  ProfileFuncBegin();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0.0f, 0.0f});

  f64 timeline_screen_w = 0.0f;
  if (ImGui::Begin("Sequencer")) {

    ImGui::BeginDisabled(seq->state == PLAYBACK_STATE__PLAY);
    if (ImGui::Button("Play")) {
      seq->state = PLAYBACK_STATE__PLAY;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(seq->state == PLAYBACK_STATE__PAUSE);
    if (ImGui::Button("Pause")) {
      seq->state = PLAYBACK_STATE__PAUSE;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
      seq->state = PLAYBACK_STATE__PAUSE;
      seq->playback_time = 0.0f;
    }

    ImDrawList *painter = ImGui::GetWindowDrawList();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();

    ImVec2 empty_sq_pos = window_pos;
    ImVec2 lister_pos = window_pos + ImVec2{0.0f, seq->timeline_height};
    ImVec2 timeline_pos = window_pos + ImVec2{seq->lister_width, 0.0f};
    ImVec2 editor_pos = window_pos + ImVec2{seq->lister_width, seq->timeline_height};


    // timeline bg
    painter->AddRectFilled(timeline_pos, timeline_pos + ImVec2{window_size.x, seq->timeline_height},
                          seq->theme.background);

    // editor bg
    painter->AddRectFilled(editor_pos, editor_pos + window_size, seq->theme.background);


    // timeline border
    painter->AddLine(timeline_pos + ImVec2{0.0f, seq->timeline_height},
                    timeline_pos + ImVec2{window_size.x, seq->timeline_height},
                    seq->theme.secondary, seq->border_width);

    // timeline

    timeline_screen_w = window_size.x - seq->lister_width;
    f64 timeline_sec_w = timeline_screen_w / seq->zoom;
    f64 second_left = seq->pan;
    f64 second_right = second_left + timeline_sec_w;

    // for now, only draw seconds
    s32 min_time = (s32)Max(second_left, 0);
    s32 max_time = (s32)second_right + 1;
    s32 num_sublines = 4;

    for (s32 t = min_time; t < max_time; t++) {
      f32 x = timeline_pos.x + (t - second_left) * seq->zoom;
      f32 y = timeline_pos.y;

      painter->AddLine(ImVec2(x, y), ImVec2(x, y + seq->timeline_height), seq->theme.accent);

      char buf[16];
      snprintf(buf, 16, "%d", t);
      painter->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f,
                      ImVec2{x + 5.0f, y + 2.5f}, seq->theme.text, buf);
    
      for (s32 i = 0; i < num_sublines; i++) {
        x += seq->zoom / (f64)(num_sublines + 1);
        painter->AddLine(ImVec2(x, y), ImVec2(x, y + seq->timeline_height * 0.5f), seq->theme.accent);
      }
    }


    // Time indicator
    //time_indicator(seq, painter, window_pos, window_size, timeline_pos);

    // empty sq bg
    painter->AddRectFilled(empty_sq_pos, empty_sq_pos + ImVec2{seq->lister_width, seq->timeline_height},
                          seq->theme.background);

    // lister bg
    painter->AddRectFilled(lister_pos, lister_pos + ImVec2{seq->lister_width, window_size.y},
                          seq->theme.background);


    // top border
    painter->AddLine(window_pos, { window_pos.x + window_size.x, window_pos.y}, seq->theme.secondary, seq->border_width);

    // empty sq borders
    painter->AddLine(empty_sq_pos + ImVec2{seq->lister_width, 0.0f},
                    empty_sq_pos + ImVec2{seq->lister_width, seq->timeline_height},
                    seq->theme.secondary, seq->border_width);
    painter->AddLine(empty_sq_pos + ImVec2{0.0f, seq->timeline_height},
                    empty_sq_pos + ImVec2{seq->lister_width, seq->timeline_height},
                    seq->theme.secondary, seq->border_width);

    // lister border
    painter->AddLine(lister_pos + ImVec2{seq->lister_width, 0.0f},
                    lister_pos + ImVec2{seq->lister_width, window_size.y},
                    seq->theme.secondary, seq->border_width);
  
    ImGui::Text("w: %f, sec w: %f", timeline_screen_w, timeline_sec_w);
    ImGui::Text("left: %f, right: %f", second_left, second_right);
  }
  ImGui::End();
  ImGui::PopStyleVar(2);

  ImGui::Begin("Sequencer Config");
  f32 old_zoom = seq->zoom;
  if (ImGui::DragFloat("zoom", &seq->zoom, 1.0f, 20.0f, 2000.0f) && timeline_screen_w > 0.0f) {
    // zoom at center
    f64 center_time = seq->pan + timeline_screen_w / (old_zoom * 2.0f);
    seq->pan = center_time - timeline_screen_w / (seq->zoom * 2.0f);
  }
  ImGui::DragFloat("pan", &seq->pan, 0.01f);
  ImGui::DragFloat("timeline height", &seq->timeline_height, 1.0f, 0.0f, 1000.0f);
  ImGui::DragFloat("lister width", &seq->lister_width, 1.0f, 1.0f, 1000.0f);
  ImGui::DragFloat("border width", &seq->border_width, 0.1f, 1.0f, 1000.0f);
  ImGui::End();

  ProfileEnd();
}
