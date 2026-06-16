# libturnstile-highlevel

[![Version](https://img.shields.io/badge/version-0.7.0-blue.svg)](https://github.com/yosh781/libturnstile-highlevel)
[![License](https://img.shields.io/badge/license-GPL-green.svg)](COPYING.md)
[![Platform](https://img.shields.io/badge/platform-Linux%20x86__64-orange.svg)]()

A high-level library for managing Turnstile sessions — a lightweight Linux session manager inspired by systemd-logind.

## 📋 Features

- **Session enumeration** — list all active sessions and filter by user
- **Seat information** — list seats, active seat, VT info
- **VT management** — virtual terminal switching
- **Runtime directory management** — create and clean up `/run/user/<UID>`
- **Graphical session support** — detect X11/Wayland, get display strings
- **Secure** — proper `/dev/tty0` permission handling via `tty` group

## 📦 Installation

### From deb package

```bash
sudo dpkg -i libturnstile-highlevel_0.7.0_amd64.deb
Build from source
bash
# Clone the repository
git clone https://github.com/yosh781/libturnstile-highlevel.git
cd libturnstile-highlevel

# Build and install
bash build.sh

# Or manually via meson
meson setup build --prefix=/usr --libdir=lib/x86_64-linux-gnu
ninja -C build
sudo ninja -C build install
Dependencies
libc6 (>= 2.17)

turnstile (>= 0.1.0) — low-level library/daemon

meson and ninja (for building)

Recommended: tty group membership for VT operations

bash
sudo usermod -a -G tty $USER
# Reboot or re-login required
🚀 Quick Start
c
#include <stdio.h>
#include <turnstile-highlevel.h>

int main() {
    // Get all sessions
    turnstile_session *sessions;
    size_t count;
    
    if (turnstile_get_sessions(&sessions, &count) == 0) {
        printf("Found %zu sessions:\n", count);
        
        for (size_t i = 0; i < count; i++) {
            printf("  Session %lu: %s (UID: %u, VT: %lu, Type: %s)\n",
                   sessions[i].id,
                   sessions[i].username,
                   sessions[i].uid,
                   sessions[i].vtnr,
                   turnstile_session_is_graphical(&sessions[i]) ? "graphical" : "console");
        }
        
        turnstile_free_sessions(sessions, count);
    }
    
    // Switch to a session
    turnstile_activate_session(1);
    
    // Get active VT number
    unsigned long vtnr;
    turnstile_get_active_vtnr(&vtnr);
    printf("Active VT: %lu\n", vtnr);
    
    return 0;
}
Compile:

bash
gcc -o myapp myapp.c -lturnstile-highlevel
📚 API Reference
Session Management
Function	Description
turnstile_get_sessions()	Get all active sessions
turnstile_get_user_sessions()	Get user sessions by UID
turnstile_get_session_by_id()	Get session by ID
turnstile_get_session_by_pid()	Find session by leader PID
turnstile_activate_session()	Switch VT to session
turnstile_stop_session()	Stop session (SIGTERM)
turnstile_stop_all_sessions()	Stop all sessions
turnstile_free_sessions()	Free session list memory
Seat Management
Function	Description
turnstile_get_seats()	Get list of seats
turnstile_get_active_seat()	Get active seat name
turnstile_get_seat_info()	Get seat information
turnstile_free_seat_info()	Free seat info memory
turnstile_get_active_vtnr()	Get active VT number
Runtime Directories
Function	Description
turnstile_get_runtime_dir()	Get runtime directory path
turnstile_setup_runtime_dir()	Create runtime directory
turnstile_cleanup_runtime_dir()	Remove runtime directory
Utilities
Function	Description
turnstile_session_is_graphical()	Check if session is graphical
turnstile_session_get_display_string()	Get display string
turnstile_create_session()	Create session (stub)
turnstile_release_session()	Release session (stub)
turnstile_set_session_state()	Set session state (stub)
turnstile_set_session_idle()	Set idle status (stub)
🔧 Command Line Tool
A command-line utility is included:

bash
turnstile-ctl list              # List all sessions
turnstile-ctl activate <ID>     # Activate a session
turnstile-ctl stop <PID>        # Stop a session
turnstile-ctl seats             # List seats
🐛 Bug Fixes in v0.7.0
Fixed: activate_session() now handles repeated activation of already active session (fast return ~11 µs)

Memory leak fixed: added free_session_fields() for stack-allocated sessions

Improved permissions: uses O_WRONLY for /dev/tty0 (compatible with tty group)

Added VT_WAITACTIVE: waits for VT switch confirmation

📊 Testing
bash
# Build and run tests
meson setup build
ninja -C build
build/turnstile-test

# Manual VT testing
gcc -o test test.c -lturnstile-highlevel
./test
Latest test results (v0.7.0):

text
✅ Repeated activation of active session — OK
✅ 100 consecutive calls without errors
✅ 1000 calls in 11.77 ms (11.77 µs/call)
✅ Non-existent session handling — correct
✅ Data integrity after multiple calls
🏗️ Architecture
text
┌─────────────────────────────────────┐
│     Application                      │
│     (test_highlevel.c, turnstile-ctl)│
└──────────────┬──────────────────────┘
               │ libturnstile-highlevel
┌──────────────▼──────────────────────┐
│  turnstile-highlevel.c              │
│  • Parse /run/turnstiled/sessions   │
│  • VT management (/dev/tty0)        │
│  • Runtime directories              │
└──────────────┬──────────────────────┘
               │ (optional)
               │ libturnstile
┌──────────────▼──────────────────────┐
│  turnstiled (daemon)                │
│  • D-Bus API                        │
│  • Session management               │
└─────────────────────────────────────┘
📄 License
GNU General Public License. See COPYING.md.

🤝 Contributing
Pull requests are welcome! Key areas for contribution:

Implement stub functions (create_session, release_session)

D-Bus backend support

Improved error handling

Additional architecture support

📞 Contact
Author: yosh781

Email: valera@mxd

GitHub: github.com/yosh781/libturnstile-highlevel

⚡ Fast. Lightweight. Turnstile.
