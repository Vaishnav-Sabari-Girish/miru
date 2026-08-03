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
#include "input.h"
#include "version.h"
#include "logo.h"
#include "config.h"
#include "config_watch.h"

#define RECAPTURE_INTERVAL_MS 200 // 5 recaptures/sec

void print_help()
{
    miru_print_logo();

    printf("Usage: miru-daemon [OPTIONS]\n\n");

    printf("Options: \n");
    printf("    -h, --help      Show this help message\n");
    printf("    -v, --version   Show version information and exit\n\n");
    printf("Description: \n");
    printf("    Starts the miru-daemon. Connects to the Wayland Compositor, opens a\n");
    printf("    control socket at $XDG_RUNTIME_DIR/miru.sock, and idles until \n");
    printf("    a toggle command is received via miructl. No overlay is shown \n");
    printf("    until toggled\n\n");

    printf("Control: \n");
    printf("    Use \"miructl toggle\" to toggle the overlay and \"miructl quit\" to exit the running daemon\n");
}

void print_version()
{
    printf("miru-daemon %s\n", MIRU_VERSION);
    printf("\nCombined Distribution subject to MIT license\n");
    printf("\nWritten by Vaishnav Sabari Girish\n");
}

static volatile sig_atomic_t should_exit = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    should_exit = 1;
}

static int activate(
    struct miru_state *state,
    struct miru_layer_surface *ls,
    struct miru_capture *capture,
    struct miru_config *config
)
{
    if (capture_output_frame(state, state->output, &should_exit, capture) != 0) {
        fprintf(stderr, "toggle: capture failed, staying inactive\n");
        return -1;
    }

    struct layer_surface_config ls_config = {
        .zoom_default = (float)config->zoom_factor,
        .zoom_max = (float)config->zoom_max_factor,
        .spotlight_radius = (float)config->spotlight_radius,
        .spotlight_dim = (float)(config->spotlight_dim < 0.0 ? 0.0 :
                                 config->spotlight_dim > 1.0 ? 1.0 :
                                                               config->spotlight_dim),
        .spotlight_softness = (float)config->spotlight_softness,
    };

    if (layer_surface_create(state, ls, capture, &ls_config) != 0) {
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
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_help();
        return 0;
    }
    if (argc > 1 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        print_version();
        return 0;
    }

    struct miru_state state = { 0 };
    struct miru_layer_surface ls = { 0 };
    struct miru_capture capture = { 0 };
    struct miru_ipc_server ipc = { 0 };
    struct miru_config config;
    config_load(&config);
    struct miru_config_watch config_watch = { 0 };
    config_watch_init(&config_watch);
    volatile sig_atomic_t request_deactivate = 0;
    int active = 0;
    bool wayland_connection_lost = false;

    struct sigaction sa = { 0 };
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART, poll() should see EINTR
    sigaction(SIGINT, &sa, NULL);

    if (wayland_state_init(&state) != 0) {
        return 1;
    }

    struct miru_input_ctx input_ctx = {
        .ls = &ls,
        .request_deactivate = &request_deactivate,
        .zoom_increment = (float)config.zoom_increment,
    };

    input_setup(&state, &input_ctx);

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
            fprintf(stderr, "fatal: wayland_state_prepare failed. connection to compositor lost\n");
            wayland_connection_lost = true;
            break;
        }

        struct pollfd pfds[3] = {
            { .fd = wayland_state_get_fd(&state), .events = wayland_events },
            { .fd = ipc_server_get_fd(&ipc), .events = POLLIN },
            { .fd = config_watch_get_fd(&config_watch), .events = POLLIN },
        };

        int timeout = input_next_repeat_timeout(&input_ctx);

        int ret = poll(pfds, 3, timeout);
        if (ret == -1) {
            wayland_state_cancel_read(&state);
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "poll failed\n");
            wayland_connection_lost = true;
            break;
        }

        if (wayland_state_process(&state, pfds[0].revents) != 0) {
            fprintf(stderr, "fatal: wayland_state_process failed. connection to compositor lost\n");
            wayland_connection_lost = true;
            break;
        }

        if (active) {
            input_process_repeats(&input_ctx);
            if (ls.dirty) {
                layer_surface_render(&ls);
                ls.dirty = 0;
            }
        }

        if (pfds[1].revents & POLLIN) {
            enum miru_ipc_command cmd = ipc_server_accept_command(&ipc);
            fprintf(stderr, "ipc: got command %d, active was %d\n", cmd, active);
            if (cmd == MIRU_IPC_TOGGLE) {
                if (!active) {
                    active = (activate(&state, &ls, &capture, &config) == 0);
                } else {
                    deactivate(&ls, &capture);
                    input_reset_repeat(&input_ctx);
                    active = 0;
                }
            } else if (cmd == MIRU_IPC_QUIT) {
                fprintf(stderr, "received quit command\n");
                should_exit = 1;
            }
            fprintf(stderr, "activate is now %d\n", active);
        }

        if (pfds[2].revents & POLLIN) {
            int changed = config_watch_check(&config_watch);
            if (changed > 0) {
                fprintf(stderr, "config: change detected, reloading\n");
                struct miru_config new_config;
                config_load(&new_config);
                config = new_config;

                input_ctx.zoom_increment = (float)config.zoom_increment;
                if (active) {
                    struct layer_surface_config ls_config = {
                        .zoom_default = (float)config.zoom_factor,
                        .zoom_max = (float)config.zoom_max_factor,
                        .spotlight_radius = (float)config.spotlight_radius,
                        .spotlight_dim = (float)config.spotlight_dim,
                        .spotlight_softness = (float)config.spotlight_softness,
                    };

                    layer_surface_apply_config(&ls, &ls_config);
                }
            } else if (changed < 0) {
                fprintf(stderr, "config_watch: error reading events, disabling hot-reloading\n");
                config_watch_cleanup(&config_watch);
            }
        }

        if (active && ls.closed) {
            // the compositor tore the surface down on it's own (output unplugged
            // etc), go back to inactive instead of looping through a dead surface
            fprintf(stderr, "layer surface closed unexpectedly, deactivating\n");
            deactivate(&ls, &capture);
            input_reset_repeat(&input_ctx);
            active = 0;
        }

        if (active && request_deactivate) {
            deactivate(&ls, &capture);
            input_reset_repeat(&input_ctx);
            active = 0;
            request_deactivate = 0;
        }
    }

    fprintf(stderr, "shutting down\n");
    if (active) {
        layer_surface_destroy(&ls);
    }

    capture_frame_destroy(&capture);
    ipc_server_cleanup(&ipc);
    config_watch_cleanup(&config_watch);
    wayland_state_cleanup(&state);
    if (wayland_connection_lost) {
        fprintf(stderr, "exiting with failure due to lost wayland connection\n");
        return 1;
    }
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
