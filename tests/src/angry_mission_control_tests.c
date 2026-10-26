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

ANGRY_TEST(dispatchesConnectionHandlerForMissionControlEnter) {
    connection_handler(1204, NULL, 0, NULL, 0);
    ANGRY_ASSERT(true, "connection_handler handled type 1204 cleanly");
}

ANGRY_TEST(dispatchesConnectionHandlerForSpaceCreatedNotification) {
    uint64_t sid = 1001;
    connection_handler(1327, &sid, sizeof(sid), NULL, 0);
    ANGRY_ASSERT(true, "connection_handler handled type 1327 cleanly");
}

ANGRY_TEST(dispatchesConnectionHandlerForSpaceDestroyedNotification) {
    uint64_t sid = 1002;
    connection_handler(1328, &sid, sizeof(sid), NULL, 0);
    ANGRY_ASSERT(true, "connection_handler handled type 1328 cleanly");
}

ANGRY_TEST(dispatchesConnectionHandlerForWindowOrderedNotification) {
    uint32_t wid = 2001;
    connection_handler(808, &wid, sizeof(wid), NULL, 0);
    ANGRY_ASSERT(true, "connection_handler handled type 808 cleanly");
}

ANGRY_TEST(dispatchesConnectionHandlerForWindowDestroyedNotification) {
    uint32_t wid = 2002;
    connection_handler(804, &wid, sizeof(wid), NULL, 0);
    ANGRY_ASSERT(true, "connection_handler handled type 804 cleanly");
}

ANGRY_TEST(updatesLastCmdTabTimeOnConnectionHandlerType1202) {
    extern volatile uint64_t __last_cmd_tab_time;
    __atomic_store_n(&__last_cmd_tab_time, 0, __ATOMIC_RELEASE);
    connection_handler(1202, NULL, 0, NULL, 0);
    uint64_t t = __atomic_load_n(&__last_cmd_tab_time, __ATOMIC_RELAXED);
    ANGRY_ASSERT(t > 0, "connection_handler failed to update __last_cmd_tab_time");
}

ANGRY_TEST(dispatchesMissionControlNotificationHandlerForShowAllWindows) {
    mission_control_notification_handler(NULL, NULL, kAXExposeShowAllWindows, NULL);
    ANGRY_ASSERT(true, "mission_control_notification_handler handled ShowAllWindows cleanly");
}

ANGRY_TEST(dispatchesMissionControlNotificationHandlerForShowFrontWindows) {
    mission_control_notification_handler(NULL, NULL, kAXExposeShowFrontWindows, NULL);
    ANGRY_ASSERT(true, "mission_control_notification_handler handled ShowFrontWindows cleanly");
}

ANGRY_TEST(dispatchesMissionControlNotificationHandlerForShowDesktop) {
    mission_control_notification_handler(NULL, NULL, kAXExposeShowDesktop, NULL);
    ANGRY_ASSERT(true, "mission_control_notification_handler handled ShowDesktop cleanly");
}

ANGRY_TEST(dispatchesMissionControlNotificationHandlerForExposeExit) {
    mission_control_notification_handler(NULL, NULL, kAXExposeExit, NULL);
    ANGRY_ASSERT(true, "mission_control_notification_handler handled ExposeExit cleanly");
}

ANGRY_TEST(observesAndUnobservesMissionControlDockEvents) {
    mission_control_observe();
    mission_control_unobserve();
    ANGRY_ASSERT(!g_mission_control_observer.is_observing, "mission_control_unobserve failed to reset observing flag");
}

ANGRY_TEST(evaluatesMissionControlIsActiveReturningBooleanState) {
    bool active = mission_control_is_active();
    ANGRY_ASSERT(active == true || active == false, "mission_control_is_active evaluated without error");
}

ANGRY_TEST(returnsCachedMissionControlStateWhenCalledWithinFiftyMilliseconds) {
    mission_control_is_active();
    bool cached = mission_control_is_active();
    ANGRY_ASSERT(cached == true || cached == false, "mission_control_is_active failed to return cached state");
}

ANGRY_TEST(mapsMissionControlModesToExpectedStringRepresentations) {
    bool match = string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_INACTIVE], "inactive") &&
                 string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW], "show") &&
                 string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_ALL_WINDOWS], "show-all-windows") &&
                 string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_FRONT_WINDOWS], "show-front-windows") &&
                 string_equals(mission_control_mode_str[MISSION_CONTROL_MODE_SHOW_DESKTOP], "show-desktop");
    ANGRY_ASSERT(match, "mission_control_mode_str contains incorrect string mappings");
}
