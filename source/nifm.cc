#include "error.h"
#include "types.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

using namespace v8;

namespace {

void nx_nifm_exit(const FunctionCallbackInfo<Value> &info) { nifmExit(); }

void nx_nifm_initialize(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Result rc = nifmInitialize(NifmServiceType_User);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "nifmInitialize");
		return;
	}
	// Return the exit function (called on `unload`), matching the old API.
	Local<Context> context = iso->GetCurrentContext();
	info.GetReturnValue().Set(
	    FunctionTemplate::New(iso, nx_nifm_exit)
	        ->GetFunction(context)
	        .ToLocalChecked());
}

void nx_network_info(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	char str[INET_ADDRSTRLEN];
	struct in_addr ip, subnet_mask, gateway, primary_dns, secondary_dns;
	Result rc = nifmGetCurrentIpConfigInfo(
	    &ip.s_addr, &subnet_mask.s_addr, &gateway.s_addr, &primary_dns.s_addr,
	    &secondary_dns.s_addr);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "nifmGetCurrentIpConfigInfo");
		return;
	}
	Local<Object> result = Object::New(iso);
	auto set_ip = [&](const char *key, struct in_addr *addr) {
		inet_ntop(AF_INET, &addr->s_addr, str, INET_ADDRSTRLEN);
		result->Set(context, nx_str(iso, key), nx_str(iso, str)).Check();
	};
	set_ip("ip", &ip);
	set_ip("subnetMask", &subnet_mask);
	set_ip("gateway", &gateway);
	set_ip("primaryDnsServer", &primary_dns);
	set_ip("secondaryDnsServer", &secondary_dns);
	info.GetReturnValue().Set(result);
}

} // namespace

void nx_init_nifm(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "nifmInitialize", nx_nifm_initialize);
	NX_SET_FUNC(init_obj, "networkInfo", nx_network_info);
}
