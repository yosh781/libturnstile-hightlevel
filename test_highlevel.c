/* test_highlevel.c - Test the high-level API */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "include/turnstile-highlevel.h"

int main() {
    printf("=== Turnstile High-Level API Test ===\n\n");
    
    /* Test 1: Get all sessions */
    printf("1. Getting all sessions:\n");
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) == 0) {
        printf("   Found %zu sessions\n", count);
        for (size_t i = 0; i < count; i++) {
            printf("   Session %lu:\n", sessions[i].id);
            printf("     User: %s (UID: %u)\n", 
                   sessions[i].username ? sessions[i].username : "none", 
                   sessions[i].uid);
            printf("     Type: %s, Class: %s\n", 
                   sessions[i].type ? sessions[i].type : "none", 
                   sessions[i].class ? sessions[i].class : "none");
            printf("     Seat: %s, VT: %lu\n", 
                   sessions[i].seat ? sessions[i].seat : "none", 
                   sessions[i].vtnr);
            printf("     Leader: %d\n", sessions[i].leader);
            printf("     Graphical: %s\n", 
                   turnstile_session_is_graphical(&sessions[i]) ? "yes" : "no");
            
            char display[64];
            turnstile_session_get_display_string(&sessions[i], display, sizeof(display));
            printf("     Display: %s\n", display);
        }
        turnstile_free_sessions(sessions, count);
    } else {
        printf("   Failed to get sessions\n");
    }
    
    /* Test 2: Get current user sessions */
    printf("\n2. Getting current user sessions:\n");
    uid_t uid = getuid();
    if (turnstile_get_user_sessions(uid, &sessions, &count) == 0) {
        printf("   Found %zu sessions for UID %u\n", count, uid);
        turnstile_free_sessions(sessions, count);
    }
    
    /* Test 3: Get seats */
    printf("\n3. Getting available seats:\n");
    char **seats;
    size_t seat_count;
    if (turnstile_get_seats(&seats, &seat_count) == 0) {
        printf("   Found %zu seats:\n", seat_count);
        for (size_t i = 0; i < seat_count; i++) {
            printf("     - %s\n", seats[i]);
            free(seats[i]);
        }
        free(seats);
    }
    
    /* Test 4: Get active seat */
    printf("\n4. Getting active seat:\n");
    char *active_seat;
    if (turnstile_get_active_seat(&active_seat) == 0) {
        printf("   Active seat: %s\n", active_seat);
        
        turnstile_seat seat_info;
        if (turnstile_get_seat_info(active_seat, &seat_info) == 0) {
            printf("   Seat info:\n");
            printf("     Name: %s\n", seat_info.name);
            printf("     Active VT: %lu\n", seat_info.active_vtnr);
            printf("     Session count: %zu\n", seat_info.session_count);
            turnstile_free_seat_info(&seat_info);
        }
        
        free(active_seat);
    }
    
    /* Test 5: Get active VT */
    printf("\n5. Getting active VT number:\n");
    unsigned long vtnr;
    if (turnstile_get_active_vtnr(&vtnr) == 0) {
        printf("   Active VT: %lu\n", vtnr);
    }
    
    /* Test 6: Runtime directory */
    printf("\n6. Runtime directory:\n");
    char *runtime_dir;
    if (turnstile_get_runtime_dir(uid, &runtime_dir) == 0) {
        printf("   XDG_RUNTIME_DIR: %s\n", runtime_dir);
        free(runtime_dir);
    }
    
    printf("\n=== Test completed ===\n");
    return 0;
}
