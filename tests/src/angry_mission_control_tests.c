#include <ApplicationServices/ApplicationServices.h>

ANGRY_TEST(recordsMissionControlStateCapturingFrontApplicationAndSpace) {
    __atomic_store_n(&g_mission_control_prev_sid, 0, __ATOMIC_RELEASE);
    g_space_manager.current_space_id = 999;
    g_process_manager.front_pid = 1234;
    g_window_manager.focused_window_id = 5678;
    mission_control_record_state();
    uint64_t recorded_sid = __atomic_load_n(&g_mission_control_prev_sid, __ATOMIC_RELAXED);
    __atomic_store_n(&g_mission_control_prev_sid, 0, __ATOMIC_RELEASE);
    ANGRY_ASSERT(recorded_sid == 999, "mission_control_record_state failed to capture space id");
}

ANGRY_TEST(preservesMissionControlStateIdempotencyWhenAlreadyRecorded) {
    __atomic_store_n(&g_mission_control_prev_sid, 777, __ATOMIC_RELEASE);
    g_space_manager.current_space_id = 888;
    mission_control_record_state();
    uint64_t recorded_sid = __atomic_load_n(&g_mission_control_prev_sid, __ATOMIC_RELAXED);
    __atomic_store_n(&g_mission_control_prev_sid, 0, __ATOMIC_RELEASE);
    ANGRY_ASSERT(recorded_sid == 777, "mission_control_record_state violated idempotency");
}

ANGRY_TEST(postsMissionControlEnterForDockConnectionEvent) {
    connection_handler(1204, NULL, 0, NULL, 0);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT(e->type == MISSION_CONTROL_ENTER, "expected a Mission Control enter event to be posted");
}

ANGRY_TEST(postsSpaceCreatedEventCarryingSpaceId) {
    uint64_t sid = 1001;
    connection_handler(1327, &sid, sizeof(sid), NULL, 0);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT_ALL("expected a space created event for the space to be posted",
                     e->type == SLS_SPACE_CREATED,
                     (uint64_t)(intptr_t)e->context == 1001);
}

ANGRY_TEST(postsSpaceDestroyedEventCarryingSpaceId) {
    uint64_t sid = 1002;
    connection_handler(1328, &sid, sizeof(sid), NULL, 0);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT_ALL("expected a space destroyed event for the space to be posted",
                     e->type == SLS_SPACE_DESTROYED,
                     (uint64_t)(intptr_t)e->context == 1002);
}

ANGRY_TEST(postsWindowOrderedEventCarryingWindowId) {
    uint32_t wid = 2001;
    connection_handler(808, &wid, sizeof(wid), NULL, 0);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT_ALL("expected a window ordered event for the window to be posted",
                     e->type == SLS_WINDOW_ORDERED,
                     (uint32_t)(intptr_t)e->context == 2001);
}

ANGRY_TEST(postsWindowDestroyedEventCarryingWindowId) {
    uint32_t wid = 2002;
    connection_handler(804, &wid, sizeof(wid), NULL, 0);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT_ALL("expected a window destroyed event for the window to be posted",
                     e->type == SLS_WINDOW_DESTROYED,
                     (uint32_t)(intptr_t)e->context == 2002);
}

ANGRY_TEST(updatesLastCmdTabTimeOnConnectionHandlerType1202) {
    extern volatile uint64_t __last_cmd_tab_time;
    __atomic_store_n(&__last_cmd_tab_time, 0, __ATOMIC_RELEASE);
    connection_handler(1202, NULL, 0, NULL, 0);
    uint64_t t = __atomic_load_n(&__last_cmd_tab_time, __ATOMIC_RELAXED);
    ANGRY_ASSERT(t > 0, "connection_handler failed to update __last_cmd_tab_time");
}

ANGRY_TEST(postsShowAllWindowsEventForExposeNotification) {
    mission_control_notification_handler(NULL, NULL, kAXExposeShowAllWindows, NULL);
    ANGRY_ASSERT(angry_last_posted_event()->type == MISSION_CONTROL_SHOW_ALL_WINDOWS, "expected a show-all-windows event to be posted");
}

ANGRY_TEST(postsShowFrontWindowsEventForExposeNotification) {
    mission_control_notification_handler(NULL, NULL, kAXExposeShowFrontWindows, NULL);
    ANGRY_ASSERT(angry_last_posted_event()->type == MISSION_CONTROL_SHOW_FRONT_WINDOWS, "expected a show-front-windows event to be posted");
}

ANGRY_TEST(postsShowDesktopEventForExposeNotification) {
    mission_control_notification_handler(NULL, NULL, kAXExposeShowDesktop, NULL);
    ANGRY_ASSERT(angry_last_posted_event()->type == MISSION_CONTROL_SHOW_DESKTOP, "expected a show-desktop event to be posted");
}

ANGRY_TEST(postsMissionControlExitEventForExposeNotification) {
    mission_control_notification_handler(NULL, NULL, kAXExposeExit, NULL);
    ANGRY_ASSERT(angry_last_posted_event()->type == MISSION_CONTROL_EXIT, "expected a Mission Control exit event to be posted");
}

ANGRY_TEST(observesAndUnobservesMissionControlDockEvents) {
    mission_control_observe();
    mission_control_unobserve();
    ANGRY_ASSERT(!g_mission_control_observer.is_observing, "mission_control_unobserve failed to reset observing flag");
}

ANGRY_TEST(detectsMissionControlFromDockOverlayWindow) {
    g_mock_dock_window_layer18 = true;
    ANGRY_ASSERT(mission_control_is_active(), "expected a Dock window on the Mission Control layer to mean Mission Control is open");
}

ANGRY_TEST(returnsCachedMissionControlStateWhenCalledWithinFiftyMilliseconds) {
    g_mock_dock_window_layer18 = true;
    mission_control_is_active();
    g_mock_dock_window_layer18 = false;
    ANGRY_ASSERT(mission_control_is_active(), "expected a second check right after the first to reuse its answer");
}

ANGRY_TEST(mapsMissionControlModesToExpectedStringRepresentations) {
    bool match = ANGRY_ALL(string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_INACTIVE], "inactive"), string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW], "show"), string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_ALL_WINDOWS], "show-all-windows"), string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_FRONT_WINDOWS], "show-front-windows"), string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_DESKTOP], "show-desktop"));
    ANGRY_ASSERT(match, "mission_control_mode_str contains incorrect string mappings");
}

ANGRY_TEST(treatsSpaceFocusCommandAfterMissionControlExitAsUserChoice) {
    uint64_t saved_exit = __atomic_load_n(&g_mission_control_exit_time, __ATOMIC_RELAXED);
    uint64_t saved_focus = __atomic_load_n(&g_mission_control_user_space_focus_time, __ATOMIC_RELAXED);
    __atomic_store_n(&g_mission_control_exit_time, 1000, __ATOMIC_RELEASE);
    __atomic_store_n(&g_mission_control_user_space_focus_time, 2000, __ATOMIC_RELEASE);
    bool result = mission_control_user_focused_space_since_exit();
    __atomic_store_n(&g_mission_control_exit_time, saved_exit, __ATOMIC_RELEASE);
    __atomic_store_n(&g_mission_control_user_space_focus_time, saved_focus, __ATOMIC_RELEASE);
    ANGRY_ASSERT(result, "expected a space focus command after the Mission Control exit to count as the user's choice");
}

ANGRY_TEST(ignoresSpaceFocusCommandFromBeforeMissionControlExit) {
    uint64_t saved_exit = __atomic_load_n(&g_mission_control_exit_time, __ATOMIC_RELAXED);
    uint64_t saved_focus = __atomic_load_n(&g_mission_control_user_space_focus_time, __ATOMIC_RELAXED);
    __atomic_store_n(&g_mission_control_exit_time, 2000, __ATOMIC_RELEASE);
    __atomic_store_n(&g_mission_control_user_space_focus_time, 1000, __ATOMIC_RELEASE);
    bool result = mission_control_user_focused_space_since_exit();
    __atomic_store_n(&g_mission_control_exit_time, saved_exit, __ATOMIC_RELEASE);
    __atomic_store_n(&g_mission_control_user_space_focus_time, saved_focus, __ATOMIC_RELEASE);
    ANGRY_ASSERT(!result, "expected a space focus command from before the Mission Control exit to be ignored");
}

ANGRY_TEST(ignoresSpaceFocusCommandWhenMissionControlNeverExited) {
    uint64_t saved_exit = __atomic_load_n(&g_mission_control_exit_time, __ATOMIC_RELAXED);
    uint64_t saved_focus = __atomic_load_n(&g_mission_control_user_space_focus_time, __ATOMIC_RELAXED);
    __atomic_store_n(&g_mission_control_exit_time, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_mission_control_user_space_focus_time, 1000, __ATOMIC_RELEASE);
    bool result = mission_control_user_focused_space_since_exit();
    __atomic_store_n(&g_mission_control_exit_time, saved_exit, __ATOMIC_RELEASE);
    __atomic_store_n(&g_mission_control_user_space_focus_time, saved_focus, __ATOMIC_RELEASE);
    ANGRY_ASSERT(!result, "expected no user choice to be recorded before Mission Control has exited once");
}

ANGRY_TEST(clearsUserSpaceFocusTimeWhenMissionControlRecordsNewSession) {
    __atomic_store_n(&g_mission_control_prev_sid, 777, __ATOMIC_RELEASE);
    __atomic_store_n(&g_mission_control_exit_time, 1000, __ATOMIC_RELEASE);
    __atomic_store_n(&g_mission_control_user_space_focus_time, 2000, __ATOMIC_RELEASE);
    mission_control_record_state();
    ANGRY_ASSERT(!mission_control_user_focused_space_since_exit(), "expected entering Mission Control to clear the previous user space focus timestamp");
}

ANGRY_TEST(recordsUserSpaceFocusForUserCommandsButNotInternalFocus) {
    uint64_t current_sid = 0x7E578001ULL;
    uint64_t target_sid = 0x7E578002ULL;
    g_mock_active_space = current_sid;
    uint64_t exit_time = read_os_timer() - 1000000ULL;
    __atomic_store_n(&g_mission_control_exit_time, exit_time, __ATOMIC_RELEASE);

    struct angry_sa_server server;

    __atomic_store_n(&g_mission_control_user_space_focus_time, 0, __ATOMIC_RELEASE);
    angry_sa_server_begin(&server);
    enum space_op_error internal_err = space_manager_focus_space(target_sid);
    angry_sa_server_end(&server);
    bool internal_marked = mission_control_user_focused_space_since_exit();

    __atomic_store_n(&g_mission_control_user_space_focus_time, 0, __ATOMIC_RELEASE);
    angry_sa_server_begin(&server);
    enum space_op_error focus_err = space_manager_focus_space_by_user(target_sid);
    angry_sa_server_end(&server);
    bool focus_marked = mission_control_user_focused_space_since_exit();

    __atomic_store_n(&g_mission_control_user_space_focus_time, 0, __ATOMIC_RELEASE);
    angry_sa_server_begin(&server);
    enum space_op_error switch_err = space_manager_switch_space(target_sid);
    angry_sa_server_end(&server);
    bool switch_marked = mission_control_user_focused_space_since_exit();

    __atomic_store_n(&g_mission_control_user_space_focus_time, 0, __ATOMIC_RELEASE);
    angry_sa_server_begin(&server);
    enum space_op_error display_err = display_manager_focus_space(CGMainDisplayID(), target_sid);
    angry_sa_server_end(&server);
    bool display_marked = mission_control_user_focused_space_since_exit();

    ANGRY_ASSERT_ALL("expected user space commands to mark a user choice without marking internal focus calls",
                     internal_err == SPACE_OP_ERROR_SUCCESS, !internal_marked,
                     focus_err == SPACE_OP_ERROR_SUCCESS, focus_marked,
                     switch_err == SPACE_OP_ERROR_SUCCESS, switch_marked,
                     display_err == SPACE_OP_ERROR_SUCCESS, display_marked);
}
