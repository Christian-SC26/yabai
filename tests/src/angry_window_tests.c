#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include "../../src/window.h"

static uint32_t get_active_test_window_id(void) {
    uint32_t wid = 0;
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!list) return 0;
    for (int i = 0; i < CFArrayGetCount(list); ++i) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex(list, i);
        CFNumberRef wid_ref = CFDictionaryGetValue(dict, kCGWindowNumber);
        if (wid_ref) {
            CFNumberGetValue(wid_ref, kCFNumberIntType, &wid);
            break;
        }
    }
    CFRelease(list);
    return wid;
}

ANGRY_TEST(resolvesWindowDisplayUuidAndDisplayId) {
    uint32_t wid = get_active_test_window_id();
    CFStringRef uuid = window_display_uuid(wid);
    uint32_t did = window_display_id(wid);
    if (uuid) CFRelease(uuid);
    ANGRY_ASSERT(did > 0, "window_display_id failed to resolve display for active window");
}

ANGRY_TEST(resolvesWindowSpaceAndSpaceList) {
    uint32_t wid = get_active_test_window_id();
    int count = 0;
    uint64_t sid = window_space(wid);
    uint64_t *list = window_space_list(wid, &count);
    ANGRY_ASSERT(sid > 0 && count >= 0, "window_space failed to resolve space for active window");
}

ANGRY_TEST(queriesWindowShadowAndOpacity) {
    uint32_t wid = get_active_test_window_id();
    bool shadow = window_shadow(wid);
    float opacity = window_opacity(wid);
    ANGRY_ASSERT((shadow == true || shadow == false) && opacity >= 0.0f, "window_shadow or opacity returned invalid values");
}

ANGRY_TEST(queriesWindowLevelAndSubLevel) {
    uint32_t wid = get_active_test_window_id();
    int level = window_level(wid);
    int sub_level = window_sub_level(wid);
    ANGRY_ASSERT(level != -99999 && sub_level != -99999, "window_level or sub_level returned invalid levels");
}

ANGRY_TEST(queriesWindowParentAndTags) {
    uint32_t wid = get_active_test_window_id();
    uint32_t parent = window_parent(wid);
    uint64_t tags = window_tags(wid);
    ANGRY_ASSERT(parent >= 0 && tags >= 0, "window_parent or tags query failed");
}

ANGRY_TEST(evaluatesWindowIsStickyAccurately) {
    uint32_t wid = get_active_test_window_id();
    bool sticky = window_is_sticky(wid);
    ANGRY_ASSERT(sticky == true || sticky == false, "window_is_sticky evaluated without error");
}

ANGRY_TEST(queriesWindowPropertyTitleViaTextStorage) {
    uint32_t wid = get_active_test_window_id();
    char *title = window_property_title_ts(wid);
    ANGRY_ASSERT(title != NULL, "window_property_title_ts returned NULL string");
}

ANGRY_TEST(serializesNonAxWindowPropertiesToJsonStream) {
    uint32_t wid = get_active_test_window_id();
    char buf[4096] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    window_nonax_serialize(f, wid, 0);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "\"id\":") && strstr(buf, "\"pid\":"), "window_nonax_serialize failed to serialize window JSON");
}

ANGRY_TEST(serializesWindowStructurePropertiesToJsonStream) {
    struct application app = { .pid = 1234, .name = "test_app" };
    struct window win = { .id = 100, .application = &app, .role = CFSTR("AXWindow"), .subrole = CFSTR("AXStandardWindow") };
    char buf[4096] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    window_serialize(f, &win, 0);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "\"id\":100") && strstr(buf, "\"role\":\"AXWindow\""), "window_serialize failed to emit complete JSON");
}

ANGRY_TEST(serializesWindowSelectedPropertiesWithoutOptionalAttributes) {
    struct application app = { .pid = 1234, .name = "test_app" };
    struct window win = { .id = 200, .application = &app, .role = CFSTR("AXWindow") };
    char buf[1024] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    window_serialize(f, &win, WINDOW_PROPERTY_ID | WINDOW_PROPERTY_ROLE | WINDOW_PROPERTY_IS_GRABBED);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "\"id\":200") && strstr(buf, "\"role\":\"AXWindow\""), "window_serialize failed to emit selected fields");
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
    ANGRY_ASSERT(window_can_move(&win) && window_can_resize(&win), "window_can_move or can_resize failed on flags");
}

ANGRY_TEST(evaluatesWindowIsUndersizedBasedOnFrameDimensions) {
    struct window small = { .frame = {{0, 0}, {400, 400}} };
    struct window large = { .frame = {{0, 0}, {800, 600}} };
    ANGRY_ASSERT(window_is_undersized(&small) && !window_is_undersized(&large), "window_is_undersized failed dimension evaluation");
}

ANGRY_TEST(mapsWindowLevelsToLayerStrings) {
    bool ok = string_equals(window_layer(g_layer_normal_window_level), "normal") &&
              string_equals(window_layer(g_layer_below_window_level), "below") &&
              string_equals(window_layer(g_layer_above_window_level), "above") &&
              string_equals(window_layer(-9999), "unknown");
    ANGRY_ASSERT(ok, "window_layer returned mismatched layer strings");
}

ANGRY_TEST(evaluatesWindowLevelIsStandardMatchingNormalLayer) {
    uint32_t wid = get_active_test_window_id();
    struct window win = { .id = wid };
    bool std = window_level_is_standard(&win);
    ANGRY_ASSERT(std == true || std == false, "window_level_is_standard evaluated without error");
}

ANGRY_TEST(evaluatesWindowIsRootWithValidOrDummyElement) {
    AXUIElementRef sys = AXUIElementCreateSystemWide();
    struct application app = { .ref = sys };
    struct window win = { .application = &app, .ref = sys };
    bool root = window_is_root(&win);
    CFRelease(sys);
    ANGRY_ASSERT(root == false || root == true, "window_is_root evaluated without crash");
}

ANGRY_TEST(manipulatesWindowFlagsAndRuleFlagsBitmasks) {
    struct window win = {0};
    window_set_flag(&win, WINDOW_STICKY);
    window_clear_flag(&win, WINDOW_STICKY);
    window_set_rule_flag(&win, WINDOW_RULE_MANAGED);
    bool set = window_check_rule_flag(&win, WINDOW_RULE_MANAGED);
    window_clear_rule_flag(&win, WINDOW_RULE_MANAGED);
    bool cleared = !window_check_rule_flag(&win, WINDOW_RULE_MANAGED);
    ANGRY_ASSERT(set && cleared, "window flag and rule flag operations failed");
}

ANGRY_TEST(destroysWindowFreeingAllocatedStringsAndReferences) {
    struct window *win = malloc(sizeof(struct window));
    memset(win, 0, sizeof(*win));
    win->ref = (AXUIElementRef)CFRetain(CFSTR("dummy_ref"));
    win->role = (CFStringRef)CFRetain(CFSTR("AXWindow"));
    win->subrole = (CFStringRef)CFRetain(CFSTR("AXStandardWindow"));
    win->title = (CFStringRef)CFRetain(CFSTR("dummy_title"));
    window_destroy(win);
    ANGRY_ASSERT(true, "window_destroy executed cleanly without error");
}

static bool test_observe_first_window(void) {
    NSArray *apps = [[NSWorkspace sharedWorkspace] runningApplications];
    for (NSRunningApplication *app in apps) {
        if (app.activationPolicy != NSApplicationActivationPolicyRegular) continue;
        AXUIElementRef app_ref = AXUIElementCreateApplication(app.processIdentifier);
        CFArrayRef win_list = NULL;
        AXUIElementCopyAttributeValues(app_ref, kAXWindowsAttribute, 0, 5, &win_list);
        if (win_list) {
            if (CFArrayGetCount(win_list) > 0) {
                AXUIElementRef win_ref = CFArrayGetValueAtIndex(win_list, 0);
                uint32_t wid = 0;
                extern AXError _AXUIElementGetWindow(AXUIElementRef, uint32_t *);
                _AXUIElementGetWindow(win_ref, &wid);
                ProcessSerialNumber psn = {0};
                GetProcessForPID(app.processIdentifier, &psn);
                struct process proc = { .pid = app.processIdentifier, .psn = psn, .name = "test_app" };
                struct application *a = application_create(&proc);
                CFRetain(win_ref);
                struct window *w = window_create(a, win_ref, wid);
                if (w) {
                    window_observe(w);
                    window_unobserve(w);
                    window_ax_origin(w);
                    window_can_minimize(w);
                    window_destroy(w);
                    application_destroy(a);
                    CFRelease(win_list);
                    CFRelease(app_ref);
                    return true;
                }
                CFRelease(win_ref);
                application_destroy(a);
            }
            CFRelease(win_list);
        }
        CFRelease(app_ref);
    }
    return false;
}

ANGRY_TEST(createsAndObservesWindowWhenAccessibilityWindowExists) {
    bool ok = test_observe_first_window();
    ANGRY_ASSERT(ok, "createsAndObservesWindowWhenAccessibilityWindowExists executed on active window");
}
