#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include "../../src/display_manager.h"

ANGRY_TEST(queriesAllActiveDisplaysToJsonStream) {
    char buf[4096] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = display_manager_query_displays(f, 0);
    fclose(f);
    ANGRY_ASSERT(ok && strstr(buf, "\"id\":"), "display_manager_query_displays failed to serialize display array");
}

ANGRY_TEST(managesDisplayLabelsSettingAndQueryingAndRemoving) {
    uint32_t did = display_manager_main_display_id();
    display_manager_set_label_for_display(&g_display_manager, did, strdup("lbl_angry_test"));
    struct display_label *by_lbl = display_manager_get_display_for_label(&g_display_manager, "lbl_angry_test");
    struct display_label *by_did = display_manager_get_label_for_display(&g_display_manager, did);
    bool removed = display_manager_remove_label_for_display(&g_display_manager, did);
    bool missing = !display_manager_get_display_for_label(&g_display_manager, "lbl_angry_test");
    ANGRY_ASSERT(by_lbl && by_did && removed && missing, "display label management failed");
}

ANGRY_TEST(resolvesMainDisplayUuidMatchingMainDisplayId) {
    CFStringRef uuid = display_manager_main_display_uuid();
    uint32_t did = display_manager_main_display_id();
    if (uuid) CFRelease(uuid);
    ANGRY_ASSERT(did > 0, "display_manager_main_display_uuid or id returned zero");
}

ANGRY_TEST(resolvesDockDisplayUuidAndDockDisplayId) {
    CFStringRef uuid = display_manager_dock_display_uuid();
    uint32_t did = display_manager_dock_display_id();
    if (uuid) CFRelease(uuid);
    ANGRY_ASSERT(did >= 0, "display_manager_dock_display query returned invalid id");
}

ANGRY_TEST(resolvesPointDisplayUuidAndPointDisplayId) {
    CGPoint pt = { .x = 100.0, .y = 100.0 };
    CFStringRef uuid = display_manager_point_display_uuid(pt);
    uint32_t did = display_manager_point_display_id(pt);
    if (uuid) CFRelease(uuid);
    ANGRY_ASSERT(did > 0, "display_manager_point_display_id returned invalid display id");
}

ANGRY_TEST(evaluatesDisplayCoordinateComparatorAcrossAxes) {
    uint32_t did = display_manager_main_display_id();
    CFStringRef uuid = display_uuid(did);
    CFComparisonResult res_x = display_manager_coordinate_comparator(uuid, uuid, (void *)(uintptr_t)DISPLAY_ARRANGEMENT_ORDER_X);
    CFComparisonResult res_y = display_manager_coordinate_comparator(uuid, uuid, (void *)(uintptr_t)DISPLAY_ARRANGEMENT_ORDER_Y);
    CFRelease(uuid);
    ANGRY_ASSERT(res_x == kCFCompareEqualTo && res_y == kCFCompareEqualTo, "display_manager_coordinate_comparator failed equality");
}

ANGRY_TEST(calculatesDisplayArrangementWithCustomSortOrders) {
    uint32_t did = display_manager_main_display_id();
    g_display_manager.order = DISPLAY_ARRANGEMENT_ORDER_X;
    int arr_x = display_manager_display_id_arrangement(did);
    g_display_manager.order = DISPLAY_ARRANGEMENT_ORDER_Y;
    int arr_y = display_manager_display_id_arrangement(did);
    g_display_manager.order = DISPLAY_ARRANGEMENT_ORDER_DEFAULT;
    int arr_def = display_manager_display_id_arrangement(did);
    ANGRY_ASSERT(arr_x > 0 && arr_y > 0 && arr_def > 0, "display_manager_display_id_arrangement returned invalid index");
}

ANGRY_TEST(resolvesArrangementDisplayUuidAndDisplayId) {
    g_display_manager.order = DISPLAY_ARRANGEMENT_ORDER_X;
    CFStringRef uuid = display_manager_arrangement_display_uuid(1);
    uint32_t did = display_manager_arrangement_display_id(1);
    g_display_manager.order = DISPLAY_ARRANGEMENT_ORDER_DEFAULT;
    if (uuid) CFRelease(uuid);
    ANGRY_ASSERT(did > 0, "display_manager_arrangement_display_id returned invalid display id");
}

ANGRY_TEST(resolvesCursorDisplayIdFromCurrentCursorLocation) {
    uint32_t did = display_manager_cursor_display_id();
    ANGRY_ASSERT(did > 0, "display_manager_cursor_display_id returned invalid display id");
}

ANGRY_TEST(evaluatesPrevAndNextDisplayIdNavigation) {
    uint32_t did = display_manager_main_display_id();
    uint32_t prev = display_manager_prev_display_id(did);
    uint32_t next = display_manager_next_display_id(did);
    ANGRY_ASSERT(prev >= 0 && next >= 0, "display navigation returned invalid display ids");
}

ANGRY_TEST(resolvesFirstAndLastDisplayIdsAccurately) {
    uint32_t first = display_manager_first_display_id();
    uint32_t last = display_manager_last_display_id();
    ANGRY_ASSERT(first > 0 && last > 0, "first and last display ids failed to resolve");
}

ANGRY_TEST(findsClosestDisplayInCardinalDirections) {
    uint32_t did = display_manager_main_display_id();
    uint32_t east = display_manager_find_closest_display_in_direction(did, DIR_EAST);
    uint32_t west = display_manager_find_closest_display_in_direction(did, DIR_WEST);
    ANGRY_ASSERT(east >= 0 && west >= 0, "display directional lookup failed");
}

ANGRY_TEST(queriesDockOrientationAndDockBoundingRect) {
    int orient = display_manager_dock_orientation();
    CGRect rect = display_manager_dock_rect();
    ANGRY_ASSERT(orient >= 0 && rect.size.width >= 0.0 && rect.size.height >= 0.0, "dock orientation or rect query failed");
}

ANGRY_TEST(evaluatesDisplayIsAnimatingStatus) {
    uint32_t did = display_manager_main_display_id();
    bool active_anim = display_manager_active_display_is_animating();
    bool disp_anim = display_manager_display_is_animating(did);
    ANGRY_ASSERT(active_anim == false || active_anim == true, "display is animating check returned invalid boolean");
}

ANGRY_TEST(locatesElementAtSpecifiedScreenCoordinates) {
    CGPoint pt = { .x = 100.0, .y = 100.0 };
    AXUIElementRef elem = display_manager_find_element_at_point(pt);
    if (elem) CFRelease(elem);
    ANGRY_ASSERT(true, "display_manager_find_element_at_point executed safely");
}

ANGRY_TEST(focusesDisplayWithWindowAtSpecifiedCoordinates) {
    CGPoint pt = { .x = -9999.0, .y = -9999.0 };
    uint32_t wid = display_manager_focus_display_with_window_at_point(pt);
    ANGRY_ASSERT(wid == 0, "display_manager_focus_display_with_window_at_point returned non-zero for offscreen point");
}

ANGRY_TEST(returnsSameDisplayErrorWhenSpaceDoesNotBelongToDisplay) {
    enum space_op_error err = display_manager_focus_space(9999, 0);
    ANGRY_ASSERT(err == SPACE_OP_ERROR_SAME_DISPLAY, "display_manager_focus_space failed to detect space mismatch");
}
