
#include <dirent.h>
#include <sys/stat.h>

enum Extension_Type {
  EXTENSION_TYPE__UNKNOWN = 0,
  EXTENSION_TYPE__VIDEO,
  EXTENSION_TYPE__INFO_JSON,
  NUM_EXTENSION_TYPES,
};

static const char *video_file_extensions[] = {
    ".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm", 
    ".mpeg", ".mpg", ".m4v", ".3gp", ".ogv", ".ts", ".asf", 
    ".rmvb", ".dv", ".gif", ".swf", ".mxf", ".gxf", ".lxf", ".nut"
};

static void search_video_dirs(Video_Lister *lister) {
  Video_Source_Dir *current_dir = &lister->root_video_dir;

  while (current_dir != NULL) {
    DIR *dir = opendir(current_dir->path);
    if (dir == NULL) {
      fprintf(stderr, "Could not open '%s'\n", current_dir->path);
      current_dir = current_dir->next;
      continue;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      // Skip . and ..
      if (entry->d_name[0] == '.' && (entry->d_name[1] == '.' || entry->d_name[1] == 0)) {
        continue;
      }

      char fullpath[MAX_PATH_LENGTH] = {0};
      snprintf(fullpath, MAX_PATH_LENGTH, "%s/%s", current_dir->path, entry->d_name);

      if (entry->d_type == DT_DIR) {
        // Add directory to search list.
        Video_Source_Dir *new_dir = (Video_Source_Dir *)malloc(sizeof(Video_Source_Dir));
        memset(new_dir, 0, sizeof(Video_Source_Dir));
        memcpy(new_dir->path, fullpath, strlen(fullpath));
        new_dir->next = current_dir->next;
        current_dir->next = new_dir;
      } else {
        // We found a file
        char *ext = entry->d_name;
        while (ext != NULL && *ext != '.') ++ext;

        if (ext) {
          int ext_type = EXTENSION_TYPE__UNKNOWN;

          if (strcmp(ext, ".info.json") == 0) {
            ext_type = EXTENSION_TYPE__INFO_JSON;
          }

          for (int i = 0; i < ArrayLength(video_file_extensions) && ext_type == EXTENSION_TYPE__UNKNOWN; ++i) {
            if (strcmp(ext, video_file_extensions[i]) == 0) {
              ext_type = EXTENSION_TYPE__VIDEO;
            }
          }

          if (ext_type == EXTENSION_TYPE__UNKNOWN) {
            continue;
          }

          char id[32] = {0}; 
          uint32_t id_length = ext - entry->d_name; // Clamp?
          memcpy(id, entry->d_name, id_length);

          Video_Source *source = NULL;
          for (uint32_t i = 0; i < lister->num_sources; ++i) {
            if (strcmp(lister->sources[i].id, id) == 0) {
              source = &lister->sources[i];
              break;
            }
          }

          if (source == NULL) {
            source = &lister->sources[lister->num_sources++];
            memcpy(source->id, id, 32);
          }

          if (ext_type == EXTENSION_TYPE__VIDEO) {
            memcpy(source->video_file, fullpath, MAX_PATH_LENGTH);
          } else if (ext_type == EXTENSION_TYPE__INFO_JSON) {
            memcpy(source->info_json_file, fullpath, MAX_PATH_LENGTH);
          }

        }
      }
    }

    closedir(dir);

    current_dir = current_dir->next;
  }

  // free all the additional dirs
  for (Video_Source_Dir *it = lister->root_video_dir.next; it != NULL; ) {
    Video_Source_Dir *next = it->next;
    free(it);
    it = next;
  }
  lister->root_video_dir.next = NULL;
}

static void video_lister_init(Video_Lister *lister) {

  const char *root_dir = "./videos";
  memcpy(lister->root_video_dir.path, root_dir, strlen(root_dir));

  search_video_dirs(lister);
}

static void video_lister_shutdown(Video_Lister *lister) {

}

static void video_lister_window(Video_Lister *lister) {
  ImGui::Begin("Video Lister");

  if (ImGui::Button("Refresh")) {
    search_video_dirs(lister);
  }

  for (uint32_t i = 0; i < lister->num_sources; ++i) {
    Video_Source *source = &lister->sources[i];
    ImGui::Text("%u id:%s vid:%s inf:%s", i, source->id, source->video_file, source->info_json_file);
  }

  ImGui::End();
}
