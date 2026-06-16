/* Test power management functions */
#include <stdio.h>
#include "include/turnstile-highlevel.h"

int main() {
    printf("=== System Capabilities ===\n");
    
    if (turnstile_can_suspend()) {
        printf("✓ Suspend supported\n");
    } else {
        printf("✗ Suspend not supported\n");
    }
    
    printf("\nTo test power operations, run as root:\n");
    printf("  turnstile_power_off() - Power off\n");
    printf("  turnstile_reboot()    - Reboot\n");
    printf("  turnstile_suspend()   - Suspend to RAM\n");
    printf("  turnstile_hibernate() - Hibernate to disk\n");
    
    return 0;
}
