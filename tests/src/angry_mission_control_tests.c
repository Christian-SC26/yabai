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
    ANGRY_ASSERT(e->type == SLS_SPACE_CREATED && (uint64_t)(intptr_t)e->context == 1001, "expected a space created event for the space to be posted");
}

ANGRY_TEST(postsSpaceDestroyedEventCarryingSpaceId) {
    uint64_t sid = 1002;
    connection_handler(1328, &sid, sizeof(sid), NULL, 0);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT(e->type == SLS_SPACE_DESTROYED && (uint64_t)(intptr_t)e->context == 1002, "expected a space destroyed event for the space to be posted");
}

ANGRY_TEST(postsWindowOrderedEventCarryingWindowId) {
    uint32_t wid = 2001;
    connection_handler(808, &wid, sizeof(wid), NULL, 0);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT(e->type == SLS_WINDOW_ORDERED && (uint32_t)(intptr_t)e->context == 2001, "expected a window ordered event for the window to be posted");
}

ANGRY_TEST(postsWindowDestroyedEventCarryingWindowId) {
    uint32_t wid = 2002;
    connection_handler(804, &wid, sizeof(wid), NULL, 0);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT(e->type == SLS_WINDOW_DESTROYED && (uint32_t)(intptr_t)e->context == 2002, "expected a window destroyed event for the window to be posted");
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
    bool match = string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_INACTIVE], "inactive") &&
                 string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW], "show") &&
                 string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_ALL_WINDOWS], "show-all-windows") &&
                 string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_FRONT_WINDOWS], "show-front-windows") &&
                 string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_DESKTOP], "show-desktop");
    ANGRY_ASSERT(match, "mission_control_mode_str contains incorrect string mappings");
}
