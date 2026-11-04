#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include "../../src/window.h"

ANGRY_TEST(resolvesDisplayOfTestWindow) {
    ANGRY_ASSERT(window_display_id(angry_test_window()) == CGMainDisplayID(), "expected the test window to be on the main display");
}

ANGRY_TEST(resolvesSpaceOfTestWindow) {
    uint64_t sid = window_space(angry_test_window());
    ANGRY_ASSERT(sid == display_space_id(CGMainDisplayID()), "expected the test window to be on the current space of the main display");
}

ANGRY_TEST(reportsNormalLevelOfTestWindow) {
    ANGRY_ASSERT(window_level(angry_test_window()) == kCGNormalWindowLevel, "expected the test window to be at the normal window level");
}

ANGRY_TEST(readsTitleOfTestWindow) {
    char *title = window_property_title_ts(angry_test_window());
    ANGRY_ASSERT_ALL("expected the title the test window was given",
                     title,
                     strcmp(title, ANGRY_TEST_WINDOW_TITLE) == 0);
}

ANGRY_TEST(serializesIdAndPidOfTestWindow) {
    uint32_t wid = angry_test_window();
    char buf[4096] = {0}, id[32], pid[32];
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    window_nonax_serialize(f, wid, 0);
    fclose(f);
    snprintf(id, sizeof(id), "\"id\":%d,", wid);
    snprintf(pid, sizeof(pid), "\"pid\":%d,", getpid());
    ANGRY_ASSERT_ALL("expected the json to carry the id and pid of the test window",
                     strstr(buf, id),
                     strstr(buf, pid));
}

ANGRY_TEST(serializesWindowStructurePropertiesToJsonStream) {
    struct application app = { .pid = 1234, .name = "test_app" };
    struct window win = { .id = 100, .application = &app, .role = CFSTR("AXWindow"), .subrole = CFSTR("AXStandardWindow") };
    char buf[4096] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    window_serialize(f, &win, 0);
    fclose(f);
    ANGRY_ASSERT_ALL("window_serialize failed to emit complete JSON",
                     strstr(buf, "\"id\":100"),
                     strstr(buf, "\"role\":\"AXWindow\""));
}

ANGRY_TEST(serializesWindowSelectedPropertiesWithoutOptionalAttributes) {
    struct application app = { .pid = 1234, .name = "test_app" };
    struct window win = { .id = 200, .application = &app, .role = CFSTR("AXWindow") };
    char buf[1024] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    window_serialize(f, &win, WINDOW_PROPERTY_ID | WINDOW_PROPERTY_ROLE | WINDOW_PROPERTY_IS_GRABBED);
    fclose(f);
    ANGRY_ASSERT_ALL("window_serialize failed to emit selected fields",
                     strstr(buf, "\"id\":200"),
                     strstr(buf, "\"role\":\"AXWindow\""));
}

ANGRY_TEST(formatsWindowTitleUsingTemporaryStorage) {
    struct window win = { .title = CFSTR("Angry Test Title") };
    char *t = window_title_ts(&win);
    ANGRY_ASSERT(string_equals(t, "Angry Test Title"), "window_title_ts returned mismatched title");
}

ANGRY_TEST(formatsWindowRoleUsingTemporaryStorage) {
    struct window win = { .role = CFSTR("AXWindow") };
    char *r = window_role_ts(&win);
    ANGRY_ASSERT(string_equals(r, "AXWindow"), "window_role_ts returned mismatched role");
}

ANGRY_TEST(formatsWindowSubroleUsingTemporaryStorage) {
    struct window win = { .subrole = CFSTR("AXStandardWindow") };
    char *sr = window_subrole_ts(&win);
    ANGRY_ASSERT(string_equals(sr, "AXStandardWindow"), "window_subrole_ts returned mismatched subrole");
}

ANGRY_TEST(identifiesRealWindowWithStandardRoleAndSubrole) {
    struct window win = { .role = kAXWindowRole, .subrole = kAXStandardWindowSubrole };
    ANGRY_ASSERT(window_is_real(&win) == true, "window_is_real failed on standard window");
}

ANGRY_TEST(identifiesStandardWindowWithStandardSubrole) {
    struct window win = { .role = kAXWindowRole, .subrole = kAXStandardWindowSubrole };
    ANGRY_ASSERT(window_is_standard(&win) == true, "window_is_standard failed on standard window");
}

ANGRY_TEST(identifiesUnknownSubroleWindow) {
    struct window win = { .subrole = kAXUnknownSubrole };
    ANGRY_ASSERT(window_is_unknown(&win) == true, "window_is_unknown failed on unknown subrole");
}

ANGRY_TEST(evaluatesWindowCanMoveAndCanResizeFromFlags) {
    struct window win = { .flags = WINDOW_MOVABLE | WINDOW_RESIZABLE };
    ANGRY_ASSERT_ALL("window_can_move or can_resize failed on flags",
                     window_can_move(&win),
                     window_can_resize(&win));
}

ANGRY_TEST(evaluatesWindowIsUndersizedBasedOnFrameDimensions) {
    struct window small = { .frame = {{0, 0}, {400, 400}} };
    struct window large = { .frame = {{0, 0}, {800, 600}} };
    ANGRY_ASSERT_ALL("window_is_undersized failed dimension evaluation",
                     window_is_undersized(&small),
                     !window_is_undersized(&large));
}

ANGRY_TEST(mapsWindowLevelsToLayerStrings) {
    bool ok = ANGRY_ALL(string_equals(window_layer(g_layer_normal_window_level), "normal"), string_equals(window_layer(g_layer_below_window_level), "below"), string_equals(window_layer(g_layer_above_window_level), "above"), string_equals(window_layer(-9999), "unknown"));
    ANGRY_ASSERT(ok, "window_layer returned mismatched layer strings");
}

ANGRY_TEST(rejectsSystemWideElementAsRootWindow) {
    AXUIElementRef sys = AXUIElementCreateSystemWide();
    struct application app = { .ref = sys };
    struct window win = { .application = &app, .ref = sys };
    bool root = window_is_root(&win);
    CFRelease(sys);
    ANGRY_ASSERT(!root, "expected the system-wide element not to be a root window");
}

ANGRY_TEST(manipulatesWindowFlagsAndRuleFlagsBitmasks) {
    struct window win = {0};
    window_set_flag(&win, WINDOW_STICKY);
    window_clear_flag(&win, WINDOW_STICKY);
    window_set_rule_flag(&win, WINDOW_RULE_MANAGED);
    bool set = window_check_rule_flag(&win, WINDOW_RULE_MANAGED);
    window_clear_rule_flag(&win, WINDOW_RULE_MANAGED);
    bool cleared = !window_check_rule_flag(&win, WINDOW_RULE_MANAGED);
    ANGRY_ASSERT_ALL("window flag and rule flag operations failed",
                     set,
                     cleared);
}

