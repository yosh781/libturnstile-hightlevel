#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "turnstile-highlevel.h"

#define SESSIONS_DIR "/run/turnstiled/sessions"

static char *read_file_value(const char *path, const char *key) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    
    char line[512];
    size_t key_len = strlen(key);
    
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *value = line + key_len + 1;
            char *result = strdup(value);
            fclose(f);
            return result;
        }
    }
    
    fclose(f);
    return NULL;
}

static unsigned long read_file_ulong(const char *path, const char *key) {
    char *value = read_file_value(path, key);
    if (!value) return 0;
    unsigned long result = strtoul(value, NULL, 10);
    free(value);
    return result;
}

static int read_file_int(const char *path, const char *key) {
    char *value = read_file_value(path, key);
    if (!value) return 0;
    int result = atoi(value);
    free(value);
    return result;
}

static int parse_session_file(const char *path, turnstile_session *session) {
    memset(session, 0, sizeof(turnstile_session));
    
    char *basename = strrchr(path, '/');
    if (basename) basename++;
    else basename = (char*)path;
    session->id = strtoul(basename, NULL, 10);
    
    session->uid = read_file_int(path, "UID");
    session->username = read_file_value(path, "USER");
    session->type = read_file_value(path, "TYPE");
    session->class = read_file_value(path, "CLASS");
    session->seat = read_file_value(path, "SEAT");
    session->tty = read_file_value(path, "TTY");
    session->vtnr = read_file_ulong(path, "VTNR");
    session->remote = read_file_int(path, "REMOTE");
    session->leader = (pid_t)read_file_ulong(path, "LEADER");
    
    return 0;
}

int turnstile_get_sessions(turnstile_session **sessions, size_t *count) {
    DIR *dir = opendir(SESSIONS_DIR);
    if (!dir) {
        *sessions = NULL;
        *count = 0;
        return -1;
    }
    
    size_t n = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        n++;
    }
    
    if (n == 0) {
        closedir(dir);
        *sessions = NULL;
        *count = 0;
        return 0;
    }
    
    turnstile_session *result = calloc(n, sizeof(turnstile_session));
    if (!result) {
        closedir(dir);
        return -1;
    }
    
    rewinddir(dir);
    size_t idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < n) {
        if (entry->d_name[0] == '.') continue;
        
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", SESSIONS_DIR, entry->d_name);
        
        if (parse_session_file(path, &result[idx]) == 0) {
            idx++;
        }
    }
    
    closedir(dir);
    
    *sessions = result;
    *count = idx;
    return 0;
}

int turnstile_get_user_sessions(uid_t uid, turnstile_session **sessions, size_t *count) {
    turnstile_session *all_sessions;
    size_t all_count;
    
    if (turnstile_get_sessions(&all_sessions, &all_count) < 0) {
        return -1;
    }
    
    size_t match_count = 0;
    for (size_t i = 0; i < all_count; i++) {
        if (all_sessions[i].uid == uid) {
            match_count++;
        }
    }
    
    if (match_count == 0) {
        turnstile_free_sessions(all_sessions, all_count);
        *sessions = NULL;
        *count = 0;
        return 0;
    }
    
    turnstile_session *result = calloc(match_count, sizeof(turnstile_session));
    if (!result) {
        turnstile_free_sessions(all_sessions, all_count);
        return -1;
    }
    
    size_t idx = 0;
    for (size_t i = 0; i < all_count; i++) {
        if (all_sessions[i].uid == uid) {
            result[idx++] = all_sessions[i];
        } else {
            free(all_sessions[i].username);
            free(all_sessions[i].type);
            free(all_sessions[i].class);
            free(all_sessions[i].seat);
            free(all_sessions[i].tty);
        }
    }
    
    free(all_sessions);
    
    *sessions = result;
    *count = match_count;
    return 0;
}

int turnstile_get_session_by_id(unsigned long id, turnstile_session *session) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%lu", SESSIONS_DIR, id);
    
    if (access(path, R_OK) != 0) {
        return -1;
    }
    
    return parse_session_file(path, session);
}

void turnstile_free_sessions(turnstile_session *sessions, size_t count) {
    if (!sessions) return;
    
    for (size_t i = 0; i < count; i++) {
        free(sessions[i].username);
        free(sessions[i].type);
        free(sessions[i].class);
        free(sessions[i].seat);
        free(sessions[i].tty);
    }
    
    free(sessions);
}

int turnstile_stop_session(pid_t leader) {
    if (leader <= 0) return -1;
    return kill(leader, SIGTERM);
}

int turnstile_stop_all_sessions(void) {
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) < 0) {
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (sessions[i].leader > 0) {
            turnstile_stop_session(sessions[i].leader);
        }
    }
    
    turnstile_free_sessions(sessions, count);
    sleep(2);
    return 0;
}

int turnstile_get_runtime_dir(uid_t uid, char **path) {
    char *runtime = NULL;
    asprintf(&runtime, "/run/user/%u", uid);
    
    if (!runtime) return -1;
    
    *path = runtime;
    return 0;
}
