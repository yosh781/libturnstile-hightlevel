/*
 * Shim library to redirect libsystemd/libelogind calls to Turnstile
 * 
 * Usage: LD_PRELOAD=/usr/lib/turnstile-shim.so program
 * 
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2025 Valera <valera@mxd>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>
#include <turnstile-highlevel.h>

// Типы для совместимости
typedef enum {
    SD_BUS_ERROR_NONE = 0,
    SD_BUS_ERROR_FAILED = 1,
} sd_bus_error;

typedef struct sd_bus sd_bus;
typedef struct sd_bus_message sd_bus_message;
typedef struct sd_bus_slot sd_bus_slot;

// Оригинальные функции (будем вызывать через dlsym)
static int (*real_sd_bus_default_system)(sd_bus **ret) = NULL;
static int (*real_sd_bus_default_user)(sd_bus **ret) = NULL;
static int (*real_sd_bus_open_system)(sd_bus **ret) = NULL;
static int (*real_sd_bus_open_user)(sd_bus **ret) = NULL;
static void (*real_sd_bus_error_free)(sd_bus_error *e) = NULL;
static int (*real_sd_bus_call_method)(
    sd_bus *bus,
    const char *destination,
    const char *path,
    const char *interface,
    const char *member,
    sd_bus_error *error,
    sd_bus_message **reply,
    const char *types,
    ...
) = NULL;

// Инициализация указателей на оригинальные функции
__attribute__((constructor))
static void init_shim(void) {
    real_sd_bus_default_system = dlsym(RTLD_NEXT, "sd_bus_default_system");
    real_sd_bus_default_user = dlsym(RTLD_NEXT, "sd_bus_default_user");
    real_sd_bus_open_system = dlsym(RTLD_NEXT, "sd_bus_open_system");
    real_sd_bus_open_user = dlsym(RTLD_NEXT, "sd_bus_open_user");
    real_sd_bus_error_free = dlsym(RTLD_NEXT, "sd_bus_error_free");
    real_sd_bus_call_method = dlsym(RTLD_NEXT, "sd_bus_call_method");
    
    fprintf(stderr, "Turnstile shim loaded: redirecting libsystemd/libelogind calls\n");
}

// Перехват sd_bus_default_system
int sd_bus_default_system(sd_bus **ret) {
    fprintf(stderr, "Turnstile shim: sd_bus_default_system intercepted\n");
    
    // Вместо реального D-Bus соединения, возвращаем фейковое
    // и будем обрабатывать вызовы через Turnstile
    if (real_sd_bus_default_system) {
        return real_sd_bus_default_system(ret);
    }
    
    return -1; // ENOSYS
}

// Перехват sd_bus_open_system  
int sd_bus_open_system(sd_bus **ret) {
    fprintf(stderr, "Turnstile shim: sd_bus_open_system intercepted\n");
    
    if (real_sd_bus_open_system) {
        return real_sd_bus_open_system(ret);
    }
    
    return -1;
}

// Перехват sd_bus_call_method - здесь мы обрабатываем logind вызовы
int sd_bus_call_method(
    sd_bus *bus,
    const char *destination,
    const char *path,
    const char *interface,
    const char *member,
    sd_bus_error *error,
    sd_bus_message **reply,
    const char *types,
    ...
) {
    fprintf(stderr, "Turnstile shim: sd_bus_call_method %s.%s\n", 
            interface, member);
    
    // Проверяем, это вызов к logind?
    if (destination && strcmp(destination, "org.freedesktop.login1") == 0) {
        // Обрабатываем через Turnstile
        if (strcmp(member, "ListSessions") == 0) {
            // TODO: Эмулировать ответ с сессиями
            fprintf(stderr, "  -> Redirecting to Turnstile\n");
            // Здесь нужно создать фейковое сообщение с данными из turnstile_get_sessions()
            return 0; // Успех
        } else if (strcmp(member, "PowerOff") == 0) {
            turnstile_stop_all_sessions();
            sync();
            // Возвращаем успех, но реальное выключение будет позже
            return 0;
        } else if (strcmp(member, "Reboot") == 0) {
            turnstile_stop_all_sessions();
            sync();
            return 0;
        }
    }
    
    // Остальные вызовы передаем оригинальной библиотеке
    if (real_sd_bus_call_method) {
        va_list args;
        va_start(args, types);
        int ret = real_sd_bus_call_method(bus, destination, path, interface, 
                                          member, error, reply, types, args);
        va_end(args);
        return ret;
    }
    
    return -1;
}

// Функции для получения информации о сессиях
int sd_pid_get_session(pid_t pid, char **session) {
    fprintf(stderr, "Turnstile shim: sd_pid_get_session(%d)\n", pid);
    
    turnstile_session session_info;
    if (turnstile_get_session_by_pid(pid, &session_info) == 0) {
        char session_id[32];
        snprintf(session_id, sizeof(session_id), "%lu", session_info.id);
        *session = strdup(session_id);
        turnstile_free_sessions(&session_info, 1);
        return 0;
    }
    
    return -ENOENT;
}

int sd_pid_get_user_unit(pid_t pid, char **unit) {
    fprintf(stderr, "Turnstile shim: sd_pid_get_user_unit(%d)\n", pid);
    // Возвращаем заглушку
    *unit = strdup("user@1000.service");
    return 0;
}

int sd_pid_get_owner_uid(pid_t pid, uid_t *uid) {
    fprintf(stderr, "Turnstile shim: sd_pid_get_owner_uid(%d)\n", pid);
    
    turnstile_session session;
    if (turnstile_get_session_by_pid(pid, &session) == 0) {
        *uid = session.uid;
        turnstile_free_sessions(&session, 1);
        return 0;
    }
    
    return -ENOENT;
}

// Версионирование
const char *sd_bus_get_version(void) {
    return "turnstile-shim-0.6.0";
}

// Для совместимости с elogind
const char *elogind_get_version(void) {
    return "turnstile-shim-0.6.0";
}
