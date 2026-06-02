#include "error.h"
#include "types.h"
#include "wrap.h"
#include <stdio.h>
#include <string.h>

using namespace v8;

namespace {

typedef struct {
	AccountUid uid;
	AccountProfile profile;
	AccountUserData userdata;
	AccountProfileBase profilebase;
	bool profile_loaded;
	bool userdata_loaded;
} nx_profile_t;

void free_profile(nx_profile_t *profile) {
	if (profile->profile_loaded) {
		accountProfileClose(&profile->profile);
	}
	free(profile);
}

Local<Object> profile_new(Isolate *iso, AccountUid uid) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> obj = nx::NewWrapped(iso);
	nx_profile_t *profile =
	    static_cast<nx_profile_t *>(calloc(1, sizeof(nx_profile_t)));
	profile->uid = uid;
	nx::Wrap<nx_profile_t>(iso, obj, profile, free_profile);
	Local<Array> uid_arr = Array::New(iso, 2);
	for (int i = 0; i < 2; i++) {
		uid_arr->Set(context, i, BigInt::NewFromUnsigned(iso, uid.uid[i]))
		    .Check();
	}
	obj->Set(context, nx_str(iso, "uid"), uid_arr).Check();
	return obj;
}

// Load profile + userdata on demand. Returns false (throwing) on failure.
bool ensure_loaded(Isolate *iso, nx_profile_t *profile) {
	Result rc;
	if (!profile->profile_loaded) {
		rc = accountGetProfile(&profile->profile, profile->uid);
		if (R_FAILED(rc)) {
			nx_throw_libnx_error(iso, rc, "accountGetProfile");
			return false;
		}
		profile->profile_loaded = true;
	}
	if (!profile->userdata_loaded) {
		rc = accountProfileGet(&profile->profile, &profile->userdata,
		                       &profile->profilebase);
		if (R_FAILED(rc)) {
			nx_throw_libnx_error(iso, rc, "accountProfileGet");
			return false;
		}
		profile->userdata_loaded = true;
	}
	return true;
}

void nx_account_exit(const FunctionCallbackInfo<Value> &info) { accountExit(); }

void nx_account_initialize(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Result rc = accountInitialize(AccountServiceType_System);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "accountInitialize");
		return;
	}
	Local<Context> context = iso->GetCurrentContext();
	info.GetReturnValue().Set(FunctionTemplate::New(iso, nx_account_exit)
	                              ->GetFunction(context)
	                              .ToLocalChecked());
}

void nx_account_current_profile(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	AccountUid uid;
	Result rc = accountGetPreselectedUser(&uid);
	if (R_FAILED(rc)) {
		fprintf(stderr, "accountGetPreselectedUser() returned 0x%x\n",
		        (unsigned)rc);
		info.GetReturnValue().SetNull();
		return;
	}
	info.GetReturnValue().Set(profile_new(iso, uid));
}

void nx_account_select_profile(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	AccountUid uid;
	PselUserSelectionSettings settings;
	memset(&settings, 0, sizeof(settings));
	Result rc = pselShowUserSelector(&uid, &settings);
	if (R_FAILED(rc)) {
		if (rc == 0x27C /* ResultCancelledByUser */) {
			info.GetReturnValue().SetNull();
			return;
		}
		nx_throw_libnx_error(iso, rc, "pselShowUserSelector");
		return;
	}
	info.GetReturnValue().Set(profile_new(iso, uid));
}

void nx_account_profile_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> arr = info[0].As<Object>();
	AccountUid uid;
	Local<Value> v0 = arr->Get(context, 0).ToLocalChecked();
	Local<Value> v1 = arr->Get(context, 1).ToLocalChecked();
	if (!v0->IsBigInt() || !v1->IsBigInt()) {
		nx_throw(iso, "invalid uid");
		return;
	}
	uid.uid[0] = v0.As<BigInt>()->Uint64Value();
	uid.uid[1] = v1.As<BigInt>()->Uint64Value();
	info.GetReturnValue().Set(profile_new(iso, uid));
}

void nx_account_profiles(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Result rc;
	s32 user_count;
	rc = accountGetUserCount(&user_count);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "accountGetUserCount");
		return;
	}
	AccountUid *uids = (AccountUid *)malloc(sizeof(AccountUid) * user_count);
	rc = accountListAllUsers(uids, user_count, &user_count);
	if (R_FAILED(rc)) {
		free(uids);
		nx_throw_libnx_error(iso, rc, "accountListAllUsers");
		return;
	}
	Local<Array> arr = Array::New(iso, user_count);
	for (int i = 0; i < user_count; i++) {
		arr->Set(context, i, profile_new(iso, uids[i])).Check();
	}
	free(uids);
	info.GetReturnValue().Set(arr);
}

void nx_account_nickname(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_profile_t *profile = nx::Unwrap<nx_profile_t>(info.This());
	if (!profile || !ensure_loaded(iso, profile))
		return;
	size_t len = strnlen(profile->profilebase.nickname,
	                     sizeof(profile->profilebase.nickname));
	info.GetReturnValue().Set(
	    String::NewFromUtf8(iso, profile->profilebase.nickname,
	                        NewStringType::kNormal, (int)len)
	        .ToLocalChecked());
}

void nx_account_image(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_profile_t *profile = nx::Unwrap<nx_profile_t>(info.This());
	if (!profile)
		return;
	Result rc;
	if (!profile->profile_loaded) {
		rc = accountGetProfile(&profile->profile, profile->uid);
		if (R_FAILED(rc)) {
			nx_throw_libnx_error(iso, rc, "accountGetProfile");
			return;
		}
		profile->profile_loaded = true;
	}
	u32 image_size;
	rc = accountProfileGetImageSize(&profile->profile, &image_size);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "accountProfileGetImageSize");
		return;
	}
	void *buf = malloc(image_size);
	rc = accountProfileLoadImage(&profile->profile, buf, image_size,
	                             &image_size);
	if (R_FAILED(rc)) {
		free(buf);
		nx_throw_libnx_error(iso, rc, "accountProfileLoadImage");
		return;
	}
	// Hand ownership of `buf` to the ArrayBuffer (freed on GC).
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, image_size,
	    [](void *data, size_t, void *) { free(data); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

void nx_account_profile_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(context, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_GET(proto, "nickname", nx_account_nickname);
	NX_DEF_GET(proto, "image", nx_account_image);
}

} // namespace

void nx_init_account(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "accountInitialize", nx_account_initialize);
	NX_SET_FUNC(init_obj, "accountProfileInit", nx_account_profile_init);
	NX_SET_FUNC(init_obj, "accountCurrentProfile", nx_account_current_profile);
	NX_SET_FUNC(init_obj, "accountSelectProfile", nx_account_select_profile);
	NX_SET_FUNC(init_obj, "accountProfileNew", nx_account_profile_new);
	NX_SET_FUNC(init_obj, "accountProfiles", nx_account_profiles);
}
