ANGRY_TEST(updatesApplicationOpenMenusCounterAndAdjustsFfm)
{
    struct application app = { .pid = 1001, .is_hidden = false };
    set_open_menus(&app, 1);
    set_open_menus(&app, 0);
    ANGRY_ASSERT(app.open_menus == 0, "expected open menus count to return to zero");
}

ANGRY_TEST(processesWindowDidReceiveFocusUpdatingWindowManagerState)
{
    struct application app = { .pid = getpid() };
    struct window win = { .id = 5555, .application = &app };
    window_did_receive_focus(&g_window_manager, &g_mouse_state, &win);
    ANGRY_ASSERT(g_window_manager.focused_window_id == 5555, "expected window focus to update focused_window_id");
}

ANGRY_TEST(initializesAndRunsEventLoopThreadProcessingEvents)
{
    struct event_loop loop = {0};
    bool started = event_loop_begin(&loop);
    event_loop_post(&loop, SYSTEM_WOKE, NULL, 0);
    usleep(1000);
    loop.is_running = false;
    sem_post(loop.semaphore);
    pthread_join(loop.thread, NULL);
    sem_close(loop.semaphore);
    if (loop.pool.memory) munmap(loop.pool.memory, loop.pool.size + getpagesize());
    ANGRY_ASSERT(started, "expected event_loop_begin to spawn worker thread and process event");
}

ANGRY_TEST(handlesApplicationLaunchedEventForTerminatedAndRunningProcess)
{
    struct process term_p = { .pid = 9991, .terminated = true, .name = "term" };
    EVENT_HANDLER_APPLICATION_LAUNCHED(&term_p, 0);
    g_mock_dispatch_after = true;
    struct process unlaunched_p = { .pid = 9992, .name = "unlaunched", .ns_application = NULL };
    EVENT_HANDLER_APPLICATION_LAUNCHED(&unlaunched_p, 0);
    g_mock_dispatch_after = false;
    ANGRY_ASSERT(term_p.terminated == true, "expected terminated application to return early");
}

ANGRY_TEST(remembersFrontSwitchOfApplicationNotYetObserved)
{
    struct process p = { .pid = 9994, .name = "front_p" };
    EVENT_HANDLER_APPLICATION_FRONT_SWITCHED(&p, 0);
    bool remembered = window_manager_find_lost_front_switched_event(&g_window_manager, p.pid);
    window_manager_remove_lost_front_switched_event(&g_window_manager, p.pid);
    ANGRY_ASSERT(remembered, "expected the front switch to be kept until the application is observed");
}

ANGRY_TEST(remembersFocusOfWindowNotYetObserved)
{
    EVENT_HANDLER_WINDOW_FOCUSED((void *)(intptr_t)9996, 0);
    bool remembered = window_manager_find_lost_focused_event(&g_window_manager, 9996);
    window_manager_remove_lost_focused_event(&g_window_manager, 9996);
    ANGRY_ASSERT(remembered, "expected the focus event to be kept until the window is observed");
}

ANGRY_TEST(dropsViewOfDestroyedSpace)
{
    uint64_t sid = 0x7E572000ULL;
    space_manager_find_view(&g_space_manager, sid);
    EVENT_HANDLER_SLS_SPACE_DESTROYED((void *)(intptr_t)sid, 0);
    ANGRY_ASSERT(table_find(&g_space_manager.view, &sid) == NULL, "expected the view of a destroyed space to be dropped");
}

ANGRY_TEST(keepsResizeGestureMovedAfterDragReturnsNearOrigin)
{
    struct application app = {0};
    struct window win = { .id = 9988, .application = &app, .frame = {{700,400},{400,300}} };
    win.id_ptr = &win.id;
    CGPoint origin = CGPointMake(1000, 700);
    CGPoint far = CGPointMake(1040, 740);
    CGPoint release = CGPointMake(1001, 701);

    g_mouse_state.window = &win;
    g_mouse_state.window_frame = win.frame;
    g_mouse_state.down_location = origin;
    g_mouse_state.origin_location = origin;
    g_mouse_state.resize_applied = false;
    g_mouse_state.direction = HANDLE_RIGHT | HANDLE_BOTTOM;
    g_mouse_state.current_action = MOUSE_MODE_RESIZE;
    g_mouse_state.last_moved_time = 0;

    EVENT_HANDLER_MOUSE_DRAGGED(CGEventCreateMouseEvent(NULL, kCGEventRightMouseDragged, far, kCGMouseButtonRight), 0);
    g_mouse_state.last_moved_time = 0;
    EVENT_HANDLER_MOUSE_DRAGGED(CGEventCreateMouseEvent(NULL, kCGEventRightMouseDragged, release, kCGMouseButtonRight), 0);
    bool moved = mouse_gesture_moved(&g_mouse_state, release);

    g_mouse_state.window = NULL;
    g_mouse_state.current_action = MOUSE_MODE_NONE;
    ANGRY_ASSERT(moved, "expected a resize released at the last drag position to count as moved, even near its origin");
}

ANGRY_TEST(ignoresFencelessLiveResizeStepAfterSplitHasMoved)
{
    struct application app = {0};
    struct window win = { .id = 9989, .application = &app, .frame = {{0,0},{496,300}} };
    win.id_ptr = &win.id;
    struct window_node root = { .split = SPLIT_Y, .ratio = 0.5f, .area = { 0, 0, 1000, 300 } };
    struct window_node left = { .parent = &root, .area = { 0, 0, 496, 300 }, .window_count = 1 };
    struct window_node right = { .parent = &root, .area = { 504, 0, 496, 300 } };
    left.window_list[0] = win.id;
    root.left = &left;
    root.right = &right;
    struct view view = { .layout = VIEW_BSP, .root = &root };
    table_add(&g_window_manager.managed_window, &win.id, &view);

    bool saved_live_resize = g_mouse_state.bsp_live_resize;
    g_mouse_state.bsp_live_resize = true;
    g_mouse_state.window = &win;
    g_mouse_state.window_frame = win.frame;
    g_mouse_state.down_location = CGPointMake(300, 200);
    g_mouse_state.live_resized = false;
    g_mouse_state.direction = HANDLE_RIGHT | HANDLE_BOTTOM;
    g_mouse_state.current_action = MOUSE_MODE_RESIZE;
    g_mouse_state.last_moved_time = 0;

    EVENT_HANDLER_MOUSE_DRAGGED(CGEventCreateMouseEvent(NULL, kCGEventRightMouseDragged, CGPointMake(340, 220), kCGMouseButtonRight), 0);
    g_mouse_state.last_moved_time = 0;
    EVENT_HANDLER_MOUSE_DRAGGED(CGEventCreateMouseEvent(NULL, kCGEventRightMouseDragged, CGPointMake(340, 270), kCGMouseButtonRight), 0);
    float height = win.frame.size.height;

    g_mouse_state.window = NULL;
    g_mouse_state.live_resized = false;
    g_mouse_state.current_action = MOUSE_MODE_NONE;
    g_mouse_state.bsp_live_resize = saved_live_resize;
    table_remove(&g_window_manager.managed_window, &win.id);
    ANGRY_ASSERT(height == 300.0f, "expected a vertical step with no fence to leave the window alone once the split has moved");
}

ANGRY_TEST(entersShowAllWindowsModeOnMissionControlShowAllWindows)
{
    EVENT_HANDLER_MISSION_CONTROL_SHOW_ALL_WINDOWS(NULL, 0);
    ANGRY_ASSERT(g_mission_control_mode == MISSION_CONTROL_MODE_SHOW_ALL_WINDOWS,
                 "expected Mission Control to be in show-all-windows mode");
}

ANGRY_TEST(entersShowFrontWindowsModeOnMissionControlShowFrontWindows)
{
    EVENT_HANDLER_MISSION_CONTROL_SHOW_FRONT_WINDOWS(NULL, 0);
    ANGRY_ASSERT(g_mission_control_mode == MISSION_CONTROL_MODE_SHOW_FRONT_WINDOWS,
                 "expected Mission Control to be in show-front-windows mode");
}

ANGRY_TEST(entersShowDesktopModeOnMissionControlShowDesktop)
{
    EVENT_HANDLER_MISSION_CONTROL_SHOW_DESKTOP(NULL, 0);
    ANGRY_ASSERT(g_mission_control_mode == MISSION_CONTROL_MODE_SHOW_DESKTOP,
                 "expected Mission Control to be in show-desktop mode");
}

ANGRY_TEST(entersShowModeOnMissionControlEnter)
{
    g_mock_dispatch_after = true;
    EVENT_HANDLER_MISSION_CONTROL_ENTER(NULL, 0);
    ANGRY_ASSERT(g_mission_control_mode == MISSION_CONTROL_MODE_SHOW, "expected Mission Control to be in show mode");
}

ANGRY_TEST(leavesMissionControlModeOnExit)
{
    g_mock_dispatch_after = true;
    g_mission_control_mode = MISSION_CONTROL_MODE_SHOW_DESKTOP;
    EVENT_HANDLER_MISSION_CONTROL_EXIT(NULL, 0);
    ANGRY_ASSERT(g_mission_control_mode == MISSION_CONTROL_MODE_INACTIVE, "expected Mission Control to be inactive after it exits");
}

ANGRY_TEST(countsMenuOpenedByObservedApplication)
{
    struct application app = { .pid = 9983, .name = "menu_app" };
    table_add(&g_window_manager.application, &app.pid, &app);
    EVENT_HANDLER_MENU_OPENED((void *)(intptr_t)app.pid, 0);
    int opened = app.open_menus;
    EVENT_HANDLER_MENU_CLOSED((void *)(intptr_t)app.pid, 0);
    table_remove(&g_window_manager.application, &app.pid);
    ANGRY_ASSERT(opened == 1, "expected an opened menu to be counted for its application");
}

ANGRY_TEST(countsMenuClosedByObservedApplication)
{
    struct application app = { .pid = 9984, .name = "menu_app" };
    table_add(&g_window_manager.application, &app.pid, &app);
    EVENT_HANDLER_MENU_OPENED((void *)(intptr_t)app.pid, 0);
    EVENT_HANDLER_MENU_CLOSED((void *)(intptr_t)app.pid, 0);
    table_remove(&g_window_manager.application, &app.pid);
    ANGRY_ASSERT(app.open_menus == 0, "expected a closed menu to be uncounted for its application");
}

ANGRY_TEST(answersConfigQueryReceivedOnDaemonSocket)
{
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    char payload[] = "config\0window_gap\0";
    int len = sizeof(payload);
    write(sv[1], &len, sizeof(int));
    write(sv[1], payload, len);
    EVENT_HANDLER_DAEMON_MESSAGE(NULL, sv[0]);
    char expected[32], answer[32] = {0};
    snprintf(expected, sizeof(expected), "%d\n", g_space_manager.window_gap);
    read(sv[1], answer, sizeof(answer) - 1);
    close(sv[1]);
    ANGRY_ASSERT(strcmp(answer, expected) == 0, "expected the daemon to answer with the configured window gap");
}

//
// NOTE: an application with two native tabs on fake spaces of the main display.
// The shown tab is on a space that is not visible; the hidden tab is ordered
// out, and WindowServer still reports the visible space it was last shown on,
// which is what happens when a tabbed window is moved to another space.
//
struct angry_tabbed_application
{
    struct application app;
    struct window shown, hidden;
    uint64_t visible_sid, other_sid;
    pid_t front_pid, last_front_pid;
    uint32_t focused_wid, last_wid;
    bool skip_animation;
};

static void angry_tabbed_application_begin(struct angry_tabbed_application *t, pid_t pid)
{
    t->visible_sid = 0x7E576001ULL;
    t->other_sid = 0x7E576002ULL;
    g_mock_active_space = t->visible_sid;

    t->app = (struct application) { .pid = pid, .name = "tabbed_app", .psn = { 0, (UInt32) pid }, .ref = AXUIElementCreateApplication(pid) };
    t->shown  = (struct window) { .id = 95001, .application = &t->app };
    t->hidden = (struct window) { .id = 95002, .application = &t->app };
    window_set_flag(&t->hidden, WINDOW_TAB_MEMBER);
    angry_place_window(t->shown.id, t->other_sid);
    angry_place_window(t->hidden.id, t->visible_sid);

    table_add(&g_window_manager.application, &t->app.pid, &t->app);
    table_add(&g_window_manager.window, &t->shown.id, &t->shown);
    table_add(&g_window_manager.window, &t->hidden.id, &t->hidden);

    t->front_pid = g_process_manager.front_pid;
    t->last_front_pid = g_process_manager.last_front_pid;
    t->focused_wid = g_window_manager.focused_window_id;
    t->last_wid = g_window_manager.last_window_id;
    t->skip_animation = g_space_manager.skip_window_focus_animation;

    __atomic_store_n(&__last_empty_space_focus_time, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&__last_gesture_time, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&__pending_gesture, false, __ATOMIC_RELEASE);
}

static void angry_tabbed_application_end(struct angry_tabbed_application *t)
{
    table_remove(&g_window_manager.window, &t->shown.id);
    table_remove(&g_window_manager.window, &t->hidden.id);
    table_remove(&g_window_manager.application, &t->app.pid);
    CFRelease(t->app.ref);

    g_process_manager.front_pid = t->front_pid;
    g_process_manager.last_front_pid = t->last_front_pid;
    g_window_manager.focused_window_id = t->focused_wid;
    g_window_manager.last_window_id = t->last_wid;
    g_space_manager.skip_window_focus_animation = t->skip_animation;
}

ANGRY_TEST(followsActivatedApplicationToSpaceOfItsShownTab)
{
    struct angry_tabbed_application t;
    angry_tabbed_application_begin(&t, 9978);
    g_space_manager.skip_window_focus_animation = true;

    struct process process = { .pid = t.app.pid, .name = "tabbed_app", .psn = t.app.psn };
    struct angry_sa_server server;
    angry_sa_server_begin(&server);
    EVENT_HANDLER_APPLICATION_FRONT_SWITCHED(&process, 0);
    angry_sa_server_end(&server);
    angry_tabbed_application_end(&t);

    uint64_t requested = 0;
    angry_sa_unpack(&server, 0, &requested, sizeof(requested));
    ANGRY_ASSERT_ALL("expected yabai to switch to the space of the shown tab, past the hidden tab left on the visible space",
                     angry_sa_opcode(&server) == SA_OPCODE_SPACE_FOCUS,
                     requested == t.other_sid);
}

ANGRY_TEST(handsFocusAwayFromApplicationWhoseOnlyWindowOnSpaceIsHiddenTab)
{
    struct angry_tabbed_application t;
    angry_tabbed_application_begin(&t, getpid());
    GetCurrentProcess(&t.app.psn);
    g_mock_front_is_test_process = true;

    EVENT_HANDLER_SPACE_CHANGED(NULL, 0);
    angry_tabbed_application_end(&t);

    ANGRY_ASSERT_ALL("expected focus to leave an application that shows nothing on the new space",
                     g_mock_front_process_count == 1,
                     g_mock_front_process_wid == 0);
}
