#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include "turnstile-highlevel.h"

#define SESSIONS_DIR "/run/turnstiled/sessions"

/* Helper functions */
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
    if (!path || !session) return -1;
    
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
    
    session->display = NULL;
    session->desktop = NULL;
    session->state = strdup("active");
    session->remote_user = NULL;
    session->remote_host = NULL;
    session->created_at = 0;
    session->idle_since = 0;
    session->linger = false;
    session->scope = 0;
    
    return 0;
}

static void free_session_fields(turnstile_session *s) {
    if (!s) return;
    free(s->username);
    free(s->type);
    free(s->class);
    free(s->seat);
    free(s->tty);
    free(s->display);
    free(s->desktop);
    free(s->state);
    free(s->remote_user);
    free(s->remote_host);
}

/* Basic session functions */
int turnstile_get_sessions(turnstile_session **sessions, size_t *count) {
    if (!sessions || !count) return -1;
    
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
    if (!sessions || !count) return -1;
    
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
            free_session_fields(&all_sessions[i]);
        }
    }
    
    free(all_sessions);
    
    *sessions = result;
    *count = match_count;
    return 0;
}

int turnstile_get_session_by_id(unsigned long id, turnstile_session *session) {
    if (!session) return -1;
    
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
        free_session_fields(&sessions[i]);
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
    if (!path) return -1;
    
    if (asprintf(path, "/run/user/%u", uid) < 0) {
        return -1;
    }
    return 0;
}

/* Seat management */
int turnstile_get_seats(char ***seats, size_t *count) {
    if (!seats || !count) return -1;
    
    turnstile_session *sessions;
    size_t session_count;
    
    if (turnstile_get_sessions(&sessions, &session_count) < 0) {
        *seats = NULL;
        *count = 0;
        return -1;
    }
    
    char **seat_list = NULL;
    size_t seat_count = 0;
    
    for (size_t i = 0; i < session_count; i++) {
        if (sessions[i].seat && sessions[i].seat[0]) {
            int found = 0;
            for (size_t j = 0; j < seat_count; j++) {
                if (strcmp(seat_list[j], sessions[i].seat) == 0) {
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                char **new_list = realloc(seat_list, (seat_count + 1) * sizeof(char*));
                if (new_list) {
                    seat_list = new_list;
                    seat_list[seat_count++] = strdup(sessions[i].seat);
                }
            }
        }
    }
    
    turnstile_free_sessions(sessions, session_count);
    
    *seats = seat_list;
    *count = seat_count;
    return 0;
}

int turnstile_get_active_vtnr(unsigned long *vtnr) {
    if (!vtnr) return -1;
    
    /* Пробуем открыть tty0 на запись (достаточно для ioctl) */
    int fd = open("/dev/tty0", O_WRONLY | O_NOCTTY);
    if (fd < 0) {
        /* Fallback: из файлов сессий */
        turnstile_session *sessions;
        size_t count;
        if (turnstile_get_sessions(&sessions, &count) == 0 && count > 0) {
            *vtnr = sessions[0].vtnr;
            turnstile_free_sessions(sessions, count);
            return 0;
        }
        return -1;
    }
    
    struct vt_stat vtstat;
    if (ioctl(fd, VT_GETSTATE, &vtstat) == 0) {
        *vtnr = vtstat.v_active;
        close(fd);
        return 0;
    }
    
    close(fd);
    return -1;
}

int turnstile_get_active_seat(char **seat) {
    if (!seat) return -1;
    
    unsigned long vtnr;
    if (turnstile_get_active_vtnr(&vtnr) < 0) {
        *seat = strdup("seat0");
        return 0;
    }
    
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) < 0) {
        *seat = strdup("seat0");
        return 0;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (sessions[i].vtnr == vtnr && sessions[i].seat) {
            *seat = strdup(sessions[i].seat);
            turnstile_free_sessions(sessions, count);
            return 0;
        }
    }
    
    turnstile_free_sessions(sessions, count);
    *seat = strdup("seat0");
    return 0;
}

int turnstile_get_seat_info(const char *seat_name, turnstile_seat *seat_info) {
    if (!seat_name || !seat_info) return -1;
    
    memset(seat_info, 0, sizeof(turnstile_seat));
    seat_info->name = strdup(seat_name);
    seat_info->can_multi_session = true;
    
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) < 0) {
        return -1;
    }
    
    seat_info->session_count = 0;
    unsigned long active_vtnr = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (sessions[i].seat && strcmp(sessions[i].seat, seat_name) == 0) {
            seat_info->session_count++;
            
            if (sessions[i].vtnr > 0) {
                active_vtnr = sessions[i].vtnr;
                if (sessions[i].username) {
                    free(seat_info->active_session);
                    seat_info->active_session = strdup(sessions[i].username);
                }
            }
        }
    }
    
    seat_info->active_vtnr = active_vtnr;
    turnstile_free_sessions(sessions, count);
    
    return 0;
}

void turnstile_free_seat_info(turnstile_seat *seat_info) {
    if (!seat_info) return;
    free(seat_info->name);
    free(seat_info->active_session);
}

/* Stub functions */
int turnstile_create_session(turnstile_session *session, unsigned long *session_id) {
    (void)session;
    (void)session_id;
    return -1;
}

int turnstile_release_session(unsigned long session_id) {
    (void)session_id;
    return -1;
}

int turnstile_activate_session(unsigned long session_id) {
    turnstile_session session;
    int ret = -1;
    
    if (turnstile_get_session_by_id(session_id, &session) < 0) {
        return -1;
    }
    
    /* Проверяем, есть ли VT у сессии */
    if (session.vtnr == 0) {
        free_session_fields(&session);
        return -1;
    }
    
    /* Открываем tty0 на запись — VT_GETSTATE работает через ioctl даже с O_WRONLY */
    int fd = open("/dev/tty0", O_WRONLY | O_NOCTTY);
    if (fd >= 0) {
        struct vt_stat vtstat;
        
        /* Проверяем текущий активный VT */
        if (ioctl(fd, VT_GETSTATE, &vtstat) == 0) {
            if (vtstat.v_active == session.vtnr) {
                /* VT уже активен — ничего не делаем, возвращаем успех */
                close(fd);
                free_session_fields(&session);
                return 0;
            }
        }
        
        /* Переключаем на нужный VT */
        if (ioctl(fd, VT_ACTIVATE, session.vtnr) == 0) {
            ret = 0;
            /* Ждём завершения переключения */
            ioctl(fd, VT_WAITACTIVE, session.vtnr);
        }
        
        close(fd);
    }
    
    free_session_fields(&session);
    return ret;
}

int turnstile_set_session_state(unsigned long session_id, const char *state) {
    (void)session_id;
    (void)state;
    return -1;
}

int turnstile_set_session_idle(unsigned long session_id, bool idle) {
    (void)session_id;
    (void)idle;
    return -1;
}

int turnstile_get_session_by_pid(pid_t pid, turnstile_session *session) {
    if (!session) return -1;
    
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) < 0) {
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (sessions[i].leader == pid) {
            memcpy(session, &sessions[i], sizeof(turnstile_session));
            session->username = sessions[i].username ? strdup(sessions[i].username) : NULL;
            session->type = sessions[i].type ? strdup(sessions[i].type) : NULL;
            session->class = sessions[i].class ? strdup(sessions[i].class) : NULL;
            session->seat = sessions[i].seat ? strdup(sessions[i].seat) : NULL;
            session->tty = sessions[i].tty ? strdup(sessions[i].tty) : NULL;
            session->state = sessions[i].state ? strdup(sessions[i].state) : NULL;
            
            turnstile_free_sessions(sessions, count);
            return 0;
        }
    }
    
    turnstile_free_sessions(sessions, count);
    return -1;
}

int turnstile_setup_runtime_dir(uid_t uid) {
    char path[256];
    snprintf(path, sizeof(path), "/run/user/%u", uid);
    
    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        return -1;
    }
    
    if (chown(path, uid, uid) != 0) {
        return -1;
    }
    
    if (chmod(path, 0700) != 0) {
        return -1;
    }
    
    setenv("XDG_RUNTIME_DIR", path, 1);
    return 0;
}

int turnstile_cleanup_runtime_dir(uid_t uid) {
    char path[256];
    snprintf(path, sizeof(path), "/run/user/%u", uid);
    
    if (rmdir(path) != 0 && errno != ENOENT) {
        return -1;
    }
    
    return 0;
}

bool turnstile_session_is_graphical(const turnstile_session *session) {
    if (!session || !session->type) return false;
    return (strcmp(session->type, "x11") == 0 || 
            strcmp(session->type, "wayland") == 0);
}

int turnstile_session_get_display_string(const turnstile_session *session, 
                                         char *buf, size_t bufsize) {
    if (!session || !buf || bufsize == 0) return -1;
    
    if (session->display && session->display[0]) {
        snprintf(buf, bufsize, "%s", session->display);
    } else if (session->seat && session->vtnr > 0) {
        snprintf(buf, bufsize, "%s:%lu", session->seat, session->vtnr);
    } else if (session->tty && session->tty[0]) {
        snprintf(buf, bufsize, "%s", session->tty);
    } else {
        snprintf(buf, bufsize, "session-%lu", session->id);
    }
    
    return 0;
}
