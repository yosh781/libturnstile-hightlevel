/*
 * High-level API for Turnstile session management
 *
 * This provides convenient functions for session inspection and control
 * without needing to understand the low-level protocol.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2025 Valera <valera@mxd>
 */

#ifndef TURNSTILE_HIGHLEVEL_H
#define TURNSTILE_HIGHLEVEL_H

#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Session information structure (ENHANCED)
 */
typedef struct {
    /* Basic session info */
    unsigned long id;      /* Session ID */
    unsigned int uid;      /* User ID */
    char *username;        /* Username (must be freed) */
    char *type;            /* Session type: x11, wayland, tty, etc. */
    char *class;           /* Session class: user, greeter, lock-screen, etc. */
    char *seat;            /* Seat name (e.g., "seat0") */
    char *tty;             /* TTY device if applicable */
    unsigned long vtnr;    /* Virtual terminal number */
    int remote;            /* 1 if remote session, 0 otherwise */
    pid_t leader;          /* Session leader process PID */
    
    /* NEW: Extended session info */
    char *display;         /* X11/Wayland display (e.g., ":0") */
    char *desktop;         /* Desktop environment (GNOME, KDE, sway, etc.) */
    char *state;           /* Session state: active, online, closing, inactive */
    char *remote_user;     /* Remote username (for SSH/VNC) */
    char *remote_host;     /* Remote hostname/IP (for SSH/VNC) */
    time_t created_at;     /* Session creation timestamp */
    time_t idle_since;     /* Idle start timestamp (0 = active) */
    bool linger;           /* Keep processes after session ends */
    unsigned long scope;   /* Service scope ID */
} turnstile_session;

/**
 * Seat information structure (NEW)
 */
typedef struct {
    char *name;            /* Seat name (e.g., "seat0") */
    char *active_session;  /* Currently active session ID */
    unsigned long active_vtnr; /* Currently active VT number */
    bool can_multi_session; /* Supports multi-session */
    size_t session_count;   /* Number of sessions on this seat */
} turnstile_seat;

/* ========== EXISTING FUNCTIONS ========== */

int turnstile_get_sessions(turnstile_session **sessions, size_t *count);
int turnstile_get_user_sessions(uid_t uid, turnstile_session **sessions, size_t *count);
int turnstile_get_session_by_id(unsigned long id, turnstile_session *session);
void turnstile_free_sessions(turnstile_session *sessions, size_t count);
int turnstile_stop_session(pid_t leader);
int turnstile_stop_all_sessions(void);
int turnstile_get_runtime_dir(uid_t uid, char **path);

/* ========== NEW: SESSION MANAGEMENT ========== */

/**
 * Create a new session
 *
 * @param session    Session information (filled by caller)
 * @param session_id Output: newly created session ID
 * @return 0 on success, -1 on error
 */
int turnstile_create_session(turnstile_session *session, unsigned long *session_id);

/**
 * Release (end) a session
 *
 * @param session_id Session ID to release
 * @return 0 on success, -1 on error
 */
int turnstile_release_session(unsigned long session_id);

/**
 * Activate a session (switch to its VT)
 *
 * @param session_id Session ID to activate
 * @return 0 on success, -1 on error
 */
int turnstile_activate_session(unsigned long session_id);

/**
 * Set session state
 *
 * @param session_id Session ID
 * @param state      New state ("active", "online", "closing", "inactive")
 * @return 0 on success, -1 on error
 */
int turnstile_set_session_state(unsigned long session_id, const char *state);

/**
 * Set session idle hint
 *
 * @param session_id Session ID
 * @param idle       true if idle, false if active
 * @return 0 on success, -1 on error
 */
int turnstile_set_session_idle(unsigned long session_id, bool idle);

/**
 * Get session by process PID
 *
 * @param pid      Process ID
 * @param session  Output session structure (must be freed with turnstile_free_sessions)
 * @return 0 on success, -1 on error
 */
int turnstile_get_session_by_pid(pid_t pid, turnstile_session *session);

/* ========== NEW: SEAT MANAGEMENT ========== */

/**
 * Get all seats
 *
 * @param seats  Pointer to array of seat strings (will be allocated)
 * @param count  Number of seats found
 * @return 0 on success, -1 on error
 */
int turnstile_get_seats(char ***seats, size_t *count);

/**
 * Get currently active seat
 *
 * @param seat  Pointer to seat name string (will be allocated)
 * @return 0 on success, -1 on error
 */
int turnstile_get_active_seat(char **seat);

/**
 * Get currently active VT number
 *
 * @param vtnr  Output VT number
 * @return 0 on success, -1 on error
 */
int turnstile_get_active_vtnr(unsigned long *vtnr);

/**
 * Get seat information
 *
 * @param seat_name  Seat name (e.g., "seat0")
 * @param seat_info  Output seat information (must be freed)
 * @return 0 on success, -1 on error
 */
int turnstile_get_seat_info(const char *seat_name, turnstile_seat *seat_info);

/**
 * Free seat info allocated by turnstile_get_seat_info()
 *
 * @param seat_info  Seat info to free
 */
void turnstile_free_seat_info(turnstile_seat *seat_info);

/* ========== NEW: RUNTIME DIRECTORY ========== */

/**
 * Setup XDG_RUNTIME_DIR for a user
 *
 * @param uid  User ID
 * @return 0 on success, -1 on error
 */
int turnstile_setup_runtime_dir(uid_t uid);

/**
 * Cleanup XDG_RUNTIME_DIR for a user
 *
 * @param uid  User ID
 * @return 0 on success, -1 on error
 */
int turnstile_cleanup_runtime_dir(uid_t uid);

/* ========== NEW: HELPER FUNCTIONS ========== */

/**
 * Check if a session is graphical (X11 or Wayland)
 *
 * @param session  Session to check
 * @return true if graphical, false otherwise
 */
bool turnstile_session_is_graphical(const turnstile_session *session);

/**
 * Get session display string (e.g., "seat0:1" or "tty2")
 *
 * @param session  Session
 * @param buf      Output buffer
 * @param bufsize  Buffer size
 * @return 0 on success, -1 on error
 */
int turnstile_session_get_display_string(const turnstile_session *session, char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif

#endif /* TURNSTILE_HIGHLEVEL_H */

/* ========== SYSTEM POWER MANAGEMENT ========== */

/**
 * Power off the system
 * @return 0 on success, -1 on error
 */
int turnstile_power_off(void);

/**
 * Reboot the system
 * @return 0 on success, -1 on error
 */
int turnstile_reboot(void);

/**
 * Check if suspend is supported
 * @return 1 if supported, 0 otherwise
 */
int turnstile_can_suspend(void);

/**
 * Suspend the system (RAM sleep)
 * @return 0 on success, -1 on error
 */
int turnstile_suspend(void);

/**
 * Hibernate the system (disk sleep)
 * @return 0 on success, -1 on error
 */
int turnstile_hibernate(void);
