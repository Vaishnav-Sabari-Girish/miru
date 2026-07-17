#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <toggle|quit>\n", argv[0]);
        return 1;
    }

    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        runtime_dir = "/tmp";
    }

    char socket_path[256];
    int n = snprintf(socket_path, sizeof(socket_path), "%s/miru.sock", runtime_dir);
    if (n < 0 || (size_t)n >= sizeof(socket_path)) {
        fprintf(stderr, "socket path is too long\n");
        return 1;
    }

    struct sockaddr_un addr_size_check;
    if ((size_t)n >= sizeof(addr_size_check.sun_path)) {
        fprintf(stderr, "ipc_server: socket path exceeds sun_path limit (%zu bytes)\n", sizeof(addr_size_check.sun_path));
        return 1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        fprintf(stderr, "is miru-daemon running?\n");
        close(fd);
        return 1;
    }
    
    char msg[64];
    snprintf(msg, sizeof(msg), "%s\n", argv[1]);
    if (write(fd, msg, strlen(msg)) < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
