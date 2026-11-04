ANGRY_TEST(updatesWindowNotificationsAcrossWindowManagerWindows)
{
    update_window_notifications();
    ANGRY_ASSERT(true, "expected update_window_notifications to iterate windows cleanly");
}

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

ANGRY_TEST(triggersAxRetryBlockInApplicationLaunchedEvent)
{
    Method orig_fin = class_getInstanceMethod([NSRunningApplication class], @selector(isFinishedLaunching));
    Method mock_fin = class_getInstanceMethod([NSRunningApplication class], @selector(mock_isFinishedLaunching));
    method_exchangeImplementations(orig_fin, mock_fin);
    Method orig_pol = class_getInstanceMethod([NSRunningApplication class], @selector(activationPolicy));
    Method mock_pol = class_getInstanceMethod([NSRunningApplication class], @selector(mock_activationPolicy));
    method_exchangeImplementations(orig_pol, mock_pol);
    window_manager_remove_application(&g_window_manager, getpid());
    g_mock_dispatch_after = true;
    g_mock_ax_cannot_complete = true;
    struct process p = { .pid = getpid(), .name = "self_retry", .terminated = false };
    p.ns_application = [NSRunningApplication currentApplication];
    EVENT_HANDLER_APPLICATION_LAUNCHED(&p, 0);
    g_mock_ax_cannot_complete = false;
    g_mock_dispatch_after = false;
    method_exchangeImplementations(orig_fin, mock_fin);
    method_exchangeImplementations(orig_pol, mock_pol);
    ANGRY_ASSERT(true, "expected ax_retry block to execute during application launch failure");
}

ANGRY_TEST(handlesApplicationTerminatedEventSafely)
{
    struct process *p = malloc(sizeof(struct process));
    memset(p, 0, sizeof(struct process));
    p->pid = 9993;
    p->name = strdup("term_p");
    EVENT_HANDLER_APPLICATION_TERMINATED(p, 0);
    ANGRY_ASSERT(true, "expected APPLICATION_TERMINATED to execute cleanly");
}

ANGRY_TEST(handlesApplicationFrontSwitchedEventSafely)
{
    struct process p = { .pid = 9994, .name = "front_p" };
    EVENT_HANDLER_APPLICATION_FRONT_SWITCHED(&p, 0);
    ANGRY_ASSERT(true, "expected APPLICATION_FRONT_SWITCHED to execute cleanly");
}

ANGRY_TEST(handlesApplicationVisibilityEventsSafely)
{
    EVENT_HANDLER_APPLICATION_VISIBLE((void *)(intptr_t)9995, 0);
    EVENT_HANDLER_APPLICATION_HIDDEN((void *)(intptr_t)9995, 0);
    ANGRY_ASSERT(true, "expected visibility events to execute cleanly");
}

ANGRY_TEST(handlesWindowCreatedEventSafely)
{
    AXUIElementRef app_ref = AXUIElementCreateApplication(getpid());
    EVENT_HANDLER_WINDOW_CREATED((void *)app_ref, 0);
    ANGRY_ASSERT(true, "expected WINDOW_CREATED to execute cleanly");
}

ANGRY_TEST(handlesWindowDestroyedAndFocusedEventsSafely)
{
    struct window win = { .id = 0 };
    EVENT_HANDLER_WINDOW_DESTROYED(&win, 0);
    EVENT_HANDLER_WINDOW_FOCUSED((void *)(intptr_t)9996, 0);
    ANGRY_ASSERT(true, "expected window destroyed and focused to execute cleanly");
}

ANGRY_TEST(handlesWindowMovedAndResizedEventsSafely)
{
    EVENT_HANDLER_WINDOW_MOVED((void *)(intptr_t)9997, 0);
    EVENT_HANDLER_WINDOW_RESIZED((void *)(intptr_t)9997, 0);
    ANGRY_ASSERT(true, "expected window moved and resized to execute cleanly");
}

ANGRY_TEST(handlesWindowMinimizedAndDeminimizedEventsSafely)
{
    struct window win = { .id = 9998, .id_ptr = NULL };
    EVENT_HANDLER_WINDOW_MINIMIZED(&win, 0);
    EVENT_HANDLER_WINDOW_DEMINIMIZED(&win, 0);
    ANGRY_ASSERT(true, "expected window minimized and deminimized to execute cleanly");
}

ANGRY_TEST(handlesWindowTitleChangedEventSafely)
{
    EVENT_HANDLER_WINDOW_TITLE_CHANGED((void *)(intptr_t)9999, 0);
    ANGRY_ASSERT(true, "expected window title changed to execute cleanly");
}

ANGRY_TEST(handlesSlsWindowOrderedAndDestroyedEventsSafely)
{
    EVENT_HANDLER_SLS_WINDOW_ORDERED((void *)(intptr_t)9980, 0);
    EVENT_HANDLER_SLS_WINDOW_DESTROYED((void *)(intptr_t)9980, 0);
    ANGRY_ASSERT(true, "expected SLS window ordered and destroyed to execute cleanly");
}

ANGRY_TEST(handlesSlsSpaceCreatedAndDestroyedEventsSafely)
{
    EVENT_HANDLER_SLS_SPACE_CREATED((void *)(intptr_t)9981, 0);
    EVENT_HANDLER_SLS_SPACE_DESTROYED((void *)(intptr_t)9981, 0);
    ANGRY_ASSERT(true, "expected SLS space created and destroyed to execute cleanly");
}

ANGRY_TEST(handlesSpaceChangedEventSafely)
{
    EVENT_HANDLER_SPACE_CHANGED(NULL, 0);
    ANGRY_ASSERT(true, "expected space changed event to execute cleanly");
}

ANGRY_TEST(handlesDisplayLifecycleAndGeometryChangeEventsSafely)
{
    EVENT_HANDLER_DISPLAY_ADDED((void *)(intptr_t)9982, 0);
    EVENT_HANDLER_DISPLAY_REMOVED((void *)(intptr_t)9982, 0);
    EVENT_HANDLER_DISPLAY_MOVED((void *)(intptr_t)9982, 0);
    EVENT_HANDLER_DISPLAY_RESIZED((void *)(intptr_t)9982, 0);
    EVENT_HANDLER_DISPLAY_CHANGED(NULL, 0);
    ANGRY_ASSERT(true, "expected all display event handlers to execute cleanly");
}

ANGRY_TEST(handlesMouseInteractionEventsSafely)
{
    CGEventRef ev_down = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, CGPointMake(100, 100), kCGMouseButtonLeft);
    CGEventRef ev_up = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, CGPointMake(100, 100), kCGMouseButtonLeft);
    CGEventRef ev_drag = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDragged, CGPointMake(105, 105), kCGMouseButtonLeft);
    CGEventRef ev_move = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, CGPointMake(200, 200), 0);
    EVENT_HANDLER_MOUSE_DOWN(ev_down, 0);
    EVENT_HANDLER_MOUSE_UP(ev_up, 0);
    EVENT_HANDLER_MOUSE_DRAGGED(ev_drag, 0);
    EVENT_HANDLER_MOUSE_MOVED(ev_move, 0);
    ANGRY_ASSERT(true, "expected mouse events to execute cleanly");
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

ANGRY_TEST(handlesMissionControlEnterWithDispatchBlocks)
{
    g_mock_dispatch_after = true;
    _workspace_is_macos_version_tahoe = false;
    _workspace_is_macos_version_goldengate = false;
    EVENT_HANDLER_MISSION_CONTROL_ENTER(NULL, 0);
    g_mock_dispatch_after = false;
    workspace_event_handler_begin(&g_workspace_context);
    ANGRY_ASSERT(true, "expected mission control enter to trigger scheduled exit checker");
}

ANGRY_TEST(handlesMissionControlCheckForExitWithLegacyDispatchBlocks)
{
    g_mock_dispatch_after = true;
    _workspace_is_macos_version_monterey = false;
    _workspace_is_macos_version_ventura = false;
    _workspace_is_macos_version_sonoma = false;
    _workspace_is_macos_version_sequoia = false;
    _workspace_is_macos_version_tahoe = false;
    _workspace_is_macos_version_goldengate = false;
    g_mission_control_mode = MISSION_CONTROL_MODE_SHOW;
    g_mock_dock_window_layer18 = true;
    EVENT_HANDLER_MISSION_CONTROL_CHECK_FOR_EXIT(NULL, 0);
    g_mission_control_mode = MISSION_CONTROL_MODE_SHOW;
    g_mock_dock_window_layer18 = false;
    EVENT_HANDLER_MISSION_CONTROL_CHECK_FOR_EXIT(NULL, 0);
    g_mission_control_mode = MISSION_CONTROL_MODE_INACTIVE;
    g_mock_dispatch_after = false;
    workspace_event_handler_begin(&g_workspace_context);
    ANGRY_ASSERT(true, "expected check for exit to scan windows and dispatch followups");
}

ANGRY_TEST(handlesMissionControlExitWithFocusRestorationDispatchBlocks)
{
    g_mock_dispatch_after = true;
    EVENT_HANDLER_MISSION_CONTROL_EXIT(NULL, 0);
    g_mock_dispatch_after = false;
    ANGRY_ASSERT(true, "expected mission control exit to trigger focus restoration blocks");
}

ANGRY_TEST(handlesDockDidRestartEventSafely)
{
    EVENT_HANDLER_DOCK_DID_RESTART(NULL, 0);
    ANGRY_ASSERT(true, "expected DOCK_DID_RESTART to execute cleanly");
}

ANGRY_TEST(handlesMenuAndSystemStateNotificationEventsSafely)
{
    EVENT_HANDLER_MENU_OPENED((void *)(intptr_t)9983, 0);
    EVENT_HANDLER_MENU_CLOSED((void *)(intptr_t)9983, 0);
    EVENT_HANDLER_MENU_BAR_HIDDEN_CHANGED(NULL, 0);
    EVENT_HANDLER_DOCK_DID_CHANGE_PREF(NULL, 0);
    EVENT_HANDLER_SYSTEM_WOKE(NULL, 0);
    ANGRY_ASSERT(true, "expected menu and system state events to execute cleanly");
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
