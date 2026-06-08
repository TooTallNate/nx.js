#include <switch.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#include "resolve.h"
#include "ui.h"

#ifndef VERSION
#define VERSION "nx.js-forwarder"
#endif

u32 __nx_applet_type = AppletType_SystemApplication;

// Force appletExit() to run the proper applet self-exit handshake even though
// this process is forwarded homebrew (envIsNso()==false). Without this, libnx's
// _appletCleanup() skips the exit commands and a plain svcExitProcess() makes
// the OS report "software was closed" instead of returning cleanly to home.
// Only consulted on the error/no-runtime path (the NRO-load path never exits).
u32 __nx_applet_exit_mode = 1;

const char g_noticeText[] =
    "nx-hbloader " VERSION "\0"
    "Do you mean to tell me that you're thinking seriously of building that way, when and if you are an architect?";

static char g_argv[2048];
static char g_nextArgv[2048];
static char g_nextNroPath[FS_MAX_PATH];
u64  g_nroAddr = 0;
static u64  g_nroSize = 0;
static NroHeader g_nroHeader;
static bool g_isApplication = 0;

static bool g_isAutomaticGameplayRecording = 0;
static enum {
    CodeMemoryUnavailable    = 0,
    CodeMemoryForeignProcess = BIT(0),
    CodeMemorySameProcess    = BIT(0) | BIT(1),
} g_codeMemoryCapability = CodeMemoryUnavailable;

static Handle g_procHandle;
static bool g_hasLoadedOnce = false;

static void*  g_heapAddr;
static size_t g_heapSize;

static u64 g_appletHeapSize = 0;
static u64 g_appletHeapReservationSize = 0;

static u128 g_userIdStorage;

static u8 g_savedTls[0x100];

// Minimize fs resource usage
u32 __nx_fs_num_sessions = 1;
u32 __nx_fsdev_direntry_cache_size = 1;
bool __nx_fsdev_support_cwd = false;

// Used by trampoline.s
Result g_lastRet = 0;

void NX_NORETURN nroEntrypointTrampoline(const ConfigEntry* entries, u64 handle, u64 entrypoint);

void __libnx_initheap(void)
{
    // 16 MiB malloc arena. Upstream nx-hbloader uses a tiny 16 KiB heap (it
    // loads the NRO into a separate svcSetHeapSize region, not malloc), but we
    // also render an on-screen console for errors / the future runtime-download
    // UI — and consoleInit()'s framebuffer allocation blocks if the malloc heap
    // is too small (confirmed on-device: tiny heap -> framebufferCreate hangs).
    static char g_innerheap[0x1000000];

    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_innerheap[0];
    fake_heap_end   = &g_innerheap[sizeof g_innerheap];
}

static Result readSetting(const char* key, void* buf, size_t size)
{
    Result rc;
    u64 actual_size;
    const char* const section_name = "hbloader";
    rc = setsysGetSettingsItemValueSize(section_name, key, &actual_size);
    if (R_SUCCEEDED(rc) && actual_size != size)
        rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (R_SUCCEEDED(rc))
        rc = setsysGetSettingsItemValue(section_name, key, buf, size, &actual_size);
    if (R_SUCCEEDED(rc) && actual_size != size)
        rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (R_FAILED(rc)) memset(buf, 0, size);
    return rc;
}

void __appInit(void)
{
    Result rc;

    // Detect Atmosphère early on. This is required for hosversion logic.
    // In the future, this check will be replaced by detectMesosphere().
    Handle dummy;
    rc = svcConnectToNamedPort(&dummy, "ams");
    u32 ams_flag = (R_VALUE(rc) != KERNELRESULT(NotFound)) ? BIT(31) : 0;
    if (R_SUCCEEDED(rc))
        svcCloseHandle(dummy);

    rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 1));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(ams_flag | MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        readSetting("applet_heap_size", &g_appletHeapSize, sizeof(g_appletHeapSize));
        readSetting("applet_heap_reservation_size", &g_appletHeapReservationSize, sizeof(g_appletHeapReservationSize));
        setsysExit();
    }

    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 2));
}

void __wrap_exit(void)
{
    // exit() effectively never gets called, so let's stub it out.
    diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 39));
}

static u64 calculateMaxHeapSize(void)
{
    u64 size = 0;
    u64 mem_available = 0, mem_used = 0;

    svcGetInfo(&mem_available, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&mem_used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);

    if (mem_available > mem_used+0x200000)
        size = (mem_available - mem_used - 0x200000) & ~0x1FFFFF;
    if (size == 0)
        size = 0x2000000*16;
    if (size > 0x6000000 && g_isAutomaticGameplayRecording)
        size -= 0x6000000;

    return size;
}

static void setupHbHeap(void)
{
    void* addr = NULL;
    u64 size = calculateMaxHeapSize();

    if (!g_isApplication) {
        if (g_appletHeapSize) {
            u64 requested_size = (g_appletHeapSize + 0x1FFFFF) &~ 0x1FFFFF;
            if (requested_size < size)
                size = requested_size;
        }
        else if (g_appletHeapReservationSize) {
            u64 reserved_size = (g_appletHeapReservationSize + 0x1FFFFF) &~ 0x1FFFFF;
            if (reserved_size < size)
                size -= reserved_size;
        }
    }

    Result rc = svcSetHeapSize(&addr, size);

    if (R_FAILED(rc) || addr==NULL)
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 9));

    g_heapAddr = addr;
    g_heapSize = size;
}

static void procHandleReceiveThread(void* arg)
{
    Handle session = (Handle)(uintptr_t)arg;
    Result rc;

    void* base = armGetTls();
    hipcMakeRequestInline(base);

    s32 idx = 0;
    rc = svcReplyAndReceive(&idx, &session, 1, INVALID_HANDLE, UINT64_MAX);
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 15));

    HipcParsedRequest r = hipcParseRequest(base);
    if (r.meta.num_copy_handles != 1)
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 17));

    g_procHandle = r.data.copy_handles[0];
    svcCloseHandle(session);
}

// Sets g_isApplication if the hbloader process is running as an Application.
static void getIsApplication(void)
{
    Result rc;

    // Try asking the kernel directly (only works on [9.0.0+] or mesosphère)
    u64 flag=0;
    rc = svcGetInfo(&flag, InfoType_IsApplication, CUR_PROCESS_HANDLE, 0);
    if (R_SUCCEEDED(rc)) {
        g_isApplication = flag!=0;
        return;
    }

    // Retrieve our process' PID
    u64 cur_pid=0;
    rc = svcGetProcessId(&cur_pid, CUR_PROCESS_HANDLE);
    if (R_FAILED(rc)) diagAbortWithResult(rc); // shouldn't happen

    // Try reading the current application PID through pm:shell - if it matches ours then we are indeed an application
    rc = pmshellInitialize();
    if (R_SUCCEEDED(rc)) {
        u64 app_pid=0;
        rc = pmshellGetApplicationProcessIdForShell(&app_pid);
        pmshellExit();

        if (cur_pid == app_pid)
            g_isApplication = 1;
    }
}

// Sets g_isAutomaticGameplayRecording if the current program has automatic gameplay recording enabled in its NACP.
//Gets the control.nacp for the current program id, and then sets g_isAutomaticGameplayRecording if less memory should be allocated.
static void getIsAutomaticGameplayRecording(void)
{
    Result rc;

    // Do nothing if the HOS version predates [4.0.0], or we're not an application.
    if (hosversionBefore(4,0,0) || !g_isApplication)
        return;

    // Retrieve our process' Program ID
    u64 cur_progid=0;
    rc = svcGetInfo(&cur_progid, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(rc)) diagAbortWithResult(rc); // shouldn't happen

    // Try reading our NACP
    rc = nsInitialize();
    if (R_SUCCEEDED(rc)) {
        NsApplicationControlData data; // note: this is 144KB, which still fits comfortably within the 1MB of stack we have
        u64 size=0;
        rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, cur_progid, &data, sizeof(data), &size);
        nsExit();

        if (R_SUCCEEDED(rc) && data.nacp.video_capture == 2)
            g_isAutomaticGameplayRecording = 1;
    }
}

static void getOwnProcessHandle(void)
{
    Result rc;

    Handle server_handle, client_handle;
    rc = svcCreateSession(&server_handle, &client_handle, 0, 0);
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 12));

    Thread t;
    rc = threadCreate(&t, &procHandleReceiveThread, (void*)(uintptr_t)server_handle, NULL, 0x1000, 0x20, 0);
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 10));

    rc = threadStart(&t);
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 13));

    hipcMakeRequestInline(armGetTls(),
        .num_copy_handles = 1,
    ).copy_handles[0] = CUR_PROCESS_HANDLE;

    svcSendSyncRequest(client_handle);
    svcCloseHandle(client_handle);

    threadWaitForExit(&t);
    threadClose(&t);
}

static bool isKernel5xOrLater(void)
{
    u64 dummy = 0;
    Result rc = svcGetInfo(&dummy, InfoType_UserExceptionContextAddress, INVALID_HANDLE, 0);
    return R_VALUE(rc) != KERNELRESULT(InvalidEnumValue);
}

static bool isKernel4x(void)
{
    u64 dummy = 0;
    Result rc = svcGetInfo(&dummy, InfoType_InitialProcessIdRange, INVALID_HANDLE, 0);
    return R_VALUE(rc) != KERNELRESULT(InvalidEnumValue);
}

static void getCodeMemoryCapability(void)
{
    if (detectMesosphere()) {
        // Mesosphère allows for same-process code memory usage.
        g_codeMemoryCapability = CodeMemorySameProcess;
    } else if (isKernel5xOrLater()) {
        // On [5.0.0+], the kernel does not allow the creator process of a CodeMemory object
        // to use svcControlCodeMemory on itself, thus returning InvalidMemoryState (0xD401).
        // However the kernel can be patched to support same-process usage of CodeMemory.
        // We can detect that by passing a bad operation and observe if we actually get InvalidEnumValue (0xF001).
        Handle code;
        Result rc = svcCreateCodeMemory(&code, g_heapAddr, 0x1000);
        if (R_SUCCEEDED(rc)) {
            rc = svcControlCodeMemory(code, (CodeMapOperation)-1, 0, 0x1000, 0);
            svcCloseHandle(code);

            if (R_VALUE(rc) == KERNELRESULT(InvalidEnumValue))
                g_codeMemoryCapability = CodeMemorySameProcess;
            else
                g_codeMemoryCapability = CodeMemoryForeignProcess;
        }
    } else if (isKernel4x()) {
        // On [4.0.0-4.1.0] there is no such restriction on same-process CodeMemory usage.
        g_codeMemoryCapability = CodeMemorySameProcess;
    } else {
        // This kernel is too old to support CodeMemory syscalls.
        g_codeMemoryCapability = CodeMemoryUnavailable;
    }
}


static void selfExit(void) {
    Service applet, proxy, self;
    Result rc=0;

    rc = smInitialize();
    if (R_FAILED(rc))
        goto fail0;

    rc = smGetService(&applet, g_isApplication ? "appletOE" : "appletAE");
    if (R_FAILED(rc))
        goto fail1;

    u32 cmd_id = g_isApplication ? 0 : 200;
    u64 reserved = 0;

    // GetSessionProxy
    rc = serviceDispatchIn(&applet, cmd_id, reserved,
        .in_send_pid = true,
        .in_num_handles = 1,
        .in_handles = { g_procHandle },
        .out_num_objects = 1,
        .out_objects = &proxy,
    );
    if (R_FAILED(rc))
        goto fail2;

    // GetSelfController
    rc = serviceDispatch(&proxy, 1,
        .out_num_objects = 1,
        .out_objects = &self,
    );
    if (R_FAILED(rc))
        goto fail3;

    // Exit
    rc = serviceDispatch(&self, 0);

    serviceClose(&self);

fail3:
    serviceClose(&proxy);

fail2:
    serviceClose(&applet);

fail1:
    smExit();

fail0:
    if (R_SUCCEEDED(rc)) {
        while(1) svcSleepThread(86400000000000ULL);
        svcExitProcess();
        __builtin_unreachable();
    } else {
        diagAbortWithResult(rc);
    }
}

// Render the shared "no runtime found" console error screen, then exit cleanly.
//
// This forwarder uses a minimal __appInit (sm/setsys/fs, then main() smExit'd),
// so the display + input stack the console needs isn't up — bring it up here in
// libnx's default __appInit order. The 16 MiB malloc heap (see
// __libnx_initheap) is required: consoleInit()'s framebufferCreate allocates
// from malloc and blocks if the heap is tiny (confirmed on-device). Does not
// return. Same code path will host the future runtime-download UI.
extern void __libnx_init_time(void);
extern void __nx_win_init(void);
extern void __nx_win_exit(void);
void NX_NORETURN __nx_exit(Result rc, LoaderReturnFn retaddr);

// Strong override of ui.c's weak nx_ui_exit(): invoked after the user presses +
// on the "no runtime found" screen. This forwarder can't exit via libc exit()
// (it links -Wl,-wrap,exit, whose __wrap_exit aborts), so we replicate libnx's
// __libnx_exit() path by hand. Tear down the display we brought up, then
// appletExit(): with __nx_applet_exit_mode=1 (set at top of file),
// _appletCleanup() registers _appletExitProcess as the exit func (via
// envSetExitFuncPtr) and returns. __nx_exit() then jumps to it, performing the
// real applet self-exit handshake the OS expects — returning to the home menu
// without the "software was closed" dialog. Does not return.
void nx_ui_exit(void)
{
    consoleExit(NULL);
    __nx_win_exit();
    hidExit();
    timeExit();
    appletExit();
    __nx_exit(0, envGetExitFuncPtr());
    __builtin_unreachable();
}

// Bring up the display + input stack the on-screen error UI needs.
//
// This forwarder uses a minimal __appInit (sm/setsys/fs, then main() smExit'd),
// so unlike a normal libnx app the display/input services aren't up — initialize
// them here in libnx's default __appInit order before any nx_fail* call. The
// 16 MiB malloc heap (see __libnx_initheap) is also required: consoleInit()'s
// framebufferCreate allocates from malloc and blocks if the heap is tiny
// (confirmed on-device). The matching teardown happens in nx_ui_exit().
static void nx_ui_bringup(void)
{
    smInitialize();
    appletInitialize();
    hidInitialize();
    if (R_SUCCEEDED(timeInitialize()))
        __libnx_init_time();
    __nx_win_init();
}

// `__nx_applet_exit_mode` (defined at top of file = 1, for the clean self-exit
// on the error screen). We need to flip it to 0 for the "continue after
// download" teardown below — see there.
extern u32 __nx_applet_exit_mode;

// No installed runtime: bring up the display, auto-download a compatible one
// (nx_resolve_or_download renders progress), and on success fill `r` + return
// true so loadNro continues to map the freshly-downloaded runtime. On failure
// nx_resolve_or_download shows the manual-install screen + exits (does not
// return).
//
// CRITICAL: on success we must restore the EXACT service state the happy path
// jumps into the runtime with — which is what main() left before loadNro:
// sm CLOSED, fs up, and NOTHING else (no applet/hid/time/display). The runtime
// does its own __appInit (smInitialize etc.); if we leave sm open or the applet
// in the self-exit-armed state (__nx_applet_exit_mode=1), the runtime's libnx
// init fails (LibnxError_InitFail). So tear down everything nx_ui_bringup
// brought up, with a PLAIN appletExit (exit mode 0), and finally smExit().
static bool nx_download_or_exit(nx_resolve_t *r)
{
    nx_ui_bringup();
    if (!nx_resolve_or_download(r))
        return false; // unreachable: the error path exits
    consoleExit(NULL);
    __nx_win_exit();
    hidExit();
    timeExit();
    // Plain applet teardown (NOT the self-exit handshake): we're continuing to
    // chainload the runtime in-process, not exiting. Restore exit mode after.
    u32 saved_mode = __nx_applet_exit_mode;
    __nx_applet_exit_mode = 2; // >1 = skip exit cmds in _appletCleanup
    appletExit();
    __nx_applet_exit_mode = saved_mode;
    smExit(); // match main()'s pre-loadNro state (sm closed)
    return true;
}

// Show a one-line error (e.g. a malformed [runtime] version specifier), then
// exit cleanly. Mirrors the NRO launcher's nx_fail() path. Does not return.
static void NX_NORETURN __attribute__((format(printf, 1, 2)))
nx_show_message_and_exit(const char *fmt, ...)
{
    nx_ui_bringup();
    va_list ap;
    va_start(ap, fmt);
    nx_failv(fmt, ap);
    va_end(ap);
    __builtin_unreachable();
}

void loadNro(void)
{
    NroHeader* header = NULL;
    size_t rw_size=0;
    Result rc=0;

    memcpy((u8*)armGetTls() + 0x100, g_savedTls, 0x100);

    // Mount the SD card once (idempotent). Both the runtime resolution (scan
    // sdmc:/nx.js) and the NRO read below need it. The forwarder's __appInit
    // only does fsInitialize(), not fsdev, so we mount here. fsdevMountSdmc()
    // fails if already mounted, so guard it (this fn runs again per loaded NRO).
    static bool s_sdmc_mounted = false;
    if (!s_sdmc_mounted) {
        rc = fsdevMountSdmc();
        if (R_FAILED(rc))
            diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 404));
        s_sdmc_mounted = true;
    }

    if (g_nroSize > 0)
    {
        // Unmap previous NRO.
        header = &g_nroHeader;
        rw_size = header->segments[2].size + header->bss_size;
        rw_size = (rw_size+0xFFF) & ~0xFFF;

        svcBreak(BreakReason_NotificationOnlyFlag | BreakReason_PreUnloadDll, g_nroAddr, g_nroSize);

        // .text
        rc = svcUnmapProcessCodeMemory(
            g_procHandle, g_nroAddr + header->segments[0].file_off, ((u64) g_heapAddr) + header->segments[0].file_off, header->segments[0].size);

        if (R_FAILED(rc))
            diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 24));

        // .rodata
        rc = svcUnmapProcessCodeMemory(
            g_procHandle, g_nroAddr + header->segments[1].file_off, ((u64) g_heapAddr) + header->segments[1].file_off, header->segments[1].size);

        if (R_FAILED(rc))
            diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 25));

       // .data + .bss
        rc = svcUnmapProcessCodeMemory(
            g_procHandle, g_nroAddr + header->segments[2].file_off, ((u64) g_heapAddr) + header->segments[2].file_off, rw_size);

        if (R_FAILED(rc))
            diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 26));

        svcBreak(BreakReason_NotificationOnlyFlag | BreakReason_PostUnloadDll, g_nroAddr, g_nroSize);

        g_nroAddr = g_nroSize = 0;
    }

    if (g_nextNroPath[0] == '\0')
    {
        if (g_hasLoadedOnce)
        {
            // The runtime NRO returned without setting a next path -- exit
            // cleanly back to the System (returns to hbmenu / home screen).
            selfExit();
        }

        // Resolve the shared nx.js runtime: read this NSP's own RomFS for the
        // [runtime] version requirement, then pick the best-matching
        // sdmc:/nx.js/nxjs-v<version>.nro. Shared with the NRO launcher.
        // (SD is already mounted at the top of loadNro; romfs:/nxjs.ini comes
        // from this title's RomFS via romfsInit.)
        nx_resolve_t r = {0};
        bool ok;
        if (R_SUCCEEDED(romfsInit()))
        {
            ok = nx_read_runtime_requirement(&r);
            romfsExit();
        }
        else
        {
            // No RomFS? Fall back to the baked default requirement.
            ok = nx_read_runtime_requirement(&r);
        }

        if (!ok)
        {
            // Malformed [runtime] version specifier — show a clear error rather
            // than silently falling through to "no runtime found" (mirrors the
            // NRO launcher). Does not return.
            nx_show_message_and_exit(
                "Invalid [runtime] version specifier: \"%s\"\n", r.version);
        }

        if (!nx_resolve_runtime(&r))
        {
            // No compatible runtime installed -- bring up the display + try to
            // download one. The forwarder's __appInit is minimal and main()
            // called smExit(), so nx_download_or_exit brings up the full display
            // stack first. On success it fills `r` (and tears the display back
            // down) so we continue below; on failure it shows the manual-install
            // screen and exits (does not return).
            nx_download_or_exit(&r);
        }

        // Hand the runtime its own path as argv[0] and the "nsp:" marker as
        // argv[1]. The runtime detects "nsp:" and mounts THIS NSP's RomFS (the
        // app's main.js + assets) via romfsMountFromCurrentProcess.
        snprintf(g_nextNroPath, sizeof(g_nextNroPath), "%s", r.runtime_path);
        snprintf(g_nextArgv, sizeof(g_nextArgv), "\"%s\" \"nsp:\"",
                 r.runtime_path);

        g_hasLoadedOnce = true;
    }

    memcpy(g_argv, g_nextArgv, sizeof g_argv);

    svcBreak(BreakReason_NotificationOnlyFlag | BreakReason_PreLoadDll, (uintptr_t)g_argv, sizeof(g_argv));

    uint8_t *nrobuf = (uint8_t*) g_heapAddr;

    NroStart*  start  = (NroStart*)  (nrobuf + 0);
    header = (NroHeader*) (nrobuf + sizeof(NroStart));
    uint8_t*   rest   = (uint8_t*)   (nrobuf + sizeof(NroStart) + sizeof(NroHeader));

    // SD was already mounted (idempotently) at the top of loadNro.
    int fd = open(g_nextNroPath, O_RDONLY);
    if (fd < 0)
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 3));

    // Reset NRO path to load hbmenu by default next time.
    g_nextNroPath[0] = '\0';

    if (read(fd, start, sizeof(*start)) != sizeof(*start))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 4));

    if (read(fd, header, sizeof(*header)) != sizeof(*header))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 4));

    if (header->magic != NROHEADER_MAGIC)
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 5));

    // Validate header->size before using it: it must be at least the header we
    // already read (else rest_size underflows), and the whole NRO image
    // (size + bss, page-aligned) must fit in our heap buffer (else the read()
    // below and the later svcMapProcessCodeMemory() touch out-of-bounds memory).
    size_t min_size = sizeof(NroStart) + sizeof(NroHeader);
    size_t image_size = (size_t)header->size + header->bss_size;
    image_size = (image_size + 0xFFF) & ~0xFFF;
    if (header->size < min_size || image_size > g_heapSize)
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 6));

    size_t rest_size = header->size - min_size;
    if (read(fd, rest, rest_size) != rest_size)
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 7));

    close(fd);
    // Keep SD mounted: loadNro runs again if the loaded NRO returns, and the
    // top-of-function mount is guarded (s_sdmc_mounted), so don't unmount here.

    size_t total_size = header->size + header->bss_size;
    total_size = (total_size+0xFFF) & ~0xFFF;

    rw_size = header->segments[2].size + header->bss_size;
    rw_size = (rw_size+0xFFF) & ~0xFFF;

    int i;
    for (i=0; i<3; i++)
    {
        if (header->segments[i].file_off >= header->size || header->segments[i].size > header->size ||
            (header->segments[i].file_off + header->segments[i].size) > header->size)
        {
            diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 6));
        }
    }

    // (NRO heap-fit + header->size bounds were validated above.)

    // Copy header to elsewhere because we're going to unmap it next.
    memcpy(&g_nroHeader, header, sizeof(g_nroHeader));
    header = &g_nroHeader;

    // Map code memory to a new randomized address
    virtmemLock();
    void* map_addr = virtmemFindCodeMemory(total_size, 0);
    rc = svcMapProcessCodeMemory(g_procHandle, (u64)map_addr, (u64)nrobuf, total_size);
    virtmemUnlock();

    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 18));

    // .text
    rc = svcSetProcessMemoryPermission(
        g_procHandle, (u64)map_addr + header->segments[0].file_off, header->segments[0].size, Perm_R | Perm_X);

    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 19));

    // .rodata
    rc = svcSetProcessMemoryPermission(
        g_procHandle, (u64)map_addr + header->segments[1].file_off, header->segments[1].size, Perm_R);

    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 20));

    // .data + .bss
    rc = svcSetProcessMemoryPermission(
        g_procHandle, (u64)map_addr + header->segments[2].file_off, rw_size, Perm_Rw);

    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 21));

    u64 nro_size = header->segments[2].file_off + rw_size;
    u64 nro_heap_start = ((u64) g_heapAddr) + nro_size;
    u64 nro_heap_size  = g_heapSize + (u64) g_heapAddr - (u64) nro_heap_start;

    #define M EntryFlag_IsMandatory

    static ConfigEntry entries[] = {
        { EntryType_MainThreadHandle,     0, {0, 0} },
        { EntryType_ProcessHandle,        0, {0, 0} },
        { EntryType_AppletType,           0, {AppletType_SystemApplication, 0} },
        { EntryType_OverrideHeap,         M, {0, 0} },
        { EntryType_Argv,                 0, {0, 0} },
        { EntryType_NextLoadPath,         0, {0, 0} },
        { EntryType_LastLoadResult,       0, {0, 0} },
        { EntryType_SyscallAvailableHint, 0, {UINT64_MAX, UINT64_MAX} },
        { EntryType_SyscallAvailableHint2, 0, {UINT64_MAX, 0} },
        { EntryType_RandomSeed,           0, {0, 0} },
        { EntryType_UserIdStorage,        0, {(u64)(uintptr_t)&g_userIdStorage, 0} },
        { EntryType_HosVersion,           0, {0, 0} },
        { EntryType_EndOfList,            0, {(u64)(uintptr_t)g_noticeText, sizeof(g_noticeText)} }
    };

    ConfigEntry *entry_AppletType = &entries[2];
    ConfigEntry *entry_Syscalls   = &entries[7];

    if (g_isApplication) {
        entry_AppletType->Value[0] = AppletType_SystemApplication;
        entry_AppletType->Value[1] = EnvAppletFlags_ApplicationOverride;
    }

    if (!(g_codeMemoryCapability & BIT(0))) {
        // Revoke access to svcCreateCodeMemory if it's not available.
        entry_Syscalls->Value[0x4B/64] &= ~(1UL << (0x4B%64));
    }

    if (!(g_codeMemoryCapability & BIT(1))) {
        // Revoke access to svcControlCodeMemory if it's not available for same-process usage.
        entry_Syscalls->Value[0x4C/64] &= ~(1UL << (0x4C%64)); // svcControlCodeMemory
    }

    // MainThreadHandle
    entries[0].Value[0] = envGetMainThreadHandle();
    // ProcessHandle
    entries[1].Value[0] = g_procHandle;
    // OverrideHeap
    entries[3].Value[0] = nro_heap_start;
    entries[3].Value[1] = nro_heap_size;
    // Argv
    entries[4].Value[1] = (u64)(uintptr_t)&g_argv[0];
    // NextLoadPath
    entries[5].Value[0] = (u64)(uintptr_t)&g_nextNroPath[0];
    entries[5].Value[1] = (u64)(uintptr_t)&g_nextArgv[0];
    // LastLoadResult
    entries[6].Value[0] = g_lastRet;
    // RandomSeed
    entries[9].Value[0] = randomGet64();
    entries[9].Value[1] = randomGet64();
    // HosVersion
    entries[11].Value[0] = hosversionGet();
    entries[11].Value[1] = hosversionIsAtmosphere() ? 0x41544d4f53504852UL : 0; // 'ATMOSPHR'

    g_nroAddr = (u64)map_addr;
    g_nroSize = nro_size;

    svcBreak(BreakReason_NotificationOnlyFlag | BreakReason_PostLoadDll, g_nroAddr, g_nroSize);

    nroEntrypointTrampoline(&entries[0], -1, g_nroAddr);
}

int main(int argc, char **argv)
{
    memcpy(g_savedTls, (u8*)armGetTls() + 0x100, 0x100);

    getIsApplication();
    getIsAutomaticGameplayRecording();
    smExit(); // Close SM as we don't need it anymore.
    setupHbHeap();
    getOwnProcessHandle();
    getCodeMemoryCapability();
    loadNro();

    diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 8));
    return 0;
}
