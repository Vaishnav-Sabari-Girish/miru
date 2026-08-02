#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
// #include <sys/time.h>
#include <poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc_server.h"

static int build_socket_path(char *out, size_t out_size)
{
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        runtime_dir = "/tmp"; // just in case
    }

    int n = snprintf(out, out_size, "%s/miru.sock", runtime_dir);
    if (n < 0 || (size_t)n >= out_size) {
        fprintf(stderr, "ipc_server : socket path too long\n");
        return -1;
    }

    struct sockaddr_un addr_size_check;
    if ((size_t)n >= sizeof(addr_size_check.sun_path)) {
        fprintf(
            stderr, "ipc_server: socket path exceeds sun_path limit (%zu bytes)\n", sizeof(addr_size_check.sun_path)
        );
        return -1;
    }

    return 0;
}

static int build_lock_path(char *out, size_t out_size)
{
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    int n;

    if (runtime_dir && *runtime_dir) {
        n = snprintf(out, out_size, "%s/miru.lock", runtime_dir);
    } else {
        n = snprintf(out, out_size, "/tmp/miru-%d.lock", (int)getuid());
    }

    if (n < 0 || (size_t)n >= out_size) {
        fprintf(stderr, "ipc_server: lock path too long\n");
        return -1;
    }
    return 0;
}

static int acquire_instance_lock(struct miru_ipc_server *srv)
{
    char lock_path[256];
    if (build_lock_path(lock_path, sizeof(lock_path))) {
        return -1;
    }

    int fd = open(lock_path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        fprintf(stderr, "ipc_server: failed to open lockfile %s: %s\n", lock_path, strerror(errno));
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_uid != getuid()) {
        fprintf(stderr, "ipc_server: refusing to use lockfile %s not owned by us\n", lock_path);
        close(fd);
        return -1;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK) {
            fprintf(
                stderr, "ipc_server: another miru-daemon instance is already running (lock held on %s)\n", lock_path
            );
        } else {
            fprintf(stderr, "ipc_server: flock failed on %s: %s\n", lock_path, strerror(errno));
        }
        close(fd);
        return -1;
    }

    srv->lock_fd = fd;
    return 0;
}

int ipc_server_init(struct miru_ipc_server *srv)
{
    srv->lock_fd = -1;

    if (acquire_instance_lock(srv) != 0) {
        return -1;
    }

    if (build_socket_path(srv->socket_path, sizeof(srv->socket_path)) != 0) {
        close(srv->lock_fd);
        srv->lock_fd = -1;
        return -1;
    }

    int probe_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (probe_fd >= 0) {
        struct sockaddr_un probe_addr = { 0 };
        probe_addr.sun_family = AF_UNIX;
        strncpy(probe_addr.sun_path, srv->socket_path, sizeof(probe_addr.sun_path) - 1);
        if (connect(probe_fd, (struct sockaddr *)&probe_addr, sizeof(probe_addr)) == 0) {
            close(probe_fd);
            fprintf(
                stderr, "ipc_server: another miru-daemon is already running (socket %s is live)\n", srv->socket_path
            );
            return -1;
        }
        close(probe_fd);
    }

    unlink(srv->socket_path); // remove any previous stale sockets

    srv->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv->listen_fd < 0) {
        perror("ipc_error: socket");
        return -1;
    }

    struct sockaddr_un addr = { 0 };
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, srv->socket_path, sizeof(addr.sun_path) - 1);

    if (bind(srv->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("ipc_server: bind");
        close(srv->listen_fd);
        srv->listen_fd = -1;
        return -1;
    }

    if (listen(srv->listen_fd, 4) < 0) {
        perror("ipc_server: listen");
        close(srv->listen_fd);
        unlink(srv->socket_path);
        srv->listen_fd = -1;
        return -1;
    }

    fprintf(stderr, "ipc_server: listening on %s\n", srv->socket_path);
    return 0;
}

int ipc_server_get_fd(const struct miru_ipc_server *srv)
{
    return srv->listen_fd;
}

enum miru_ipc_command ipc_server_accept_command(struct miru_ipc_server *srv)
{
    int client_fd = accept(srv->listen_fd, NULL, NULL);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("ipc_server: accept");
        }
        return MIRU_IPC_NONE;
    }

    struct timeval tv = {
        .tv_sec = 2,
        .tv_usec = 0,
    };

    // if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    //     perror("ipc_server: setsockopt SO_RCVTIMEO");
    // }

    struct pollfd client_pfd = {
        .fd = client_fd,
        .events = POLLIN,
    };
    int ready = poll(&client_pfd, 1, 200);
    if (ready <= 0) {
        if (ready < 0) {
            perror("ipc_server: poll on accepted client");
        }

        fprintf(stderr, "ipc_server: client sent no data in time, dropping\n");
        close(client_fd);
        return MIRU_IPC_NONE;
    }

    char buf[64] = { 0 };
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);

    close(client_fd);

    if (n <= 0) {
        return MIRU_IPC_NONE;
    }

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    if (strcmp(buf, "toggle") == 0) {
        return MIRU_IPC_TOGGLE;
    }

    if (strcmp(buf, "quit") == 0) {
        return MIRU_IPC_QUIT;
    }

    fprintf(stderr, "ipc_server: unknown command %s\n", buf);
    return MIRU_IPC_UNKNOWN;
}

void ipc_server_cleanup(struct miru_ipc_server *srv)
{
    if (srv->listen_fd > 0) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }
    unlink(srv->socket_path);

    if (srv->lock_fd >= 0) {
        close(srv->lock_fd);
        srv->lock_fd = -1;
    }
}
