#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <malloc.h>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <switch.h>

using namespace v8;

namespace {

struct nx_usb_device_t {
	UsbHsInterface inf;
	UsbHsClientIfSession if_session;
	bool opened;
	UsbHsClientEpSession in_eps[16];
	UsbHsClientEpSession out_eps[16];
	bool in_open[16];
	bool out_open[16];
};

bool g_initialized = false;

uint32_t get_u32_prop(Isolate *iso, Local<Object> obj, const char *name,
	                    bool *has) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> v;
	*has = false;
	if (!obj->Get(ctx, nx_str(iso, name)).ToLocal(&v) || v->IsUndefined() ||
	    v->IsNull()) {
		return 0;
	}
	*has = true;
	return v->Uint32Value(ctx).FromMaybe(0);
}

bool get_string_prop(Isolate *iso, Local<Object> obj, const char *name,
	                    Local<Value> *out) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> v;
	if (!obj->Get(ctx, nx_str(iso, name)).ToLocal(&v) || !v->IsString()) {
		char message[128];
		snprintf(message, sizeof(message), "USB control transfer setup.%s must be a string", name);
		nx_throw(iso, message);
		return false;
	}
	*out = v;
	return true;
}

void usb_device_close(nx_usb_device_t *dev) {
	if (!dev)
		return;
	for (int i = 0; i < 16; i++) {
		if (dev->in_open[i]) {
			usbHsEpClose(&dev->in_eps[i]);
			dev->in_open[i] = false;
		}
		if (dev->out_open[i]) {
			usbHsEpClose(&dev->out_eps[i]);
			dev->out_open[i] = false;
		}
	}
	if (dev->opened) {
		usbHsIfClose(&dev->if_session);
		dev->opened = false;
	}
}

void usb_device_free(nx_usb_device_t *dev) {
	usb_device_close(dev);
	free(dev);
}

bool ensure_usb(Isolate *iso) {
	if (g_initialized)
		return true;
	Result rc = usbHsInitialize();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsInitialize");
		return false;
	}
	g_initialized = true;
	return true;
}

nx_usb_device_t *unwrap_device(Isolate *iso, Local<Value> v) {
	nx_usb_device_t *dev = nx::Unwrap<nx_usb_device_t>(v);
	if (!dev) {
		nx_throw(iso, "Invalid USBDevice native handle");
		return nullptr;
	}
	return dev;
}

void set_u32(Isolate *iso, Local<Object> obj, const char *name, uint32_t value) {
	Local<Context> ctx = iso->GetCurrentContext();
	obj->Set(ctx, nx_str(iso, name), Integer::NewFromUnsigned(iso, value)).Check();
}

Local<Object> endpoint_to_object(Isolate *iso,
	                               const struct usb_endpoint_descriptor *desc) {
	Local<Object> obj = Object::New(iso);
	set_u32(iso, obj, "endpointNumber",
	        desc->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK);
	obj->Set(iso->GetCurrentContext(), nx_str(iso, "direction"),
	         nx_str(iso, (desc->bEndpointAddress & USB_ENDPOINT_IN) ? "in" : "out"))
	    .Check();
	const char *type = "bulk";
	switch (desc->bmAttributes & USB_TRANSFER_TYPE_MASK) {
	case USB_TRANSFER_TYPE_ISOCHRONOUS:
		type = "isochronous";
		break;
	case USB_TRANSFER_TYPE_INTERRUPT:
		type = "interrupt";
		break;
	}
	obj->Set(iso->GetCurrentContext(), nx_str(iso, "type"), nx_str(iso, type))
	    .Check();
	set_u32(iso, obj, "packetSize", desc->wMaxPacketSize);
	return obj;
}

Local<Object> interface_to_object(Isolate *iso, const UsbHsInterface *inf) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> alt = Object::New(iso);
	const struct usb_interface_descriptor *desc = &inf->inf.interface_desc;
	set_u32(iso, alt, "alternateSetting", desc->bAlternateSetting);
	set_u32(iso, alt, "interfaceClass", desc->bInterfaceClass);
	set_u32(iso, alt, "interfaceSubclass", desc->bInterfaceSubClass);
	set_u32(iso, alt, "interfaceProtocol", desc->bInterfaceProtocol);
	Local<Array> eps = Array::New(iso);
	uint32_t ep_idx = 0;
	for (int i = 0; i < 15; i++) {
		const struct usb_endpoint_descriptor *in = &inf->inf.input_endpoint_descs[i];
		if (in->bLength == USB_DT_ENDPOINT_SIZE) {
			eps->Set(ctx, ep_idx++, endpoint_to_object(iso, in)).Check();
		}
		const struct usb_endpoint_descriptor *out = &inf->inf.output_endpoint_descs[i];
		if (out->bLength == USB_DT_ENDPOINT_SIZE) {
			eps->Set(ctx, ep_idx++, endpoint_to_object(iso, out)).Check();
		}
	}
	alt->Set(ctx, nx_str(iso, "endpoints"), eps).Check();

	Local<Object> iface = Object::New(iso);
	set_u32(iso, iface, "interfaceNumber", desc->bInterfaceNumber);
	set_u32(iso, iface, "claimed", 0);
	Local<Array> alternates = Array::New(iso, 1);
	alternates->Set(ctx, 0, alt).Check();
	iface->Set(ctx, nx_str(iso, "alternates"), alternates).Check();
	return iface;
}

void set_device_descriptor(Isolate *iso, Local<Object> obj,
	                         const UsbHsInterface *inf) {
	Local<Context> ctx = iso->GetCurrentContext();
	struct usb_device_descriptor dev;
	memcpy(&dev, &inf->device_desc, sizeof(dev));
	set_u32(iso, obj, "usbVersionMajor", (dev.bcdUSB >> 8) & 0xff);
	set_u32(iso, obj, "usbVersionMinor", (dev.bcdUSB >> 4) & 0x0f);
	set_u32(iso, obj, "usbVersionSubminor", dev.bcdUSB & 0x0f);
	set_u32(iso, obj, "deviceClass", dev.bDeviceClass);
	set_u32(iso, obj, "deviceSubclass", dev.bDeviceSubClass);
	set_u32(iso, obj, "deviceProtocol", dev.bDeviceProtocol);
	set_u32(iso, obj, "vendorId", dev.idVendor);
	set_u32(iso, obj, "productId", dev.idProduct);
	set_u32(iso, obj, "deviceVersionMajor", (dev.bcdDevice >> 8) & 0xff);
	set_u32(iso, obj, "deviceVersionMinor", (dev.bcdDevice >> 4) & 0x0f);
	set_u32(iso, obj, "deviceVersionSubminor", dev.bcdDevice & 0x0f);

	Local<Object> config = Object::New(iso);
	set_u32(iso, config, "configurationValue", inf->config_desc.bConfigurationValue);
	Local<Array> interfaces = Array::New(iso, 1);
	interfaces->Set(ctx, 0, interface_to_object(iso, inf)).Check();
	config->Set(ctx, nx_str(iso, "interfaces"), interfaces).Check();
	Local<Array> configurations = Array::New(iso, 1);
	configurations->Set(ctx, 0, config).Check();
	obj->Set(ctx, nx_str(iso, "configurations"), configurations).Check();
}

struct usb_endpoint_descriptor *find_endpoint(nx_usb_device_t *dev,
	                                            uint8_t endpoint,
	                                            bool in) {
	struct usb_endpoint_descriptor *descs = in
	    ? dev->inf.inf.input_endpoint_descs
	    : dev->inf.inf.output_endpoint_descs;
	for (int i = 0; i < 15; i++) {
		struct usb_endpoint_descriptor *desc = &descs[i];
		if (desc->bLength != USB_DT_ENDPOINT_SIZE)
			continue;
		if ((desc->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK) == endpoint)
			return desc;
	}
	return nullptr;
}

UsbHsClientEpSession *open_endpoint(Isolate *iso, nx_usb_device_t *dev,
	                                  uint8_t endpoint, bool in,
	                                  uint32_t xfer_size) {
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return nullptr;
	}
	if (endpoint >= 16) {
		nx_throw(iso, "Invalid USB endpoint number");
		return nullptr;
	}
	bool *open = in ? dev->in_open : dev->out_open;
	UsbHsClientEpSession *eps = in ? dev->in_eps : dev->out_eps;
	if (open[endpoint])
		return &eps[endpoint];
	struct usb_endpoint_descriptor *desc = find_endpoint(dev, endpoint, in);
	if (!desc) {
		nx_throw(iso, "USB endpoint not found");
		return nullptr;
	}
	uint32_t max_xfer = xfer_size;
	if (max_xfer < desc->wMaxPacketSize)
		max_xfer = desc->wMaxPacketSize;
	if (max_xfer < 0x1000)
		max_xfer = 0x1000;
	Result rc = usbHsIfOpenUsbEp(&dev->if_session, &eps[endpoint], 4, max_xfer,
	                             desc);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsIfOpenUsbEp");
		return nullptr;
	}
	open[endpoint] = true;
	return &eps[endpoint];
}

void nx_usb_init(const FunctionCallbackInfo<Value> &info) {
	ensure_usb(info.GetIsolate());
}

void nx_usb_exit(const FunctionCallbackInfo<Value> &info) {
	(void)info;
	if (g_initialized) {
		usbHsExit();
		g_initialized = false;
	}
}

void nx_usb_get_devices(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	if (!ensure_usb(iso))
		return;

	UsbHsInterfaceFilter filter = {0};
	if (info[0]->IsObject()) {
		Local<Object> f = info[0].As<Object>();
		bool has = false;
		uint32_t v = get_u32_prop(iso, f, "vendorId", &has);
		if (has) {
			filter.Flags |= UsbHsInterfaceFilterFlags_idVendor;
			filter.idVendor = (u16)v;
		}
		v = get_u32_prop(iso, f, "productId", &has);
		if (has) {
			filter.Flags |= UsbHsInterfaceFilterFlags_idProduct;
			filter.idProduct = (u16)v;
		}
		v = get_u32_prop(iso, f, "classCode", &has);
		if (has) {
			filter.Flags |= UsbHsInterfaceFilterFlags_bDeviceClass;
			filter.bDeviceClass = (u8)v;
		}
		v = get_u32_prop(iso, f, "subclassCode", &has);
		if (has) {
			filter.Flags |= UsbHsInterfaceFilterFlags_bDeviceSubClass;
			filter.bDeviceSubClass = (u8)v;
		}
		v = get_u32_prop(iso, f, "protocolCode", &has);
		if (has) {
			filter.Flags |= UsbHsInterfaceFilterFlags_bDeviceProtocol;
			filter.bDeviceProtocol = (u8)v;
		}
		v = get_u32_prop(iso, f, "interfaceClass", &has);
		if (has) {
			filter.Flags |= UsbHsInterfaceFilterFlags_bInterfaceClass;
			filter.bInterfaceClass = (u8)v;
		}
		v = get_u32_prop(iso, f, "interfaceSubclass", &has);
		if (has) {
			filter.Flags |= UsbHsInterfaceFilterFlags_bInterfaceSubClass;
			filter.bInterfaceSubClass = (u8)v;
		}
		v = get_u32_prop(iso, f, "interfaceProtocol", &has);
		if (has) {
			filter.Flags |= UsbHsInterfaceFilterFlags_bInterfaceProtocol;
			filter.bInterfaceProtocol = (u8)v;
		}
	}

	UsbHsInterface interfaces[32];
	s32 total = 0;
	Result rc = usbHsQueryAvailableInterfaces(&filter, interfaces,
	                                         sizeof(interfaces), &total);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsQueryAvailableInterfaces");
		return;
	}
	if (total > 32)
		total = 32;
	Local<Array> arr = Array::New(iso, total);
	for (s32 i = 0; i < total; i++) {
		nx_usb_device_t *dev = (nx_usb_device_t *)calloc(1, sizeof(*dev));
		if (!dev) {
			nx_throw_oom(iso, sizeof(*dev));
			return;
		}
		dev->inf = interfaces[i];
		Local<Object> obj = nx::NewWrapped(iso);
		nx::Wrap(iso, obj, dev, usb_device_free);
		set_device_descriptor(iso, obj, &interfaces[i]);
		arr->Set(ctx, i, obj).Check();
	}
	info.GetReturnValue().Set(arr);
}

void nx_usb_device_open(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (dev->opened)
		return;
	Result rc = usbHsAcquireUsbIf(&dev->if_session, &dev->inf);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsAcquireUsbIf");
		return;
	}
	dev->opened = true;
}

void nx_usb_device_close(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	usb_device_close(dev);
}

void nx_usb_claim_interface(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return;
	}
	uint32_t iface = info[1]->Uint32Value(ctx).FromMaybe(0);
	if (iface != dev->inf.inf.interface_desc.bInterfaceNumber) {
		nx_throw(iso, "USB interface not found on this device handle");
	}
}

void nx_usb_transfer_out(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	uint32_t endpoint = info[1]->Uint32Value(ctx).FromMaybe(0);
	size_t len = 0;
	uint8_t *src = NX_GetBufferSource(iso, &len, info[2]);
	if (!src) {
		nx_throw(iso, "transferOut data must be a BufferSource");
		return;
	}
	UsbHsClientEpSession *ep = open_endpoint(iso, dev, (uint8_t)endpoint, false,
	                                        (uint32_t)len);
	if (!ep)
		return;
	if (len > 0xff0000) {
		nx_throw(iso, "USB transfer is too large");
		return;
	}
	size_t alloc_size = (len + 0xfff) & ~((size_t)0xfff);
	if (alloc_size == 0)
		alloc_size = 0x1000;
	void *buf = memalign(0x1000, alloc_size);
	if (!buf) {
		nx_throw_oom(iso, alloc_size);
		return;
	}
	memset(buf, 0, alloc_size);
	memcpy(buf, src, len);
	u32 transferred = 0;
	Result rc = usbHsEpPostBuffer(ep, buf, (u32)len, &transferred);
	free(buf);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsEpPostBuffer");
		return;
	}
	info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, transferred));
}

void nx_usb_transfer_in(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	uint32_t endpoint = info[1]->Uint32Value(ctx).FromMaybe(0);
	uint32_t len = info[2]->Uint32Value(ctx).FromMaybe(0);
	UsbHsClientEpSession *ep = open_endpoint(iso, dev, (uint8_t)endpoint, true,
	                                        len);
	if (!ep)
		return;
	if (len > 0xff0000) {
		nx_throw(iso, "USB transfer is too large");
		return;
	}
	size_t alloc_size = (len + 0xfff) & ~((size_t)0xfff);
	if (alloc_size == 0)
		alloc_size = 0x1000;
	void *buf = memalign(0x1000, alloc_size);
	if (!buf) {
		nx_throw_oom(iso, alloc_size);
		return;
	}
	memset(buf, 0, alloc_size);
	u32 transferred = 0;
	Result rc = usbHsEpPostBuffer(ep, buf, len, &transferred);
	if (R_FAILED(rc)) {
		free(buf);
		nx_throw_libnx_error(iso, rc, "usbHsEpPostBuffer");
		return;
	}
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, transferred, [](void *p, size_t, void *) { free(p); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

uint8_t request_type_bits(const char *s) {
	if (strcmp(s, "class") == 0)
		return USB_REQUEST_TYPE_CLASS;
	if (strcmp(s, "vendor") == 0)
		return USB_REQUEST_TYPE_VENDOR;
	if (strcmp(s, "reserved") == 0)
		return USB_REQUEST_TYPE_RESERVED;
	return USB_REQUEST_TYPE_STANDARD;
}

uint8_t recipient_bits(const char *s) {
	if (strcmp(s, "interface") == 0)
		return USB_RECIPIENT_INTERFACE;
	if (strcmp(s, "endpoint") == 0)
		return USB_RECIPIENT_ENDPOINT;
	if (strcmp(s, "other") == 0)
		return USB_RECIPIENT_OTHER;
	return USB_RECIPIENT_DEVICE;
}

void nx_usb_control_transfer_in(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return;
	}
	if (!info[1]->IsObject()) {
		nx_throw(iso, "controlTransferIn setup must be an object");
		return;
	}
	Local<Object> setup = info[1].As<Object>();
	Local<Value> request_type_val;
	Local<Value> recipient_val;
	if (!get_string_prop(iso, setup, "requestType", &request_type_val) ||
	    !get_string_prop(iso, setup, "recipient", &recipient_val)) {
		return;
	}
	String::Utf8Value request_type(iso, request_type_val);
	String::Utf8Value recipient(iso, recipient_val);
	bool has = false;
	uint32_t request = get_u32_prop(iso, setup, "request", &has);
	uint32_t value = get_u32_prop(iso, setup, "value", &has);
	uint32_t index = get_u32_prop(iso, setup, "index", &has);
	uint32_t len = info[2]->Uint32Value(ctx).FromMaybe(0);
	if (len > 0xffff) {
		nx_throw(iso, "USB control transfer length is too large");
		return;
	}
	size_t alloc_size = (len + 0xfff) & ~((size_t)0xfff);
	if (alloc_size == 0)
		alloc_size = 0x1000;
	void *buf = memalign(0x1000, alloc_size);
	if (!buf) {
		nx_throw_oom(iso, alloc_size);
		return;
	}
	memset(buf, 0, alloc_size);
	u8 bmRequestType = USB_ENDPOINT_IN |
	    request_type_bits(*request_type ? *request_type : "standard") |
	    recipient_bits(*recipient ? *recipient : "device");
	u32 transferred = 0;
	Result rc = usbHsIfCtrlXfer(&dev->if_session, bmRequestType, (u8)request,
	                            (u16)value, (u16)index, (u16)len, buf,
	                            &transferred);
	if (R_FAILED(rc)) {
		free(buf);
		nx_throw_libnx_error(iso, rc, "usbHsIfCtrlXfer");
		return;
	}
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, transferred, [](void *p, size_t, void *) { free(p); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

void nx_usb_reset_device(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return;
	}
	Result rc = usbHsIfResetDevice(&dev->if_session);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsIfResetDevice");
	}
}

} // namespace

void nx_init_usb(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "usbInit", nx_usb_init);
	NX_SET_FUNC(init_obj, "usbExit", nx_usb_exit);
	NX_SET_FUNC(init_obj, "usbGetDevices", nx_usb_get_devices);
	NX_SET_FUNC(init_obj, "usbDeviceOpen", nx_usb_device_open);
	NX_SET_FUNC(init_obj, "usbDeviceClose", nx_usb_device_close);
	NX_SET_FUNC(init_obj, "usbClaimInterface", nx_usb_claim_interface);
	NX_SET_FUNC(init_obj, "usbTransferIn", nx_usb_transfer_in);
	NX_SET_FUNC(init_obj, "usbTransferOut", nx_usb_transfer_out);
	NX_SET_FUNC(init_obj, "usbControlTransferIn", nx_usb_control_transfer_in);
	NX_SET_FUNC(init_obj, "usbResetDevice", nx_usb_reset_device);
}
