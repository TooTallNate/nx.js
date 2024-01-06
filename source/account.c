#include "account.h"

static JSClassID nx_profile_class_id;

typedef struct
{
	AccountUid uid;
	AccountProfile profile;
	AccountUserData userdata;
	AccountProfileBase profilebase;
	bool profile_loaded;
	bool userdata_loaded;
} nx_profile_t;

static void finalizer_profile(JSRuntime *rt, JSValue val)
{
	nx_profile_t *profile = JS_GetOpaque(val, nx_profile_class_id);
	if (profile)
	{
		if (profile->profile_loaded)
		{
			accountProfileClose(&profile->profile);
		}
		js_free_rt(rt, profile);
	}
}

JSValue profile_new(JSContext *ctx, AccountUid uid)
{
	JSValue obj = JS_NewObjectClass(ctx, nx_profile_class_id);
	if (JS_IsException(obj))
	{
		return obj;
	}
	nx_profile_t *profile = js_mallocz(ctx, sizeof(nx_profile_t));
	if (!profile)
	{
		return JS_EXCEPTION;
	}
	profile->uid = uid;
	JS_SetOpaque(obj, profile);
	JSValue uid_arr = JS_NewArray(ctx);
	for (int i = 0; i < 2; i++)
	{
		JS_SetPropertyUint32(ctx, uid_arr, i, JS_NewBigUint64(ctx, uid.uid[i]));
	}
	JS_SetPropertyStr(ctx, obj, "uid", uid_arr);
	return obj;
}

nx_profile_t *nx_get_profile(JSContext *ctx, JSValueConst obj)
{
	return JS_GetOpaque2(ctx, obj, nx_profile_class_id);
}

static JSValue nx_account_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	accountExit();
	return JS_UNDEFINED;
}

static JSValue nx_account_initialize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Result rc = accountInitialize(AccountServiceType_System);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "accountInitialize() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewCFunction(ctx, nx_account_exit, "", 0);
}

static JSValue nx_account_current_profile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	AccountUid uid;
	Result rc = accountGetPreselectedUser(&uid);
	if (R_FAILED(rc))
	{
		fprintf(stderr, "accountGetPreselectedUser() returned 0x%x\n", rc);
		return JS_NULL;
	}
	return profile_new(ctx, uid);
}

static JSValue nx_account_select_profile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	AccountUid uid;
	PselUserSelectionSettings settings;
	memset(&settings, 0, sizeof(settings));

	Result rc = pselShowUserSelector(&uid, &settings);
	if (R_FAILED(rc))
	{
		if (rc == 0x27C /* ResultCancelledByUser*/)
		{
			return JS_NULL;
		}
		JS_ThrowInternalError(ctx, "pselShowUserSelector() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return profile_new(ctx, uid);
}

static JSValue nx_account_profiles(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Result rc;
	s32 user_count;
	rc = accountGetUserCount(&user_count);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "accountGetUserCount() returned 0x%x", rc);
		return JS_EXCEPTION;
	}

	AccountUid uids[user_count];
	rc = accountListAllUsers(uids, user_count, &user_count);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "accountListAllUsers() returned 0x%x", rc);
		return JS_EXCEPTION;
	}

	JSValue arr = JS_NewArray(ctx);
	for (int i = 0; i < user_count; i++)
	{
		JSValue obj = profile_new(ctx, uids[i]);
		if (JS_IsException(obj))
		{
			return obj;
		}
		JS_SetPropertyUint32(ctx, arr, i, obj);
	}
	return arr;
}

static JSValue nx_account_nickname(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Result rc;
	nx_profile_t *profile = nx_get_profile(ctx, this_val);
	if (!profile)
	{
		return JS_EXCEPTION;
	}
	if (!profile->profile_loaded)
	{
		rc = accountGetProfile(&profile->profile, profile->uid);
		if (R_FAILED(rc))
		{
			JS_ThrowInternalError(ctx, "accountGetProfile() returned 0x%x", rc);
			return JS_EXCEPTION;
		}
		profile->profile_loaded = true;
	}
	if (!profile->userdata_loaded)
	{
		rc = accountProfileGet(&profile->profile, &profile->userdata, &profile->profilebase);
		if (R_FAILED(rc))
		{
			JS_ThrowInternalError(ctx, "accountProfileGet() returned 0x%x", rc);
			return JS_EXCEPTION;
		}
		profile->userdata_loaded = true;
	}
	size_t len = strnlen(profile->profilebase.nickname, sizeof(profile->profilebase.nickname));
	return JS_NewStringLen(ctx, profile->profilebase.nickname, len);
}

static JSValue nx_account_image(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Result rc;
	nx_profile_t *profile = nx_get_profile(ctx, this_val);
	if (!profile)
	{
		return JS_EXCEPTION;
	}
	if (!profile->profile_loaded)
	{
		rc = accountGetProfile(&profile->profile, profile->uid);
		if (R_FAILED(rc))
		{
			JS_ThrowInternalError(ctx, "accountGetProfile() returned 0x%x", rc);
			return JS_EXCEPTION;
		}
		profile->profile_loaded = true;
	}

	u32 image_size;
	rc = accountProfileGetImageSize(&profile->profile, &image_size);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "accountProfileGetImageSize() returned 0x%x", rc);
		return JS_EXCEPTION;
	}

	u8 buf[image_size];
	rc = accountProfileLoadImage(&profile->profile, buf, image_size, &image_size);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "accountProfileLoadImage() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewArrayBufferCopy(ctx, buf, image_size);
}

static JSValue nx_account_profile_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "nickname", nx_account_nickname);
	NX_DEF_GET(proto, "image", nx_account_image);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("accountInitialize", 0, nx_account_initialize),
	JS_CFUNC_DEF("accountProfileInit", 1, nx_account_profile_init),
	JS_CFUNC_DEF("accountCurrentProfile", 0, nx_account_current_profile),
	JS_CFUNC_DEF("accountSelectProfile", 0, nx_account_select_profile),
	JS_CFUNC_DEF("accountProfiles", 0, nx_account_profiles),
};

void nx_init_account(JSContext *ctx, JSValueConst init_obj)
{
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_profile_class_id);
	JSClassDef profile_class = {
		"Profile",
		.finalizer = finalizer_profile,
	};
	JS_NewClass(rt, nx_profile_class_id, &profile_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
