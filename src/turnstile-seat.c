#define _GNU_SOURCE

/*
 * Seat management for Turnstile
 *
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2025 Valera <valera@mxd>
 */

#include "turnstile-highlevel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define SESSIONS_DIR "/run/turnstiled/sessions"

int turnstile_get_seats(char ***seats, size_t *count) {
    turnstile_session *sessions;
    size_t session_count;
    
    if (turnstile_get_sessions(&sessions, &session_count) < 0) {
        return -1;
    }
    
    // Count unique seats
    size_t unique_seats = 0;
    char **seat_list = NULL;
    
    for (size_t i = 0; i < session_count; i++) {
        if (!sessions[i].seat) continue;
        
        int found = 0;
        for (size_t j = 0; j < unique_seats; j++) {
            if (seat_list[j] && strcmp(seat_list[j], sessions[i].seat) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            seat_list = realloc(seat_list, (unique_seats + 1) * sizeof(char*));
            if (!seat_list) {
                turnstile_free_sessions(sessions, session_count);
                return -1;
            }
            seat_list[unique_seats] = strdup(sessions[i].seat);
            unique_seats++;
        }
    }
    
    turnstile_free_sessions(sessions, session_count);
    
    *seats = seat_list;
    *count = unique_seats;
    return 0;
}

int turnstile_get_active_seat(char **seat) {
    // Simple implementation: return first seat with active session
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) < 0) {
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (sessions[i].state && strcmp(sessions[i].state, "active") == 0) {
            *seat = strdup(sessions[i].seat ? sessions[i].seat : "seat0");
            turnstile_free_sessions(sessions, count);
            return 0;
        }
    }
    
    // Fallback to first seat
    if (count > 0 && sessions[0].seat) {
        *seat = strdup(sessions[0].seat);
        turnstile_free_sessions(sessions, count);
        return 0;
    }
    
    turnstile_free_sessions(sessions, count);
    *seat = strdup("seat0");
    return 0;
}

int turnstile_get_active_vtnr(unsigned long *vtnr) {
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) < 0) {
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (sessions[i].state && strcmp(sessions[i].state, "active") == 0) {
            *vtnr = sessions[i].vtnr;
            turnstile_free_sessions(sessions, count);
            return 0;
        }
    }
    
    turnstile_free_sessions(sessions, count);
    *vtnr = 0;
    return 0;
}

int turnstile_get_seat_info(const char *seat_name, turnstile_seat *seat_info) {
    memset(seat_info, 0, sizeof(turnstile_seat));
    
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) < 0) {
        return -1;
    }
    
    seat_info->name = strdup(seat_name);
    seat_info->session_count = 0;
    seat_info->can_multi_session = true;
    
    for (size_t i = 0; i < count; i++) {
        if (sessions[i].seat && strcmp(sessions[i].seat, seat_name) == 0) {
            seat_info->session_count++;
            
            if (sessions[i].state && strcmp(sessions[i].state, "active") == 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lu", sessions[i].id);
                seat_info->active_session = strdup(buf);
                seat_info->active_vtnr = sessions[i].vtnr;
            }
        }
    }
    
    turnstile_free_sessions(sessions, count);
    return 0;
}

void turnstile_free_seat_info(turnstile_seat *seat_info) {
    if (!seat_info) return;
    free(seat_info->name);
    free(seat_info->active_session);
}
