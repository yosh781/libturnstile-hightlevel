/*
 * XDG_RUNTIME_DIR management for Turnstile
 *
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2025 Valera <valera@mxd>
 */

#include "turnstile-highlevel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define RUNTIME_DIR_BASE "/run/user"

int turnstile_setup_runtime_dir(uid_t uid) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%u", RUNTIME_DIR_BASE, uid);
    
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = ENOTDIR;
        return -1;
    }
    
    if (mkdir(path, 0700) < 0) {
        return -1;
    }
    
    if (chown(path, uid, uid) < 0) {
        rmdir(path);
        return -1;
    }
    
    return 0;
}

int turnstile_cleanup_runtime_dir(uid_t uid) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%u", RUNTIME_DIR_BASE, uid);
    
    DIR *dir = opendir(path);
    if (!dir) {
        return (errno == ENOENT) ? 0 : -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        unlink(filepath);
    }
    closedir(dir);
    
    if (rmdir(path) < 0 && errno != ENOENT) {
        return -1;
    }
    
    return 0;
}
