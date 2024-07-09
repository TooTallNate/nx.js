#include "nifm.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static JSValue nx_nifm_exit(JSContext *ctx, JSValueConst this_val, int argc,
							JSValueConst *argv) {
	nifmExit();
	return JS_UNDEFINED;
}

static JSValue nx_nifm_initialize(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	Result rc = nifmInitialize(NifmServiceType_User);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "nifmInitialize() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewCFunction(ctx, nx_nifm_exit, "", 0);
}

static JSValue nx_network_info(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	JSValue info = JS_NewObject(ctx);
	char str[INET_ADDRSTRLEN];
	struct in_addr ip;
	struct in_addr subnet_mask;
	struct in_addr gateway;
	struct in_addr primary_dns_server;
	struct in_addr secondary_dns_server;
	Result rc = nifmGetCurrentIpConfigInfo(
		&ip.s_addr, &subnet_mask.s_addr, &gateway.s_addr,
		&primary_dns_server.s_addr, &secondary_dns_server.s_addr);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "nifmGetCurrentIpConfigInfo() returned 0x%x",
							  rc);
		return JS_EXCEPTION;
	}

	inet_ntop(AF_INET, &(ip.s_addr), str, INET_ADDRSTRLEN);
	JS_SetPropertyStr(ctx, info, "ip", JS_NewString(ctx, str));

	inet_ntop(AF_INET, &(subnet_mask.s_addr), str, INET_ADDRSTRLEN);
	JS_SetPropertyStr(ctx, info, "subnetMask", JS_NewString(ctx, str));

	inet_ntop(AF_INET, &(gateway.s_addr), str, INET_ADDRSTRLEN);
	JS_SetPropertyStr(ctx, info, "gateway", JS_NewString(ctx, str));

	inet_ntop(AF_INET, &(primary_dns_server.s_addr), str, INET_ADDRSTRLEN);
	JS_SetPropertyStr(ctx, info, "primaryDnsServer", JS_NewString(ctx, str));

	inet_ntop(AF_INET, &(secondary_dns_server.s_addr), str, INET_ADDRSTRLEN);
	JS_SetPropertyStr(ctx, info, "secondaryDnsServer", JS_NewString(ctx, str));

	return info;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("nifmInitialize", 1, nx_nifm_initialize),
	JS_CFUNC_DEF("networkInfo", 1, nx_network_info),
};

void nx_init_nifm(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
