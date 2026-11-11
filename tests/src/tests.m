unsigned char __src_osax_payload[1];
unsigned int __src_osax_payload_len;
unsigned char __src_osax_loader[1];
unsigned int __src_osax_loader_len;

#include "../../src/manifest.m"
#include <dlfcn.h>

typedef bool (test_function)(void);

#define TEST_SIG(name) bool test_##name(void)

// Legacy test harness macros for reference compatibility
#define TEST_FUNC(name, code) static TEST_SIG(name) { char *test_name = #name; bool result = true; {code} return result; }
#define TEST_CHECK(r, e) if ((r) != (e)) { printf("                   \e[1;33m%s\e[m\e[1;31m#%d %s == %s\e[m \e[1;31m(%d == %d)\e[m\n", test_name, __LINE__, #r, #e, r, e); result = false; }
#define TEST_ENTRY(name) { #name, test_##name },

//
// NOTE: the suite runs inside a logged-in GUI session. Tests may read the real
// displays, spaces and Dock, and only assert what holds on any such session
// (the Dock is on some display, the active space is visible, ...). Nothing a
// test does can change the session: every call yabai uses to move, focus,
// order, hide or spawn is replaced by a fake below, the scripting addition is
// out of reach, and window queries use a window the suite owns.
//

static const char *g_angry_failed_part;

// Angry Tests infrastructure (Yegor Bugayenko rules: verb-named, one assertion, short, explicit message)
//
// NOTE: the body runs in its own function so that ANGRY_ASSERT can stop the
// test on a failure while letting the next assertion run on a success. A test
// passes when its body reaches the end without a failed assertion.
//
#define ANGRY_TEST(name) \
    static void name##_body(bool *angry_ok, const char *angry_name); \
    static bool name(void) { bool ok = true; name##_body(&ok, #name); return ok; } \
    static void name##_body(bool *angry_ok, const char *angry_name)

#define ANGRY_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("\n    \e[1;31m✖ [ASSERTION FAILED]\e[m %s\n" \
                   "      \e[1;33mCondition:\e[m %s\n" \
                   "      \e[1;33mLocation:\e[m  %s:%d (%s)\n", \
                   (msg), #cond, __FILE__, __LINE__, angry_name); \
            if (g_angry_failed_part) printf("      \e[1;33mBroken:\e[m    %s\n", g_angry_failed_part); \
            *angry_ok = false; \
            return; \
        } \
    } while (0)

//
// NOTE: ANGRY_ASSERT_ALL is a single assertion over several conditions, like
// allOf in Hamcrest. The conditions are checked in order, so an earlier one
// can guard a later one, and a failure names the first condition that broke.
//
#define ANGRY_ASSERT_PART(cond) \
    if (!(cond)) { \
        printf("\n    \e[1;31m✖ [ASSERTION FAILED]\e[m %s\n" \
               "      \e[1;33mCondition:\e[m %s\n" \
               "      \e[1;33mOf:\e[m        %s\n" \
               "      \e[1;33mLocation:\e[m  %s:%d (%s)\n", \
               angry_msg, #cond, angry_all, __FILE__, __LINE__, angry_name); \
        *angry_ok = false; \
        return; \
    }

#define ANGRY_FE_1(m, x) m(x)
#define ANGRY_FE_2(m, x, ...) m(x) ANGRY_FE_1(m, __VA_ARGS__)
#define ANGRY_FE_3(m, x, ...) m(x) ANGRY_FE_2(m, __VA_ARGS__)
#define ANGRY_FE_4(m, x, ...) m(x) ANGRY_FE_3(m, __VA_ARGS__)
#define ANGRY_FE_5(m, x, ...) m(x) ANGRY_FE_4(m, __VA_ARGS__)
#define ANGRY_FE_6(m, x, ...) m(x) ANGRY_FE_5(m, __VA_ARGS__)
#define ANGRY_FE_7(m, x, ...) m(x) ANGRY_FE_6(m, __VA_ARGS__)
#define ANGRY_FE_8(m, x, ...) m(x) ANGRY_FE_7(m, __VA_ARGS__)
#define ANGRY_FE_PICK(_1, _2, _3, _4, _5, _6, _7, _8, name, ...) name
#define ANGRY_FOR_EACH(m, ...) \
    ANGRY_FE_PICK(__VA_ARGS__, ANGRY_FE_8, ANGRY_FE_7, ANGRY_FE_6, ANGRY_FE_5, ANGRY_FE_4, ANGRY_FE_3, ANGRY_FE_2, ANGRY_FE_1)(m, __VA_ARGS__)

#define ANGRY_ASSERT_ALL(msg, ...) \
    do { \
        const char *angry_msg = (msg); \
        const char *angry_all = #__VA_ARGS__; \
        ANGRY_FOR_EACH(ANGRY_ASSERT_PART, __VA_ARGS__) \
    } while (0)

//
// NOTE: ANGRY_ALL is the same conjunction as an expression, for a test that
// has to compute its result before cleaning up and asserting. It remembers
// the first condition that broke, and ANGRY_ASSERT prints it on a failure.
//
#define ANGRY_ALL_PART(cond) \
    if (angry_all_ok && !(cond)) { angry_all_ok = false; g_angry_failed_part = #cond; }

#define ANGRY_ALL(...) \
    ({ \
        bool angry_all_ok = true; \
        ANGRY_FOR_EACH(ANGRY_ALL_PART, __VA_ARGS__) \
        angry_all_ok; \
    })

#define ANGRY_ENTRY(func_name) { #func_name, func_name },

//
// NOTE: the Dock never receives a notification during the tests; the fake
// keeps the name of the last one so that tests can check what was asked.
//
static char g_mock_dock_notification[128];
CGError CoreDockSendNotification(CFStringRef notification, int unknown)
{
    CFStringGetCString(notification, g_mock_dock_notification, sizeof(g_mock_dock_notification), kCFStringEncodingUTF8);
    return kCGErrorSuccess;
}

static uid_t g_mock_uid = (uid_t)-1;
uid_t getuid(void)
{
    if (g_mock_uid != (uid_t)-1) return g_mock_uid;
    typedef uid_t (*real_getuid_fn)(void);
    static real_getuid_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_getuid_fn)dlsym(RTLD_NEXT, "getuid");
    return real_fn ? real_fn() : 501;
}

static int g_mock_csr_config = -1;
int csr_get_active_config(uint32_t *config)
{
    if (g_mock_csr_config != -1) {
        *config = (uint32_t)g_mock_csr_config;
        return 0;
    }
    typedef int (*csr_fn)(uint32_t *);
    static csr_fn real_csr = NULL;
    if (!real_csr) real_csr = (csr_fn)dlsym(RTLD_NEXT, "csr_get_active_config");
    return real_csr ? real_csr(config) : 0;
}

//
// NOTE: system() never runs a command during the tests; it only keeps a log
// of what it was asked to run, which tests can check.
//
static char g_mock_system_log[4096];
int system(const char *command)
{
    if (command) {
        size_t used = strlen(g_mock_system_log);
        snprintf(g_mock_system_log + used, sizeof(g_mock_system_log) - used, "%s\n", command);
    }
    return 0;
}

sem_t *sem_open(const char *name, int oflag, ...)
{
    char isolated_name[256];
    const char *target_name = name;
    if (name && strstr(name, "yabai_event_loop_semaphore")) {
        snprintf(isolated_name, sizeof(isolated_name), "yabai_test_sem_%d", getpid());
        target_name = isolated_name;
    }
    typedef sem_t *(*real_sem_open_fn)(const char *, int, ...);
    static real_sem_open_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_sem_open_fn)dlsym(RTLD_NEXT, "sem_open");

    va_list ap;
    va_start(ap, oflag);
    mode_t mode = (mode_t)va_arg(ap, int);
    unsigned int value = va_arg(ap, unsigned int);
    va_end(ap);

    return real_fn ? real_fn(target_name, oflag, mode, value) : SEM_FAILED;
}

int sem_unlink(const char *name)
{
    char isolated_name[256];
    const char *target_name = name;
    if (name && strstr(name, "yabai_event_loop_semaphore")) {
        snprintf(isolated_name, sizeof(isolated_name), "yabai_test_sem_%d", getpid());
        target_name = isolated_name;
    }
    typedef int (*real_sem_unlink_fn)(const char *);
    static real_sem_unlink_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_sem_unlink_fn)dlsym(RTLD_NEXT, "sem_unlink");
    return real_fn ? real_fn(target_name) : -1;
}

static void dummy_event_tap_mach_port_cb(CFMachPortRef port, void *msg, CFIndex size, void *info) {}

CFMachPortRef CGEventTapCreate(CGEventTapLocation tap,
                               CGEventTapPlacement place,
                               CGEventTapOptions options,
                               CGEventMask eventsOfInterest,
                               CGEventTapCallBack callback,
                               void *refcon)
{
    CFMachPortContext ctx = { 0, refcon, NULL, NULL, NULL };
    return CFMachPortCreate(kCFAllocatorDefault, dummy_event_tap_mach_port_cb, &ctx, NULL);
}

bool CGEventTapIsEnabled(CFMachPortRef tap)
{
    return tap != NULL;
}

void CGEventTapEnable(CFMachPortRef tap, bool enable)
{
}

int execvp(const char *file, char *const argv[])
{
    _exit(0);
}

CGError CGSetLocalEventsSuppressionInterval(CFTimeInterval seconds)
{
    return kCGErrorSuccess;
}

CGError CGEnableEventStateCombining(boolean_t doCombineState)
{
    return kCGErrorSuccess;
}

//
// NOTE: synthesized events are never delivered; the fake keeps the first few
// of them so that tests can check which window an event was addressed to.
//
static int g_mock_posted_event_count;
static uint8_t g_mock_posted_events[4][0xf8];
CGError SLPSPostEventRecordTo(ProcessSerialNumber *psn, uint8_t *bytes)
{
    if (g_mock_posted_event_count < 4) memcpy(g_mock_posted_events[g_mock_posted_event_count], bytes, 0xf8);
    ++g_mock_posted_event_count;
    return kCGErrorSuccess;
}

static uint32_t angry_posted_event_window_id(int index)
{
    uint32_t wid;
    memcpy(&wid, g_mock_posted_events[index] + 0x3c, sizeof(uint32_t));
    return wid;
}

static const char *g_mock_home = NULL;
NSString *NSHomeDirectoryForUser(NSString *userName)
{
    if (g_mock_home) return [NSString stringWithUTF8String:g_mock_home];
    typedef NSString *(*real_home_fn)(NSString *);
    static real_home_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_home_fn)dlsym(RTLD_NEXT, "NSHomeDirectoryForUser");
    return real_fn ? real_fn(userName) : @"/tmp";
}

//
// NOTE: no process is ever spawned during the tests, so that a test of the
// service commands can not reach the real launchd; the fake keeps the
// command lines it was given in the same log as system().
//
int posix_spawn(pid_t *restrict pid, const char *restrict path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *restrict attrp,
                char *const argv[restrict], char *const envp[restrict])
{
    size_t used = strlen(g_mock_system_log);
    for (int i = 0; argv && argv[i] && used < sizeof(g_mock_system_log) - 1; ++i) {
        used += snprintf(g_mock_system_log + used, sizeof(g_mock_system_log) - used, i ? " %s" : "%s", argv[i]);
    }
    if (used < sizeof(g_mock_system_log) - 1) g_mock_system_log[used++] = '\n', g_mock_system_log[used] = '\0';
    if (pid) *pid = 0;
    return 0;
}

//
// NOTE: the calls through which yabai changes the session (focus, spaces,
// window order, alpha, level, transforms, the cursor, synthesized events,
// accessibility writes) never reach the system during the tests. The fakes
// succeed, and keep what a test may want to check.
//
static int g_mock_front_process_count;
static uint32_t g_mock_front_process_wid;
CGError _SLPSSetFrontProcessWithOptions(ProcessSerialNumber *psn, uint32_t wid, uint32_t mode)
{
    ++g_mock_front_process_count;
    g_mock_front_process_wid = wid;
    return kCGErrorSuccess;
}

static int g_mock_cursor_warp_count;
static CGPoint g_mock_cursor_warp_point;
CGError CGWarpMouseCursorPosition(CGPoint point)
{
    ++g_mock_cursor_warp_count;
    g_mock_cursor_warp_point = point;
    return kCGErrorSuccess;
}

static int g_mock_event_post_count;
void CGEventPost(CGEventTapLocation tap, CGEventRef event)
{
    ++g_mock_event_post_count;
}

static uint64_t g_mock_current_space_set;
CGError SLSManagedDisplaySetCurrentSpace(int cid, CFStringRef uuid, uint64_t sid)
{
    g_mock_current_space_set = sid;
    return kCGErrorSuccess;
}

static uint64_t g_mock_windows_moved_to_space;
void SLSMoveWindowsToManagedSpace(int cid, CFArrayRef window_list, uint64_t sid)
{
    g_mock_windows_moved_to_space = sid;
}

static int g_mock_bridged_operation_count;
static int64_t tests_bridged_window_management_operation(void *operation)
{
    ++g_mock_bridged_operation_count;
    return 0;
}

CGError SLSSetWindowAlpha(int cid, uint32_t wid, float alpha) { return kCGErrorSuccess; }
CGError SLSOrderWindow(int cid, uint32_t wid, int mode, uint32_t rel_wid) { return kCGErrorSuccess; }
CGError SLSSetWindowLevel(int cid, uint32_t wid, int level) { return kCGErrorSuccess; }
CGError SLSSetMenuBarInsetAndAlpha(int cid, double unused1, double unused2, float alpha) { return kCGErrorSuccess; }
CGError SLSSpaceSetFrontPSN(int cid, uint64_t sid, ProcessSerialNumber psn) { return kCGErrorSuccess; }
CGError SLSWindowSetShadowProperties(uint32_t wid, CFDictionaryRef options) { return kCGErrorSuccess; }
CGError SLSSetWindowTransform(int cid, uint32_t wid, CGAffineTransform t) { return kCGErrorSuccess; }
CGError SLSDisableUpdate(int cid) { return kCGErrorSuccess; }
CGError SLSReenableUpdate(int cid) { return kCGErrorSuccess; }
CGError SLSTransactionCommit(CFTypeRef transaction, int synchronous) { return kCGErrorSuccess; }

static int g_mock_ax_write_count;
static CGPoint g_mock_ax_position;
static CGSize g_mock_ax_size;
static char g_mock_ax_bool_attribute[64];
static bool g_mock_ax_bool_value;
AXError AXUIElementSetAttributeValue(AXUIElementRef element, CFStringRef attribute, CFTypeRef value)
{
    if (!element || CFGetTypeID(element) != AXUIElementGetTypeID()) return kAXErrorIllegalArgument;
    ++g_mock_ax_write_count;
    if (value && CFGetTypeID(value) == AXValueGetTypeID()) {
        if (AXValueGetType(value) == kAXValueTypeCGPoint) AXValueGetValue(value, kAXValueTypeCGPoint, &g_mock_ax_position);
        if (AXValueGetType(value) == kAXValueTypeCGSize)  AXValueGetValue(value, kAXValueTypeCGSize, &g_mock_ax_size);
    } else if (value && CFGetTypeID(value) == CFBooleanGetTypeID()) {
        CFStringGetCString(attribute, g_mock_ax_bool_attribute, sizeof(g_mock_ax_bool_attribute), kCFStringEncodingUTF8);
        g_mock_ax_bool_value = CFBooleanGetValue(value);
    }
    return kAXErrorSuccess;
}

AXError AXUIElementPerformAction(AXUIElementRef element, CFStringRef action)
{
    if (!element || CFGetTypeID(element) != AXUIElementGetTypeID()) return kAXErrorIllegalArgument;
    ++g_mock_ax_write_count;
    return kAXErrorSuccess;
}

static bool g_mock_fast_usleep = false;
int usleep(useconds_t usec)
{
    if (g_mock_fast_usleep) return 0;
    typedef int (*real_fn)(useconds_t);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "usleep");
    return r ? r(usec) : 0;
}

static uint64_t g_mock_active_space = 0;
uint64_t SLSGetActiveSpace(int cid)
{
    if (g_mock_active_space) return g_mock_active_space;
    typedef uint64_t (*real_fn)(int);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "SLSGetActiveSpace");
    return r ? r(cid) : 1;
}

uint64_t SLSManagedDisplayGetCurrentSpace(int cid, CFStringRef uuid)
{
    if (g_mock_active_space) return g_mock_active_space;
    typedef uint64_t (*real_fn)(int, CFStringRef);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "SLSManagedDisplayGetCurrentSpace");
    return r ? r(cid, uuid) : 0;
}

//
// NOTE: a test can put single windows on spaces of its own; any other window
// is on the fake active space when there is one, and where WindowServer says
// otherwise.
//
static struct { uint32_t wid; uint64_t sid; } g_mock_window_space[8];
static int g_mock_window_space_count;

static void angry_place_window(uint32_t wid, uint64_t sid)
{
    g_mock_window_space[g_mock_window_space_count++] = (typeof(g_mock_window_space[0])) { wid, sid };
}

CFArrayRef SLSCopySpacesForWindows(int cid, int selector, CFArrayRef window_list)
{
    if (CFArrayGetCount(window_list) == 1) {
        uint32_t wid = 0;
        CFNumberGetValue(CFArrayGetValueAtIndex(window_list, 0), kCFNumberSInt32Type, &wid);
        for (int i = 0; i < g_mock_window_space_count; ++i) {
            if (g_mock_window_space[i].wid != wid) continue;
            CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt64Type, &g_mock_window_space[i].sid);
            CFArrayRef arr = CFArrayCreate(NULL, (const void **)&num, 1, &kCFTypeArrayCallBacks);
            CFRelease(num);
            return arr;
        }
    }
    if (g_mock_active_space) {
        CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt64Type, &g_mock_active_space);
        CFArrayRef arr = CFArrayCreate(NULL, (const void **)&num, 1, &kCFTypeArrayCallBacks);
        CFRelease(num);
        return arr;
    }
    typedef CFArrayRef (*real_fn)(int, int, CFArrayRef);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "SLSCopySpacesForWindows");
    return r ? r(cid, selector, window_list) : NULL;
}

//
// NOTE: the spaces tests make up (ANGRY_PRIVATE_SID and friends, 0x7E57xxxx)
// are user spaces of the main display, so that switching between them never
// looks like moving to another display.
//
CFStringRef SLSCopyManagedDisplayForSpace(int cid, uint64_t sid)
{
    if ((sid >> 16) == 0x7E57) {
        CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(CGMainDisplayID());
        CFStringRef result = CFUUIDCreateString(NULL, uuid);
        CFRelease(uuid);
        return result;
    }
    typedef CFStringRef (*real_fn)(int, uint64_t);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "SLSCopyManagedDisplayForSpace");
    return r ? r(cid, sid) : NULL;
}

//
// NOTE: a test can make its own process the front process, so that code which
// asks WindowServer for the front application finds one the test registered.
//
static bool g_mock_front_is_test_process;
OSStatus _SLPSGetFrontProcess(ProcessSerialNumber *psn)
{
    if (g_mock_front_is_test_process) return GetCurrentProcess(psn);
    typedef OSStatus (*real_fn)(ProcessSerialNumber *);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "_SLPSGetFrontProcess");
    return r ? r(psn) : -1;
}

int SLSSpaceGetType(int cid, uint64_t sid)
{
    if ((sid >> 16) == 0x7E57) return 0;
    typedef int (*real_fn)(int, uint64_t);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "SLSSpaceGetType");
    return r ? r(cid, sid) : 0;
}

static int g_mock_mouse_event_count;
CGError CGPostMouseEvent(CGPoint position, boolean_t update, CGButtonCount count, boolean_t down, ...)
{
    ++g_mock_mouse_event_count;
    return kCGErrorSuccess;
}

static bool g_mock_dispatch_after = false;
void dispatch_after(dispatch_time_t when, dispatch_queue_t queue, dispatch_block_t block)
{
    if (g_mock_dispatch_after) {
        block();
        return;
    }
    typedef void (*real_dispatch_after_fn)(dispatch_time_t, dispatch_queue_t, dispatch_block_t);
    static real_dispatch_after_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_dispatch_after_fn)dlsym(RTLD_NEXT, "dispatch_after");
    if (real_fn) real_fn(when, queue, block);
}

//
// NOTE: no accessibility observer is ever registered with another process
// during the tests; registration succeeds unless a test asks it to fail.
//
static bool g_mock_ax_cannot_complete = false;
AXError AXObserverAddNotification(AXObserverRef observer, AXUIElementRef element, CFStringRef notification, void *refcon)
{
    return g_mock_ax_cannot_complete ? kAXErrorCannotComplete : kAXErrorSuccess;
}

AXError AXObserverRemoveNotification(AXObserverRef observer, AXUIElementRef element, CFStringRef notification)
{
    return kAXErrorSuccess;
}

static bool g_mock_dock_window_layer18 = false;
CFArrayRef CGWindowListCopyWindowInfo(CGWindowListOption option, CGWindowID relativeToWindow)
{
    if (g_mock_dock_window_layer18) {
        int layer18 = 18;
        CFNumberRef layer_num = CFNumberCreate(NULL, kCFNumberIntType, &layer18);
        CFTypeRef keys[] = { (const void *)kCGWindowOwnerName, (const void *)kCGWindowLayer };
        CFTypeRef values[] = { (const void *)CFSTR("Dock"), (const void *)layer_num };
        CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFArrayRef arr = CFArrayCreate(NULL, (const void **)&dict, 1, &kCFTypeArrayCallBacks);
        CFRelease(layer_num);
        CFRelease(dict);
        return arr;
    }
    typedef CFArrayRef (*real_cg_fn)(CGWindowListOption, CGWindowID);
    static real_cg_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_cg_fn)dlsym(RTLD_NEXT, "CGWindowListCopyWindowInfo");
    return real_fn ? real_fn(option, relativeToWindow) : NULL;
}

@implementation NSRunningApplication (AngryEventLoopMock)
- (BOOL)mock_isFinishedLaunching { return YES; }
- (NSApplicationActivationPolicy)mock_activationPolicy { return NSApplicationActivationPolicyRegular; }
@end

//
// NOTE: the suite runs inside the user's session, and the scripting addition acts on every window in it.
// window_manager_scratchpad_recover_windows alone orders in every window of every application, minimized
// ones included, which leaves them drawn on their space while the Dock still lists them as minimized.
// No test may reach the real Dock, so the socket points nowhere for the whole run.
//
//
// NOTE: a child that returns from the code under test, or exits through
// exit() with the parent's unflushed stdout, would replay output or keep
// running the rest of the suite. SIGCHLD is set back to the default because
// configure_settings_and_acquire_lock ignores it, which makes waitpid fail.
//
//
// NOTE: the event loop thread never runs during the tests, so whatever a
// handler posts stays at the tail of the queue for the test to look at.
//
static struct event *angry_last_posted_event(void)
{
    return g_event_loop.tail;
}

//
// NOTE: a window the test process owns, so that window queries have a known
// answer instead of reading whatever window the user has on screen. It sits
// in the corner of the main display, fully transparent and ignoring the mouse,
// and the fakes above keep every test from moving or changing it.
//
#define ANGRY_TEST_WINDOW_TITLE "angry-test-window"

static uint32_t angry_test_window(void)
{
    static NSWindow *window;
    if (!window) {
        [NSApplication sharedApplication];
        NSRect frame = NSMakeRect(NSMinX([[NSScreen mainScreen] frame]), NSMinY([[NSScreen mainScreen] frame]), 4, 4);
        window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
        [window setTitle:@ANGRY_TEST_WINDOW_TITLE];
        [window setAlphaValue:0.0];
        [window setIgnoresMouseEvents:YES];
        [window setReleasedWhenClosed:NO];
        [window orderFrontRegardless];
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
    }
    return (uint32_t)[window windowNumber];
}

//
// NOTE: a scripting addition a test stands up for the one request it expects.
// It answers a single connection on the test socket, the way the real one
// does, and keeps the payload: int16 length, opcode, then the packed values.
// It gives up after a second, so a request that never comes fails the test
// instead of hanging it.
//
struct angry_sa_server
{
    int fd;
    pthread_t thread;
    char payload[1024];
    int length;
};

static void *angry_sa_server_run(void *context)
{
    struct angry_sa_server *server = context;
    struct pollfd pfd = { .fd = server->fd, .events = POLLIN };
    if (poll(&pfd, 1, 1000) != 1) return NULL;
    int client = accept(server->fd, NULL, NULL);
    if (client == -1) return NULL;
    server->length = (int)recv(client, server->payload, sizeof(server->payload), 0);
    char done = 1;
    send(client, &done, 1, 0);
    close(client);
    return NULL;
}

static void angry_sa_server_begin(struct angry_sa_server *server)
{
    memset(server, 0, sizeof(*server));
    unlink(g_sa_socket_file);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", g_sa_socket_file);
    server->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(server->fd, (struct sockaddr *) &addr, sizeof(addr));
    listen(server->fd, 1);
    pthread_create(&server->thread, NULL, angry_sa_server_run, server);
}

static void angry_sa_server_end(struct angry_sa_server *server)
{
    pthread_join(server->thread, NULL);
    close(server->fd);
    unlink(g_sa_socket_file);
}

static uint8_t angry_sa_opcode(struct angry_sa_server *server)
{
    return server->length > 2 ? (uint8_t) server->payload[2] : 0;
}

static void angry_sa_unpack(struct angry_sa_server *server, int offset, void *value, size_t size)
{
    memcpy(value, server->payload + 3 + offset, size);
}

static pid_t angry_fork(void)
{
    signal(SIGCHLD, SIG_DFL);
    fflush(NULL);
    return fork();
}

#define TESTS_SA_SOCKET_FILE "/tmp/yabai-tests-no-sa.socket"

static void tests_isolate_scripting_addition(void)
{
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", TESTS_SA_SOCKET_FILE);
}

#include "area.c"
#include "structures.c"
#include "angry_tests.c"
#include "angry_misc_tests.c"
#include "angry_rule_tests.c"
#include "angry_event_serialize_tests.c"
#include "angry_signal_tests.c"
#include "angry_application_tests.c"
#include "angry_mouse_tests.c"
#include "angry_process_tests.c"
#include "angry_mission_control_tests.c"
#include "angry_display_space_tests.c"
#include "angry_view_tests.c"
#include "angry_window_tests.c"
#include "angry_display_manager_tests.c"
#include "angry_space_manager_tests.c"
#include "angry_message_tests.c"
#include "angry_layout_store_tests.m"
#include "angry_workspace_tests.m"
#include "angry_sa_tests.m"
#include "angry_event_loop_tests.c"
#include "angry_yabai_tests.c"
#include "angry_window_manager_tests.c"

#define TEST_LIST                                                               \
    ANGRY_ENTRY(allocatesChunkInEmptyMemoryPool)                                \
    ANGRY_ENTRY(preservesStoredPointerAcrossThousandAllocations)                 \
    ANGRY_ENTRY(wrapsMemoryPoolPointerToOriginOnOverflow)                       \
    ANGRY_ENTRY(rehashesTableWithoutDataLossUnderHighLoad)                      \
    ANGRY_ENTRY(removesKeyAndCleansEntryFromTable)                              \
    ANGRY_ENTRY(preservesUnremovedKeysAfterPartialDeletion)                     \
    ANGRY_ENTRY(ignoresDuplicateAddWhenKeyAlreadyExists)                       \
    ANGRY_ENTRY(identifiesWestDisplayAcrossNegativeCoordinates)                 \
    ANGRY_ENTRY(rejectsEastDisplayWhenTargetIsPositionedWest)                   \
    ANGRY_ENTRY(identifiesEastDisplayAcrossPositiveCoordinates)                 \
    ANGRY_ENTRY(rejectsWestDisplayWhenTargetIsPositionedEast)                   \
    ANGRY_ENTRY(findsNearestWestDisplayAmongMultipleMonitors)                   \
    ANGRY_ENTRY(findsNearestEastDisplayAcrossIrregularGaps)                     \
    ANGRY_ENTRY(returnsNegativeIndexWhenNoDisplayInDirection)                   \
    ANGRY_ENTRY(splitsParentAreaEquallyWithSpecifiedGapOnVerticalAxis)          \
    ANGRY_ENTRY(splitsParentAreaAsymmetricallyOnHorizontalAxis)                  \
    ANGRY_ENTRY(preservesTotalDimensionWithinPixelRoundingTolerance)            \
    ANGRY_ENTRY(allocatesMinimumDimensionWhenSplitRatioIsExtremelySmall)         \
    ANGRY_ENTRY(allocatesMinimumDimensionWhenSplitRatioIsExtremelyLarge)         \
    ANGRY_ENTRY(ignoresDropWhenCursorDeltaIsBelowThreshold)                     \
    ANGRY_ENTRY(permitsDropWhenCursorDeltaCrossesThreshold)                     \
    ANGRY_ENTRY(preservesWindowWhenExitingToMatchingSpace)                      \
    ANGRY_ENTRY(guardsAgainstPrematureUntileDuringFullscreenSpaceDestruction)   \
    ANGRY_ENTRY(swapsWindowIdsBetweenTwoBspNodesPreservingTreeStructure)        \
    ANGRY_ENTRY(mirrorsBspTreeHorizontallyInvertingChildOrder)                  \
    ANGRY_ENTRY(mirrorsBspTreeVerticallyInvertingChildOrder)                    \
    ANGRY_ENTRY(promotesSiblingToRootWhenRemovingLeafFromTwoNodeTree)           \
    ANGRY_ENTRY(requiresAllAttributesForLegacyMacOsVersions)                    \
    ANGRY_ENTRY(requiresReducedAttributesForTahoeAndGoldengate)                 \
    ANGRY_ENTRY(verifiesReducedAttributeMaskMatchesSubset)                      \
    ANGRY_ENTRY(defaultsWindowFocusAnimationToDisabled)                         \
    ANGRY_ENTRY(recordsPendingStateOnVerticalGestureBegin)                      \
    ANGRY_ENTRY(updatesLastGestureTimeOnVerticalGestureEnd)                     \
    ANGRY_ENTRY(capturesFrontmostWindowAndSpaceStateOnMissionControlEnter)      \
    ANGRY_ENTRY(maintainsIdempotencyWhenMissionControlStateAlreadyRecorded)     \
    ANGRY_ENTRY(coalescesConsecutiveMouseDraggedEventsInQueue)                  \
    ANGRY_ENTRY(keepsWaitingEventsWhenEventQueueIsFull) \
    ANGRY_ENTRY(clearsDirtyFlagOnDirectViewFlush)                               \
    ANGRY_ENTRY(defaultsBspLiveResizeToEnabled)                                 \
    ANGRY_ENTRY(coercesInsertDirectionToStackWhenTargetViewIsStacked)           \
    ANGRY_ENTRY(updatesFocusFollowsMouseMode)                                   \
    ANGRY_ENTRY(suppressesRapidGestureToSameSpace)                              \
    ANGRY_ENTRY(rateLimitsRapidGesturesUnderThreshold)                         \
    ANGRY_ENTRY(resolvesActiveSpaceForRunningApplications)                      \
    ANGRY_ENTRY(balancesBspTreeLeafRatiosEquallyForTwoLeaves)                   \
    ANGRY_ENTRY(balancesBspTreeLeafRatiosProportionallyForAsymmetricSubtrees)   \
    ANGRY_ENTRY(rotatesBspNodeNinetyDegreesTogglingSplitAxis)                   \
    ANGRY_ENTRY(rotatesBspNodeOneEightyDegreesInvertingRatioAndChildren)        \
    ANGRY_ENTRY(rotatesBspNodeTwoSeventyDegreesTogglingSplitAxis)               \
    ANGRY_ENTRY(findsNorthFenceAncestorWhenBoundedVertically)                   \
    ANGRY_ENTRY(returnsNullFenceWhenNodeLiesOnDisplayEdge)                      \
    ANGRY_ENTRY(stacksWindowOnLeafNodePlacingItAtFrontOfOrder)                  \
    ANGRY_ENTRY(combinesRuleEffectsOverridingManageProperty)                    \
    ANGRY_ENTRY(combinesRuleEffectsClampingOpacityToUnitInterval)               \
    ANGRY_ENTRY(combinesRuleEffectsPreservingScratchpadIdentifier)              \
    ANGRY_ENTRY(parsesRgbaColorFromHexWithAlphaChannel)                         \
    ANGRY_ENTRY(clampsFloatValueWithinSpecifiedBoundary)                        \
    ANGRY_ENTRY(clampsNegativeFloatValueToMinimumBoundary)                      \
    ANGRY_ENTRY(identifiesPointInsideBoundingBox)                               \
    ANGRY_ENTRY(rejectsPointOutsideBoundingBox)                                 \
    ANGRY_ENTRY(identifiesPointInsideTriangle)                                  \
    ANGRY_ENTRY(rejectsPointOutsideTriangle)                                    \
    ANGRY_ENTRY(escapesSpecialCharactersForJsonSerialization)                   \
    ANGRY_ENTRY(returnsNullWhenStringRequiresNoEscaping)                        \
    ANGRY_ENTRY(matchesValidRegularExpression)                                  \
    ANGRY_ENTRY(rejectsMismatchedRegularExpression)                             \
    ANGRY_ENTRY(allocatesAndPushesElementsIntoStretchBuffer)                    \
    ANGRY_ENTRY(deletesElementFromStretchBufferDecreasingLength)                 \
    ANGRY_ENTRY(allocatesAndPushesElementsIntoTemporaryStretchBuffer)           \
    ANGRY_ENTRY(deletesElementFromTemporaryStretchBufferDecreasingLength)        \
    ANGRY_ENTRY(allocatesAlignedMemoryAcrossSpecificByteBoundaries)              \
    ANGRY_ENTRY(expandsTemporaryStorageBufferInPlace)                           \
    ANGRY_ENTRY(shrinksTemporaryStorageBufferAdjustingUsedCounter)              \
    ANGRY_ENTRY(resetsTemporaryStorageUsedCounterToZero)                        \
    ANGRY_ENTRY(readsNonZeroMonotonicCpuTimerTicks)                             \
    ANGRY_ENTRY(readsPositiveCpuFrequencyValue)                                 \
    ANGRY_ENTRY(recordsProfilerAnchorsDuringExecution)                          \
    ANGRY_ENTRY(suppressesDebugOutputWhenVerboseIsDisabled)                     \
    ANGRY_ENTRY(locatesExportedSymbolFromDynamicLibrary)                        \
    ANGRY_ENTRY(returnsNullWhenSearchingForNonexistentSymbol)                   \
    ANGRY_ENTRY(returnsNullWhenSearchingInNonexistentLibrary)                   \
    ANGRY_ENTRY(populatesCurrentUserLaunchdPlistPath)                           \
    ANGRY_ENTRY(populatesLegacyCurrentUserLaunchdPlistPath)                     \
    ANGRY_ENTRY(executesSimpleBinaryViaSafeExecReturningZero)                   \
    ANGRY_ENTRY(initializesNotificationCenterDelegateAndIcon)                   \
    ANGRY_ENTRY(computesEaseInSineBoundaryValues)                               \
    ANGRY_ENTRY(computesEaseOutSineBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseInOutSineBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInQuadBoundaryValues)                               \
    ANGRY_ENTRY(computesEaseOutQuadBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseInOutQuadBoundaryValues)                            \
    ANGRY_ENTRY(computesEaseInCubicBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseOutCubicBoundaryValues)                             \
    ANGRY_ENTRY(computesEaseInOutCubicBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInQuartBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseOutQuartBoundaryValues)                             \
    ANGRY_ENTRY(computesEaseInOutQuartBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInQuintBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseOutQuintBoundaryValues)                             \
    ANGRY_ENTRY(computesEaseInOutQuintBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInExpoBoundaryValues)                               \
    ANGRY_ENTRY(computesEaseOutExpoBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseInOutExpoBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInCircBoundaryValues)                               \
    ANGRY_ENTRY(computesEaseOutCircBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseInOutCircBoundaryValues)                            \
    ANGRY_ENTRY(formatsOptionalBoolJsonValues)                                  \
    ANGRY_ENTRY(formatsBooleanJsonValues)                                       \
    ANGRY_ENTRY(determinesRootPrivilegesAccurately)                             \
    ANGRY_ENTRY(copiesCfStringToHeapAllocatedCString)                           \
    ANGRY_ENTRY(copiesHeapStringAccurately)                                     \
    ANGRY_ENTRY(clampsCornerRadiiToHalfDimensionBoundary)                       \
    ANGRY_ENTRY(verifiesDirectoryExistenceAccurately)                           \
    ANGRY_ENTRY(verifiesFileExistenceAccurately)                                \
    ANGRY_ENTRY(verifiesExecutablePermissionsAccurately)                       \
    ANGRY_ENTRY(readsOperatingSystemFrequency)                                  \
    ANGRY_ENTRY(convertsInt32ToCfString)                                        \
    ANGRY_ENTRY(resolvesConfigurationFileUnderXdgConfigHome)                    \
    ANGRY_ENTRY(executesConfigurationFileSafelyWhenMissing)                      \
    ANGRY_ENTRY(resolvesPidFromAccessibilityElement)                            \
    ANGRY_ENTRY(reportsNoEnhancedUserInterfaceForTestProcess)                   \
    ANGRY_ENTRY(restoresAlphaChannelInQuartzImage)                              \
    ANGRY_ENTRY(formatsDebugMessageIteratingTokens)                             \
    ANGRY_ENTRY(terminatesWithExitFailureOnError)                               \
    ANGRY_ENTRY(terminatesWithExitSuccessOnRequire)                             \
    ANGRY_ENTRY(allowsUserNotificationPresentation)                             \
    ANGRY_ENTRY(profilesExecutionTimerAndPrintsResults)                         \
    ANGRY_ENTRY(populatesLaunchdServicePlist)                                   \
    ANGRY_ENTRY(ensuresDirectoryPathExistsBeforeWriting)                        \
    ANGRY_ENTRY(installsServiceInternalToFile)                                  \
    ANGRY_ENTRY(removesLegacyServicePlist)                                      \
    ANGRY_ENTRY(installsLaunchdServiceToMockHomeDirectory)                      \
    ANGRY_ENTRY(uninstallsLaunchdServiceFromMockHomeDirectory)                    \
    ANGRY_ENTRY(startsLaunchdServiceInMockEnvironment)                          \
    ANGRY_ENTRY(restartsLaunchdServiceInMockEnvironment)                         \
    ANGRY_ENTRY(stopsLaunchdServiceInMockEnvironment)                           \
    ANGRY_ENTRY(serializesRuleConfigurationWithLabelAndEscapedStringsToJsonStream) \
    ANGRY_ENTRY(serializesRuleConfigurationWithoutOptionalStringsUsingEmptyDefaults) \
    ANGRY_ENTRY(copiesDisplayIdentifierAndFollowSpaceWhenFollowSpaceFlagSet)   \
    ANGRY_ENTRY(clearsFollowSpaceFlagWhenSourceDisplaySpecifiedWithoutFollowSpaceFlag) \
    ANGRY_ENTRY(copiesSpaceIdentifierAndFollowSpaceWhenFollowSpaceFlagSet)     \
    ANGRY_ENTRY(clearsFollowSpaceFlagWhenSourceSpaceSpecifiedWithoutFollowSpaceFlag) \
    ANGRY_ENTRY(ignoresOpacityOutsideUnitIntervalDuringCombination)             \
    ANGRY_ENTRY(copiesSublayerWhenRuleLayerFlagSet)                             \
    ANGRY_ENTRY(replacesScratchpadStringFreeingPreexistingValue)                \
    ANGRY_ENTRY(preservesDestinationPropertyWhenSourcePropertyIsUndecided)     \
    ANGRY_ENTRY(appliesGridDimensionsWhenBothRowAndColumnAreNonZero)            \
    ANGRY_ENTRY(ignoresGridDimensionsWhenRowOrColumnIsZero)                     \
    ANGRY_ENTRY(addsRuleToWindowManagerRulesBuffer)                             \
    ANGRY_ENTRY(replacesExistingRuleWhenAddingRuleWithIdenticalLabel)           \
    ANGRY_ENTRY(removesRuleByIndexFromWindowManagerBuffer)                      \
    ANGRY_ENTRY(keepsRegistrationOrderOfRulesAfterRemovingFirstRule) \
    ANGRY_ENTRY(returnsFalseWhenRemovingRuleByInvalidIndex)                     \
    ANGRY_ENTRY(removesRuleByLabelFromWindowManagerBuffer)                      \
    ANGRY_ENTRY(returnsFalseWhenRemovingRuleByNonexistentLabel)                 \
    ANGRY_ENTRY(skipsOneShotRuleWhenReapplyingByIndex)                          \
    ANGRY_ENTRY(returnsFalseWhenReapplyingByNonexistentIndex)                    \
    ANGRY_ENTRY(skipsOneShotRuleWhenReapplyingByLabel)                          \
    ANGRY_ENTRY(returnsFalseWhenReapplyingByNonexistentLabel)                    \
    ANGRY_ENTRY(combinesStickyMffAndFullscreenProperties)                       \
    ANGRY_ENTRY(reappliesRuleByIndexIteratingActiveWindows)                     \
    ANGRY_ENTRY(reappliesRuleByLabelIteratingActiveWindows)                     \
    ANGRY_ENTRY(clearsRuleFlagFromRuleBitmask)                                  \
    ANGRY_ENTRY(setsRuleFlagInRuleBitmask)                                      \
    ANGRY_ENTRY(clearsRuleEffectsFlagFromEffectsBitmask)                         \
    ANGRY_ENTRY(augmentsDockSwipeEventWithRawIoHidPayload)                      \
    ANGRY_ENTRY(augmentsDockSwipeEndedEventWithVelocityPayload)                 \
    ANGRY_ENTRY(replacesPreexistingPayloadWhenAugmentingAlreadyAugmentedEvent)  \
    ANGRY_ENTRY(returnsNullWhenAugmentingNullEvent)                             \
    ANGRY_ENTRY(augmentsBareEventDefaultingMissingFieldsToZero)                 \
    ANGRY_ENTRY(rejectsTruncatedEventDataUnderFourBytes)                        \
    ANGRY_ENTRY(rejectsUnsupportedEventDataVersionNumber)                       \
    ANGRY_ENTRY(rejectsTruncatedFieldHeaderInEventData)                         \
    ANGRY_ENTRY(clampsExtremePositiveAndNegativeFixedPointValues)               \
    ANGRY_ENTRY(resolvesSignalTypeFromStringAccurately)                         \
    ANGRY_ENTRY(resolvesSignalTypeForDockDidChangePref)                         \
    ANGRY_ENTRY(returnsUnknownSignalTypeForInvalidString)                       \
    ANGRY_ENTRY(addsSignalToSignalEventBuffer)                                 \
    ANGRY_ENTRY(replacesSignalWhenAddingWithDuplicateLabel)                     \
    ANGRY_ENTRY(removesSignalByIndexAcrossGlobalSignalTypes)                    \
    ANGRY_ENTRY(returnsFalseWhenRemovingSignalByInvalidIndex)                   \
    ANGRY_ENTRY(removesSignalByLabelFromSignalEventBuffer)                      \
    ANGRY_ENTRY(returnsFalseWhenRemovingSignalByNonexistentLabel)               \
    ANGRY_ENTRY(serializesSignalListToJsonStream)                               \
    ANGRY_ENTRY(serializesEmptySignalListAsEmptyJsonArray)                      \
    ANGRY_ENTRY(filtersSignalWhenApplicationRegexDoesNotMatch)                  \
    ANGRY_ENTRY(permitsSignalWhenApplicationRegexMatches)                       \
    ANGRY_ENTRY(filtersSignalWhenActivePropertyMismatches)                      \
    ANGRY_ENTRY(pushesApplicationSignalIntoStorageWhenSubscriberExists)         \
    ANGRY_ENTRY(skipsSignalPushWhenNoSubscribersExist)                          \
    ANGRY_ENTRY(pushesDisplaySignalIntoStorageWhenSubscriberExists)              \
    ANGRY_ENTRY(pushesSpaceSignalIntoStorageWhenSubscriberExists)                \
    ANGRY_ENTRY(pushesMissionControlSignalIntoStorageWhenSubscriberExists)       \
    ANGRY_ENTRY(pushesSystemWokeSignalIntoStorageWhenSubscriberExists)          \
    ANGRY_ENTRY(pushesApplicationTerminatedSignalIntoStorage)                   \
    ANGRY_ENTRY(pushesApplicationFrontSwitchedSignalIntoStorage)                 \
    ANGRY_ENTRY(pushesApplicationHiddenSignalIntoStorage)                        \
    ANGRY_ENTRY(pushesWindowDestroyedSignalIntoStorage)                         \
    ANGRY_ENTRY(pushesSpaceCreatedAndDestroyedSignalsIntoStorage)               \
    ANGRY_ENTRY(pushesDisplayRemovedAndChangedSignalsIntoStorage)               \
    ANGRY_ENTRY(pushesMissionControlExitSignalIntoStorage)                      \
    ANGRY_ENTRY(flushesSignalStorageImmediatelyWhenEmpty)                       \
    ANGRY_ENTRY(createsApplicationInstanceFromCurrentProcess)                   \
    ANGRY_ENTRY(reportsTestProcessAsNotFrontmost)                               \
    ANGRY_ENTRY(reportsTestProcessAsNotHidden)                                  \
    ANGRY_ENTRY(listsNoWindowsForApplicationWithoutWindows)                     \
    ANGRY_ENTRY(reportsNoMainWindowForApplicationWithoutWindows)                \
    ANGRY_ENTRY(reportsNoFocusedWindowForApplicationWithoutWindows)             \
    ANGRY_ENTRY(observesEveryNotificationOfApplication)                         \
    ANGRY_ENTRY(stopsObservingApplicationOnUnobserve)                           \
    ANGRY_ENTRY(postsWindowCreatedEventForCreatedNotification)                  \
    ANGRY_ENTRY(postsWindowFocusedEventForFocusedWindowChange)                  \
    ANGRY_ENTRY(postsWindowMovedEventForMovedNotification)                      \
    ANGRY_ENTRY(postsWindowResizedEventForResizedNotification)                  \
    ANGRY_ENTRY(postsWindowTitleChangedEventForTitleNotification)               \
    ANGRY_ENTRY(postsMenuOpenedEventCarryingApplicationPid)                     \
    ANGRY_ENTRY(postsMenuClosedEventCarryingApplicationPid)                     \
    ANGRY_ENTRY(postsWindowMinimizedEventCarryingContext)                       \
    ANGRY_ENTRY(postsWindowDeminimizedEventCarryingContext)                     \
    ANGRY_ENTRY(dispatchesNotificationHandlerForWindowDestroyedNotification)    \
    ANGRY_ENTRY(initializesMouseStateWithDefaultParameters)                     \
    ANGRY_ENTRY(parsesMouseModifierFromCommandAndAltFlags)                      \
    ANGRY_ENTRY(parsesMouseModifierFromShiftAndControlFlags)                    \
    ANGRY_ENTRY(populatesMouseWindowInfoCalculatingDeltasAndFlags)              \
    ANGRY_ENTRY(treatsReleaseNearGestureOriginAsStationaryClick)                \
    ANGRY_ENTRY(treatsReleaseFarFromGestureOriginAsDrag)                        \
    ANGRY_ENTRY(treatsResizedGestureAsMovedWhenCursorReturnsToOrigin)           \
    ANGRY_ENTRY(determinesStackDropActionWhenCursorInCenterAndModeIsStack)       \
    ANGRY_ENTRY(determinesSwapDropActionWhenCursorInCenterAndModeIsSwap)        \
    ANGRY_ENTRY(determinesWarpTopDropActionWhenCursorInTopTriangle)             \
    ANGRY_ENTRY(determinesWarpRightDropActionWhenCursorInRightTriangle)         \
    ANGRY_ENTRY(determinesWarpBottomDropActionWhenCursorInBottomTriangle)       \
    ANGRY_ENTRY(determinesWarpLeftDropActionWhenCursorInLeftTriangle)           \
    ANGRY_ENTRY(returnsDropActionNoneWhenCursorIsOutsideWindowBounds)           \
    ANGRY_ENTRY(swapsWindowsBetweenNodesUpdatingInsertionPoints)                \
    ANGRY_ENTRY(consumesMouseClickWhenModifiersMatchAndFreesOnRelease)          \
    ANGRY_ENTRY(stacksWindowIntoDestinationNodeOnMouseDropActionStack)          \
    ANGRY_ENTRY(warpsWindowBetweenSiblingsWithMatchingSplitSwappingNodes)       \
    ANGRY_ENTRY(handlesMouseRightMouseDownConsumingWhenModifierMatches)         \
    ANGRY_ENTRY(handlesMouseDraggedEventSettingDragDetectedFlag)                \
    ANGRY_ENTRY(handlesMouseMovedEventPassingThroughWhenModifierMatches)        \
    ANGRY_ENTRY(handlesDockSwipeGestureBeganAndEndedEvents)                     \
    ANGRY_ENTRY(swapsWindowsAcrossDifferentSpacesUpdatingManagedTable)          \
    ANGRY_ENTRY(warpsWindowBetweenSiblingsWithDifferentSplitModifyingParent)    \
    ANGRY_ENTRY(handlesMouseMovedEventPostingEventWhenModifierDiffers)          \
    ANGRY_ENTRY(handlesZoomedNodeOnMouseDropActionStack)                        \
    ANGRY_ENTRY(hashesProcessSerialNumberUsingLowLong)                          \
    ANGRY_ENTRY(comparesIdenticalProcessSerialNumbersReturningTrue)             \
    ANGRY_ENTRY(comparesDifferentProcessSerialNumbersReturningFalse)            \
    ANGRY_ENTRY(retrievesPidForCurrentProcessSerialNumber)                      \
    ANGRY_ENTRY(createsProcessInstanceForCurrentProcess)                       \
    ANGRY_ENTRY(rejectsInvalidProcessInProcessCreateReturningNull)              \
    ANGRY_ENTRY(reportsTestProcessAsNotBeingDebugged)                           \
    ANGRY_ENTRY(findsProcessInProcessManagerTable)                              \
    ANGRY_ENTRY(returnsNullWhenProcessNotFoundInProcessManager)                 \
    ANGRY_ENTRY(handlesProcessLaunchEventInProcessHandler)                      \
    ANGRY_ENTRY(handlesDuplicateProcessLaunchEventDiscardingDupe)               \
    ANGRY_ENTRY(handlesProcessFrontSwitchedEventInProcessHandler)               \
    ANGRY_ENTRY(handlesProcessTerminatedEventMarkingProcessTerminated)         \
    ANGRY_ENTRY(returnsErrorWhenProcessEventLacksProcessIdParameter)            \
    ANGRY_ENTRY(returnsZeroActiveSpaceForInvalidConnectionId)                   \
    ANGRY_ENTRY(addsRunningProcessesAndCachesFinderPsn)                         \
    ANGRY_ENTRY(initializesProcessManagerInstallingCarbonEventHandler)          \
    ANGRY_ENTRY(recordsMissionControlStateCapturingFrontApplicationAndSpace)    \
    ANGRY_ENTRY(preservesMissionControlStateIdempotencyWhenAlreadyRecorded)     \
    ANGRY_ENTRY(postsMissionControlEnterForDockConnectionEvent)                 \
    ANGRY_ENTRY(postsSpaceCreatedEventCarryingSpaceId)                          \
    ANGRY_ENTRY(postsSpaceDestroyedEventCarryingSpaceId)                        \
    ANGRY_ENTRY(postsWindowOrderedEventCarryingWindowId)                        \
    ANGRY_ENTRY(postsWindowDestroyedEventCarryingWindowId)                      \
    ANGRY_ENTRY(updatesLastCmdTabTimeOnConnectionHandlerType1202)               \
    ANGRY_ENTRY(postsShowAllWindowsEventForExposeNotification)                  \
    ANGRY_ENTRY(postsShowFrontWindowsEventForExposeNotification)                \
    ANGRY_ENTRY(postsShowDesktopEventForExposeNotification)                     \
    ANGRY_ENTRY(postsMissionControlExitEventForExposeNotification)              \
    ANGRY_ENTRY(observesAndUnobservesMissionControlDockEvents)                  \
    ANGRY_ENTRY(detectsMissionControlFromDockOverlayWindow)                     \
    ANGRY_ENTRY(returnsCachedMissionControlStateWhenCalledWithinFiftyMilliseconds)\
    ANGRY_ENTRY(mapsMissionControlModesToExpectedStringRepresentations)         \
    ANGRY_ENTRY(postsDisplayAddedEventCarryingDisplayId)                        \
    ANGRY_ENTRY(postsDisplayRemovedEventCarryingDisplayId)                      \
    ANGRY_ENTRY(postsDisplayMovedEventCarryingDisplayId)                        \
    ANGRY_ENTRY(postsDisplayResizedEventCarryingDisplayId)                      \
    ANGRY_ENTRY(serializesDisplayPropertiesToJsonStream)                        \
    ANGRY_ENTRY(serializesIndividualDisplayPropertyFields)                      \
    ANGRY_ENTRY(countsSpacesForActiveDisplay)                                   \
    ANGRY_ENTRY(returnsZeroSpacesForInvalidDisplayId)                           \
    ANGRY_ENTRY(resolvesDisplayBoundsConstrainedWithExternalBarPadding)          \
    ANGRY_ENTRY(convertsDisplayIdToUuidAndBack)                                 \
    ANGRY_ENTRY(identifiesSystemSpaceAccurately)                                 \
    ANGRY_ENTRY(evaluatesSpaceVisibilityForActiveSpace)                          \
    ANGRY_ENTRY(identifiesLeftChildInBinaryTreeHierarchy)                       \
    ANGRY_ENTRY(equalizesBspNodeRatioAcrossSpecifiedSplitAxis)                  \
    ANGRY_ENTRY(locatesIndexOfWindowInWindowNodeList)                           \
    ANGRY_ENTRY(clearsZoomNodeRecursivelyFromBspSubtree)                       \
    ANGRY_ENTRY(locatesLastLeafNodeInDeepBspTree)                               \
    ANGRY_ENTRY(locatesPreviousLeafNodeInBspTree)                               \
    ANGRY_ENTRY(returnsNullWhenSearchingDirectionalNodeOnInvalidSpace)          \
    ANGRY_ENTRY(allocatesAndPopulatesWindowListFromLeafNodes)                   \
    ANGRY_ENTRY(evaluatesViewIsInvalidWhenValidFlagIsUnset)                      \
    ANGRY_ENTRY(marksViewDirtyWhenFlushingInvisibleSpace)                       \
    ANGRY_ENTRY(serializesBspViewPropertiesToJsonStream)                        \
    ANGRY_ENTRY(clearsViewRootTreeAndResetsNodeDimensions)                     \
    ANGRY_ENTRY(displaysAndDestroysInsertFeedbackWindow)                        \
    ANGRY_ENTRY(movesInsertFeedbackEntryToNewFrontWindowOfStack) \
    ANGRY_ENTRY(handsInsertFeedbackEntryToParentWhenSiblingIsRemoved) \
    ANGRY_ENTRY(keepsWholeWindowListOfFullStackOutOfNextTemporaryAllocation) \
    ANGRY_ENTRY(leavesFullStackLayoutUntouchedWhenOneMoreWindowArrives)         \
    ANGRY_ENTRY(tilesWindowBesideFullStackInsteadOfStackingOntoIt)              \
    ANGRY_ENTRY(leavesWindowOutOfLayoutWhenTilingOnFullStackSpace)              \
    ANGRY_ENTRY(resolvesDisplayOfTestWindow)                                    \
    ANGRY_ENTRY(resolvesSpaceOfTestWindow)                                      \
    ANGRY_ENTRY(reportsNormalLevelOfTestWindow)                                 \
    ANGRY_ENTRY(readsTitleOfTestWindow)                                         \
    ANGRY_ENTRY(serializesIdAndPidOfTestWindow)                                 \
    ANGRY_ENTRY(serializesWindowStructurePropertiesToJsonStream)                \
    ANGRY_ENTRY(serializesWindowSelectedPropertiesWithoutOptionalAttributes)    \
    ANGRY_ENTRY(formatsWindowTitleUsingTemporaryStorage)                       \
    ANGRY_ENTRY(formatsWindowRoleUsingTemporaryStorage)                         \
    ANGRY_ENTRY(formatsWindowSubroleUsingTemporaryStorage)                      \
    ANGRY_ENTRY(identifiesRealWindowWithStandardRoleAndSubrole)                  \
    ANGRY_ENTRY(identifiesStandardWindowWithStandardSubrole)                    \
    ANGRY_ENTRY(identifiesUnknownSubroleWindow)                                 \
    ANGRY_ENTRY(evaluatesWindowCanMoveAndCanResizeFromFlags)                     \
    ANGRY_ENTRY(evaluatesWindowIsUndersizedBasedOnFrameDimensions)              \
    ANGRY_ENTRY(mapsWindowLevelsToLayerStrings)                                 \
    ANGRY_ENTRY(rejectsSystemWideElementAsRootWindow)                           \
    ANGRY_ENTRY(manipulatesWindowFlagsAndRuleFlagsBitmasks)                     \
    ANGRY_ENTRY(queriesAllActiveDisplaysToJsonStream)                           \
    ANGRY_ENTRY(managesDisplayLabelsSettingAndQueryingAndRemoving)              \
    ANGRY_ENTRY(resolvesMainDisplayUuidMatchingMainDisplayId)                   \
    ANGRY_ENTRY(findsDisplayHoldingTheDock)                                     \
    ANGRY_ENTRY(resolvesPointDisplayUuidAndPointDisplayId)                      \
    ANGRY_ENTRY(evaluatesDisplayCoordinateComparatorAcrossAxes)                 \
    ANGRY_ENTRY(calculatesDisplayArrangementWithCustomSortOrders)               \
    ANGRY_ENTRY(resolvesArrangementDisplayUuidAndDisplayId)                     \
    ANGRY_ENTRY(resolvesCursorDisplayIdFromCurrentCursorLocation)               \
    ANGRY_ENTRY(resolvesPreviousAndNextDisplayOtherThanCurrent)                 \
    ANGRY_ENTRY(resolvesFirstAndLastDisplayIdsAccurately)                       \
    ANGRY_ENTRY(findsClosestDisplayOtherThanCurrentInCardinalDirections)        \
    ANGRY_ENTRY(reportsKnownDockOrientation)                                    \
    ANGRY_ENTRY(reportsMainDisplayAsNotAnimating)                               \
    ANGRY_ENTRY(focusesDisplayWithWindowAtSpecifiedCoordinates)                \
    ANGRY_ENTRY(returnsSameDisplayErrorWhenSpaceDoesNotBelongToDisplay)         \
    ANGRY_ENTRY(comparesViewsUsingKeyEquality)                                  \
    ANGRY_ENTRY(queriesSpaceToJsonStream)                                       \
    ANGRY_ENTRY(queriesSpacesForWindowToJsonStream)                             \
    ANGRY_ENTRY(queriesSpacesForDisplayToJsonStream)                            \
    ANGRY_ENTRY(queriesSpacesForDisplaysToJsonStream)                           \
    ANGRY_ENTRY(returnsSameViewOnRepeatedLookupOfSpace)                         \
    ANGRY_ENTRY(marksViewInvalidForSpace)                                       \
    ANGRY_ENTRY(untilesWindowFromSpaceView)                                     \
    ANGRY_ENTRY(managesSpaceLabelsSettingAndQueryingAndRemoving)                \
    ANGRY_ENTRY(configuresSpaceLayoutAndValidates)                              \
    ANGRY_ENTRY(configuresGapForSpaceWithAbsAndRelTypes)                        \
    ANGRY_ENTRY(togglesGapForSpaceUpdatingState)                                \
    ANGRY_ENTRY(asksDockToOpenMissionControl)                                   \
    ANGRY_ENTRY(asksDockToShowDesktop)                                          \
    ANGRY_ENTRY(configuresLayoutForAllSpaces)                                   \
    ANGRY_ENTRY(configuresWindowGapForAllSpaces)                                 \
    ANGRY_ENTRY(configuresAllPaddingDimensionsForAllSpaces)                     \
    ANGRY_ENTRY(configuresSplitTypeAndAutoBalanceForAllSpaces)                  \
    ANGRY_ENTRY(configuresPaddingForSpaceWithAbsAndRelTypes)                     \
    ANGRY_ENTRY(togglesPaddingForSpaceUpdatingState)                            \
    ANGRY_ENTRY(rotatesAndMirrorsSpaceSubtree)                                  \
    ANGRY_ENTRY(equalizesAndBalancesSpaceSubtree)                               \
    ANGRY_ENTRY(tilesWindowOnSpaceWithInsertionPoint)                           \
    ANGRY_ENTRY(togglesWindowSplitDirectionInBspSubtree)                        \
    ANGRY_ENTRY(resolvesMissionControlSpaceByDesktopIndex)                      \
    ANGRY_ENTRY(resolvesCursorSpaceMatchingCurrentCursor)                      \
    ANGRY_ENTRY(resolvesPreviousAndNextSpaceOtherThanCurrent)                   \
    ANGRY_ENTRY(resolvesFirstAndLastSpaceAcrossDisplays)                       \
    ANGRY_ENTRY(reportsUnknownWindowAsNotOnActiveSpace)                         \
    ANGRY_ENTRY(swapsSpaceWithSpaceOnSameAndCrossDisplay)                       \
    ANGRY_ENTRY(movesSpaceToSpaceDetectingErrors)                               \
    ANGRY_ENTRY(movesSpaceToDisplayDetectingErrors)                             \
    ANGRY_ENTRY(switchesSpaceDetectingSameSpace)                                \
    ANGRY_ENTRY(destroysSpaceDetectingMissingAndInvalidSrc)                      \
    ANGRY_ENTRY(addsSpaceDetectingMissingSrc)                                   \
    ANGRY_ENTRY(reportsNoRefreshWhenNoApplicationIsPending)                     \
    ANGRY_ENTRY(findsFirstUserSpaceOfMainDisplay)                               \
    ANGRY_ENTRY(enablesNaturalScrollingWhenExplicitlyOn)                        \
    ANGRY_ENTRY(disablesNaturalScrollingWhenExplicitlyOff)                      \
    ANGRY_ENTRY(advancesTokenPointerAndExtractsTokenText)                       \
    ANGRY_ENTRY(evaluatesTokenPrefixMatchingStartOfString)                      \
    ANGRY_ENTRY(evaluatesTokenEqualsAgainstExactStrings)                        \
    ANGRY_ENTRY(evaluatesTokenIsValidCheckingLengthAndPointer)                  \
    ANGRY_ENTRY(evaluatesTokenIsPositiveIntegerForValidAndInvalidStrings)       \
    ANGRY_ENTRY(evaluatesTokenIsHexadecimalForPrefixedNumbers)                  \
    ANGRY_ENTRY(evaluatesTokenIsFloatForValidFloatingPointNumbers)              \
    ANGRY_ENTRY(convertsTokenToTypedTokenValue)                                 \
    ANGRY_ENTRY(parsesKeyValuePairExtractingKeyAndValueAndExclusion)            \
    ANGRY_ENTRY(parsesValueTypeForAbsAndRelModifiers)                          \
    ANGRY_ENTRY(parsesResizeHandleForCardinalAndCornerDirections)              \
    ANGRY_ENTRY(parsesLabelValidatingReservedIdentifiers)                      \
    ANGRY_ENTRY(parsesPropertiesFromCommaSeparatedToken)                        \
    ANGRY_ENTRY(parsesDisplaySelectorForIndexAndDirections)                    \
    ANGRY_ENTRY(parsesSpaceSelectorForIndexAndRelativeSelectors)                \
    ANGRY_ENTRY(parsesWindowSelectorForKeywordsAndWindowIds)                    \
    ANGRY_ENTRY(parsesInsertSelectorForCardinalDirectionsAndStack)              \
    ANGRY_ENTRY(parsesRuleSpecifyingAppFilterAndManageEffect)                   \
    ANGRY_ENTRY(dispatchesConfigDomainModifyingRuntimeSettings)                 \
    ANGRY_ENTRY(configuresSpaceGestureNaturalScrollingMode)                     \
    ANGRY_ENTRY(rejectsFocusOfMissingDisplay)                                   \
    ANGRY_ENTRY(rejectsLayoutChangeOfMissingSpace)                              \
    ANGRY_ENTRY(rejectsToggleOfMissingWindow)                                   \
    ANGRY_ENTRY(dispatchesQueryDomainEmittingFormattedJson)                     \
    ANGRY_ENTRY(dispatchesRuleDomainListingActiveRules)                         \
    ANGRY_ENTRY(dispatchesSignalDomainListingRegisteredSignals)                 \
    ANGRY_ENTRY(dispatchesMessageHandlingForInvalidDomain)                      \
    ANGRY_ENTRY(formatsDaemonFailureMessageToJsonStream)                        \
    ANGRY_ENTRY(formatsDaemonDeprecatedWarningToJsonStream)                     \
    ANGRY_ENTRY(terminatesMessageLoopImmediatelyWhenNotRunning)                 \
    ANGRY_ENTRY(initializesAndTerminatesMessageLoopSocket)                     \
    ANGRY_ENTRY(resolvesLayoutStorePathForNullAndCustomPaths)                   \
    ANGRY_ENTRY(resolvesLayoutStoreSessionPathForNullAndCustomPaths)            \
    ANGRY_ENTRY(ensuresDirectoryExistsForTargetFilePath)                       \
    ANGRY_ENTRY(serializesLeafAndBranchWindowNodesToDictionary)                 \
    ANGRY_ENTRY(collectsAppNamesRecursivelyFromSerializedNodeDictionary)        \
    ANGRY_ENTRY(reconstructsWindowNodeHierarchyFromSerializedDictionary)        \
    ANGRY_ENTRY(reconstructsWindowNodeFromDictionaryWithWrongValueTypes) \
    ANGRY_ENTRY(buildsSessionDictionaryCapturingDisplaysAndSpaces)              \
    ANGRY_ENTRY(refusesToSaveLayoutOfFloatingSpace)                             \
    ANGRY_ENTRY(refusesToDumpLayoutOfFloatingSpace)                             \
    ANGRY_ENTRY(restoresSpaceLayoutFromFileHandlingMissingFile)                 \
    ANGRY_ENTRY(savesSessionStateToFileHandlingErrors)                          \
    ANGRY_ENTRY(dumpsSessionStateToJsonStreamHandlingErrors)                    \
    ANGRY_ENTRY(restoresSessionStateFromFileHandlingMissingFile)                \
    ANGRY_ENTRY(observesFinishedLaunchingOfApplication)                         \
    ANGRY_ENTRY(observesActivationPolicyOfApplication)                          \
    ANGRY_ENTRY(treatsProcessWithoutApplicationAsUnobservable)                  \
    ANGRY_ENTRY(evaluatesApplicationFinishedLaunchingState)                     \
    ANGRY_ENTRY(reportsNoNotchForUnknownDisplay)                                \
    ANGRY_ENTRY(deallocatesWorkspaceContextProperly)                            \
    ANGRY_ENTRY(processesActivationPolicyChangeObservation)                     \
    ANGRY_ENTRY(processesFinishedLaunchingChangeObservation)                    \
    ANGRY_ENTRY(postsSystemWokeEventOnWake)                                     \
    ANGRY_ENTRY(postsMenuBarHiddenChangedEventOnMenuBarHidingChange)            \
    ANGRY_ENTRY(postsDockDidRestartEventOnDockRestart)                          \
    ANGRY_ENTRY(postsDockDidChangePrefEventOnDockPreferenceChange)              \
    ANGRY_ENTRY(postsDisplayChangedEventOnActiveDisplayChange)                  \
    ANGRY_ENTRY(postsSpaceChangedEventOnActiveSpaceChange)                      \
    ANGRY_ENTRY(postsApplicationHiddenEventCarryingPid)                         \
    ANGRY_ENTRY(postsApplicationVisibleEventCarryingPid)                        \
    ANGRY_ENTRY(configuresOsaxFilesystemPathsProperly)                          \
    ANGRY_ENTRY(failsToCreateOsaxDirectoryWithoutRootPrivileges)                \
    ANGRY_ENTRY(writesAndRemovesTemporaryOsaxFile)                              \
    ANGRY_ENTRY(preparesOsaxBinariesPermissionsAndSignatures)                   \
    ANGRY_ENTRY(restartsDockApplicationSafelyWithSwizzledTerminate)             \
    ANGRY_ENTRY(configuresSocketPathWhenRunningUnderSudo)                       \
    ANGRY_ENTRY(removesOsaxInstallationReturningStatusCode)                     \
    ANGRY_ENTRY(handlesOsaxInstallationFailureGracefully)                       \
    ANGRY_ENTRY(failsHandshakeRequestWhenSocketServerIsAbsent)                  \
    ANGRY_ENTRY(returnsExpectedAttributesForOperatingSystemVersion)             \
    ANGRY_ENTRY(performsValidationFailingWhenSocketIsAbsent)                    \
    ANGRY_ENTRY(evaluatesSystemIntegrityProtectionCompatibility)                \
    ANGRY_ENTRY(attemptsMachLoaderPayloadInjection)                             \
    ANGRY_ENTRY(uninstallsScriptingAdditionWithPermissionChecks)                \
    ANGRY_ENTRY(loadsScriptingAdditionVerifyingRootAndSipEnforcement)             \
    ANGRY_ENTRY(failsSendingRawBytesWhenSocketUnavailable)                      \
    ANGRY_ENTRY(dispatchesSpaceFocusAndCreationCommandsSafely)                  \
    ANGRY_ENTRY(dispatchesSpaceMoveToDisplayCommandSafely)                      \
    ANGRY_ENTRY(dispatchesWindowMoveCommandSafely)                              \
    ANGRY_ENTRY(dispatchesWindowOpacityCommandsForInstantAndFade)               \
    ANGRY_ENTRY(dispatchesWindowStickyAndFocusAndScaleCommandsSafely)           \
    ANGRY_ENTRY(dispatchesWindowSwapProxyInAndOutCommandsSafely)                \
    ANGRY_ENTRY(dispatchesWindowOrderInCommandSafely)                           \
    ANGRY_ENTRY(updatesApplicationOpenMenusCounterAndAdjustsFfm)                \
    ANGRY_ENTRY(processesWindowDidReceiveFocusUpdatingWindowManagerState)       \
    ANGRY_ENTRY(initializesAndRunsEventLoopThreadProcessingEvents)              \
    ANGRY_ENTRY(handlesApplicationLaunchedEventForTerminatedAndRunningProcess)  \
    ANGRY_ENTRY(remembersFrontSwitchOfApplicationNotYetObserved)                \
    ANGRY_ENTRY(remembersFocusOfWindowNotYetObserved)                           \
    ANGRY_ENTRY(dropsViewOfDestroyedSpace)                                      \
    ANGRY_ENTRY(keepsResizeGestureMovedAfterDragReturnsNearOrigin)              \
    ANGRY_ENTRY(ignoresFencelessLiveResizeStepAfterSplitHasMoved)               \
    ANGRY_ENTRY(entersShowAllWindowsModeOnMissionControlShowAllWindows)         \
    ANGRY_ENTRY(entersShowFrontWindowsModeOnMissionControlShowFrontWindows)     \
    ANGRY_ENTRY(entersShowDesktopModeOnMissionControlShowDesktop)               \
    ANGRY_ENTRY(entersShowModeOnMissionControlEnter)                            \
    ANGRY_ENTRY(leavesMissionControlModeOnExit)                                 \
    ANGRY_ENTRY(countsMenuOpenedByObservedApplication)                          \
    ANGRY_ENTRY(countsMenuClosedByObservedApplication)                          \
    ANGRY_ENTRY(answersConfigQueryReceivedOnDaemonSocket)                       \
    ANGRY_ENTRY(abortsWhenClientSendMessageReceivesNoArguments)                 \
    ANGRY_ENTRY(printsVersionAndExitsInParseArguments)                          \
    ANGRY_ENTRY(keepsNativeTabsAfterMissionControl)                             \
    ANGRY_ENTRY(keepsInsertionPointOnTabSwap)                                   \
    ANGRY_ENTRY(releasesTabMembersWhenDisabled)                                 \
    ANGRY_ENTRY(swapsHiddenTabIntoNode)                                         \
    ANGRY_ENTRY(rejectsSwapOfUntrackedTab)                                      \
    ANGRY_ENTRY(validatesWindowEligibilityForManagedRules)                      \
    ANGRY_ENTRY(queriesWindowsForSpaceListAsJsonStream)                         \
    ANGRY_ENTRY(queriesWindowsForSingleDisplayAsJsonStream)                     \
    ANGRY_ENTRY(queriesWindowsForDisplaysAsJsonStream)                         \
    ANGRY_ENTRY(matchesWindowAgainstRegexRule)                                  \
    ANGRY_ENTRY(appliesManageRuleEffectsToWindow)                               \
    ANGRY_ENTRY(appliesGeneralRuleEffectsToWindow)                              \
    ANGRY_ENTRY(clearsManagedFlagWhenMatchingRuleTurnsManagementOff)            \
    ANGRY_ENTRY(setsFocusFollowsMouseFlagWhenMatchingRuleEnablesIt)             \
    ANGRY_ENTRY(togglesGlobalWindowOpacityState)                                \
    ANGRY_ENTRY(evaluatesWhetherWindowShouldBeManaged)                          \
    ANGRY_ENTRY(adjustsWindowRatioReturningErrorOnUnmanagedWindow)              \
    ANGRY_ENTRY(movesWindowRelativelyForUnmanagedWindow)                        \
    ANGRY_ENTRY(resizesWindowRelativeHandlingUnmanagedWindow)                   \
    ANGRY_ENTRY(setsWindowPurifyModeAndIteratesWindows)                         \
    ANGRY_ENTRY(reportsFailedOpacityChangeWithoutScriptingAddition)             \
    ANGRY_ENTRY(setsMenubarOpacityDirectly)                                     \
    ANGRY_ENTRY(setsActiveWindowOpacityDirectly)                                \
    ANGRY_ENTRY(setsNormalWindowOpacityDirectly)                                \
    ANGRY_ENTRY(remembersRequestedWindowLayer)                                  \
    ANGRY_ENTRY(findsRankOfWindowInWindowList)                                  \
    ANGRY_ENTRY(createsWindowProxySkippingNullImage)                            \
    ANGRY_ENTRY(destroysWindowProxyCleaningUpResources)                         \
    ANGRY_ENTRY(processesWindowAnimationListStepInThreadProc)                  \
    ANGRY_ENTRY(findsClosestManagedWindowInDirection)                          \
    ANGRY_ENTRY(findsPrevManagedWindowOnActiveSpace)                            \
    ANGRY_ENTRY(findsNextManagedWindowOnActiveSpace)                            \
    ANGRY_ENTRY(findsFirstManagedWindowOnActiveSpace)                           \
    ANGRY_ENTRY(findsLastManagedWindowOnActiveSpace)                            \
    ANGRY_ENTRY(findsRecentManagedWindowUsingLastWindowId)                      \
    ANGRY_ENTRY(findsPrevWindowInStackReturningNull)                            \
    ANGRY_ENTRY(findsNextWindowInStackReturningNull)                            \
    ANGRY_ENTRY(findsFirstWindowInStackReturningNull)                           \
    ANGRY_ENTRY(findsLastWindowInStackReturningNull)                            \
    ANGRY_ENTRY(findsRecentWindowInStackReturningNull)                          \
    ANGRY_ENTRY(findsWindowInStackByIndexReturningNull)                         \
    ANGRY_ENTRY(findsLargestManagedWindowOnActiveSpace)                          \
    ANGRY_ENTRY(findsSmallestManagedWindowOnActiveSpace)                         \
    ANGRY_ENTRY(findsSiblingForManagedWindowReturningNull)                      \
    ANGRY_ENTRY(findsFirstNephewForManagedWindowReturningNull)                  \
    ANGRY_ENTRY(findsSecondNephewForManagedWindowReturningNull)                 \
    ANGRY_ENTRY(findsUncleForManagedWindowReturningNull)                        \
    ANGRY_ENTRY(findsFirstCousinForManagedWindowReturningNull)                  \
    ANGRY_ENTRY(findsSecondCousinForManagedWindowReturningNull)                 \
    ANGRY_ENTRY(focusesWindowOfAnotherProcessWithoutRaiseByMakingItKey)         \
    ANGRY_ENTRY(deactivatesFocusedWindowBeforeFocusingSiblingWithoutRaise)      \
    ANGRY_ENTRY(tracksLostFrontSwitchedEventInTable)                            \
    ANGRY_ENTRY(tracksLostFocusedEventInTable)                                  \
    ANGRY_ENTRY(addsAndRemovesWindowFromTable)                                  \
    ANGRY_ENTRY(addsAndRemovesApplicationFromTable)                             \
    ANGRY_ENTRY(findsWindowsBelongingToApplication)                             \
    ANGRY_ENTRY(postsTwoEventsToMakeWindowKey)                                  \
    ANGRY_ENTRY(addressesKeyWindowEventsToWindow)                               \
    ANGRY_ENTRY(dropsWindowThatCannotBeObserved)                                \
    ANGRY_ENTRY(addsApplicationWindowsSafely)                                   \
    ANGRY_ENTRY(addsExistingApplicationWindowsSafely)                           \
    ANGRY_ENTRY(refusesInsertionPointForWindowOutsideTheLayout)                 \
    ANGRY_ENTRY(stacksWindowReturningErrorOnSelfStack)                          \
    ANGRY_ENTRY(warpsWindowReturningErrorOnSelfWarp)                            \
    ANGRY_ENTRY(swapsWindowReturningErrorOnSelfSwap)                            \
    ANGRY_ENTRY(minimizesWindowCheckingCapabilities)                            \
    ANGRY_ENTRY(deminimizesWindowCheckingMinimizedState)                        \
    ANGRY_ENTRY(closesWindowReturningFailureOnNullButton)                       \
    ANGRY_ENTRY(appliesGridToWindowHandlingInvalidView)                         \
    ANGRY_ENTRY(keepsIneligibleWindowFromFloating)                              \
    ANGRY_ENTRY(keepsIneligibleWindowFromBecomingSticky)                        \
    ANGRY_ENTRY(keepsShadowFlagWhenScriptingAdditionCannotChangeIt)             \
    ANGRY_ENTRY(togglesScratchpadWindowByLabelReturningFalse)                   \
    ANGRY_ENTRY(togglesScratchpadWindowDirectly)                                \
    ANGRY_ENTRY(setsAndRemovesScratchpadForWindow)                              \
    ANGRY_ENTRY(findsScratchpadWindowByLabel)                                   \
    ANGRY_ENTRY(zoomsWindowToItsParentNode)                                     \
    ANGRY_ENTRY(unzoomsWindowFromParentNodeOnSecondToggle)                      \
    ANGRY_ENTRY(zoomsWindowToWholeSpace)                                        \
    ANGRY_ENTRY(unzoomsWindowFromWholeSpaceOnSecondToggle)                      \
    ANGRY_ENTRY(marksHiddenSpaceDirtyWhenZoomingWindow)                         \
    ANGRY_ENTRY(leavesSoleWindowOfSpaceUnzoomed)                                \
    ANGRY_ENTRY(remembersFrameWhenEnteringWindowedFullscreen)                   \
    ANGRY_ENTRY(fillsDisplayWhenEnteringWindowedFullscreen)                     \
    ANGRY_ENTRY(restoresFrameWhenLeavingWindowedFullscreen)                     \
    ANGRY_ENTRY(requestsNativeFullscreenForWindowNotInFullscreen)               \
    ANGRY_ENTRY(focusesWindowBeforeChangingNativeFullscreen)                    \
    ANGRY_ENTRY(bringsWindowToFrontBeforeExpose)                                \
    ANGRY_ENTRY(asksDockToExposeWindowsOfFrontApplication)                      \
    ANGRY_ENTRY(asksScriptingAdditionToScaleWindowForPictureInPicture)          \
    ANGRY_ENTRY(scalesWindowToDisplayBoundsForPictureInPicture)                 \
    ANGRY_ENTRY(keepsSpacePaddingForPictureInPicture)                           \
    ANGRY_ENTRY(followsActivatedApplicationToSpaceOfItsShownTab)                \
    ANGRY_ENTRY(handsFocusAwayFromApplicationWhoseOnlyWindowOnSpaceIsHiddenTab) \
    ANGRY_ENTRY(followsApplicationRunningUnderDebuggerToSpaceOfItsWindow)

static struct test {
    char *name;
    test_function *func;
} tests[] = {
    TEST_LIST
};

//
// NOTE: the suite runs in a shuffled order so that a test relying on state
// left behind by another test fails instead of passing by luck. YT_SEED=<n>
// replays an order, YT_SEED=0 keeps the declaration order.
//
static const char *g_running_test_name;
static uint32_t g_tests_seed;

static void tests_report_crash(int sig)
{
    char line[512];
    int len = snprintf(line, sizeof(line), "\n\e[1;31m -- %s crashed in %s (replay with YT_SEED=%u)\e[m\n",
                       strsignal(sig), g_running_test_name ? g_running_test_name : "?", g_tests_seed);
    write(STDERR_FILENO, line, len);
    _exit(128 + sig);
}

static void tests_report_early_exit(void)
{
    if (!g_running_test_name || getpid() != g_pid) return;
    fprintf(stderr, "\n\e[1;31m -- exited inside %s (replay with YT_SEED=%u)\e[m\n", g_running_test_name, g_tests_seed);
}

//
// NOTE: every test starts with the fakes switched off and Mission Control
// inactive, whatever the previous test left behind.
//
static void tests_reset_shared_state(void)
{
    g_mock_uid = (uid_t)-1;
    g_mock_csr_config = -1;
    g_mock_system_log[0] = '\0';
    g_mock_home = NULL;
    g_mock_front_process_count = 0;
    g_mock_front_process_wid = 0;
    g_mock_cursor_warp_count = 0;
    g_mock_event_post_count = 0;
    g_mock_current_space_set = 0;
    g_mock_window_space_count = 0;
    g_mock_mouse_event_count = 0;
    g_mock_front_is_test_process = false;
    memset(&g_last_follow_to_space, 0, sizeof(g_last_follow_to_space));
    g_mock_windows_moved_to_space = 0;
    g_mock_bridged_operation_count = 0;
    g_mock_ax_write_count = 0;
    g_mock_ax_position = CGPointZero;
    g_mock_ax_size = CGSizeZero;
    g_mock_ax_bool_attribute[0] = '\0';
    g_mock_ax_bool_value = false;
    g_mock_fast_usleep = false;
    g_mock_active_space = 0;
    g_mock_dispatch_after = false;
    g_mock_ax_cannot_complete = false;
    g_mock_dock_window_layer18 = false;
    g_mock_posted_event_count = 0;
    g_angry_failed_part = NULL;
    g_mock_dock_notification[0] = '\0';

    g_mission_control_mode = MISSION_CONTROL_MODE_INACTIVE;
    g_mission_control_prev_wid = 0;
    g_mission_control_prev_sid = 0;
    g_mission_control_prev_pid = 0;
    g_mission_control_mouse_clicked = false;
    g_mission_control_exit_time = 0;
    g_mission_control_posted_exit = false;
    g_mission_control_hover_wid = 0;
    g_mission_control_cursor_moved = false;
    g_mission_control_reverted_activations = 0;
    memset(&g_mission_control_active_cache, 0, sizeof(g_mission_control_active_cache));
}

static void tests_shuffle(struct test *list, int count, uint32_t seed)
{
    uint32_t x = seed;
    for (int i = count - 1; i > 0; --i) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        int j = x % (uint32_t)(i + 1);
        struct test tmp = list[i]; list[i] = list[j]; list[j] = tmp;
    }
}

//
// NOTE: the process-wide state the suite runs against. The benchmarks in
// bench/ include this file for its fakes and call this before they measure.
//
static void tests_init_process(void)
{
    ts_init(MEGABYTES(8));
    tests_isolate_scripting_addition();
    g_connection = SLSMainConnectionID();
    CGSGetConnectionPortById = macho_find_symbol("/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/SkyLight", "_CGSGetConnectionPortById");
    g_layer_below_window_level  = CGWindowLevelForKey(LAYER_BELOW);
    g_layer_normal_window_level = CGWindowLevelForKey(LAYER_NORMAL);
    g_layer_above_window_level  = CGWindowLevelForKey(LAYER_ABOVE);
    workspace_event_handler_begin(&g_workspace_context);
    mouse_state_init(&g_mouse_state);
    window_manager_init(&g_window_manager);
    display_manager_begin(&g_display_manager);
    space_manager_begin(&g_space_manager);
    memory_pool_init(&g_signal_storage, KILOBYTES(256));
    memory_pool_init(&g_event_loop.pool, KILOBYTES(512));
    g_event_loop.semaphore = sem_open("yabai_event_loop_test_sem", O_CREAT, 0600, 0);
    sem_unlink("yabai_event_loop_test_sem");
    g_event_loop.head = memory_pool_push(&g_event_loop.pool, sizeof(struct event));
    g_event_loop.head->next = NULL;
    g_event_loop.tail = g_event_loop.head;

    //
    // NOTE: the process-wide state yabai sets up at launch, so that no test
    // depends on another test having run configure_settings_and_acquire_lock
    // or process_manager_begin first.
    //
    g_pid = getpid();
    g_event_bytes = calloc(1, 0x100);
    g_cv_host_clock_frequency = CVGetHostClockFrequency();
    SLSPerformAsynchronousBridgedWindowManagementOperation = tests_bridged_window_management_operation;
    task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &g_bs_port);
    table_init(&g_process_manager.process, 125, hash_psn, compare_psn);
    signal(SIGPIPE, SIG_IGN);
}

#ifndef ANGRY_BENCH
int main(int argc, char **argv)
{
    tests_init_process();

    int succeeded = 0;
    int failed = 0;
    int slow = 0;
    int total = array_count(tests);
    char *seed_env = getenv("YT_SEED");
    g_tests_seed = seed_env ? (uint32_t)strtoul(seed_env, NULL, 10) : (arc4random() | 1);
    if (g_tests_seed) tests_shuffle(tests, total, g_tests_seed);
    atexit(tests_report_early_exit);
    signal(SIGSEGV, tests_report_crash);
    signal(SIGBUS, tests_report_crash);
    signal(SIGABRT, tests_report_crash);
    signal(SIGILL, tests_report_crash);
    signal(SIGTRAP, tests_report_crash);

    printf("\e[1;34m -- Running %d Angry Tests (<50ms/test standard, YT_SEED=%u)\e[m\n\n", total, g_tests_seed);

    uint64_t cpu_freq  = read_cpu_freq();
    uint64_t begin_tsc = read_cpu_timer();

    for (int i = 0; i < total; ++i) {
        uint64_t tsc = read_cpu_timer();
        g_running_test_name = tests[i].name;
        tests_reset_shared_state();
        bool result = tests[i].func();
        double ms_elapsed = 1000.0 * (double)(read_cpu_timer() - tsc) / (double)cpu_freq;

        bool is_slow = ms_elapsed > 50.0;
        if (is_slow) ++slow;

        printf("(%0.4fms) %s \e[1;33m%s\e[m%s\n",
               ms_elapsed,
               result ? "\e[1;32msuccess\e[m" : " \e[1;31mfailed\e[m",
               tests[i].name,
               is_slow ? " \e[1;35m[SLOW > 50ms]\e[m" : "");
        if (result) ++succeeded; else ++failed;
    }
    g_running_test_name = NULL;

    double ms_elapsed = 1000.0 * (double)(read_cpu_timer() - begin_tsc) / (double)cpu_freq;
    printf("\n\e[1;34m -- Completed (%0.4fms)\e[m\n", ms_elapsed);
    printf("\t%d \e[1;32msucceeded\e[m\n", succeeded);
    printf("\t%d \e[1;31mfailed\e[m\n", failed);
    if (slow > 0) {
        printf("\t%d \e[1;35mslow (>50ms)\e[m\n", slow);
    }
    printf("\t%d \e[1;33mtotal\e[m\n", total);
    if (failed) printf("\n\e[1;34m -- Replay this order with YT_SEED=%u\e[m\n", g_tests_seed);

    return total == succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
