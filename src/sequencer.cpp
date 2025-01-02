


static void time_indicator(Sequencer *seq, ImDrawList *painter,
                          ImVec2 window_pos, ImVec2 window_size,
                          ImVec2 timeline_pos) {
  ProfileFuncBegin();

  f32 stride = seq->stride * 5.0f;
  f32 zoom = seq->zoom / seq->stride;
  f32 pan = seq->pan;

  f32 xmin = seq->playback_time * zoom / stride;
  f32 ymin = seq->timeline_height;
  f32 ymax = window_size.y;

  xmin += timeline_pos.x + pan; 
  ymin += timeline_pos.y; 
  ymax += timeline_pos.y; 

  ImVec2 cursor_size = {10.0f, 20.0f};
  ImVec2 cursor_pos = {xmin, ymin};

  if (xmin + cursor_size.x < window_pos.x + seq->lister_width) {
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
    seq->playback_time = initial_time + (ImGui::GetMouseDragDelta().x / (zoom / stride));
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
    f32 stride = seq->stride * 5.0f; // * 5 for artificially increasing stride? remove...
    f32 zoom = seq->zoom / seq->stride;
    s32 min_time = 0;
    s32 max_time = seq->max_time / stride;
    
    for (s32 time = min_time; time < max_time + 1; time++) {
      f32 xmin = time * zoom;
      f32 ymin = 0.0f;
      f32 ymax = seq->timeline_height;

      xmin += timeline_pos.x + seq->pan;
      ymin += timeline_pos.y;
      ymax += timeline_pos.y;

      if (xmin > window_pos.x + window_size.x) break;

      painter->AddLine(ImVec2(xmin, ymin), ImVec2(xmin, ymax - 1), seq->theme.accent);

      char buf[16];
      snprintf(buf, 16, "%d", (s32)(time * stride));
      painter->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f,
                      ImVec2{xmin + 5.0f, ymin + 2.5f}, seq->theme.text, buf);

      for (s32 z = 0; z < 4; z++) {
        f32 inner_spacing = zoom / 5.0f;
        f32 subline = inner_spacing * (z + 1);
        painter->AddLine(ImVec2{xmin + subline, ymin + seq->timeline_height * 0.5f},
                        ImVec2{xmin + subline, ymin + seq->timeline_height - 1},
                        seq->theme.accent);
      }
    }

    // Time indicator
    time_indicator(seq, painter, window_pos, window_size, timeline_pos);

    // empty sq bg
    painter->AddRectFilled(empty_sq_pos, empty_sq_pos + ImVec2{seq->lister_width, seq->timeline_height},
                          seq->theme.background);

    // lister bg
    painter->AddRectFilled(lister_pos, lister_pos + ImVec2{seq->lister_width, window_size.y},
                          seq->theme.background);

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
  }
  ImGui::End();
  ImGui::PopStyleVar(2);

  ImGui::Begin("Sequencer Config");
  ImGui::DragFloat("zoom", &seq->zoom, 1.0f, 0.0f);
  ImGui::DragFloat("stride", &seq->stride, 0.1f, 0.0f, 100.0f);
  ImGui::DragFloat("pan", &seq->pan, 5.0f);
  ImGui::DragFloat("timeline height", &seq->timeline_height, 1.0f, 0.0f, 1000.0f);
  ImGui::DragFloat("lister width", &seq->lister_width, 1.0f, 1.0f, 1000.0f);
  ImGui::DragFloat("border width", &seq->border_width, 0.1f, 1.0f, 1000.0f);
  ImGui::End();

  ProfileEnd();
}
