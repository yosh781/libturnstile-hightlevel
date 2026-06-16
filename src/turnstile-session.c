/*
 * Session management implementation for Turnstile
 *
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2025 Valera <valera@mxd>
 */

#define _GNU_SOURCE
#include "turnstile-highlevel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <signal.h>

#define SESSIONS_DIR "/run/turnstiled/sessions"

static int ensure_sessions_dir(void) {
    struct stat st;
    if (stat(SESSIONS_DIR, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = ENOTDIR;
        return -1;
    }
    
    if (mkdir(SESSIONS_DIR, 0755) < 0) {
        return -1;
    }
    
    chown(SESSIONS_DIR, 0, 0);
    return 0;
}

static int write_session_file(const turnstile_session *session, unsigned long id) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%lu", SESSIONS_DIR, id);
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "UID=%u\n", session->uid);
    fprintf(f, "USER=%s\n", session->username ? session->username : "");
    fprintf(f, "REMOTE=%d\n", session->remote);
    fprintf(f, "TYPE=%s\n", session->type ? session->type : "");
    fprintf(f, "CLASS=%s\n", session->class ? session->class : "");
    fprintf(f, "SEAT=%s\n", session->seat ? session->seat : "seat0");
    fprintf(f, "TTY=%s\n", session->tty ? session->tty : "");
    fprintf(f, "VTNR=%lu\n", session->vtnr);
    fprintf(f, "LEADER=%d\n", session->leader);
    
    if (session->display)
        fprintf(f, "DISPLAY=%s\n", session->display);
    if (session->desktop)
        fprintf(f, "DESKTOP=%s\n", session->desktop);
    if (session->state)
        fprintf(f, "STATE=%s\n", session->state);
    if (session->remote_user)
        fprintf(f, "REMOTE_USER=%s\n", session->remote_user);
    if (session->remote_host)
        fprintf(f, "REMOTE_HOST=%s\n", session->remote_host);
    
    fprintf(f, "CREATED_AT=%ld\n", session->created_at);
    fprintf(f, "IDLE_SINCE=%ld\n", session->idle_since);
    fprintf(f, "LINGER=%d\n", session->linger);
    
    fclose(f);
    return 0;
}

int turnstile_create_session(turnstile_session *session, unsigned long *session_id) {
    if (ensure_sessions_dir() < 0) {
        return -1;
    }
    
    unsigned long id = 1;
    char path[512];
    while (1) {
        snprintf(path, sizeof(path), "%s/%lu", SESSIONS_DIR, id);
        if (access(path, F_OK) != 0) break;
        id++;
    }
    
    session->created_at = time(NULL);
    session->idle_since = 0;
    if (!session->state) session->state = strdup("online");
    if (!session->seat) session->seat = strdup("seat0");
    
    if (write_session_file(session, id) < 0) {
        return -1;
    }
    
    *session_id = id;
    return 0;
}

int turnstile_release_session(unsigned long session_id) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%lu", SESSIONS_DIR, session_id);
    
    turnstile_session session;
    if (turnstile_get_session_by_id(session_id, &session) == 0) {
        turnstile_session *user_sessions;
        size_t count;
        if (turnstile_get_user_sessions(session.uid, &user_sessions, &count) == 0) {
            if (count <= 1) {
                turnstile_cleanup_runtime_dir(session.uid);
            }
            turnstile_free_sessions(user_sessions, count);
        }
        
        free(session.username);
        free(session.type);
        free(session.class);
        free(session.seat);
        free(session.tty);
        free(session.display);
        free(session.desktop);
        free(session.state);
        free(session.remote_user);
        free(session.remote_host);
    }
    
    if (unlink(path) < 0) {
        return -1;
    }
    
    return 0;
}

int turnstile_activate_session(unsigned long session_id) {
    turnstile_session session;
    if (turnstile_get_session_by_id(session_id, &session) < 0) {
        return -1;
    }
    
    if (session.leader > 0) {
        kill(session.leader, SIGCONT);
    }
    
    turnstile_free_sessions(&session, 1);
    return 0;
}

int turnstile_set_session_state(unsigned long session_id, const char *state) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%lu", SESSIONS_DIR, session_id);
    
    turnstile_session session;
    if (turnstile_get_session_by_id(session_id, &session) < 0) {
        return -1;
    }
    
    free(session.state);
    session.state = strdup(state);
    
    if (write_session_file(&session, session_id) < 0) {
        turnstile_free_sessions(&session, 1);
        return -1;
    }
    
    turnstile_free_sessions(&session, 1);
    return 0;
}

int turnstile_set_session_idle(unsigned long session_id, bool idle) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%lu", SESSIONS_DIR, session_id);
    
    turnstile_session session;
    if (turnstile_get_session_by_id(session_id, &session) < 0) {
        return -1;
    }
    
    session.idle_since = idle ? time(NULL) : 0;
    
    if (write_session_file(&session, session_id) < 0) {
        turnstile_free_sessions(&session, 1);
        return -1;
    }
    
    turnstile_free_sessions(&session, 1);
    return 0;
}

int turnstile_get_session_by_pid(pid_t pid, turnstile_session *session) {
    turnstile_session *all_sessions;
    size_t count;
    
    if (turnstile_get_sessions(&all_sessions, &count) < 0) {
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (all_sessions[i].leader == pid) {
            memcpy(session, &all_sessions[i], sizeof(turnstile_session));
            session->username = strdup(all_sessions[i].username);
            session->type = strdup(all_sessions[i].type);
            session->class = strdup(all_sessions[i].class);
            session->seat = strdup(all_sessions[i].seat);
            session->tty = strdup(all_sessions[i].tty);
            if (all_sessions[i].display) session->display = strdup(all_sessions[i].display);
            if (all_sessions[i].desktop) session->desktop = strdup(all_sessions[i].desktop);
            if (all_sessions[i].state) session->state = strdup(all_sessions[i].state);
            
            turnstile_free_sessions(all_sessions, count);
            return 0;
        }
    }
    
    turnstile_free_sessions(all_sessions, count);
    errno = ESRCH;
    return -1;
}

bool turnstile_session_is_graphical(const turnstile_session *session) {
    if (!session->type) return false;
    return (strcmp(session->type, "x11") == 0 || 
            strcmp(session->type, "wayland") == 0);
}

int turnstile_session_get_display_string(const turnstile_session *session, char *buf, size_t bufsize) {
    if (turnstile_session_is_graphical(session) && session->display) {
        snprintf(buf, bufsize, "%s:%s", session->seat ? session->seat : "seat0", session->display);
    } else if (session->tty) {
        snprintf(buf, bufsize, "%s", session->tty);
    } else {
        snprintf(buf, bufsize, "unknown");
    }
    return 0;
}
