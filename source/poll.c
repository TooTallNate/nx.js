#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "poll.h"

int set_nonblocking(int fd)
{
    /* Retrieve current flags */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;

    /* Set non-blocking flag */
    flags |= O_NONBLOCK;

    /* Update flags */
    if (fcntl(fd, F_SETFL, flags) == -1)
        return -1;

    return 0;
}

int nx_add_watcher(nx_poll_t *p, nx_watcher_t *req)
{
    // Insert watcher into linked list
    SLIST_INSERT_HEAD(&p->watchers_head, req, next);

    if (p->poll_fds == NULL)
    {
        p->poll_fds_size = 20;
        size_t s = p->poll_fds_size * sizeof(struct pollfd);
        p->poll_fds = malloc(s);
        if (p->poll_fds == NULL)
        {
            // out of memory
            return -1;
        }
        memset(p->poll_fds, 0, s);
    }
    else if (p->poll_fds_used == p->poll_fds_size)
    {
        // Double the size of the array
        p->poll_fds_size *= 2;
        size_t s = p->poll_fds_size * sizeof(struct pollfd);
        struct pollfd *new_poll_fds = realloc(p->poll_fds, s);
        if (new_poll_fds == NULL)
        {
            // out of memory
            return -1;
        }
        // Assign the new array to poll_fds
        p->poll_fds = new_poll_fds;
    }

    int index = 0;
    for (; index < p->poll_fds_used; index++)
    {
        if (p->poll_fds[index].fd == req->fd)
            break;
    }

    p->poll_fds[index].fd = req->fd;
    p->poll_fds[index].events |= req->events;
    p->poll_fds[index].revents = 0;

    if (index == p->poll_fds_used)
    {
        p->poll_fds_used = index + 1;
    }

    return 0;
}

int nx_remove_watcher(nx_poll_t *p, nx_watcher_t *req)
{
    // Modify the `poll_fds` array to remove the `req->fd` value,
    // but only if no other watchers are watching with the same fd
    int fd_count = 0;
    nx_watcher_t *watcher;
    SLIST_FOREACH(watcher, &p->watchers_head, next)
    {
        if (watcher->fd == req->fd)
        {
            fd_count++;
            if (fd_count > 1)
                break;
        }
    }

    if (fd_count == 1)
    {
        for (int i = 0; i < p->poll_fds_used; i++)
        {
            if (p->poll_fds[i].fd == req->fd)
            {
                // Shift all elements down one
                for (int j = i; j < p->poll_fds_used - 1; j++)
                {
                    p->poll_fds[j] = p->poll_fds[j + 1];
                }
                p->poll_fds_used--;
                break;
            }
        }
    }

    // Remove the watcher from the linked list of watchers
    SLIST_REMOVE(&p->watchers_head, req, nx_watcher_s, next);

    return 0;
}

void nx_poll(nx_poll_t *p)
{
    if (p->poll_fds_used == 0 || SLIST_EMPTY(&p->watchers_head))
        return;

    nx_watcher_t *watcher;

    int ready_fds = poll(p->poll_fds, p->poll_fds_used, 0);
    if (ready_fds < 0)
    {
        printf("poll() error: %s\n", strerror(errno));
    }
    else if (ready_fds > 0)
    {
        /* One or more file descriptors are ready, handle them */
        for (int i = 0; i < p->poll_fds_used; i++)
        {
            SLIST_FOREACH(watcher, &p->watchers_head, next)
            {
                if (p->poll_fds[i].fd == watcher->fd && p->poll_fds[i].revents & watcher->events)
                {
                    watcher->watcher_callback(p, watcher, p->poll_fds[i].revents);
                }
            }
        }
    }
}

void nx_tcp_connect_cb(nx_poll_t *p, nx_watcher_t *watcher, int revents)
{
    nx_remove_watcher(p, watcher);
    nx_connect_t *req = (nx_connect_t *)watcher;

    int so_error;
    socklen_t len = sizeof(so_error);

    /* Get error status for the socket. */
    getsockopt(req->fd, SOL_SOCKET, SO_ERROR, &so_error, &len);

    if (so_error == 0)
    {
        req->err = 0;
    }
    else
    {
        /* Error during connect. */
        req->err = errno;
    }
    req->callback(p, req);
}

int nx_tcp_connect(nx_poll_t *p, nx_connect_t *req, const char *ip, int port, nx_connect_cb callback)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("socket() err: %s\n", strerror(errno));
        return sockfd;
    }

    if (set_nonblocking(sockfd) == -1)
    {
        printf("fcntl() err: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        close(sockfd);
        return -1;
    }

    int r = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (r < 0 && errno != EINPROGRESS)
    {
        req->err = errno;
        printf("connect() err: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    if (r == 0)
    {
        printf("connect() did not return EINPROGRESS. Shouldn't happen\n");
        req->err = -EINVAL;
        return -1;
    }
    errno = 0;

    req->fd = sockfd;
    req->events = POLLOUT | POLLERR;
    req->watcher_callback = nx_tcp_connect_cb;
    req->callback = callback;
    nx_add_watcher(p, (nx_watcher_t *)req);

    return 0;
}

void nx_read_ready(nx_poll_t *p, nx_watcher_t *watcher, int revents)
{
    nx_remove_watcher(p, watcher);
    nx_read_t *req = (nx_read_t *)watcher;
    ssize_t n = read(req->fd, (void *)req->buffer, req->buffer_size);
    if (n < 0)
    {
        req->err = errno;
    }
    else
    {
        req->bytes_read = n;
    }
    req->callback(p, req);
}

int nx_read(nx_poll_t *p, nx_read_t *req, int fd, const uint8_t *buffer, size_t buffer_size, nx_read_cb callback)
{
    req->fd = fd;
    req->err = 0;
    req->events = POLLIN;
    req->watcher_callback = nx_read_ready;
    req->bytes_read = 0;
    req->buffer = buffer;
    req->buffer_size = buffer_size;
    req->callback = callback;
    nx_add_watcher(p, (nx_watcher_t *)req);
    return 0;
}

void nx_write_ready(nx_poll_t *p, nx_watcher_t *watcher, int revents)
{
    nx_remove_watcher(p, watcher);
    nx_write_t *req = (nx_write_t *)watcher;
    ssize_t n = write(req->fd, req->buffer + req->bytes_written, req->buffer_size - req->bytes_written);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // The socket's output buffer is full, need to wait before trying again
            req->events = POLLOUT;
            nx_add_watcher(p, watcher);
        }
        else
        {
            // An error occurred
            req->err = errno;
            req->callback(p, req);
        }
    }
    else
    {
        req->bytes_written += n;
        if (req->bytes_written < req->buffer_size)
        {
            // Not all data was written, need to wait before trying again
            req->events = POLLOUT;
            nx_add_watcher(p, watcher);
        }
        else
        {
            // All data was written
            req->callback(p, req);
        }
    }
}

int nx_write(nx_poll_t *p, nx_write_t *req, int fd, const uint8_t *data, size_t num_bytes, nx_write_cb callback)
{
    req->err = 0;
    req->fd = fd;
    req->buffer_size = num_bytes;
    req->bytes_written = 0;
    req->callback = callback;

    ssize_t n = write(fd, data, num_bytes);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // The socket's output buffer is full, need to wait before trying again
            req->events = POLLOUT;
            nx_add_watcher(p, (nx_watcher_t *)req);
            return 0;
        }
        else
        {
            // An error occurred
            req->err = errno;
            callback(p, req);
            return -1;
        }
    }
    else
    {
        req->bytes_written += n;
        callback(p, req);
    }
    return 0;
}

void nx_poll_init(nx_poll_t *p)
{
    p->poll_fds = NULL;
    p->poll_fds_size = 0;
    p->poll_fds_used = 0;
    SLIST_INIT(&p->watchers_head);
}

void nx_tcp_server_cb(nx_poll_t *p, nx_watcher_t *watcher, int revents)
{
    nx_server_t *req = (nx_server_t *)watcher;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(req->fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd < 0)
    {
        printf("accept() err: %s\n", strerror(errno));
        req->err = errno;
    }
    else
    {
        req->err = 0;

        if (set_nonblocking(client_fd) == -1)
        {
            close(client_fd);
            printf("fcntl() err: %s\n", strerror(errno));
            req->err = errno;
        }
    }
    req->callback(p, req, client_fd);
}

int nx_tcp_server(nx_poll_t *p, nx_server_t *req, const char *ip, int port, nx_server_cb callback)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("socket() err: %s\n", strerror(errno));
        return sockfd;
    }

    if (set_nonblocking(sockfd) == -1)
    {
        close(sockfd);
        printf("fcntl() err: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        close(sockfd);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        close(sockfd);
        printf("bind() err: %s\n", strerror(errno));
        return -1;
    }

    if (listen(sockfd, 5) < 0)
    {
        close(sockfd);
        printf("listen() err: %s\n", strerror(errno));
        return -1;
    }

    req->fd = sockfd;
    req->events = POLLIN;
    req->watcher_callback = nx_tcp_server_cb;
    req->callback = callback;
    nx_add_watcher(p, (nx_watcher_t *)req);

    return 0;
}
