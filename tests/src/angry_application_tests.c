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

ANGRY_TEST(reportsTestProcessAsNotFrontmost) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    bool is_front = application_is_frontmost(app);
    application_destroy(app);
    ANGRY_ASSERT(!is_front, "expected the test process, which never activates, not to be frontmost");
}

ANGRY_TEST(reportsTestProcessAsNotHidden) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    bool hidden = application_is_hidden(app);
    application_destroy(app);
    ANGRY_ASSERT(!hidden, "expected the test process not to be hidden");
}

ANGRY_TEST(listsNoWindowsForApplicationWithoutWindows) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    CFArrayRef list = application_window_list(app);
    int count = list ? CFArrayGetCount(list) : 0;
    if (list) CFRelease(list);
    application_destroy(app);
    ANGRY_ASSERT(count == 0, "expected no windows for the test process");
}

ANGRY_TEST(reportsNoMainWindowForApplicationWithoutWindows) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    uint32_t wid = application_main_window(app);
    application_destroy(app);
    ANGRY_ASSERT(wid == 0, "expected no main window for the test process");
}

ANGRY_TEST(reportsNoFocusedWindowForApplicationWithoutWindows) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    uint32_t wid = application_focused_window(app);
    application_destroy(app);
    ANGRY_ASSERT(wid == 0, "expected no focused window for the test process");
}

ANGRY_TEST(observesEveryNotificationOfApplication) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    bool observed = application_observe(app);
    application_unobserve(app);
    application_destroy(app);
    ANGRY_ASSERT(observed, "expected every accessibility notification of the application to be observed");
}

ANGRY_TEST(stopsObservingApplicationOnUnobserve) {
    pid_t pid = getpid();
    ProcessSerialNumber psn = {0};
    GetProcessForPID(pid, &psn);
    struct process proc = { .pid = pid, .psn = psn, .name = "yabai_tests" };
    struct application *app = application_create(&proc);
    application_observe(app);
    application_unobserve(app);
    bool observing = app->is_observing;
    application_destroy(app);
    ANGRY_ASSERT(!observing, "expected the application to be no longer observed");
}

ANGRY_TEST(postsWindowCreatedEventForCreatedNotification) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXCreatedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == WINDOW_CREATED;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a window created event to be posted");
}

ANGRY_TEST(postsWindowFocusedEventForFocusedWindowChange) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXFocusedWindowChangedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == WINDOW_FOCUSED;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a window focused event to be posted");
}

ANGRY_TEST(postsWindowMovedEventForMovedNotification) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXWindowMovedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == WINDOW_MOVED;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a window moved event to be posted");
}

ANGRY_TEST(postsWindowResizedEventForResizedNotification) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXWindowResizedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == WINDOW_RESIZED;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a window resized event to be posted");
}

ANGRY_TEST(postsWindowTitleChangedEventForTitleNotification) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXTitleChangedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == WINDOW_TITLE_CHANGED;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a window title changed event to be posted");
}

ANGRY_TEST(postsMenuOpenedEventCarryingApplicationPid) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXMenuOpenedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == MENU_OPENED && (pid_t)(intptr_t)e->context == app.pid;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a menu opened event for the application to be posted");
}

ANGRY_TEST(postsMenuClosedEventCarryingApplicationPid) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXMenuClosedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == MENU_CLOSED && (pid_t)(intptr_t)e->context == app.pid;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a menu closed event for the application to be posted");
}

ANGRY_TEST(postsWindowMinimizedEventCarryingContext) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXWindowMiniaturizedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == WINDOW_MINIMIZED && e->context == &app;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a window minimized event to be posted");
}

ANGRY_TEST(postsWindowDeminimizedEventCarryingContext) {
    struct application app = { .pid = 4321 };
    AXUIElementRef ref = AXUIElementCreateApplication(app.pid);
    application_notification_handler(NULL, ref, kAXWindowDeminiaturizedNotification, &app);
    struct event *e = angry_last_posted_event();
    bool posted = e->type == WINDOW_DEMINIMIZED && e->context == &app;
    CFRelease(ref);
    ANGRY_ASSERT(posted, "expected a window deminimized event to be posted");
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
