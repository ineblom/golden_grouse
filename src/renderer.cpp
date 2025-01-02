
const char *vertex_shader_source = R"(
  #version 330 core

  layout (location = 0) in vec3 a_pos;
  layout (location = 1) in vec2 a_uv;

  out vec2 pass_uv;

  void main() {
    pass_uv = a_uv;
    gl_Position = vec4(a_pos, 1.0f);
  }
)";

const char *fragment_shader_source = R"(
  #version 330 core

  in vec2 pass_uv;
  out vec4 frag_color;

  uniform vec3 draw_color;

  uniform sampler2D y_tex;
  uniform sampler2D u_tex;
  uniform sampler2D v_tex;

  void main() {
    float y = texture(y_tex, pass_uv).r;
    float u = texture(u_tex, pass_uv).r - 0.5;
    float v = texture(v_tex, pass_uv).r - 0.5;

    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;

    frag_color = vec4(vec3(r, g, b) * draw_color, 1.0);
  }
)";

static u32 create_shader_program() {
  ProfileFuncBegin();

  s32 success;
  char info_log[512];

  u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
    fprintf(stderr, "VERTEX shader failed to compile: %s\n", info_log);
  }

  u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
    fprintf(stderr, "FRAGMENT shader failed to compile: %s\n", info_log);
  }

  u32 shader_program = glCreateProgram();
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if(!success) {
    glGetProgramInfoLog(shader_program, 512, NULL, info_log);
    fprintf(stderr, "PROGRAM LINK ERROR: %s\n", info_log);
  }

  glUseProgram(shader_program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  ProfileEnd();

  return shader_program;
}

static YUV_Texture create_yuv_texture(u32 width, u32 height) {
  ProfileFuncBegin();

  u32 textures[3];
  glGenTextures(3, textures);

  glBindTexture(GL_TEXTURE_2D, textures[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, textures[1]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, textures[2]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  YUV_Texture result = {
    .width = width,
    .height = height,
  };
  memcpy(result.ids, textures, sizeof(textures));

  ProfileEnd();

  return result;
}

static void upload_frame_to_texture(YUV_Texture texture, Video_Frame_YUV frame) {
  ProfileFuncBegin();

  u32 *textures = texture.ids;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures[0]);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height,
                  GL_RED, GL_UNSIGNED_BYTE, frame.y_data);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, textures[1]);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width / 2, frame.height / 2,
                  GL_RED, GL_UNSIGNED_BYTE, frame.u_data);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures[2]);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width / 2, frame.height / 2,
                  GL_RED, GL_UNSIGNED_BYTE, frame.v_data);

  ProfileEnd();
}

static Render_Target create_render_target(u32 width, u32 height) {
  ProfileFuncBegin();

  u32 fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  u32 tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

  ProfileEnd();

  return {
    .width = width,
    .height = height,
    .fbo = fbo,
    .tex = tex,
  };
}

static void renderer_init(Renderer *r) {
  ProfileFuncBegin();

  r->draw_color[0] = 1.0f;
  r->draw_color[1] = 1.0f;
  r->draw_color[2] = 1.0f;

  r->shader_program = create_shader_program();
  glUniform1i(glGetUniformLocation(r->shader_program, "y_tex"), 0);
  glUniform1i(glGetUniformLocation(r->shader_program, "u_tex"), 1);
  glUniform1i(glGetUniformLocation(r->shader_program, "v_tex"), 2);

  r->target = create_render_target(1280, 720);

  float vertices[] = {
   -1.0f, 1.0f, 0.0f,  0.0f, 1.0f,
    1.0f, 1.0f, 0.0f,  1.0f, 1.0f,
    1.0f,-1.0f, 0.0f,  1.0f, 0.0f,
   -1.0f,-1.0f, 0.0f,  0.0f, 0.0f,
  };

  u32 indices[] = {
    0, 1, 2,
    2, 3, 0,
  };

  glGenVertexArrays(1, &r->vao);
  glBindVertexArray(r->vao);

  glGenBuffers(1, &r->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &r->ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  u32 vertex_size = 5 * sizeof(f32);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vertex_size, (void*)(3 * sizeof(f32)));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  ProfileEnd();
}

static void renderer_shutdown(Renderer *r) {
  // TODO: Delete stuff
}

static void renderer_draw_debug_ui(Renderer *r) {
  ProfileFuncBegin();

  ImGui::Begin("renderer");

  ImGui::ColorEdit3("draw color", r->draw_color);

  ImGui::End();

  ProfileEnd();
}

static void renderer_draw(Renderer *r, YUV_Texture texture) {
  ProfileFuncBegin();

  int err = glGetError();
  if (err != 0) {
    fprintf(stderr, "GL ERROR: %d\n", err);
  }

  glUseProgram(r->shader_program);
  glBindFramebuffer(GL_FRAMEBUFFER, r->target.fbo);
  glViewport(0, 0, r->target.width, r->target.height);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture.ids[0]);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texture.ids[1]);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, texture.ids[2]);

  s32 loc = glGetUniformLocation(r->shader_program, "draw_color");
  glUniform3fv(loc, 1, r->draw_color);

  glBindVertexArray(r->vao);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  ProfileEnd();
}
