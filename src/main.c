#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
// #include <time.h>
#include "wayland_state.h"
#include "layer_surface.h"
#include "capture.h"
#include "ipc_server.h"
#include "version.h"
#include "logo.h"

#define RECAPTURE_INTERVAL_MS 200 // 5 recaptures/sec

// static long long now_ms(void)
// {
//     struct timespec ts;
//     clock_gettime(CLOCK_MONOTONIC, &ts); // monotonoic: immune to system clock changes
//     return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
// }

static volatile sig_atomic_t should_exit = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    should_exit = 1;
}

static int activate(struct miru_state *state, struct miru_layer_surface *ls, struct miru_capture *capture)
{
    if (capture_output_frame(state, state->output, &should_exit, capture) != 0) {
        fprintf(stderr, "toggle: capture failed, staying inactive\n");
        return -1;
    }
    if (layer_surface_create(state, ls, capture) != 0) {
        fprintf(stderr, "toggle: failed to create layer surface \n");
        capture_frame_destroy(capture);
        return -1;
    }
    fprintf(stderr, "toggle: activated\n");
    return 0;
}

static void deactivate(struct miru_layer_surface *ls, struct miru_capture *capture)
{
    layer_surface_destroy(ls);
    *ls = (struct miru_layer_surface){ 0 };
    capture_frame_destroy(capture);
    fprintf(stderr, "toggle: deactivated\n");
}

int main(int argc, char *argv[])
{
    if (argc > 1 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        printf("\n");
        for (unsigned int i = 0; i < miru_ans_len; i++) {
            putchar(miru_ans[i]);
        }
        // Color reset and newline
        printf("\x1b[0m\n");

        printf("miru-daemon %s\n", MIRU_VERSION);
        printf("\nCombined Distribution subject to MIT license\n");
        printf("\nWritten by Vaishnav Sabari Girish\n");
        return 0;
    }

    struct miru_state state = { 0 };
    struct miru_layer_surface ls = { 0 };
    struct miru_capture capture = { 0 };
    struct miru_ipc_server ipc = { 0 };
    int active = 0;

    struct sigaction sa = { 0 };
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART, poll() should see EINTR
    sigaction(SIGINT, &sa, NULL);

    if (wayland_state_init(&state) != 0) {
        return 1;
    }

    if (ipc_server_init(&ipc) != 0) {
        fprintf(stderr, "failed to start up IPC server\n");
        wayland_state_cleanup(&state);
        return -1;
    }

    fprintf(stderr, "miru-daemon ready. waiting for toggle commands on %s\n", ipc.socket_path);
    state.running = 1;

    while (state.running && !should_exit) {
        short wayland_events = 0;
        if (wayland_state_prepare(&state, &wayland_events) != 0) {
            break;
        }

        struct pollfd pfds[2] = {
            { .fd = wayland_state_get_fd(&state), .events = wayland_events },
            { .fd = ipc_server_get_fd(&ipc), .events = POLLIN },
        };

        int ret = poll(pfds, 2, -1);
        if (ret == -1) {
            wayland_state_cancel_read(&state);
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "poll failed\n");
            break;
        }

        if (wayland_state_process(&state, pfds[0].revents) != 0) {
            break;
        }

        if (pfds[1].revents & POLLIN) {
            enum miru_ipc_command cmd = ipc_server_accept_command(&ipc);
            fprintf(stderr, "ipc: got command %d, active was %d\n", cmd, active);
            if (cmd == MIRU_IPC_TOGGLE) {
                if (!active) {
                    active = (activate(&state, &ls, &capture) == 0);
                } else {
                    deactivate(&ls, &capture);
                    active = 0;
                }
            } else if (cmd == MIRU_IPC_QUIT) {
                fprintf(stderr, "received quit command\n");
                should_exit = 1;
            }
            fprintf(stderr, "activate is now %d\n", active);
        }

        if (active && ls.closed) {
            // the compositor tore the surface down on it's own (output unplugged
            // etc), go back to inactive instead of looping through a dead surface
            fprintf(stderr, "layer surface closed unexpectedly, deactivating\n");
            deactivate(&ls, &capture);
            active = 0;
        }
    }

    fprintf(stderr, "shutting down\n");
    if (active) {
        layer_surface_destroy(&ls);
    }

    capture_frame_destroy(&capture);
    ipc_server_cleanup(&ipc);
    wayland_state_cleanup(&state);
    return 0;

    // if (capture_output_frame(&state, state.output, &should_exit, &capture) != 0) {
    //     fprintf(stderr, "screencopy capture failed\n");
    // } else {
    //     fprintf(
    //         stderr,
    //         "captured frame: %ux%u stride = %u format = %u y_invert = %d\n",
    //         capture.width,
    //         capture.height,
    //         capture.stride,
    //         capture.format,
    //         capture.y_invert
    //     );
    // }
    // // capture_frame_destroy(&capture);
    //
    // // Capture stays alive; handle_configure needs it to blit the frame in
    // if (layer_surface_create(&state, &ls, &capture) != 0) {
    //     fprintf(stderr, "failed to create layer surface\n");
    //     capture_frame_destroy(&capture);
    //     wayland_state_cleanup(&state);
    //     return 1;
    // }
    //
    // fprintf(stderr, "layer surface created, entering event loop\n");
    // state.running = 1;
    //
    // long long last_capture_ms = now_ms();
    //
    // while (state.running && !should_exit && !ls.closed) {
    //     // short timeout instead of blocking forever
    //     // so the loop wakes up regularly enough
    //     // to check whether it's time to recapture even when the compositor
    //     // sends nothing
    //     if (wayland_state_dispatch(&state, 50) != 0) {
    //         break;
    //     }
    //
    //     long long t = now_ms();
    //     if (ls.configured && (t - last_capture_ms) >= RECAPTURE_INTERVAL_MS) {
    //         fprintf(stderr, "recapture: starting\n");
    //         long long capture_start = now_ms();
    //
    //         struct miru_capture fresh_capture = { 0 };
    //         if (capture_output_frame(&state, state.output, &should_exit, &fresh_capture) == 0) {
    //             fprintf(stderr, "recapture: succeeded in %lld\n", now_ms() - capture_start);
    //             capture_frame_destroy(&capture); // free the previous frame's shm/wl_buffer first
    //             capture = fresh_capture; // ls.captuer already points at &capture, no update needed
    //             layer_surface_render(&ls);
    //         } else {
    //             fprintf(stderr, "recapture: FAILED after %lld\n", now_ms() - capture_start);
    //             capture_frame_destroy(&fresh_capture);
    //         }
    //         last_capture_ms = t;
    //     }
    // }
    //
    // fprintf(stderr, "shutting down\n");
    // capture_frame_destroy(&capture);
    // layer_surface_destroy(&ls);
    // wayland_state_cleanup(&state);
    // return 0;
}
