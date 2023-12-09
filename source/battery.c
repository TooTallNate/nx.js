#include "types.h"

static JSValue nx_battery_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{

	Result rc = psmInitialize();
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "Failed to initialize PSM");
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static JSValue nx_battery_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	psmExit();
	return JS_UNDEFINED;
}

static JSValue nx_battery_charging(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	PsmChargerType type;
	Result rc = psmGetChargerType(&type);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "Failed to get charger type: %08X", rc);
		return JS_EXCEPTION;
	}
	return JS_NewBool(ctx, type != PsmChargerType_Unconnected);
}

static JSValue nx_battery_level(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	double rawCharge;
	Result rc = psmGetRawBatteryChargePercentage(&rawCharge);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "Failed to read battery level: %08X", rc);
		return JS_EXCEPTION;
	}
	return JS_NewFloat64(ctx, rawCharge / 100.0);
}

/* Initialize the `BatteryManager` class */
static JSValue nx_battery_init_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "charging", nx_battery_charging);
	NX_DEF_GET(proto, "level", nx_battery_level);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("batteryInit", 1, nx_battery_init),
	JS_CFUNC_DEF("batteryInitClass", 1, nx_battery_init_class),
	JS_CFUNC_DEF("batteryExit", 1, nx_battery_exit),
};

void nx_init_battery(JSContext *ctx, JSValueConst init_obj)
{
	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
