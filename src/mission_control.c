extern struct event_loop g_event_loop;
extern enum mission_control_mode g_mission_control_mode;
extern volatile uint64_t __last_cmd_tab_time;
extern volatile bool __pending_gesture;
extern volatile uint64_t __last_gesture_time;
extern struct window_manager g_window_manager;
extern struct space_manager g_space_manager;
extern struct process_manager g_process_manager;

volatile uint32_t g_mission_control_prev_wid = 0;
volatile uint64_t g_mission_control_prev_sid = 0;
volatile pid_t g_mission_control_prev_pid = 0;
volatile bool g_mission_control_mouse_clicked = false;
volatile uint64_t g_mission_control_exit_time = 0;
volatile bool g_mission_control_posted_exit = false;
volatile uint32_t g_mission_control_hover_wid = 0;
volatile bool g_mission_control_cursor_moved = false;
CGPoint g_mission_control_enter_cursor;

void mission_control_record_state(void)
{
    if (!__atomic_load_n(&g_mission_control_prev_sid, __ATOMIC_RELAXED)) {
        uint32_t wid = g_window_manager.focused_window_id ? g_window_manager.focused_window_id : g_window_manager.last_window_id;
        uint64_t sid = g_space_manager.current_space_id;
        pid_t pid = g_process_manager.front_pid;

        //
        // NOTE: A space change that happened inside the previous Mission Control session was ignored,
        // so current_space_id can be stale. The focused window knows which space the user is on.
        //

        uint64_t wid_sid = (wid && window_manager_find_window(&g_window_manager, wid)) ? window_space(wid) : 0;
        if (wid_sid && space_is_user(wid_sid) && space_is_visible(wid_sid)) sid = wid_sid;
        if (sid) {
            __atomic_store_n(&g_mission_control_prev_wid, wid, __ATOMIC_RELEASE);
            __atomic_store_n(&g_mission_control_prev_pid, pid, __ATOMIC_RELEASE);
            __atomic_store_n(&g_mission_control_prev_sid, sid, __ATOMIC_RELEASE);
            __atomic_store_n(&g_mission_control_mouse_clicked, false, __ATOMIC_RELEASE);
            __atomic_store_n(&g_mission_control_hover_wid, 0, __ATOMIC_RELEASE);
            __atomic_store_n(&g_mission_control_cursor_moved, false, __ATOMIC_RELEASE);
            CGEventRef cursor_event = CGEventCreate(NULL);
            g_mission_control_enter_cursor = CGEventGetLocation(cursor_event);
            CFRelease(cursor_event);
            debug("%s: recorded prev_wid=%d, prev_pid=%d, prev_sid=%lld\n", __FUNCTION__, wid, pid, sid);
        }
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static CONNECTION_CALLBACK(connection_handler)
{
    if (type == 1204) {
        event_loop_post(&g_event_loop, MISSION_CONTROL_ENTER, NULL, 0);
    } else if (type == 1327) {
        uint64_t sid; memcpy(&sid, data, sizeof(uint64_t));
        event_loop_post(&g_event_loop, SLS_SPACE_CREATED, (void *) (intptr_t) sid , 0);
    } else if (type == 1328) {
        uint64_t sid; memcpy(&sid, data, sizeof(uint64_t));
        event_loop_post(&g_event_loop, SLS_SPACE_DESTROYED, (void *) (intptr_t) sid, 0);
    } else if (type == 808) {
        uint32_t wid; memcpy(&wid, data, sizeof(uint32_t));
        event_loop_post(&g_event_loop, SLS_WINDOW_ORDERED, (void *) (intptr_t) wid, 0);
    } else if (type == 804) {
        uint32_t wid; memcpy(&wid, data, sizeof(uint32_t));
        event_loop_post(&g_event_loop, SLS_WINDOW_DESTROYED, (void *) (intptr_t) wid, 0);
    } else if (type == 1202) {
        __atomic_store_n(&__last_cmd_tab_time, read_os_timer(), __ATOMIC_RELEASE);
    }
}
#pragma clang diagnostic pop

enum mission_control_mode
{
    MISSION_CONTROL_MODE_INACTIVE           = 0,
    MISSION_CONTROL_MODE_SHOW               = 1,
    MISSION_CONTROL_MODE_SHOW_ALL_WINDOWS   = 2,
    MISSION_CONTROL_MODE_SHOW_FRONT_WINDOWS = 3,
    MISSION_CONTROL_MODE_SHOW_DESKTOP       = 4
};

static const char *mission_control_mode_str[] = {
    [MISSION_CONTROL_MODE_INACTIVE]           = "inactive",
    [MISSION_CONTROL_MODE_SHOW]               = "show",
    [MISSION_CONTROL_MODE_SHOW_ALL_WINDOWS]   = "show-all-windows",
    [MISSION_CONTROL_MODE_SHOW_FRONT_WINDOWS] = "show-front-windows",
    [MISSION_CONTROL_MODE_SHOW_DESKTOP]       = "show-desktop"
};

static struct {
    AXUIElementRef ref;
    AXObserverRef observer_ref;
    bool is_observing;
} g_mission_control_observer;

static CFStringRef kAXExposeShowAllWindows   = CFSTR("AXExposeShowAllWindows");
static CFStringRef kAXExposeShowFrontWindows = CFSTR("AXExposeShowFrontWindows");
static CFStringRef kAXExposeShowDesktop      = CFSTR("AXExposeShowDesktop");
static CFStringRef kAXExposeExit             = CFSTR("AXExposeExit");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static OBSERVER_CALLBACK(mission_control_notification_handler)
{
    if (CFEqual(notification, kAXExposeShowAllWindows)) {
        event_loop_post(&g_event_loop, MISSION_CONTROL_SHOW_ALL_WINDOWS, NULL, 0);
    } else if (CFEqual(notification, kAXExposeShowFrontWindows)) {
        event_loop_post(&g_event_loop, MISSION_CONTROL_SHOW_FRONT_WINDOWS, NULL, 0);
    } else if (CFEqual(notification, kAXExposeShowDesktop)) {
        event_loop_post(&g_event_loop, MISSION_CONTROL_SHOW_DESKTOP, NULL, 0);
    } else if (CFEqual(notification, kAXExposeExit)) {
        event_loop_post(&g_event_loop, MISSION_CONTROL_EXIT, NULL, 0);
    }
}
#pragma clang diagnostic pop

void mission_control_observe(void)
{
    if (!g_mission_control_observer.is_observing) {
        uint32_t pid = workspace_get_dock_pid();
        g_mission_control_observer.ref = AXUIElementCreateApplication(pid);

        if (pid && g_mission_control_observer.ref) {
            if (AXObserverCreate(pid, mission_control_notification_handler, &g_mission_control_observer.observer_ref) == kAXErrorSuccess) {
                AXObserverAddNotification(g_mission_control_observer.observer_ref, g_mission_control_observer.ref, kAXExposeShowAllWindows, NULL);
                AXObserverAddNotification(g_mission_control_observer.observer_ref, g_mission_control_observer.ref, kAXExposeShowFrontWindows, NULL);
                AXObserverAddNotification(g_mission_control_observer.observer_ref, g_mission_control_observer.ref, kAXExposeShowDesktop, NULL);
                AXObserverAddNotification(g_mission_control_observer.observer_ref, g_mission_control_observer.ref, kAXExposeExit, NULL);

                g_mission_control_observer.is_observing = true;
                CFRunLoopAddSource(CFRunLoopGetMain(), AXObserverGetRunLoopSource(g_mission_control_observer.observer_ref), kCFRunLoopCommonModes);
            }
        }
    }
}

void mission_control_unobserve(void)
{
    if (g_mission_control_observer.is_observing) {
        AXObserverRemoveNotification(g_mission_control_observer.observer_ref, g_mission_control_observer.ref, kAXExposeShowAllWindows);
        AXObserverRemoveNotification(g_mission_control_observer.observer_ref, g_mission_control_observer.ref, kAXExposeShowFrontWindows);
        AXObserverRemoveNotification(g_mission_control_observer.observer_ref, g_mission_control_observer.ref, kAXExposeShowDesktop);
        AXObserverRemoveNotification(g_mission_control_observer.observer_ref, g_mission_control_observer.ref, kAXExposeExit);

        g_mission_control_observer.is_observing = false;
        CFRunLoopSourceInvalidate(AXObserverGetRunLoopSource(g_mission_control_observer.observer_ref));
        CFRelease(g_mission_control_observer.observer_ref);
        CFRelease(g_mission_control_observer.ref);
    }
}

static inline bool mission_control_is_active(void)
{
    static pthread_mutex_t mc_mutex = PTHREAD_MUTEX_INITIALIZER;
    static uint64_t last_check_time = 0;
    static bool cached_state = false;
    static uint64_t mc_absent_start_time = 0;

    pthread_mutex_lock(&mc_mutex);

    uint64_t now = read_os_timer();
    uint64_t dt_ms = (now > last_check_time) ? (now - last_check_time) / 1000000ULL : 0;
    if (dt_ms < 50) {
        bool res = cached_state || (g_mission_control_mode != MISSION_CONTROL_MODE_INACTIVE);
        pthread_mutex_unlock(&mc_mutex);
        return res;
    }

    last_check_time = now;

    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!list) {
        cached_state = false;
        bool res = g_mission_control_mode != MISSION_CONTROL_MODE_INACTIVE;
        pthread_mutex_unlock(&mc_mutex);
        return res;
    }

    int count = CFArrayGetCount(list);
    bool detected_mc = false;

    for (int i = 0; i < count; ++i) {
        CFDictionaryRef d = CFArrayGetValueAtIndex(list, i);
        CFNumberRef layer_ref = CFDictionaryGetValue(d, kCGWindowLayer);
        if (!layer_ref) continue;

        int layer = 0;
        CFNumberGetValue(layer_ref, kCFNumberIntType, &layer);
        if (layer == 16 || layer == 17 || layer == 18 || layer == 19) {
            CFStringRef owner = CFDictionaryGetValue(d, kCGWindowOwnerName);
            if (owner && (CFEqual(owner, CFSTR("WindowManager")) || CFEqual(owner, CFSTR("Dock")))) {
                detected_mc = true;
                break;
            }
        }
    }

    CFRelease(list);

    bool in_mc = false;
    if (detected_mc) {
        mc_absent_start_time = 0;
        in_mc = true;
    } else {
        bool gesture_in_progress = __atomic_load_n(&__pending_gesture, __ATOMIC_RELAXED);
        uint64_t last_gesture_time = __atomic_load_n(&__last_gesture_time, __ATOMIC_RELAXED);
        uint64_t dt_gesture_ms = (now > last_gesture_time) ? (now - last_gesture_time) / 1000000ULL : 0;

        if (g_mission_control_mode != MISSION_CONTROL_MODE_INACTIVE || cached_state) {
            if (gesture_in_progress || dt_gesture_ms < 150) {
                mc_absent_start_time = 0;
                in_mc = true;
            } else {
                if (!mc_absent_start_time) mc_absent_start_time = now;
                uint64_t absent_dt_ms = (now > mc_absent_start_time) ? (now - mc_absent_start_time) / 1000000ULL : 0;
                if (absent_dt_ms < 150) {
                    in_mc = true;
                } else {
                    in_mc = false;
                    mc_absent_start_time = 0;
                }
            }
        } else {
            in_mc = false;
            mc_absent_start_time = 0;
        }
    }

    if (in_mc && g_mission_control_mode == MISSION_CONTROL_MODE_INACTIVE) {
        mission_control_record_state();
        g_mission_control_mode = MISSION_CONTROL_MODE_SHOW;
        __atomic_store_n(&g_mission_control_posted_exit, false, __ATOMIC_RELEASE);
        event_loop_post(&g_event_loop, MISSION_CONTROL_ENTER, NULL, 0);
    } else if (!in_mc && g_mission_control_mode != MISSION_CONTROL_MODE_INACTIVE && !__atomic_load_n(&g_mission_control_posted_exit, __ATOMIC_RELAXED)) {
        __atomic_store_n(&g_mission_control_posted_exit, true, __ATOMIC_RELEASE);
        event_loop_post(&g_event_loop, MISSION_CONTROL_EXIT, NULL, 0);
    }

    cached_state = in_mc;
    bool res = in_mc || (g_mission_control_mode != MISSION_CONTROL_MODE_INACTIVE);
    pthread_mutex_unlock(&mc_mutex);
    return res;
}
