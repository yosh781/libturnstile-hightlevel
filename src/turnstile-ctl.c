#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include "turnstile-highlevel.h"

static const char *progname;

static void print_usage(void) {
    printf("Usage: %s [OPTIONS] COMMAND [ARGS]\n\n", progname);
    printf("Commands:\n");
    printf("  list                    List all sessions\n");
    printf("  list-user [UID]         List sessions for user (default: current)\n");
    printf("  show <ID>               Show session details\n");
    printf("  stop <ID|PID>           Stop session by ID or leader PID\n");
    printf("  stop-all                Stop all sessions\n");
    printf("  seats                   List all seats\n");
    printf("  seat-info <SEAT>        Show seat information\n");
    printf("  active-vt               Show active VT number\n");
    printf("  active-seat             Show active seat\n");
    printf("  activate <ID>           Activate session (switch VT)\n");
    printf("  runtime [UID]           Show XDG_RUNTIME_DIR for user\n");
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help\n");
    printf("  -v, --verbose           Verbose output\n");
}

static void print_session_short(const turnstile_session *sess) {
    char display_str[64] = "unknown";
    if (sess) {
        turnstile_session_get_display_string(sess, display_str, sizeof(display_str));
    }
    
    printf("%-8lu %-12s %-8s %-8s %-10s %s\n",
           sess ? sess->id : 0,
           (sess && sess->username) ? sess->username : "-",
           (sess && sess->type) ? sess->type : "-",
           (sess && sess->seat) ? sess->seat : "-",
           (sess && sess->state) ? sess->state : "-",
           display_str);
}

static void print_session_full(const turnstile_session *sess) {
    if (!sess) {
        printf("Session is NULL\n");
        return;
    }
    
    char display_str[64] = "unknown";
    turnstile_session_get_display_string(sess, display_str, sizeof(display_str));
    
    printf("Session ID:       %lu\n", sess->id);
    printf("Username:         %s\n", sess->username ? sess->username : "-");
    printf("UID:              %u\n", sess->uid);
    printf("Type:             %s\n", sess->type ? sess->type : "-");
    printf("Class:            %s\n", sess->class ? sess->class : "-");
    printf("Seat:             %s\n", sess->seat ? sess->seat : "-");
    printf("TTY:              %s\n", sess->tty ? sess->tty : "-");
    printf("VT Number:        %lu\n", sess->vtnr);
    printf("Remote:           %d\n", sess->remote);
    printf("Leader PID:       %d\n", sess->leader);
    printf("State:            %s\n", sess->state ? sess->state : "-");
    printf("Display:          %s\n", sess->display ? sess->display : "-");
    printf("Desktop:          %s\n", sess->desktop ? sess->desktop : "-");
    printf("Graphical:        %s\n", turnstile_session_is_graphical(sess) ? "yes" : "no");
    printf("Display string:   %s\n", display_str);
}

static int cmd_list(int verbose) {
    turnstile_session *sessions = NULL;
    size_t count = 0;
    
    if (turnstile_get_sessions(&sessions, &count) != 0) {
        fprintf(stderr, "Failed to get sessions\n");
        return 1;
    }
    
    if (count == 0) {
        printf("No active sessions\n");
        return 0;
    }
    
    if (verbose) {
        printf("\nFound %zu session(s):\n\n", count);
        for (size_t i = 0; i < count; i++) {
            printf("=== Session %zu ===\n", i + 1);
            print_session_full(&sessions[i]);
            printf("\n");
        }
    } else {
        printf("%-8s %-12s %-8s %-8s %-10s %s\n", 
               "ID", "USER", "TYPE", "SEAT", "STATE", "DISPLAY");
        printf("%-8s %-12s %-8s %-8s %-10s %s\n",
               "--------", "------------", "--------", "--------", "----------", "----------");
        for (size_t i = 0; i < count; i++) {
            print_session_short(&sessions[i]);
        }
    }
    
    turnstile_free_sessions(sessions, count);
    return 0;
}

static int cmd_list_user(const char *uid_str, int verbose) {
    (void)verbose;
    
    uid_t uid = uid_str ? atoi(uid_str) : getuid();
    
    turnstile_session *sessions = NULL;
    size_t count = 0;
    
    if (turnstile_get_user_sessions(uid, &sessions, &count) != 0) {
        fprintf(stderr, "Failed to get sessions for UID %u\n", uid);
        return 1;
    }
    
    if (count == 0) {
        printf("No sessions for UID %u\n", uid);
        return 0;
    }
    
    printf("Sessions for UID %u (%zu):\n", uid, count);
    for (size_t i = 0; i < count; i++) {
        printf("  %lu: %s on %s\n", 
               sessions[i].id,
               sessions[i].type ? sessions[i].type : "?",
               sessions[i].seat ? sessions[i].seat : "?");
    }
    
    turnstile_free_sessions(sessions, count);
    return 0;
}

static int cmd_show(const char *id_str) {
    unsigned long id = strtoul(id_str, NULL, 10);
    turnstile_session session;
    memset(&session, 0, sizeof(session));
    
    if (turnstile_get_session_by_id(id, &session) != 0) {
        fprintf(stderr, "Session %lu not found\n", id);
        return 1;
    }
    
    print_session_full(&session);
    
    /* Free only the allocated strings, not the session itself */
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
    
    return 0;
}

static int cmd_stop(const char *target) {
    unsigned long id = strtoul(target, NULL, 10);
    
    turnstile_session session;
    memset(&session, 0, sizeof(session));
    if (turnstile_get_session_by_id(id, &session) == 0) {
        if (session.leader > 0) {
            printf("Stopping session %lu (leader PID %d)\n", id, session.leader);
            if (turnstile_stop_session(session.leader) == 0) {
                printf("Session stopped\n");
                free(session.username);
                free(session.type);
                free(session.class);
                free(session.seat);
                free(session.tty);
                free(session.state);
                return 0;
            }
        }
        free(session.username);
        free(session.type);
        free(session.class);
        free(session.seat);
        free(session.tty);
        free(session.state);
    }
    
    pid_t pid = (pid_t)atoi(target);
    if (pid > 0 && kill(pid, 0) == 0) {
        printf("Stopping process PID %d\n", (int)pid);
        if (turnstile_stop_session(pid) == 0) {
            printf("Process terminated\n");
            return 0;
        }
    }
    
    fprintf(stderr, "Failed to stop %s\n", target);
    return 1;
}

static int cmd_stop_all(void) {
    printf("Stopping all sessions...\n");
    if (turnstile_stop_all_sessions() == 0) {
        printf("All sessions stopped\n");
        return 0;
    }
    fprintf(stderr, "Failed to stop all sessions\n");
    return 1;
}

static int cmd_seats(void) {
    char **seats = NULL;
    size_t count = 0;
    
    if (turnstile_get_seats(&seats, &count) != 0) {
        fprintf(stderr, "Failed to get seats\n");
        return 1;
    }
    
    if (count == 0) {
        printf("No seats found\n");
        return 0;
    }
    
    printf("Seats:\n");
    for (size_t i = 0; i < count; i++) {
        if (seats[i]) {
            printf("  %s\n", seats[i]);
            free(seats[i]);
        }
    }
    free(seats);
    return 0;
}

static int cmd_seat_info(const char *seat_name) {
    turnstile_seat seat_info;
    memset(&seat_info, 0, sizeof(seat_info));
    
    if (turnstile_get_seat_info(seat_name, &seat_info) != 0) {
        fprintf(stderr, "Failed to get info for seat %s\n", seat_name);
        return 1;
    }
    
    printf("Seat: %s\n", seat_info.name ? seat_info.name : "unknown");
    printf("  Active session: %s\n", seat_info.active_session ? seat_info.active_session : "none");
    printf("  Active VT: %lu\n", seat_info.active_vtnr);
    printf("  Multi-session capable: %s\n", seat_info.can_multi_session ? "yes" : "no");
    printf("  Session count: %zu\n", seat_info.session_count);
    
    turnstile_free_seat_info(&seat_info);
    return 0;
}

static int cmd_active_vt(void) {
    unsigned long vtnr = 0;
    
    if (turnstile_get_active_vtnr(&vtnr) != 0) {
        fprintf(stderr, "Failed to get active VT\n");
        return 1;
    }
    
    printf("Active VT: %lu\n", vtnr);
    return 0;
}

static int cmd_active_seat(void) {
    char *seat = NULL;
    
    if (turnstile_get_active_seat(&seat) != 0) {
        fprintf(stderr, "Failed to get active seat\n");
        return 1;
    }
    
    printf("Active seat: %s\n", seat ? seat : "unknown");
    free(seat);
    return 0;
}

static int cmd_activate(const char *id_str) {
    unsigned long id = strtoul(id_str, NULL, 10);
    
    printf("Activating session %lu...\n", id);
    if (turnstile_activate_session(id) == 0) {
        printf("Session activated\n");
        return 0;
    }
    
    fprintf(stderr, "Failed to activate session %lu\n", id);
    return 1;
}

static int cmd_runtime(const char *uid_str) {
    uid_t uid = uid_str ? atoi(uid_str) : getuid();
    char *runtime = NULL;
    
    if (turnstile_get_runtime_dir(uid, &runtime) != 0) {
        fprintf(stderr, "Failed to get runtime dir for UID %u\n", uid);
        return 1;
    }
    
    printf("XDG_RUNTIME_DIR for UID %u: %s\n", uid, runtime ? runtime : "unknown");
    free(runtime);
    return 0;
}

int main(int argc, char **argv) {
    progname = argv[0];
    
    int verbose = 0;
    int opt;
    
    static struct option long_opts[] = {
        {"help",    no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "hv", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 0;
            case 'v':
                verbose = 1;
                break;
            default:
                print_usage();
                return 1;
        }
    }
    
    if (optind >= argc) {
        print_usage();
        return 1;
    }
    
    const char *cmd = argv[optind];
    
    if (strcmp(cmd, "list") == 0) {
        return cmd_list(verbose);
    }
    else if (strcmp(cmd, "list-user") == 0) {
        const char *uid = (optind + 1 < argc) ? argv[optind + 1] : NULL;
        return cmd_list_user(uid, verbose);
    }
    else if (strcmp(cmd, "show") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Missing session ID\n");
            return 1;
        }
        return cmd_show(argv[optind + 1]);
    }
    else if (strcmp(cmd, "stop") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Missing session ID or PID\n");
            return 1;
        }
        return cmd_stop(argv[optind + 1]);
    }
    else if (strcmp(cmd, "stop-all") == 0) {
        return cmd_stop_all();
    }
    else if (strcmp(cmd, "seats") == 0) {
        return cmd_seats();
    }
    else if (strcmp(cmd, "seat-info") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Missing seat name\n");
            return 1;
        }
        return cmd_seat_info(argv[optind + 1]);
    }
    else if (strcmp(cmd, "active-vt") == 0) {
        return cmd_active_vt();
    }
    else if (strcmp(cmd, "active-seat") == 0) {
        return cmd_active_seat();
    }
    else if (strcmp(cmd, "activate") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Missing session ID\n");
            return 1;
        }
        return cmd_activate(argv[optind + 1]);
    }
    else if (strcmp(cmd, "runtime") == 0) {
        const char *uid = (optind + 1 < argc) ? argv[optind + 1] : NULL;
        return cmd_runtime(uid);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
}
