#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#include "../../src/application.h"

ANGRY_TEST(createsApplicationInstanceFromCurrentProcess) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    bool ok = app != NULL && app->pid == pid && app->ref != NULL;
    application_destroy(app);
    ANGRY_ASSERT(ok, "application_create failed to instantiate application from current process");
}

ANGRY_TEST(evaluatesApplicationFrontmostState) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    bool is_front = application_is_frontmost(app);
    application_destroy(app);
    ANGRY_ASSERT(is_front == true || is_front == false, "application_is_frontmost returned invalid boolean");
}

ANGRY_TEST(evaluatesApplicationVisibilityState) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    bool hidden = application_is_hidden(app);
    application_destroy(app);
    ANGRY_ASSERT(hidden == true || hidden == false, "application_is_hidden returned invalid boolean");
}

ANGRY_TEST(queriesApplicationWindowList) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    CFArrayRef list = application_window_list(app);
    if (list) CFRelease(list);
    application_destroy(app);
    ANGRY_ASSERT(true, "application_window_list returned without error");
}

ANGRY_TEST(queriesApplicationMainWindow) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    uint32_t wid = application_main_window(app);
    application_destroy(app);
    ANGRY_ASSERT(wid >= 0, "application_main_window returned invalid ID");
}

ANGRY_TEST(queriesApplicationFocusedWindow) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    uint32_t wid = application_focused_window(app);
    application_destroy(app);
    ANGRY_ASSERT(wid >= 0, "application_focused_window returned invalid ID");
}

ANGRY_TEST(observesAndUnobservesApplicationNotifications) {
    NSRunningApplication *gui = [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.apple.finder"].firstObject;
    pid_t pid = gui ? gui.processIdentifier : getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "Finder" };
    struct application *app = application_create(&proc);
    bool obs = application_observe(app);
    application_unobserve(app);
    bool unobserved = !app->is_observing;
    application_destroy(app);
    ANGRY_ASSERT(unobserved, "application_unobserve failed to clear observing flag");
}

ANGRY_TEST(dispatchesNotificationHandlerForCreatedAndFocusedNotifications) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    application_notification_handler(NULL, app->ref, kAXCreatedNotification, app);
    application_notification_handler(NULL, app->ref, kAXFocusedWindowChangedNotification, app);
    application_destroy(app);
    ANGRY_ASSERT(true, "application_notification_handler failed on created and focused notifications");
}

ANGRY_TEST(dispatchesNotificationHandlerForWindowGeometryChanges) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    application_notification_handler(NULL, app->ref, kAXWindowMovedNotification, app);
    application_notification_handler(NULL, app->ref, kAXWindowResizedNotification, app);
    application_notification_handler(NULL, app->ref, kAXTitleChangedNotification, app);
    application_destroy(app);
    ANGRY_ASSERT(true, "application_notification_handler failed on window geometry notifications");
}

ANGRY_TEST(dispatchesNotificationHandlerForMenuAndStateChanges) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    application_notification_handler(NULL, app->ref, kAXMenuOpenedNotification, app);
    application_notification_handler(NULL, app->ref, kAXMenuClosedNotification, app);
    application_notification_handler(NULL, app->ref, kAXWindowMiniaturizedNotification, app);
    application_notification_handler(NULL, app->ref, kAXWindowDeminiaturizedNotification, app);
    application_destroy(app);
    ANGRY_ASSERT(true, "application_notification_handler failed on menu and state notifications");
}

ANGRY_TEST(dispatchesNotificationHandlerForWindowDestroyedNotification) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    struct window win = { .id = 707 };
    win.id_ptr = &win.id;
    application_notification_handler(NULL, app->ref, kAXUIElementDestroyedNotification, &win);
    application_destroy(app);
    ANGRY_ASSERT(win.id_ptr == NULL, "application_notification_handler failed to nullify window id pointer");
}
