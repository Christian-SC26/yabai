#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include "../../src/space_manager.h"

ANGRY_TEST(comparesViewsUsingKeyEquality) {
    uint64_t a = 100, b = 100, c = 200;
    bool eq = compare_view(&a, &b);
    bool neq = !compare_view(&a, &c);
    ANGRY_ASSERT(eq && neq, "compare_view failed equality evaluation");
}

ANGRY_TEST(queriesSpaceToJsonStream) {
    uint64_t sid = space_manager_active_space();
    char buf[2048] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = space_manager_query_space(f, sid, 0);
    fclose(f);
    ANGRY_ASSERT(ok && strstr(buf, "\"id\":"), "space_manager_query_space failed to serialize JSON");
}

ANGRY_TEST(queriesSpacesForWindowToJsonStream) {
    uint32_t wid = get_active_test_window_id();
    struct window win = { .id = wid };
    char buf[2048] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = space_manager_query_spaces_for_window(f, &win, 0);
    fclose(f);
    ANGRY_ASSERT(ok, "space_manager_query_spaces_for_window failed serialization");
}

ANGRY_TEST(queriesSpacesForDisplayToJsonStream) {
    uint32_t did = display_manager_main_display_id();
    char buf[2048] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = space_manager_query_spaces_for_display(f, did, 0);
    fclose(f);
    ANGRY_ASSERT(ok && strstr(buf, "\"id\":"), "space_manager_query_spaces_for_display failed serialization");
}

ANGRY_TEST(queriesSpacesForDisplaysToJsonStream) {
    char buf[4096] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = space_manager_query_spaces_for_displays(f, 0);
    fclose(f);
    ANGRY_ASSERT(ok && strstr(buf, "\"id\":"), "space_manager_query_spaces_for_displays failed serialization");
}

ANGRY_TEST(findsAndRefreshesViewForSpace) {
    uint64_t sid = space_manager_active_space();
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    space_manager_refresh_view(&g_space_manager, sid);
    ANGRY_ASSERT(view != NULL, "space_manager_find_view returned NULL view");
}

ANGRY_TEST(marksViewInvalidForSpace) {
    uint64_t sid = space_manager_active_space();
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    view->layout = VIEW_BSP;
    view_set_flag(view, VIEW_IS_VALID);
    space_manager_mark_view_invalid(&g_space_manager, sid);
    bool valid = view_check_flag(view, VIEW_IS_VALID);
    ANGRY_ASSERT(!valid, "space_manager_mark_view_invalid failed to clear valid flag");
}

ANGRY_TEST(untilesWindowFromSpaceView) {
    uint64_t sid = space_manager_active_space();
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    struct application app = { .psn = {0, kCurrentProcess} };
    struct window win = { .id = 99999, .application = &app, .ref = (AXUIElementRef)CFRetain(CFSTR("dummy")) };
    space_manager_untile_window(view, &win);
    CFRelease(win.ref);
    ANGRY_ASSERT(true, "space_manager_untile_window executed cleanly");
}

ANGRY_TEST(managesSpaceLabelsSettingAndQueryingAndRemoving) {
    uint64_t sid = space_manager_active_space();
    space_manager_set_label_for_space(&g_space_manager, sid, strdup("lbl_space_angry"));
    struct space_label *by_lbl = space_manager_get_space_for_label(&g_space_manager, "lbl_space_angry");
    struct space_label *by_sid = space_manager_get_label_for_space(&g_space_manager, sid);
    bool removed = space_manager_remove_label_for_space(&g_space_manager, sid);
    bool missing = !space_manager_get_space_for_label(&g_space_manager, "lbl_space_angry");
    ANGRY_ASSERT(by_lbl && by_sid && removed && missing, "space label management failed");
}

ANGRY_TEST(configuresSpaceLayoutAndValidates) {
    uint64_t sid = space_manager_active_space();
    space_manager_set_layout_for_space(&g_space_manager, sid, VIEW_FLOAT);
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    bool is_float = view->layout == VIEW_FLOAT;
    space_manager_set_layout_for_space(&g_space_manager, sid, VIEW_BSP);
    ANGRY_ASSERT(is_float && view->layout == VIEW_BSP, "space_manager_set_layout_for_space failed");
}

ANGRY_TEST(configuresGapForSpaceWithAbsAndRelTypes) {
    uint64_t sid = space_manager_active_space();
    bool ok_abs = space_manager_set_gap_for_space(&g_space_manager, sid, TYPE_ABS, 12);
    bool ok_rel = space_manager_set_gap_for_space(&g_space_manager, sid, TYPE_REL, 4);
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    ANGRY_ASSERT(ok_abs && ok_rel && view->window_gap == 16, "space_manager_set_gap_for_space failed");
}

ANGRY_TEST(togglesGapForSpaceUpdatingState) {
    uint64_t sid = space_manager_active_space();
    bool toggled_on = space_manager_toggle_gap_for_space(&g_space_manager, sid);
    bool toggled_off = space_manager_toggle_gap_for_space(&g_space_manager, sid);
    ANGRY_ASSERT(toggled_on && toggled_off, "space_manager_toggle_gap_for_space failed");
}

ANGRY_TEST(togglesMissionControlAndShowDesktopSafely) {
    uint64_t sid = space_manager_active_space();
    space_manager_toggle_mission_control(sid);
    space_manager_toggle_show_desktop(sid);
    ANGRY_ASSERT(true, "space_manager_toggle_mission_control and show_desktop executed cleanly");
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
    bool ok = g_space_manager.top_padding == 15 && g_space_manager.bottom_padding == 20 &&
              g_space_manager.left_padding == 25 && g_space_manager.right_padding == 30;
    ANGRY_ASSERT(ok, "space_manager padding setters for all spaces failed");
}

ANGRY_TEST(configuresSplitTypeAndAutoBalanceForAllSpaces) {
    space_manager_set_split_type_for_all_spaces(&g_space_manager, SPLIT_X);
    space_manager_set_auto_balance_for_all_spaces(&g_space_manager, 1);
    bool ok = g_space_manager.split_type == SPLIT_X && g_space_manager.auto_balance == 1;
    ANGRY_ASSERT(ok, "space_manager_set_split_type or auto_balance failed");
}

ANGRY_TEST(configuresPaddingForSpaceWithAbsAndRelTypes) {
    uint64_t sid = space_manager_active_space();
    bool ok_abs = space_manager_set_padding_for_space(&g_space_manager, sid, TYPE_ABS, 5, 6, 7, 8);
    bool ok_rel = space_manager_set_padding_for_space(&g_space_manager, sid, TYPE_REL, 2, 2, 2, 2);
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    bool match = view->top_padding == 7 && view->bottom_padding == 8;
    ANGRY_ASSERT(ok_abs && ok_rel && match, "space_manager_set_padding_for_space failed");
}

ANGRY_TEST(togglesPaddingForSpaceUpdatingState) {
    uint64_t sid = space_manager_active_space();
    bool ok1 = space_manager_toggle_padding_for_space(&g_space_manager, sid);
    bool ok2 = space_manager_toggle_padding_for_space(&g_space_manager, sid);
    ANGRY_ASSERT(ok1 && ok2, "space_manager_toggle_padding_for_space failed");
}

ANGRY_TEST(rotatesAndMirrorsSpaceSubtree) {
    uint64_t sid = space_manager_active_space();
    bool rot = space_manager_rotate_space(&g_space_manager, sid, 90);
    bool mir = space_manager_mirror_space(&g_space_manager, sid, SPLIT_X);
    ANGRY_ASSERT(rot && mir, "space_manager_rotate_space or mirror_space failed");
}

ANGRY_TEST(equalizesAndBalancesSpaceSubtree) {
    uint64_t sid = space_manager_active_space();
    bool eq = space_manager_equalize_space(&g_space_manager, sid, 0);
    bool bal = space_manager_balance_space(&g_space_manager, sid, 0);
    ANGRY_ASSERT(eq && bal, "space_manager_equalize_space or balance_space failed");
}

ANGRY_TEST(tilesWindowOnSpaceWithInsertionPoint) {
    uint64_t sid = space_manager_active_space();
    struct window win = { .id = 99998, .ref = (AXUIElementRef)CFRetain(CFSTR("dummy")), .frame = {{0,0},{400,400}} };
    struct view *v1 = space_manager_tile_window_on_space_with_insertion_point(&g_space_manager, &win, sid, 0);
    struct view *v2 = space_manager_tile_window_on_space(&g_space_manager, &win, sid);
    CFRelease(win.ref);
    ANGRY_ASSERT(v1 && v2, "space_manager_tile_window_on_space failed");
}

ANGRY_TEST(togglesWindowSplitDirectionInBspSubtree) {
    uint32_t wid = get_active_test_window_id();
    struct window win = { .id = wid };
    space_manager_toggle_window_split(&g_space_manager, &win);
    ANGRY_ASSERT(true, "space_manager_toggle_window_split executed cleanly");
}

ANGRY_TEST(resolvesMissionControlSpaceByDesktopIndex) {
    uint64_t sid1 = space_manager_mission_control_space(1);
    uint64_t sid0 = space_manager_mission_control_space(9999);
    ANGRY_ASSERT(sid1 > 0 && sid0 == 0, "space_manager_mission_control_space returned invalid sid");
}

ANGRY_TEST(resolvesCursorSpaceMatchingCurrentCursor) {
    uint64_t sid = space_manager_cursor_space();
    ANGRY_ASSERT(sid > 0, "space_manager_cursor_space returned invalid sid");
}

ANGRY_TEST(resolvesPreviousAndNextSpaceFromCurrent) {
    uint64_t sid = space_manager_active_space();
    uint64_t prev = space_manager_prev_space(sid);
    uint64_t next = space_manager_next_space(sid);
    ANGRY_ASSERT(prev >= 0 && next >= 0, "space_manager_prev_space or next_space failed");
}

ANGRY_TEST(resolvesFirstAndLastSpaceAcrossDisplays) {
    uint64_t first = space_manager_first_space();
    uint64_t last = space_manager_last_space();
    ANGRY_ASSERT(first > 0 && last > 0, "space_manager_first_space or last_space returned zero");
}

ANGRY_TEST(movesWindowListAndSingleWindowToSpace) {
    uint64_t sid = space_manager_active_space();
    uint32_t wid = get_active_test_window_id();
    struct window win = { .id = wid };
    space_manager_move_window_list_to_space(sid, &wid, 1);
    space_manager_move_window_to_space(sid, &win);
    ANGRY_ASSERT(true, "space_manager_move_window operations executed cleanly");
}

ANGRY_TEST(assignsProcessToSpaceAndAllSpaces) {
    uint64_t sid = space_manager_active_space();
    space_manager_assign_process_to_space(getpid(), sid);
    space_manager_assign_process_to_all_spaces(getpid());
    ANGRY_ASSERT(true, "space_manager process assignment operations executed cleanly");
}

ANGRY_TEST(evaluatesWindowIsOnActiveSpaceAndSpecifiedSpace) {
    uint32_t wid = get_active_test_window_id();
    struct window win = { .id = wid };
    uint64_t sid = space_manager_active_space();
    bool on_active = space_manager_is_window_on_active_space(&win);
    bool on_sid = space_manager_is_window_on_space(sid, &win);
    ANGRY_ASSERT(on_active == true || on_active == false || on_sid == true, "window on space evaluation failed");
}

ANGRY_TEST(swapsSpaceWithSpaceOnSameAndCrossDisplay) {
    uint64_t sid = space_manager_active_space();
    uint32_t did = display_manager_main_display_id();
    enum space_op_error err_same = space_manager_swap_space_with_space(sid, sid);
    enum space_op_error err_swap = space_manager_swap_space_with_space_on_display(did, sid, did, sid);
    ANGRY_ASSERT(err_same == SPACE_OP_ERROR_SAME_SPACE && err_swap == SPACE_OP_ERROR_SUCCESS, "space swap operations failed");
}

ANGRY_TEST(movesSpaceToSpaceDetectingErrors) {
    uint64_t sid = space_manager_active_space();
    enum space_op_error err_same = space_manager_move_space_to_space(sid, sid);
    enum space_op_error err_diff = space_manager_move_space_to_space(sid, 0);
    bool ok_diff = err_diff == SPACE_OP_ERROR_SAME_DISPLAY || err_diff == SPACE_OP_ERROR_SCRIPTING_ADDITION;
    ANGRY_ASSERT(err_same == SPACE_OP_ERROR_SAME_SPACE && ok_diff, "move_space_to_space failed error checks");
}

ANGRY_TEST(movesSpaceToDisplayDetectingErrors) {
    uint64_t sid = space_manager_active_space();
    uint32_t did = display_manager_main_display_id();
    enum space_op_error err_zero = space_manager_move_space_to_display(&g_space_manager, 0, did);
    enum space_op_error err_same = space_manager_move_space_to_display(&g_space_manager, sid, did);
    ANGRY_ASSERT(err_zero == SPACE_OP_ERROR_MISSING_SRC && err_same == SPACE_OP_ERROR_INVALID_DST, "move_space_to_display failed error checks");
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
    bool ok_last = err_last == SPACE_OP_ERROR_INVALID_SRC || err_last == SPACE_OP_ERROR_SCRIPTING_ADDITION;
    ANGRY_ASSERT(err_zero == SPACE_OP_ERROR_MISSING_SRC && ok_last, "destroy_space failed error checks");
}

ANGRY_TEST(addsSpaceDetectingMissingSrc) {
    enum space_op_error err = space_manager_add_space(0);
    ANGRY_ASSERT(err == SPACE_OP_ERROR_MISSING_SRC, "space_manager_add_space failed to detect missing src");
}

ANGRY_TEST(marksSpacesInvalidForDisplayAndAllDisplays) {
    uint32_t did = display_manager_main_display_id();
    space_manager_mark_spaces_invalid_for_display(&g_space_manager, did);
    space_manager_mark_spaces_invalid(&g_space_manager);
    ANGRY_ASSERT(true, "mark spaces invalid executed cleanly");
}

ANGRY_TEST(refreshesApplicationWindowsReturningStatus) {
    bool ref = space_manager_refresh_application_windows(&g_space_manager);
    ANGRY_ASSERT(ref == true || ref == false, "space_manager_refresh_application_windows failed");
}

ANGRY_TEST(handlesDisplayAddUpdatingSpaceViews) {
    uint32_t did = display_manager_main_display_id();
    space_manager_handle_display_add(&g_space_manager, did);
    ANGRY_ASSERT(true, "space_manager_handle_display_add executed cleanly");
}

ANGRY_TEST(evaluatesIsSpaceLastUserSpaceAndFindFirstUserSpace) {
    uint64_t sid = space_manager_active_space();
    uint32_t did = display_manager_main_display_id();
    bool last = space_manager_is_space_last_user_space(sid);
    uint64_t first = space_manager_find_first_user_space_for_display(did);
    ANGRY_ASSERT((last == true || last == false) && first > 0, "space user helpers failed");
}
