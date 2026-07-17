#ifndef IPC_SERVER_H
#define IPC_SERVER_H

enum miru_ipc_command {
    MIRU_IPC_NONE, // nothing to report
    MIRU_IPC_TOGGLE,
    MIRU_IPC_QUIT,
    MIRU_IPC_UNKNOWN, // client connected, but sent something unrecognizable
};

struct miru_ipc_server {
    int listen_fd;
    char socket_path[256];
};

int ipc_server_init(struct miru_ipc_server *srv);

int ipc_server_get_fd(const struct miru_ipc_server *srv);

enum miru_ipc_command ipc_server_accept_command(struct miru_ipc_server *srv);

void ipc_server_cleanup(struct miru_ipc_server *srv);

#endif // !IPC_SERVER_H
