
#include <regex.h>

// TODO:
// - Queue downloads
// - Nicer progress bar
// - Pull metadata and display before fetch
// - Fetch options, resolution, fps...
// - Error message reporting (from yt-dlp)

static void *vid_fetcher_thread(void *ptr) {
  Video_Fetcher *fetch = (Video_Fetcher *)ptr;

  while (app->is_open) {
    pthread_mutex_lock(&fetch->mutex);
    while (!fetch->is_fetching && app->is_open) {
      pthread_cond_wait(&fetch->cond, &fetch->mutex);
    }

    if (!app->is_open) {
      pthread_mutex_unlock(&fetch->mutex);
      break;
    }

    char command[512] = {0};
    const char *params = "--restrict-filenames --write-info-json -q --progress --newline -o \"./videos/%(id)s.%(ext)s\"";
    snprintf(command, 512, "yt-dlp %s \"%s\"", params, fetch->url);

    const char *progress_message = "Fetching metadata...";
    memcpy(fetch->progress, progress_message, strlen(progress_message));

    FILE *pipe = popen(command, "r");
    while (fgets(fetch->progress, sizeof(fetch->progress), pipe) != NULL); 

    pclose(pipe);

    fetch->is_fetching = false;

    pthread_mutex_unlock(&fetch->mutex);
  }

  return NULL;
}

static void video_fetcher_init(Video_Fetcher *fetch) {
  pthread_create(&app->vid_fetcher.thread, NULL, vid_fetcher_thread, (void *)fetch);
  app->vid_fetcher.mutex = PTHREAD_MUTEX_INITIALIZER;
  app->vid_fetcher.cond = PTHREAD_COND_INITIALIZER;
}

static void video_fetcher_shutdown(Video_Fetcher *fetch) {
  pthread_cond_signal(&fetch->cond);
  pthread_join(fetch->thread, NULL);
}

static bool is_valid_url(const char *url) {
  const char *pattern = "^(https?|ftp)://[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}(:[0-9]{1,5})?(/.*)?$";

  regex_t regex;

  int reti = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
  if (reti) {
      fprintf(stderr, "Could not compile regex\n");
      return 0;
  }

  reti = regexec(&regex, url, 0, NULL, 0);
  regfree(&regex);

  return !reti;
}

static void video_fetcher_window(Video_Fetcher *fetch) {
  ImGui::Begin("Video Fetcher");

  if (fetch->is_fetching) {
    ImGui::Text("%s", fetch->progress);
  } else {
    static char url[MAX_URL_LENGTH] = {0};
    ImGui::InputText("URL", url, MAX_URL_LENGTH);

    bool disabled = !is_valid_url(url);

    if (disabled) ImGui::BeginDisabled();
    
    if (ImGui::Button("Fetch")) {
      pthread_mutex_lock(&fetch->mutex);
      memcpy(fetch->url, url, MAX_URL_LENGTH);
      fetch->is_fetching = true;
      pthread_cond_signal(&fetch->cond);
      pthread_mutex_unlock(&fetch->mutex);
    }

    if (disabled) {
      if (strlen(url) > 0) {
        ImGui::SameLine();
        ImGui::Text("Invalid URL");
      }

      ImGui::EndDisabled();
    }
  }

  ImGui::End();
}
