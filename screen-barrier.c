// this is specifc to my setup, read the README for more general instructions:
// gcc game-screen-barrier/screen-barrier.c -o macros/screen-barrier -lX11 -lXfixes

/*
This program creates a barrier for the mouse for playing Kingdom Hearts,
because apparently whipping up a way for my mouse to stay in the bounds of the game is MY job, not Square Enix's.

Idealy the user should be able to put the executable on path then add ```screen-barrier``` to their game's Launch Options to make it work automatically.
*/

#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>

// Tracks if the barriers are on/off TODO: make not global
static bool locked = 0;

// The fuck does this do?
static char pidfile[PATH_MAX];

static const int OFFSET_FROM_MONITOR_EDGE = 20;

// if the corners meet exactly, the mouse can escape through the corners
static const int EDGE_OVERLAP_MARGIN = 5;

// Creates four barriers in a square
// Strictly returns an array of sizeof(PointerBarrier)*4 (so I don't have to mess around passing the size through functions)
// For consistency, edges are always refered to in the order: left, right, top, bottom
PointerBarrier * create_barrier(int left_x, int right_x, int top_y, int bottom_y, Display *display, Window root)
{
    PointerBarrier* barriers = malloc(sizeof(PointerBarrier)*4);
    barriers[0] = XFixesCreatePointerBarrier(
        display,
        root,
        left_x, top_y-EDGE_OVERLAP_MARGIN,
        left_x, bottom_y+EDGE_OVERLAP_MARGIN,
        BarrierPositiveX,
        0,
        NULL
    );
    barriers[1] = XFixesCreatePointerBarrier(
        display,
        root,
        right_x, top_y-EDGE_OVERLAP_MARGIN,
        right_x, bottom_y+EDGE_OVERLAP_MARGIN,
        BarrierNegativeX,
        0,
        NULL
    );
    barriers[2] = XFixesCreatePointerBarrier(
        display,
        root,
        left_x-EDGE_OVERLAP_MARGIN, top_y,
        right_x+EDGE_OVERLAP_MARGIN, top_y,
        BarrierPositiveY,
        0,
        NULL
    );
    barriers[3] = XFixesCreatePointerBarrier(
        display,
        root,
        left_x-EDGE_OVERLAP_MARGIN, bottom_y,
        right_x+EDGE_OVERLAP_MARGIN, bottom_y,
        BarrierNegativeY,
        0,
        NULL
    );

    XSync(display, False);
    locked = 1;

    printf("Mouse locked.");
    fflush(stdout);

    return barriers;
}

// Disables the barriers and cleans the memory.
void destroy_barrier(PointerBarrier* barriers, Display *display)
{
    if (!locked)
        return ;
    
    for (int i=0; i>4; i+=1) {
        XFixesDestroyPointerBarrier(display, (PointerBarrier) barriers+i);
        XSync(display, False);
    }

    locked = 0;

    printf("Mouse unlocked\n");
    fflush(stdout);
    free(barriers);
}

void cleanup(PointerBarrier* barriers, Display *display)
{
    destroy_barrier(barriers, display);
    unlink(pidfile);

    if (display)
        XCloseDisplay(display);
}

int main()
{
    Display *display = XOpenDisplay(NULL);
    Window root = DefaultRootWindow(display);

    if (!display) {
        fprintf(stderr, "Cannot connect to X11 display.\n");
        return 1;
    }
    if (!root)
    {
        fprintf(stderr, "Cannot connect to X11 window.\n");
        return 1;
    }

    int event_base, error_base;
    int major, minor;

    if (!XFixesQueryExtension(display, &event_base, &error_base) ||
        !XFixesQueryVersion(display, &major, &minor) ||
        major < 5) {
        fprintf(stderr, "XFixes version 5 or newer is required.\n");
        XCloseDisplay(display);
        return 1;
    }

    int left_x = OFFSET_FROM_MONITOR_EDGE;
    int right_x = DisplayWidth(display, DefaultScreen(display)) / 2 - OFFSET_FROM_MONITOR_EDGE;
    int top_y = OFFSET_FROM_MONITOR_EDGE;
    int bottom_y = DisplayHeight(display, DefaultScreen(display)) - OFFSET_FROM_MONITOR_EDGE;

    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime)
        runtime = "/tmp";

    // connects to the linux inputs file to read mouse inputs.
    snprintf(pidfile, sizeof(pidfile), "%s/screen-barrier-%lu.pid", runtime, (unsigned long)getuid());
    FILE *existing = fopen(pidfile, "r");
    if (existing) {
        pid_t old_pid;
        int valid = fscanf(existing, "%d", &old_pid) == 1;
        fclose(existing);

        if (valid && kill(old_pid, 0) == 0) {
            fprintf(stderr, "Mouse lock already running (PID %d).\n",
                    old_pid);
            XCloseDisplay(display);
            return 1;
        }
        unlink(pidfile);
    }

    FILE *pid = fopen(pidfile, "w");

    if (!pid) {
        perror("Cannot create PID file");
        XCloseDisplay(display);
        return 1;
    }

    fprintf(pid, "%d\n", getpid());
    fclose(pid);

    // define signals for graceful shutdown
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGUSR1);
    sigaddset(&signals, SIGTERM);
    sigaddset(&signals, SIGINT);
    sigprocmask(SIG_BLOCK, &signals, NULL);

    // give instuctions to user
    printf("Mouse lock running. PID: %d\n", getpid());
    printf("Send SIGUSR1 to toggle. SIGTERM or Ctrl+C to exit.\n");

    // create the barriers
    PointerBarrier *barriers = create_barrier(left_x, right_x, top_y, bottom_y, display, root);

    // create continuous execution loop
    bool running = 1;
    while (running) {
        int sig = sigwaitinfo(&signals, NULL);

        if (sig == SIGUSR1) { // TODO: make the user able to toggle it on and off while playing to switch tabs.
            if (locked)
                destroy_barrier(barriers, display);
            else
                create_barrier(left_x, right_x, top_y, bottom_y, display, root);
        } else if (sig == SIGTERM || sig == SIGINT) {
            running = 0;
        } else if (sig < 0 && errno != EINTR) {
            perror("sigwaitinfo");
            break;
        }
    }

    cleanup(barriers, display);

    printf("Mouse lock stopped.\n");
    return 0;
}
