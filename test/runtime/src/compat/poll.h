#pragma once
// Compat poll.h — source/poll.h includes <poll.h> which would shadow itself
// since source/ is in the include path (-I). This compat header is found first
// (compat/ is earlier in the include path) and forwards to <sys/poll.h> which
// provides struct pollfd, nfds_t, POLLIN, etc. without the name collision.
#include <sys/poll.h>
