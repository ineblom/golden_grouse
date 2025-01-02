#include <stdio.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#define IMGUI_DEFINE_MATH_OPERATORS
#include "deps/imgui_impl_glfw.cpp"
#include "deps/imgui_impl_opengl3.cpp"

#if PROFILE_ENABLE

#include "deps/spall.h"

static SpallProfile spall_ctx;
static SpallBuffer spall_buffer;

static inline uint64_t get_micro() {
  uint64_t absolute_time = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
  return absolute_time / 10000;
}

#define ProfileFuncBegin() spall_buffer_begin(&spall_ctx, &spall_buffer, __FUNCTION__, sizeof(__FUNCTION__) - 1, get_micro())
#define ProfileBegin(str) spall_buffer_begin(&spall_ctx, &spall_buffer, str, sizeof(str) - 1, get_micro())
#define ProfileEnd() spall_buffer_end(&spall_ctx, &spall_buffer, get_micro())

static void profile_init() {
  spall_ctx = spall_init_file("profile.spall", 1);
  u64 buffer_size = MiB(1);
  spall_buffer = (SpallBuffer){
    .data = malloc(buffer_size),
    .length = buffer_size,
  };
  spall_buffer_init(&spall_ctx, &spall_buffer);
}

static void profile_shutdown() {
  spall_buffer_quit(&spall_ctx, &spall_buffer);
  free(spall_buffer.data);
  spall_quit(&spall_ctx);
}

static void profile_new_frame() {
  spall_flush(&spall_ctx);
}
#else
#define ProfileFuncBegin()
#define ProfileBegin(str)
#define ProfileEnd()
#define profile_init()
#define profile_shutdown()
#define profile_new_frame()
#endif

#include "app.cpp"

int main() {
  profile_init();

  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);

  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

  GLFWwindow *window = glfwCreateWindow(1280, 720, "Golden Grouse", NULL, NULL);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // viewports overreact to movement, imgui bug

  io.Fonts->AddFontFromFileTTF("./assets/CommitMono.ttf", 24.0f);
  io.FontGlobalScale = 0.5f;

  ImGui::StyleColorsDark();

  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 150");

  app_init();

  f64 last_time = glfwGetTime();

  while (app->is_open) {
    f64 now = glfwGetTime();
    f64 delta_time = now - last_time;
    last_time = now;
    
    if (glfwWindowShouldClose(window)) {
      app->is_open = false;
    }
    
    glfwPollEvents();

    ProfileBegin("ImgGui::NewFrame");
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ProfileEnd();

    app_update(delta_time);

    ProfileBegin("ImgGui::Render");
    ImGui::Render();
    ProfileEnd();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    app_draw();

    ProfileBegin("ImgGui::RenderDrawData");
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ProfileEnd();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      GLFWwindow* backup_current_context = glfwGetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(backup_current_context);
    }

    ProfileBegin("glfwSwapBuffers");
    glfwSwapBuffers(window);
    ProfileEnd();

    profile_new_frame();
  }

  app_shutdown();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  profile_shutdown();

  return 0;
}
