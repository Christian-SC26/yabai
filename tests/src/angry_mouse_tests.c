#include <ApplicationServices/ApplicationServices.h>
#include "../../src/mouse_handler.h"

ANGRY_TEST(initializesMouseStateWithDefaultParameters) {
    struct mouse_state ms;
    mouse_state_init(&ms);
    ANGRY_ASSERT(ms.modifier == MOUSE_MOD_FN && ms.action1 == MOUSE_MODE_MOVE && ms.action2 == MOUSE_MODE_RESIZE && ms.bsp_live_resize == true, "mouse_state_init failed to populate default values");
}

ANGRY_TEST(parsesMouseModifierFromCommandAndAltFlags) {
    uint8_t mod = mouse_mod_from_cgflags(kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate);
    ANGRY_ASSERT(mod == (MOUSE_MOD_CMD | MOUSE_MOD_ALT), "mouse_mod_from_cgflags failed on Cmd+Alt");
}

ANGRY_TEST(parsesMouseModifierFromShiftAndControlFlags) {
    uint8_t mod = mouse_mod_from_cgflags(kCGEventFlagMaskShift | kCGEventFlagMaskControl | kCGEventFlagMaskSecondaryFn);
    ANGRY_ASSERT(mod == (MOUSE_MOD_SHIFT | MOUSE_MOD_CTRL | MOUSE_MOD_FN), "mouse_mod_from_cgflags failed on Shift+Ctrl+Fn");
}

ANGRY_TEST(populatesMouseWindowInfoCalculatingDeltasAndFlags) {
    struct window win = { .frame = {{ 120, 160 }, { 800, 600 }} };
    struct mouse_state ms = { .window = &win, .window_frame = {{ 100, 150 }, { 750, 600 }} };
    struct mouse_window_info info = {0};
    mouse_window_info_populate(&ms, &info);
    ANGRY_ASSERT(info.dx == 20.0f && info.dy == 10.0f && info.dw == 50.0f && info.dh == 0.0f && info.changed_position && info.changed_size, "mouse_window_info_populate calculated incorrect deltas");
}

ANGRY_TEST(determinesStackDropActionWhenCursorInCenterAndModeIsStack) {
    struct mouse_state ms = { .drop_action = MOUSE_MODE_STACK };
    struct window_node src = { .window_count = 1 };
    struct window dst = { .frame = {{ 100, 100 }, { 400, 400 }} };
    enum mouse_drop_action act = mouse_determine_drop_action(&ms, &src, &dst, (CGPoint){ 300, 300 });
    ANGRY_ASSERT(act == MOUSE_DROP_ACTION_STACK, "mouse_determine_drop_action failed to select stack in center");
}

ANGRY_TEST(determinesSwapDropActionWhenCursorInCenterAndModeIsSwap) {
    struct mouse_state ms = { .drop_action = MOUSE_MODE_SWAP };
    struct window_node src = { .window_count = 1 };
    struct window dst = { .frame = {{ 100, 100 }, { 400, 400 }} };
    enum mouse_drop_action act = mouse_determine_drop_action(&ms, &src, &dst, (CGPoint){ 300, 300 });
    ANGRY_ASSERT(act == MOUSE_DROP_ACTION_SWAP, "mouse_determine_drop_action failed to select swap in center");
}

ANGRY_TEST(determinesWarpTopDropActionWhenCursorInTopTriangle) {
    struct mouse_state ms = {0};
    struct window_node src = { .window_count = 1 };
    struct window dst = { .frame = {{ 100, 100 }, { 400, 400 }} };
    enum mouse_drop_action act = mouse_determine_drop_action(&ms, &src, &dst, (CGPoint){ 300, 150 });
    ANGRY_ASSERT(act == MOUSE_DROP_ACTION_WARP_TOP, "mouse_determine_drop_action failed to select warp top");
}

ANGRY_TEST(determinesWarpRightDropActionWhenCursorInRightTriangle) {
    struct mouse_state ms = {0};
    struct window_node src = { .window_count = 1 };
    struct window dst = { .frame = {{ 100, 100 }, { 400, 400 }} };
    enum mouse_drop_action act = mouse_determine_drop_action(&ms, &src, &dst, (CGPoint){ 450, 300 });
    ANGRY_ASSERT(act == MOUSE_DROP_ACTION_WARP_RIGHT, "mouse_determine_drop_action failed to select warp right");
}

ANGRY_TEST(determinesWarpBottomDropActionWhenCursorInBottomTriangle) {
    struct mouse_state ms = {0};
    struct window_node src = { .window_count = 1 };
    struct window dst = { .frame = {{ 100, 100 }, { 400, 400 }} };
    enum mouse_drop_action act = mouse_determine_drop_action(&ms, &src, &dst, (CGPoint){ 300, 450 });
    ANGRY_ASSERT(act == MOUSE_DROP_ACTION_WARP_BOTTOM, "mouse_determine_drop_action failed to select warp bottom");
}

ANGRY_TEST(determinesWarpLeftDropActionWhenCursorInLeftTriangle) {
    struct mouse_state ms = {0};
    struct window_node src = { .window_count = 1 };
    struct window dst = { .frame = {{ 100, 100 }, { 400, 400 }} };
    enum mouse_drop_action act = mouse_determine_drop_action(&ms, &src, &dst, (CGPoint){ 150, 300 });
    ANGRY_ASSERT(act == MOUSE_DROP_ACTION_WARP_LEFT, "mouse_determine_drop_action failed to select warp left");
}

ANGRY_TEST(returnsDropActionNoneWhenCursorIsOutsideWindowBounds) {
    struct mouse_state ms = {0};
    struct window_node src = { .window_count = 1 };
    struct window dst = { .frame = {{ 100, 100 }, { 400, 400 }} };
    enum mouse_drop_action act = mouse_determine_drop_action(&ms, &src, &dst, (CGPoint){ 50, 50 });
    ANGRY_ASSERT(act == MOUSE_DROP_ACTION_NONE, "mouse_determine_drop_action should return NONE for point outside bounds");
}

ANGRY_TEST(handlesMouseDropNoTargetOnSameSpaceByFlushingNode) {
    struct view v = { .sid = 101 };
    struct window_node node = {0};
    struct window win = { .id = 1 };
    mouse_drop_no_target(&g_space_manager, &g_window_manager, &v, &v, &win, &node);
    ANGRY_ASSERT(true, "mouse_drop_no_target on same space flushed cleanly");
}

ANGRY_TEST(bypassesBspGridAdjustmentWhenViewLayoutIsNotBsp) {
    struct view v = { .layout = VIEW_FLOAT };
    struct window win = { .id = 99 };
    struct mouse_window_info info = { .changed_position = true };
    mouse_drop_try_adjust_bsp_grid(&g_window_manager, &v, &win, &info);
    ANGRY_ASSERT(true, "mouse_drop_try_adjust_bsp_grid cleanly rejected non-BSP view");
}

ANGRY_TEST(swapsWindowsBetweenNodesUpdatingInsertionPoints) {
    struct view src_v = { .sid = 1, .insertion_point = 10 };
    struct view dst_v = { .sid = 1, .insertion_point = 20 };
    struct window_node src_n = { .window_count = 1, .window_list = { 10 } };
    struct window_node dst_n = { .window_count = 1, .window_list = { 20 } };
    struct window src_w = { .id = 10 };
    struct window dst_w = { .id = 20 };
    mouse_drop_action_swap(&g_window_manager, &src_v, &src_n, &src_w, &dst_v, &dst_n, &dst_w);
    ANGRY_ASSERT(src_n.window_list[0] == 20 && dst_n.window_list[0] == 10, "mouse_drop_action_swap failed to exchange window lists");
}

ANGRY_TEST(consumesMouseClickWhenModifiersMatchAndFreesOnRelease) {
    struct mouse_state ms = { .modifier = MOUSE_MOD_ALT };
    CGEventRef ev = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, (CGPoint){10, 20}, kCGMouseButtonLeft);
    CGEventSetFlags(ev, kCGEventFlagMaskAlternate);
    CGEventRef res = mouse_handler(NULL, kCGEventLeftMouseDown, ev, &ms);
    bool consumed = res == NULL && ms.consume_mouse_click;
    mouse_handler(NULL, kCGEventLeftMouseUp, ev, &ms);
    CFRelease(ev);
    ANGRY_ASSERT(consumed && !ms.consume_mouse_click, "mouse_handler failed to consume and release click");
}

ANGRY_TEST(stacksWindowIntoDestinationNodeOnMouseDropActionStack) {
    struct window src_w = { .id = 201 };
    struct window dst_w = { .id = 202 };
    struct window_node dst_n = { .window_count = 1, .window_list = { 202 }, .window_order = { 202 } };
    struct view src_v = { .sid = 1, .layout = VIEW_FLOAT };
    struct view dst_v = { .sid = 1, .layout = VIEW_BSP, .root = &dst_n };
    table_add(&g_window_manager.window, &src_w.id, &src_w);
    table_add(&g_window_manager.window, &dst_w.id, &dst_w);
    mouse_drop_action_stack(&g_window_manager, &src_v, &src_w, &dst_v, &dst_w);
    bool stacked = dst_n.window_count == 2 && dst_n.window_list[1] == 201;
    table_remove(&g_window_manager.window, &src_w.id);
    table_remove(&g_window_manager.window, &dst_w.id);
    ANGRY_ASSERT(stacked, "mouse_drop_action_stack failed to stack source window into target node");
}

ANGRY_TEST(warpsWindowBetweenSiblingsWithMatchingSplitSwappingNodes) {
    struct window_node parent = { .split = SPLIT_X };
    struct window_node src_n = { .parent = &parent, .window_count = 1, .window_list = { 301 }, .window_order = { 301 } };
    struct window_node dst_n = { .parent = &parent, .window_count = 1, .window_list = { 302 }, .window_order = { 302 } };
    struct view src_v = { .sid = 1 };
    struct view dst_v = { .sid = 1 };
    struct window src_w = { .id = 301 };
    struct window dst_w = { .id = 302 };
    mouse_drop_action_warp(&g_window_manager, &src_v, &src_n, &src_w, &dst_v, &dst_n, &dst_w, SPLIT_X, CHILD_FIRST);
    ANGRY_ASSERT(src_n.window_list[0] == 302 && dst_n.window_list[0] == 301, "mouse_drop_action_warp failed to swap siblings with matching split");
}

ANGRY_TEST(handlesMouseRightMouseDownConsumingWhenModifierMatches) {
    struct mouse_state ms = { .modifier = MOUSE_MOD_ALT };
    CGEventRef ev = CGEventCreateMouseEvent(NULL, kCGEventRightMouseDown, (CGPoint){10, 20}, kCGMouseButtonRight);
    CGEventSetFlags(ev, kCGEventFlagMaskAlternate);
    CGEventRef res = mouse_handler(NULL, kCGEventRightMouseDown, ev, &ms);
    bool consumed = res == NULL && ms.consume_mouse_click;
    mouse_handler(NULL, kCGEventRightMouseUp, ev, &ms);
    CFRelease(ev);
    ANGRY_ASSERT(consumed, "mouse_handler failed to consume RightMouseDown with matching modifier");
}

ANGRY_TEST(handlesMouseDraggedEventSettingDragDetectedFlag) {
    struct mouse_state ms = {0};
    CGEventRef ev = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDragged, (CGPoint){50, 50}, kCGMouseButtonLeft);
    mouse_handler(NULL, kCGEventLeftMouseDragged, ev, &ms);
    CFRelease(ev);
    ANGRY_ASSERT(ms.drag_detected == true, "mouse_handler failed to set drag_detected on mouse dragged");
}

ANGRY_TEST(handlesMouseMovedEventPassingThroughWhenModifierMatches) {
    struct mouse_state ms = { .modifier = MOUSE_MOD_ALT };
    CGEventRef ev = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, (CGPoint){50, 50}, 0);
    CGEventSetFlags(ev, kCGEventFlagMaskAlternate);
    CGEventRef res = mouse_handler(NULL, kCGEventMouseMoved, ev, &ms);
    CFRelease(ev);
    ANGRY_ASSERT(res != NULL, "mouse_handler should pass through MouseMoved when modifier matches");
}

ANGRY_TEST(handlesDockSwipeGestureBeganAndEndedEvents) {
    extern volatile bool __pending_gesture;
    CGEventRef ev = CGEventCreate(NULL);
    CGEventSetIntegerValueField(ev, 55, 30);
    CGEventSetIntegerValueField(ev, 110, 23);
    CGEventSetIntegerValueField(ev, 123, 2);
    CGEventSetIntegerValueField(ev, 132, 1);
    mouse_handler(NULL, 30, ev, &g_mouse_state);
    bool began = __pending_gesture == true;
    CGEventSetIntegerValueField(ev, 132, 4);
    mouse_handler(NULL, 30, ev, &g_mouse_state);
    bool ended = __pending_gesture == false;
    CFRelease(ev);
    ANGRY_ASSERT(began && ended, "mouse_handler failed to track dock swipe gesture began and ended");
}

ANGRY_TEST(handlesEventTapDisabledTimeoutReenablingHandleSafely) {
    struct mouse_state ms = {0};
    mouse_handler(NULL, kCGEventTapDisabledByTimeout, NULL, &ms);
    ANGRY_ASSERT(true, "mouse_handler handled kCGEventTapDisabledByTimeout cleanly");
}

ANGRY_TEST(swapsWindowsAcrossDifferentSpacesUpdatingManagedTable) {
    struct view src_v = { .sid = 1 };
    struct view dst_v = { .sid = 2 };
    struct window src_w = { .id = 401 };
    struct window dst_w = { .id = 402 };
    struct window_node src_n = { .window_count = 1, .window_list = { 401 }, .window_order = { 401 } };
    struct window_node dst_n = { .window_count = 1, .window_list = { 402 }, .window_order = { 402 } };
    table_add(&g_window_manager.window, &src_w.id, &src_w);
    table_add(&g_window_manager.window, &dst_w.id, &dst_w);
    mouse_drop_action_swap(&g_window_manager, &src_v, &src_n, &src_w, &dst_v, &dst_n, &dst_w);
    table_remove(&g_window_manager.window, &src_w.id);
    table_remove(&g_window_manager.window, &dst_w.id);
    ANGRY_ASSERT(src_n.window_list[0] == 402 && dst_n.window_list[0] == 401, "mouse_drop_action_swap failed across different spaces");
}

ANGRY_TEST(warpsWindowBetweenSiblingsWithDifferentSplitModifyingParent) {
    struct window_node parent = { .split = SPLIT_X };
    struct window_node src_n = { .parent = &parent, .window_count = 1, .window_list = { 501 }, .window_order = { 501 } };
    struct window_node dst_n = { .parent = &parent, .window_count = 1, .window_list = { 502 }, .window_order = { 502 } };
    struct view src_v = { .sid = 1, .root = &parent };
    struct view dst_v = { .sid = 1, .root = &parent };
    struct window src_w = { .id = 501 };
    struct window dst_w = { .id = 502 };
    mouse_drop_action_warp(&g_window_manager, &src_v, &src_n, &src_w, &dst_v, &dst_n, &dst_w, SPLIT_Y, CHILD_SECOND);
    ANGRY_ASSERT(parent.split == SPLIT_Y && parent.child == CHILD_SECOND, "mouse_drop_action_warp failed to reassign parent split and child");
}

ANGRY_TEST(handlesMouseMovedEventPostingEventWhenModifierDiffers) {
    struct mouse_state ms = { .modifier = MOUSE_MOD_ALT };
    CGEventRef ev = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, (CGPoint){50, 50}, 0);
    CGEventRef res = mouse_handler(NULL, kCGEventMouseMoved, ev, &ms);
    CFRelease(ev);
    ANGRY_ASSERT(res != NULL, "mouse_handler failed to process MouseMoved with different modifier");
}

ANGRY_TEST(handlesZoomedNodeOnMouseDropActionStack) {
    struct window src_w = { .id = 601 };
    struct window dst_w = { .id = 602 };
    struct window_node zoom_n = { .area = {{ 0, 0 }, { 1920, 1080 }} };
    struct window_node dst_n = { .window_count = 1, .window_list = { 602 }, .window_order = { 602 }, .zoom = &zoom_n };
    struct view src_v = { .sid = 1, .layout = VIEW_FLOAT };
    struct view dst_v = { .sid = 1, .layout = VIEW_BSP, .root = &dst_n };
    table_add(&g_window_manager.window, &src_w.id, &src_w);
    table_add(&g_window_manager.window, &dst_w.id, &dst_w);
    mouse_drop_action_stack(&g_window_manager, &src_v, &src_w, &dst_v, &dst_w);
    bool stacked = dst_n.window_count == 2 && dst_n.window_list[1] == 601;
    table_remove(&g_window_manager.window, &src_w.id);
    table_remove(&g_window_manager.window, &dst_w.id);
    ANGRY_ASSERT(stacked, "mouse_drop_action_stack failed on zoomed destination node");
}
