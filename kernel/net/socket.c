#include "net/socket.h"
#include "console.h"
#include "string.h"
#include "heap.h"
#include "process.h"
#include "spinlock.h"

 
#define MAX_SOCKETS 128
static struct socket *socket_table[MAX_SOCKETS];
static spinlock_t socket_lock;
 
static int inet_bind(struct socket *sock, const struct sockaddr *uaddr, int addr_len) {
    (void)sock; (void)uaddr; (void)addr_len;
    return 0;
}
static int inet_connect(struct socket *sock, const struct sockaddr *uaddr, int addr_len, int flags) {
    (void)sock; (void)uaddr; (void)addr_len; (void)flags;
    return 0;
}
static int inet_release(struct socket *sock) {
    (void)sock;
    return 0;
}

static struct proto_ops inet_stream_ops = {
    .bind = inet_bind,
    .connect = inet_connect,
    .release = inet_release,
};

static struct proto_ops inet_dgram_ops = {
    .bind = inet_bind,
    .connect = inet_connect,
    .release = inet_release,
};

struct socket *sock_alloc(void) {
    struct socket *sock = kmalloc(sizeof(struct socket));
    if (!sock) return 0;
    memset(sock, 0, sizeof(struct socket));
    sock->state = SS_UNCONNECTED;
    return sock;
}

struct socket *sock_get(int fd) {
    if (fd < 0 || fd >= MAX_SOCKETS) return 0;
    
    spinlock_acquire(&socket_lock);
    struct socket *sock = socket_table[fd];
    spinlock_release(&socket_lock);
    return sock;
}

static int install_fd(struct socket *sock) {
    spinlock_acquire(&socket_lock);
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i] == 0) {
            socket_table[i] = sock;
            spinlock_release(&socket_lock);
            return i;
        }
    }
    spinlock_release(&socket_lock);
    return -1;
}

void sock_release(struct socket *sock) {
    if (sock->ops && sock->ops->release) {
        sock->ops->release(sock);
    }
    kfree(sock);
}

int sys_socket(int domain, int type, int protocol) {
    (void)protocol;
    if (domain != AF_INET) return -1;  
    
    struct socket *sock = sock_alloc();
    if (!sock) return -1;  
    
    sock->type = type;
    
    if (type == SOCK_STREAM) {
        sock->ops = &inet_stream_ops;
    } else if (type == SOCK_DGRAM) {
        sock->ops = &inet_dgram_ops;
    } else {
        sock_release(sock);
        return -1;  
    }
    
    int fd = install_fd(sock);
    if (fd < 0) {
        sock_release(sock);
        return -1;  
    }
    
    return fd;
}

int sys_bind(int fd, const struct sockaddr *addr, int addrlen) {
    struct socket *sock = sock_get(fd);
    if (!sock || !sock->ops || !sock->ops->bind) return -1;
    return sock->ops->bind(sock, addr, addrlen);
}

int sys_connect(int fd, const struct sockaddr *addr, int addrlen) {
    struct socket *sock = sock_get(fd);
    if (!sock || !sock->ops || !sock->ops->connect) return -1;
    return sock->ops->connect(sock, addr, addrlen, 0);
}

void sock_init() {
    spinlock_init(&socket_lock);
    memset(socket_table, 0, sizeof(socket_table));
    kprint_str("Socket: Initialized\n");
}
