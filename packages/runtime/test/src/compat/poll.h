#pragma once
/**
 * Redirect <poll.h> to <sys/poll.h> to avoid collision with source/poll.h.
 *
 * When source/types.h includes "poll.h", it picks up source/poll.h (the nx.js
 * custom poll implementation). But when source/poll.c includes <poll.h>, the
 * compat directory is searched first and would find this file instead of the
 * system header. We redirect to <sys/poll.h> to get the real system poll.
 */
#include <sys/poll.h>
