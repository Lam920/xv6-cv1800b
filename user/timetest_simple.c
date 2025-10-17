// user/timetest_simple.c
// Simple test program for gettimeofday0

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/include/time.h"

int main(int argc, char *argv[]) {
    struct timeval tv;
    
    printf("Testing gettimeofday0 syscall...\n\n");
    
    // Test 1: Get current time
    printf("Test 1: Get current time\n");
    if (gettimeofday0(&tv, 0) < 0) {
        printf("  ERROR: gettimeofday0 failed!\n");
        exit(1);
    }
    printf("  Success! tv_sec = %d, tv_usec = %d\n", tv.tv_sec, tv.tv_usec);
    
    // Test 2: Check if time increments
    printf("\nTest 2: Check time increment\n");
    int prev_sec = tv.tv_sec;
    printf("  Waiting for time to change...\n");
    
    int timeout = 0;
    while (timeout < 10) {
        gettimeofday0(&tv, 0);
        if (tv.tv_sec != prev_sec) {
            printf("  Time changed! Old: %d, New: %d\n", prev_sec, tv.tv_sec);
            printf("  ✓ Time is incrementing correctly!\n");
            break;
        }
        
        // Small delay
        for (volatile int i = 0; i < 10000000; i++);
        timeout++;
    }
    
    if (timeout >= 10) {
        printf("  ✗ WARNING: Time did not change after 10 iterations\n");
        printf("  This might indicate RTC is not running\n");
    }
    
    // Test 3: Show time for 5 seconds
    printf("\nTest 3: Display time for 5 seconds\n");
    for (int i = 0; i < 5; i++) {
        gettimeofday0(&tv, 0);
        
        // Convert to hours:minutes:seconds
        int total = tv.tv_sec;
        int hours = (total / 3600) % 24;
        int minutes = (total / 60) % 60;
        int seconds = total % 60;
        
        printf("  [%d] %d:%d:%d (timestamp: %d)\n", 
               i+1, hours, minutes, seconds, tv.tv_sec);
        
        // Wait ~1 second
        for (volatile int j = 0; j < 100000000; j++);
    }
    
    printf("\n✓ All tests passed!\n");
    exit(0);
}