#include "stdio.h"
#include "string.h"
#include "dirent.h"
#include "endin.h"



int endsWith(const char *filename, const char *suffix) {
    if (!filename || !suffix) return 0;
    size_t len_filename = strlen(filename);
    size_t len_suffix = strlen(suffix);
    if (len_suffix > len_filename) return 0;
    return strcmp(filename + len_filename - len_suffix, suffix) == 0;
}

void cmd_endin(const char *sfx){
    if(!sfx){
        printf("Usage: endin <suffix>\n");
        return;
    }

    DIR *dir = opendir(".");
    if(!dir){
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    int found = 0;
    while((entry = readdir(dir)) != NULL) {
        if (endsWith(entry->d_name, sfx)) {
            printf("%s\n", entry->d_name);
            found = 1;
        }
    }
    closedir(dir);
    if (!found) {
        printf("No files found with suffix '%s'.\n", sfx);
    }
}