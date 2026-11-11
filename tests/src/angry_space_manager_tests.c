#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include "../../src/space_manager.h"

//
// NOTE: tests that change a view work on a space id of their own instead of
// the real active space, so they neither depend on nor leave behind a layout,
// gap or tiled window that another test would see.
//
#define ANGRY_PRIVATE_SID(n) (0x7E570000ULL + (n))

static struct view *angry_private_bsp_view(uint64_t sid)
{
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    view->layout = VIEW_BSP;
    return view;
}

ANGRY_TEST(comparesViewsUsingKeyEquality) {
    uint64_t a = 100, b = 100, c = 200;
    bool eq = compare_view(&a, &b);
    bool neq = !compare_view(&a, &c);
    ANGRY_ASSERT_ALL("compare_view failed equality evaluation",
                     eq,
                     neq);
}

ANGRY_TEST(queriesSpaceToJsonStream) {
    uint64_t sid = space_manager_active_space();
    char buf[2048] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = space_manager_query_space(f, sid, 0);
    fclose(f);
    ANGRY_ASSERT_ALL("space_manager_query_space failed to serialize JSON",
                     ok,
                     strstr(buf, "\"id\":"));
}

ANGRY_TEST(queriesSpacesForWindowToJsonStream) {
    struct window win = { .id = angry_test_window() };
    char buf[2048] = {0}, expected[64];
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    space_manager_query_spaces_for_window(f, &win, 0);
    fclose(f);
    snprintf(expected, sizeof(expected), "\"id\":%lld,", display_space_id(CGMainDisplayID()));
    ANGRY_ASSERT_ALL("expected the spaces of the test window to list its space",
                     buf[0] == '[',
                     strstr(buf, expected));
}

ANGRY_TEST(queriesSpacesForDisplayToJsonStream) {
    uint32_t did = display_manager_main_display_id();
    char buf[2048] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = space_manager_query_spaces_for_display(f, did, 0);
    fclose(f);
    ANGRY_ASSERT_ALL("space_manager_query_spaces_for_display failed serialization",
                     ok,
                     strstr(buf, "\"id\":"));
}

ANGRY_TEST(queriesSpacesForDisplaysToJsonStream) {
    char buf[4096] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = space_manager_query_spaces_for_displays(f, 0);
    fclose(f);
    ANGRY_ASSERT_ALL("space_manager_query_spaces_for_displays failed serialization",
                     ok,
                     strstr(buf, "\"id\":"));
}

ANGRY_TEST(returnsSameViewOnRepeatedLookupOfSpace) {
    uint64_t sid = ANGRY_PRIVATE_SID(12);
    struct view *first = space_manager_find_view(&g_space_manager, sid);
    ANGRY_ASSERT(space_manager_find_view(&g_space_manager, sid) == first, "expected a space to keep the view created for it");
}

ANGRY_TEST(marksViewInvalidForSpace) {
    uint64_t sid = ANGRY_PRIVATE_SID(1);
    struct view *view = angry_private_bsp_view(sid);
    view_set_flag(view, VIEW_IS_VALID);
    space_manager_mark_view_invalid(&g_space_manager, sid);
    bool valid = view_check_flag(view, VIEW_IS_VALID);
    ANGRY_ASSERT(!valid, "space_manager_mark_view_invalid failed to clear valid flag");
}

ANGRY_TEST(untilesWindowFromSpaceView) {
    uint64_t sid = ANGRY_PRIVATE_SID(2);
    struct view *view = angry_private_bsp_view(sid);
    struct window win = { .id = 99999, .ref = (AXUIElementRef)CFRetain(CFSTR("dummy")), .frame = {{0,0},{400,400}} };
    space_manager_tile_window_on_space(&g_space_manager, &win, sid);
    space_manager_untile_window(view, &win);
    CFRelease(win.ref);
    ANGRY_ASSERT(view_find_window_node(view, win.id) == NULL, "expected the untiled window to leave the view");
}

ANGRY_TEST(managesSpaceLabelsSettingAndQueryingAndRemoving) {
    uint64_t sid = space_manager_active_space();
    space_manager_set_label_for_space(&g_space_manager, sid, strdup("lbl_space_angry"));
    struct space_label *by_lbl = space_manager_get_space_for_label(&g_space_manager, "lbl_space_angry");
    struct space_label *by_sid = space_manager_get_label_for_space(&g_space_manager, sid);
    bool removed = space_manager_remove_label_for_space(&g_space_manager, sid);
    bool missing = !space_manager_get_space_for_label(&g_space_manager, "lbl_space_angry");
    ANGRY_ASSERT_ALL("space label management failed",
                     by_lbl,
                     by_sid,
                     removed,
                     missing);
}

ANGRY_TEST(configuresSpaceLayoutAndValidates) {
    uint64_t sid = ANGRY_PRIVATE_SID(3);
    angry_private_bsp_view(sid);
    space_manager_set_layout_for_space(&g_space_manager, sid, VIEW_FLOAT);
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    bool is_float = view->layout == VIEW_FLOAT;
    space_manager_set_layout_for_space(&g_space_manager, sid, VIEW_BSP);
    ANGRY_ASSERT_ALL("space_manager_set_layout_for_space failed",
                     is_float,
                     view->layout == VIEW_BSP);
}

ANGRY_TEST(configuresGapForSpaceWithAbsAndRelTypes) {
    uint64_t sid = ANGRY_PRIVATE_SID(4);
    angry_private_bsp_view(sid);
    bool ok_abs = space_manager_set_gap_for_space(&g_space_manager, sid, TYPE_ABS, 12);
    bool ok_rel = space_manager_set_gap_for_space(&g_space_manager, sid, TYPE_REL, 4);
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    ANGRY_ASSERT_ALL("space_manager_set_gap_for_space failed",
                     ok_abs,
                     ok_rel,
                     view->window_gap == 16);
}

ANGRY_TEST(togglesGapForSpaceUpdatingState) {
    uint64_t sid = ANGRY_PRIVATE_SID(5);
    angry_private_bsp_view(sid);
    bool toggled_on = space_manager_toggle_gap_for_space(&g_space_manager, sid);
    bool toggled_off = space_manager_toggle_gap_for_space(&g_space_manager, sid);
    ANGRY_ASSERT_ALL("space_manager_toggle_gap_for_space failed",
                     toggled_on,
                     toggled_off);
}

ANGRY_TEST(asksDockToOpenMissionControl) {
    space_manager_toggle_mission_control(space_manager_active_space());
    ANGRY_ASSERT(strcmp(g_mock_dock_notification, "com.apple.expose.awake") == 0, "expected the Dock to be asked to open Mission Control");
}

ANGRY_TEST(asksDockToShowDesktop) {
    space_manager_toggle_show_desktop(space_manager_active_space());
    ANGRY_ASSERT(strcmp(g_mock_dock_notification, "com.apple.showdesktop.awake") == 0, "expected the Dock to be asked to show the desktop");
}

ANGRY_TEST(configuresLayoutForAllSpaces) {
    space_manager_set_layout_for_all_spaces(&g_space_manager, VIEW_BSP);
    ANGRY_ASSERT(g_space_manager.layout == VIEW_BSP, "space_manager_set_layout_for_all_spaces failed");
}

ANGRY_TEST(configuresWindowGapForAllSpaces) {
    space_manager_set_window_gap_for_all_spaces(&g_space_manager, 10);
    ANGRY_ASSERT(g_space_manager.window_gap == 10, "space_manager_set_window_gap_for_all_spaces failed");
}

ANGRY_TEST(configuresAllPaddingDimensionsForAllSpaces) {
    space_manager_set_top_padding_for_all_spaces(&g_space_manager, 15);
    space_manager_set_bottom_padding_for_all_spaces(&g_space_manager, 20);
    space_manager_set_left_padding_for_all_spaces(&g_space_manager, 25);
    space_manager_set_right_padding_for_all_spaces(&g_space_manager, 30);
    bool ok = ANGRY_ALL(g_space_manager.top_padding == 15, g_space_manager.bottom_padding == 20, g_space_manager.left_padding == 25, g_space_manager.right_padding == 30);
    ANGRY_ASSERT(ok, "space_manager padding setters for all spaces failed");
}

ANGRY_TEST(configuresSplitTypeAndAutoBalanceForAllSpaces) {
    space_manager_set_split_type_for_all_spaces(&g_space_manager, SPLIT_X);
    space_manager_set_auto_balance_for_all_spaces(&g_space_manager, 1);
    bool ok = ANGRY_ALL(g_space_manager.split_type == SPLIT_X, g_space_manager.auto_balance == 1);
    ANGRY_ASSERT(ok, "space_manager_set_split_type or auto_balance failed");
}

ANGRY_TEST(configuresPaddingForSpaceWithAbsAndRelTypes) {
    uint64_t sid = ANGRY_PRIVATE_SID(6);
    angry_private_bsp_view(sid);
    bool ok_abs = space_manager_set_padding_for_space(&g_space_manager, sid, TYPE_ABS, 5, 6, 7, 8);
    bool ok_rel = space_manager_set_padding_for_space(&g_space_manager, sid, TYPE_REL, 2, 2, 2, 2);
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    bool match = view->top_padding == 7 && view->bottom_padding == 8;
    ANGRY_ASSERT_ALL("space_manager_set_padding_for_space failed",
                     ok_abs,
                     ok_rel,
                     match);
}

ANGRY_TEST(togglesPaddingForSpaceUpdatingState) {
    uint64_t sid = ANGRY_PRIVATE_SID(7);
    angry_private_bsp_view(sid);
    bool ok1 = space_manager_toggle_padding_for_space(&g_space_manager, sid);
    bool ok2 = space_manager_toggle_padding_for_space(&g_space_manager, sid);
    ANGRY_ASSERT_ALL("space_manager_toggle_padding_for_space failed",
                     ok1,
                     ok2);
}

ANGRY_TEST(rotatesAndMirrorsSpaceSubtree) {
    uint64_t sid = ANGRY_PRIVATE_SID(8);
    angry_private_bsp_view(sid);
    bool rot = space_manager_rotate_space(&g_space_manager, sid, 90);
    bool mir = space_manager_mirror_space(&g_space_manager, sid, SPLIT_X);
    ANGRY_ASSERT_ALL("space_manager_rotate_space or mirror_space failed",
                     rot,
                     mir);
}

ANGRY_TEST(equalizesAndBalancesSpaceSubtree) {
    uint64_t sid = ANGRY_PRIVATE_SID(9);
    angry_private_bsp_view(sid);
    bool eq = space_manager_equalize_space(&g_space_manager, sid, 0);
    bool bal = space_manager_balance_space(&g_space_manager, sid, 0);
    ANGRY_ASSERT_ALL("space_manager_equalize_space or balance_space failed",
                     eq,
                     bal);
}

ANGRY_TEST(tilesWindowOnSpaceWithInsertionPoint) {
    uint64_t sid = ANGRY_PRIVATE_SID(10);
    angry_private_bsp_view(sid);
    struct window win = { .id = 99998, .ref = (AXUIElementRef)CFRetain(CFSTR("dummy")), .frame = {{0,0},{400,400}} };
    struct view *v1 = space_manager_tile_window_on_space_with_insertion_point(&g_space_manager, &win, sid, 0);
    struct view *v2 = space_manager_tile_window_on_space(&g_space_manager, &win, sid);
    CFRelease(win.ref);
    ANGRY_ASSERT_ALL("space_manager_tile_window_on_space failed",
                     v1,
                     v2);
}

ANGRY_TEST(togglesWindowSplitDirectionInBspSubtree) {
    uint64_t sid = ANGRY_PRIVATE_SID(11);
    struct view *view = angry_private_bsp_view(sid);
    g_mock_active_space = sid;
    struct window a = { .id = 99996, .ref = (AXUIElementRef)CFRetain(CFSTR("dummy")), .frame = {{0,0},{400,400}} };
    struct window b = { .id = 99997, .ref = (AXUIElementRef)CFRetain(CFSTR("dummy")), .frame = {{0,0},{400,400}} };
    space_manager_tile_window_on_space(&g_space_manager, &a, sid);
    space_manager_tile_window_on_space(&g_space_manager, &b, sid);
    enum window_node_split before = view->root->split;
    space_manager_toggle_window_split(&g_space_manager, &b);
    enum window_node_split after = view->root->split;
    space_manager_untile_window(view, &a);
    space_manager_untile_window(view, &b);
    CFRelease(a.ref);
    CFRelease(b.ref);
    ANGRY_ASSERT(before != after, "expected the split of the parent node to change direction");
}

ANGRY_TEST(resolvesMissionControlSpaceByDesktopIndex) {
    uint64_t sid1 = space_manager_mission_control_space(1);
    uint64_t sid0 = space_manager_mission_control_space(9999);
    ANGRY_ASSERT_ALL("space_manager_mission_control_space returned invalid sid",
                     sid1 > 0,
                     sid0 == 0);
}

ANGRY_TEST(resolvesCursorSpaceMatchingCurrentCursor) {
    uint64_t sid = space_manager_cursor_space();
    ANGRY_ASSERT(sid > 0, "space_manager_cursor_space returned invalid sid");
}

ANGRY_TEST(resolvesPreviousAndNextSpaceOtherThanCurrent) {
    uint64_t sid = space_manager_active_space();
    uint64_t prev = space_manager_prev_space(sid);
    uint64_t next = space_manager_next_space(sid);
    ANGRY_ASSERT_ALL("expected the previous and next space to differ from the current one",
                     prev != sid,
                     next != sid);
}

ANGRY_TEST(resolvesFirstAndLastSpaceAcrossDisplays) {
    uint64_t first = space_manager_first_space();
    uint64_t last = space_manager_last_space();
    ANGRY_ASSERT_ALL("space_manager_first_space or last_space returned zero",
                     first > 0,
                     last > 0);
}

ANGRY_TEST(reportsUnknownWindowAsNotOnActiveSpace) {
    struct window win = { .id = 0x7FFFFFF0 };
    ANGRY_ASSERT(!space_manager_is_window_on_active_space(&win), "expected a window that does not exist to be on no space");
}

ANGRY_TEST(swapsSpaceWithSpaceOnSameAndCrossDisplay) {
    uint64_t sid = space_manager_active_space();
    uint32_t did = display_manager_main_display_id();
    enum space_op_error err_same = space_manager_swap_space_with_space(sid, sid);
    enum space_op_error err_swap = space_manager_swap_space_with_space_on_display(did, sid, did, sid);
    ANGRY_ASSERT_ALL("space swap operations failed",
                     err_same == SPACE_OP_ERROR_SAME_SPACE,
                     err_swap == SPACE_OP_ERROR_SUCCESS);
}

ANGRY_TEST(movesSpaceToSpaceDetectingErrors) {
    uint64_t sid = space_manager_active_space();
    enum space_op_error err_same = space_manager_move_space_to_space(sid, sid);
    enum space_op_error err_diff = space_manager_move_space_to_space(sid, 0);
    bool ok_diff = err_diff == SPACE_OP_ERROR_MISSING_DST;
    ANGRY_ASSERT_ALL("move_space_to_space failed error checks",
                     err_same == SPACE_OP_ERROR_SAME_SPACE,
                     ok_diff);
}

ANGRY_TEST(movesSpaceToDisplayDetectingErrors) {
    uint64_t sid = space_manager_active_space();
    uint32_t did = display_manager_main_display_id();
    enum space_op_error err_zero = space_manager_move_space_to_display(&g_space_manager, 0, did);
    enum space_op_error err_same = space_manager_move_space_to_display(&g_space_manager, sid, did);
    ANGRY_ASSERT_ALL("move_space_to_display failed error checks",
                     err_zero == SPACE_OP_ERROR_MISSING_SRC,
                     err_same == SPACE_OP_ERROR_INVALID_DST);
}

ANGRY_TEST(switchesSpaceDetectingSameSpace) {
    uint64_t sid = space_manager_active_space();
    enum space_op_error err = space_manager_switch_space(sid);
    ANGRY_ASSERT(err == SPACE_OP_ERROR_SAME_SPACE, "space_manager_switch_space failed to detect same space");
}

ANGRY_TEST(destroysSpaceDetectingMissingAndInvalidSrc) {
    enum space_op_error err_zero = space_manager_destroy_space(0);
    uint64_t sid = space_manager_active_space();
    enum space_op_error err_last = space_manager_destroy_space(sid);
    bool ok_last = err_last == SPACE_OP_ERROR_INVALID_SRC || err_last == SPACE_OP_ERROR_SCRIPTING_ADDITION || err_last == SPACE_OP_ERROR_INVALID_TYPE;
    ANGRY_ASSERT_ALL("destroy_space failed error checks",
                     err_zero == SPACE_OP_ERROR_MISSING_SRC,
                     ok_last);
}

ANGRY_TEST(addsSpaceDetectingMissingSrc) {
    enum space_op_error err = space_manager_add_space(0);
    ANGRY_ASSERT(err == SPACE_OP_ERROR_MISSING_SRC, "space_manager_add_space failed to detect missing src");
}

ANGRY_TEST(reportsNoRefreshWhenNoApplicationIsPending) {
    struct application **saved = g_window_manager.applications_to_refresh;
    g_window_manager.applications_to_refresh = NULL;
    bool refreshed = space_manager_refresh_application_windows(&g_space_manager);
    g_window_manager.applications_to_refresh = saved;
    ANGRY_ASSERT(!refreshed, "expected no refresh without pending applications");
}

ANGRY_TEST(findsFirstUserSpaceOfMainDisplay) {
    uint64_t first = space_manager_find_first_user_space_for_display(display_manager_main_display_id());
    ANGRY_ASSERT(space_is_user(first), "expected the first space found on the main display to be a user space");
}

ANGRY_TEST(enablesNaturalScrollingWhenExplicitlyOn) {
    struct space_manager sm = { .gesture_natural_scrolling = SPACE_GESTURE_NATURAL_SCROLL_ON };
    ANGRY_ASSERT(space_manager_is_natural_scrolling_active(&sm), "expected natural scrolling to be active when turned on");
}

ANGRY_TEST(disablesNaturalScrollingWhenExplicitlyOff) {
    struct space_manager sm = { .gesture_natural_scrolling = SPACE_GESTURE_NATURAL_SCROLL_OFF };
    ANGRY_ASSERT(!space_manager_is_natural_scrolling_active(&sm), "expected natural scrolling to be inactive when turned off");
}
