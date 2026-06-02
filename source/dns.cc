#include "async.h"
#include "error.h"
#include "types.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

using namespace v8;

namespace {

typedef struct {
	int err;
	char *hostname; // owned copy (worker thread can't hold a v8 string)
	char **entries;
	size_t num_entries;
} nx_dns_resolve_t;

// Worker thread: blocking getaddrinfo. No V8 API.
void nx_dns_resolve_do(nx_work_t *req) {
	nx_dns_resolve_t *data = (nx_dns_resolve_t *)req->data;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *result;
	int status = getaddrinfo(data->hostname, NULL, &hints, &result);
	if (status != 0) {
		data->err = status;
		return;
	}

	size_t count = 0;
	for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
		count++;
	}
	data->num_entries = count;
	if (!count) {
		freeaddrinfo(result);
		return;
	}

	data->entries = (char **)malloc(count * sizeof(char *));
	char ip[INET6_ADDRSTRLEN];
	int i = 0;
	for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
		void *addr;
		if (rp->ai_family == AF_INET) {
			addr = &((struct sockaddr_in *)rp->ai_addr)->sin_addr;
		} else {
			addr = &((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr;
		}
		inet_ntop(rp->ai_family, addr, ip, sizeof(ip));
		data->entries[i++] = strdup(ip);
	}
	freeaddrinfo(result);
}

// Loop thread: build the result array or throw.
MaybeLocal<Value> nx_dns_resolve_cb(Isolate *iso, nx_work_t *req) {
	Local<Context> context = iso->GetCurrentContext();
	nx_dns_resolve_t *data = (nx_dns_resolve_t *)req->data;
	free(data->hostname);
	data->hostname = nullptr;

	if (data->err) {
		if (data->entries) {
			for (size_t i = 0; i < data->num_entries; i++)
				free(data->entries[i]);
			free(data->entries);
			data->entries = nullptr;
		}
		iso->ThrowException(
		    Exception::Error(nx_str(iso, gai_strerror(data->err))));
		return MaybeLocal<Value>();
	}

	Local<Array> result = Array::New(iso, data->num_entries);
	for (size_t i = 0; i < data->num_entries; i++) {
		result->Set(context, (uint32_t)i, nx_str(iso, data->entries[i]))
		    .Check();
		free(data->entries[i]);
	}
	free(data->entries);
	data->entries = nullptr;
	return result;
}

void nx_dns_resolve(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T(nx_dns_resolve_t);
	String::Utf8Value hostname(iso, info[0]);
	data->hostname = strdup(*hostname ? *hostname : "");
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, nx_dns_resolve_do, nx_dns_resolve_cb));
}

} // namespace

void nx_init_dns(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "dnsResolve", nx_dns_resolve);
}
