extern struct event_loop g_event_loop;
extern void *g_workspace_context;

static TABLE_HASH_FUNC(hash_psn)
{
    return ((ProcessSerialNumber *) key)->lowLongOfPSN;
}

static TABLE_COMPARE_FUNC(compare_psn)
{
    return psn_equals(key_a, key_b);
}

static const char *process_name_blacklist[] =
{
    "Übersicht",
    "Slack Helper (Plugin)",
    "Google Chrome Helper (Plugin)",
    "qlmanage",
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static inline pid_t process_pid_for_psn(ProcessSerialNumber psn)
{
    pid_t pid = 0;
    GetProcessPID(&psn, &pid);
    return pid;
}

static struct process *process_create(ProcessSerialNumber psn, pid_t pid)
{
    ProcessInfoRec process_info = { .processInfoLength = sizeof(ProcessInfoRec) };
    GetProcessInformation(&psn, &process_info);

    CFStringRef process_name_ref = NULL;
    CopyProcessName(&psn, &process_name_ref);

    if (!process_name_ref) {
        debug("%s: could not retrieve process name! ignoring..\n", __FUNCTION__);
        return NULL;
    }

    char *process_name = cfstring_copy(process_name_ref);
    CFRelease(process_name_ref);

    if (process_info.processType == 'XPC!') {
        debug("%s: xpc service '%s' detected! ignoring..\n", __FUNCTION__, process_name);
        free(process_name);
        return NULL;
    }

    for (int i = 0; i < array_count(process_name_blacklist); ++i) {
        if (string_equals(process_name, process_name_blacklist[i])) {
            debug("%s: %s is blacklisted! ignoring..\n", __FUNCTION__, process_name);
            free(process_name);
            return NULL;
        }
    }

    struct process *process = malloc(sizeof(struct process));
    process->psn = psn;
    process->pid = pid;
    process->name = process_name;
    __atomic_store_n(&process->terminated, false, __ATOMIC_RELEASE);
    __atomic_store_n(&process->ns_application, workspace_application_create_running_ns_application(process), __ATOMIC_RELEASE);
    return process;
}

static bool process_is_being_debugged(pid_t pid)
{
    struct kinfo_proc info;
    info.kp_proc.p_flag = 0;

    size_t size = sizeof(info);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };

    sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    return ((info.kp_proc.p_flag & P_TRACED) != 0);
}

uint64_t process_manager_active_space_for_psn(int connection)
{
    uint64_t sid = 0;
    if (connection <= 0) return sid;

    int display_count;
    uint32_t *display_list = display_manager_active_display_list(&display_count);
    if (!display_list) return sid;

    int space_count = 0;
    uint64_t *space_list = NULL;

    for (int i = 0; i < display_count; ++i) {
        int count;
        uint64_t *list = display_space_list(display_list[i], &count);
        if (!list) continue;

        //
        // NOTE(asmvik): display_space_list(..) uses a linear allocator,
        // and so we only need to track the beginning of the first list along
        // with the total number of windows that have been allocated.
        //

        if (!space_list) space_list = list;
        space_count += count;
    }

    uint64_t set_tags = 0;
    uint64_t clear_tags = 0;
    uint32_t options = 0x7;

    CFArrayRef space_list_ref = cfarray_of_cfnumbers(space_list, sizeof(uint64_t), space_count, kCFNumberSInt64Type);
    CFArrayRef window_list_ref = SLSCopyWindowsWithOptionsAndTags(g_connection, connection, space_list_ref, options, &set_tags, &clear_tags);
    if (!window_list_ref) goto err;

    int count = CFArrayGetCount(window_list_ref);
    if (!count) goto out;

    CFTypeRef query = SLSWindowQueryWindows(g_connection, window_list_ref, count);
    CFTypeRef iterator = SLSWindowQueryResultCopyWindows(query);

    while (SLSWindowIteratorAdvance(iterator)) {
        uint64_t tags = SLSWindowIteratorGetTags(iterator);
        uint64_t attributes = SLSWindowIteratorGetAttributes(iterator);
        uint32_t parent_wid = SLSWindowIteratorGetParentID(iterator);
        uint32_t wid = SLSWindowIteratorGetWindowID(iterator);
        int level = SLSWindowIteratorGetLevel(iterator);

        if (parent_wid == 0) {
            if (level == 0 || level == 3 || level == 8) {
                struct window *window = window_manager_find_window(&g_window_manager, wid);
                if (window) {
                    if (!window_check_flag(window, WINDOW_STICKY) && !window_is_sticky(wid)) {
                        sid = window_space(wid);
                        if (sid) break;
                    }
                } else if (((attributes & 0x3) || (tags & 0x400000000000000)) && (((tags & 0x1)) || ((tags & 0x2) && (tags & 0x80000000)))) {
                    if (!window_is_sticky(wid)) {
                        sid = window_space(wid);
                        if (sid) break;
                    }
                }
            }
        }
    }

    CFRelease(query);
    CFRelease(iterator);
out:
    CFRelease(window_list_ref);
err:
    CFRelease(space_list_ref);
    return sid;
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static PROCESS_EVENT_HANDLER(process_handler)
{
    struct process_manager *pm = context;

    ProcessSerialNumber psn;
    if (GetEventParameter(event, kEventParamProcessID, typeProcessSerialNumber, NULL, sizeof(psn), NULL, &psn) != noErr) {
        return -1;
    }

    switch (GetEventKind(event)) {
    case kEventAppLaunched: {
        if (process_manager_find_process(pm, &psn)) {

            //
            // NOTE(asmvik): Some garbage applications (e.g Steam) are reported twice with the same PID and PSN for some hecking reason.
            // It is by definition NOT possible for two processes to exist at the same time with the same PID and PSN.
            // If we detect such a scenario we simply discard the dupe notification..
            //

            return noErr;
        }

        pid_t pid = process_pid_for_psn(psn);
        if (process_is_being_debugged(pid)) {
            debug("%s: process with pid %d is running under a debugger! ignoring..\n", __FUNCTION__, pid);
            return noErr;
        }

        struct process *process = process_create(psn, pid);
        if (!process) return noErr;

        table_add(&pm->process, &process->psn, process);
        event_loop_post(&g_event_loop, APPLICATION_LAUNCHED, process, 0);
    } break;
    case kEventAppTerminated: {
        struct process *process = process_manager_find_process(pm, &psn);
        if (!process) return noErr;

        __atomic_store_n(&process->terminated, true, __ATOMIC_RELEASE);
        table_remove(&pm->process, &psn);
        workspace_application_unobserve(g_workspace_context, process);
        __asm__ __volatile__ ("" ::: "memory");

        event_loop_post(&g_event_loop, APPLICATION_TERMINATED, process, 0);
    } break;
    case kEventAppFrontSwitched: {
        struct process *process = process_manager_find_process(pm, &psn);
        if (!process) return noErr;

        event_loop_post(&g_event_loop, APPLICATION_FRONT_SWITCHED, process, 0);
    } break;
    }

    return noErr;
}
#pragma clang diagnostic pop

static void process_manager_add_running_processes(struct process_manager *pm)
{
    ProcessSerialNumber psn = { kNoProcess, kNoProcess };
    while (GetNextProcess(&psn) == noErr) {
        pid_t pid = process_pid_for_psn(psn);
        if (process_is_being_debugged(pid)) {
            debug("%s: process with pid %d is running under a debugger! ignoring..\n", __FUNCTION__, pid);
            continue;
        }

        struct process *process = process_create(psn, pid);
        if (!process) continue;

        if (string_equals(process->name, "Finder")) {
            debug("%s: %s (%d) was found! caching psn..\n", __FUNCTION__, process->name, process->pid);
            pm->finder_psn = psn;
        }

        table_add(&pm->process, &process->psn, process);
    }
}

bool process_manager_begin(struct process_manager *pm)
{
    pm->target = GetApplicationEventTarget();
    pm->handler = NewEventHandlerUPP(process_handler);
    pm->type[0].eventClass = kEventClassApplication;
    pm->type[0].eventKind  = kEventAppLaunched;
    pm->type[1].eventClass = kEventClassApplication;
    pm->type[1].eventKind  = kEventAppTerminated;
    pm->type[2].eventClass = kEventClassApplication;
    pm->type[2].eventKind  = kEventAppFrontSwitched;
    table_init(&pm->process, 125, hash_psn, compare_psn);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    process_manager_add_running_processes(pm);
    [pool drain];

    ProcessSerialNumber front_psn;
    _SLPSGetFrontProcess(&front_psn);
    GetProcessPID(&front_psn, &pm->front_pid);
    pm->last_front_pid = pm->front_pid;
    pm->switch_event_time = GetCurrentEventTime();
    return InstallEventHandler(pm->target, pm->handler, 3, pm->type, pm, &pm->ref) == noErr;
}
#pragma clang diagnostic pop

struct process *process_manager_find_process(struct process_manager *pm, ProcessSerialNumber *psn)
{
    return table_find(&pm->process, psn);
}

void process_destroy(struct process *process)
{
    workspace_application_destroy_running_ns_application(g_workspace_context, process);
    free(process->name);
    free(process);
}
