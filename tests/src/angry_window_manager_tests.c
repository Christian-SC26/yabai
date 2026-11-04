ANGRY_TEST(keepsNativeTabsAfterMissionControl)
{
    bool saved = g_window_manager.window_native_tabs;
    g_window_manager.window_native_tabs = true;
    window_manager_correct_for_mission_control_changes(&g_space_manager, &g_window_manager);
    bool kept = g_window_manager.window_native_tabs;
    g_window_manager.window_native_tabs = saved;
    ANGRY_ASSERT(kept, "expected leaving mission control to keep window_native_tabs");
}

ANGRY_TEST(keepsInsertionPointOnTabSwap)
{
    struct window_node node = { .window_count = 1 };
    node.window_list[0] = node.window_order[0] = 9505;
    struct view view = { .layout = VIEW_BSP, .root = &node, .insertion_point = 9505 };

    ANGRY_ASSERT(view_swap_node_window(&view, 9505, 9506), "expected the swap to find the outgoing tab");
    ANGRY_ASSERT(view.insertion_point == 9506, "expected a pending insertion to follow the visible tab");
}

ANGRY_TEST(releasesTabMembersWhenDisabled)
{
    struct window window = { .id = 9507 };
    window.id_ptr = &window.id;
    window_set_flag(&window, WINDOW_TAB_MEMBER);
    table_add(&g_window_manager.window, &window.id, &window);

    window_manager_release_tab_members(&g_window_manager);
    table_remove(&g_window_manager.window, &window.id);

    ANGRY_ASSERT(!window_check_flag(&window, WINDOW_TAB_MEMBER), "expected the setting to release hidden tabs");
}

ANGRY_TEST(swapsHiddenTabIntoNode)
{
    struct window_node node = { .window_count = 1 };
    node.window_list[0] = node.window_order[0] = 9501;
    struct view view = { .layout = VIEW_BSP, .root = &node };

    ANGRY_ASSERT(view_swap_node_window(&view, 9501, 9502), "expected the swap to find the outgoing tab");
    ANGRY_ASSERT_ALL("expected the node to hold the incoming tab",
                     node.window_list[0] == 9502,
                     node.window_order[0] == 9502);
    ANGRY_ASSERT(node.window_count == 1, "expected a tab switch to keep the node size");
}

ANGRY_TEST(rejectsSwapOfUntrackedTab)
{
    struct window_node node = { .window_count = 1 };
    node.window_list[0] = node.window_order[0] = 9511;
    struct view view = { .layout = VIEW_BSP, .root = &node };

    ANGRY_ASSERT(!view_swap_node_window(&view, 9599, 9512), "expected no swap for a window outside the view");
    ANGRY_ASSERT(node.window_list[0] == 9511, "expected the node to stay untouched");
}

ANGRY_TEST(validatesWindowEligibilityForManagedRules)
{
    struct window win = { .is_root = true };
    bool elig = window_manager_is_window_eligible(&win);
    ANGRY_ASSERT(!elig, "expected window without real status or managed rule to be ineligible");
}

ANGRY_TEST(queriesWindowsForSpaceListAsJsonStream)
{
    char buf[128];
    FILE *rsp = fmemopen(buf, sizeof(buf), "w");
    uint64_t spaces[] = { 1 };
    window_manager_query_windows_for_spaces(rsp, spaces, 1, 0);
    fclose(rsp);
    ANGRY_ASSERT(buf[0] == '[', "expected query_windows_for_spaces to write json array");
}

ANGRY_TEST(queriesWindowsForSingleDisplayAsJsonStream)
{
    char buf[128];
    FILE *rsp = fmemopen(buf, sizeof(buf), "w");
    window_manager_query_windows_for_display(rsp, 1, 0);
    fclose(rsp);
    ANGRY_ASSERT(buf[0] == '[', "expected query_windows_for_display to write json array");
}

ANGRY_TEST(queriesWindowsForDisplaysAsJsonStream)
{
    char buf[128];
    FILE *rsp = fmemopen(buf, sizeof(buf), "w");
    window_manager_query_windows_for_displays(rsp, 0);
    fclose(rsp);
    ANGRY_ASSERT(buf[0] == '[', "expected query_windows_for_displays to write json array");
}

ANGRY_TEST(matchesWindowAgainstRegexRule)
{
    struct rule r = {0};
    struct application app = { .name = "Finder" };
    struct window win = { .application = &app };
    bool m = window_manager_rule_matches_window(&r, &win, "title", "role", "subrole");
    ANGRY_ASSERT(m, "expected empty rule to match window");
}

ANGRY_TEST(appliesManageRuleEffectsToWindow)
{
    struct window win = { .id = 9301 };
    struct rule_effects eff = { .manage = RULE_PROP_OFF };
    window_manager_apply_manage_rule_effects_to_window(&g_space_manager, &g_window_manager, &win, &eff);
    ANGRY_ASSERT(!window_check_rule_flag(&win, WINDOW_RULE_MANAGED), "expected manage off effect to clear managed flag");
}

ANGRY_TEST(appliesGeneralRuleEffectsToWindow)
{
    struct window win = { .id = 9302 };
    struct rule_effects eff = { .sticky = RULE_PROP_ON, .ffm = RULE_PROP_ON, .mff = RULE_PROP_ON };
    window_manager_apply_rule_effects_to_window(&g_space_manager, &g_window_manager, &win, &eff);
    ANGRY_ASSERT_ALL("expected rule effects to set FFM and MFF flags",
                     window_check_rule_flag(&win, WINDOW_RULE_FFM),
                     window_check_rule_flag(&win, WINDOW_RULE_MFF));
}

ANGRY_TEST(clearsManagedFlagWhenMatchingRuleTurnsManagementOff)
{
    struct window_manager wm = {0};
    struct rule rule = { .effects = { .manage = RULE_PROP_OFF } };
    buf_push(wm.rules, rule);
    struct application app = { .name = "Finder" };
    struct window win = { .id = 9303, .application = &app };
    window_set_rule_flag(&win, WINDOW_RULE_MANAGED);
    window_manager_apply_manage_rules_to_window(&g_space_manager, &wm, &win, "t", "AXWindow", "AXStandardWindow", false);
    buf_free(wm.rules);
    ANGRY_ASSERT(!window_check_rule_flag(&win, WINDOW_RULE_MANAGED), "expected a matching manage=off rule to clear the managed flag");
}

ANGRY_TEST(setsFocusFollowsMouseFlagWhenMatchingRuleEnablesIt)
{
    struct window_manager wm = {0};
    struct rule rule = { .effects = { .ffm = RULE_PROP_ON } };
    buf_push(wm.rules, rule);
    struct application app = { .name = "Finder" };
    struct window win = { .id = 9304, .application = &app };
    window_manager_apply_rules_to_window(&g_space_manager, &wm, &win, "t", "AXWindow", "AXStandardWindow", false);
    buf_free(wm.rules);
    ANGRY_ASSERT(window_check_rule_flag(&win, WINDOW_RULE_FFM), "expected a matching rule to turn on focus follows mouse for the window");
}

ANGRY_TEST(togglesGlobalWindowOpacityState)
{
    window_manager_set_window_opacity_enabled(&g_window_manager, true);
    bool enabled = g_window_manager.enable_window_opacity;
    window_manager_set_window_opacity_enabled(&g_window_manager, false);
    ANGRY_ASSERT_ALL("expected window opacity enable/disable toggling",
                     enabled,
                     !g_window_manager.enable_window_opacity);
}

ANGRY_TEST(evaluatesWhetherWindowShouldBeManaged)
{
    struct window win = { .is_root = false };
    bool manage = window_manager_should_manage_window(&win);
    ANGRY_ASSERT(!manage, "expected non-root window not to be managed");
}

ANGRY_TEST(adjustsWindowRatioReturningErrorOnUnmanagedWindow)
{
    struct window win = { .id = 9305 };
    enum window_op_error err = window_manager_adjust_window_ratio(&g_window_manager, &win, TYPE_REL, 0.1f);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_INVALID_SRC_VIEW, "expected unmanaged window ratio adjust to fail view check");
}

ANGRY_TEST(movesWindowRelativelyForUnmanagedWindow)
{
    struct window win = { .id = 9306, .frame = {{0,0},{100,100}} };
    enum window_op_error err = window_manager_move_window_relative(&g_window_manager, &win, TYPE_ABS, 10, 10);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_SUCCESS, "expected relative move on floating window to succeed");
}

ANGRY_TEST(resizesWindowRelativeHandlingUnmanagedWindow)
{
    struct window win = { .id = 9308, .frame = {{0,0},{100,100}} };
    enum window_op_error err = window_manager_resize_window_relative(&g_window_manager, &win, HANDLE_ABS, 150, 150, true);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_SUCCESS, "expected relative resize on floating window to succeed");
}

ANGRY_TEST(setsWindowPurifyModeAndIteratesWindows)
{
    window_manager_set_purify_mode(&g_window_manager, PURIFY_ALWAYS);
    bool is_always = g_window_manager.purify_mode == PURIFY_ALWAYS;
    window_manager_set_purify_mode(&g_window_manager, PURIFY_DISABLED);
    ANGRY_ASSERT_ALL("expected purify mode update",
                     is_always,
                     g_window_manager.purify_mode == PURIFY_DISABLED);
}

ANGRY_TEST(reportsFailedOpacityChangeWithoutScriptingAddition)
{
    struct window win = { .id = 9311 };
    bool changed = window_manager_set_opacity(&g_window_manager, &win, 0.8f);
    ANGRY_ASSERT(!changed, "expected the opacity change to fail while the scripting addition is unreachable");
}

ANGRY_TEST(setsMenubarOpacityDirectly)
{
    window_manager_set_menubar_opacity(&g_window_manager, 0.95f);
    ANGRY_ASSERT(g_window_manager.menubar_opacity == 0.95f, "expected menubar opacity to be 0.95");
}

ANGRY_TEST(setsActiveWindowOpacityDirectly)
{
    window_manager_set_active_window_opacity(&g_window_manager, 0.9f);
    ANGRY_ASSERT(g_window_manager.active_window_opacity == 0.9f, "expected active window opacity to be 0.9");
}

ANGRY_TEST(setsNormalWindowOpacityDirectly)
{
    window_manager_set_normal_window_opacity(&g_window_manager, 0.85f);
    ANGRY_ASSERT(g_window_manager.normal_window_opacity == 0.85f, "expected normal window opacity to be 0.85");
}

ANGRY_TEST(remembersRequestedWindowLayer)
{
    struct window win = { .id = 9312, .layer = LAYER_NORMAL };
    window_manager_set_window_layer(&win, LAYER_ABOVE);
    ANGRY_ASSERT(win.layer == LAYER_ABOVE, "expected the window to remember the requested layer");
}

ANGRY_TEST(findsRankOfWindowInWindowList)
{
    uint32_t list[] = { 101, 102, 103 };
    int rank = window_manager_find_rank_of_window_in_list(102, list, 3);
    ANGRY_ASSERT(rank == 1, "expected window 102 to have rank 1");
}

ANGRY_TEST(createsWindowProxySkippingNullImage)
{
    struct window_proxy proxy = { .image = NULL };
    window_manager_create_window_proxy(0, 1.0f, &proxy);
    ANGRY_ASSERT(proxy.id == 0, "expected create_window_proxy with null image to return early");
}

ANGRY_TEST(destroysWindowProxyCleaningUpResources)
{
    struct window_proxy proxy = { .image = NULL, .context = NULL, .id = 0 };
    window_manager_destroy_window_proxy(0, &proxy);
    ANGRY_ASSERT(proxy.id == 0, "expected destroy_window_proxy to clear proxy id");
}

ANGRY_TEST(processesWindowAnimationListStepInThreadProc)
{
    struct window_animation_context ctx = { .animation_count = 0, .animation_duration = 1.0f, .animation_clock = 1000 };
    CVTimeStamp now = { .hostTime = 1000 };
    CVTimeStamp out_time = { .hostTime = 1000 };
    CVReturn ret = window_manager_animate_window_list_thread_proc(NULL, &now, &out_time, 0, NULL, &ctx);
    ANGRY_ASSERT(ret == kCVReturnSuccess, "expected animate_window_list_thread_proc to return success");
}

ANGRY_TEST(findsClosestManagedWindowInDirection)
{
    struct window win = { .id = 9320 };
    struct window *res = window_manager_find_closest_managed_window_in_direction(&g_window_manager, &win, DIR_NORTH);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null closest window");
}

ANGRY_TEST(findsPrevManagedWindowOnActiveSpace)
{
    struct window win = { .id = 9321 };
    struct window *res = window_manager_find_prev_managed_window(&g_space_manager, &g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null prev window");
}

ANGRY_TEST(findsNextManagedWindowOnActiveSpace)
{
    struct window win = { .id = 9322 };
    struct window *res = window_manager_find_next_managed_window(&g_space_manager, &g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null next window");
}

//
// NOTE: a fake active space whose view tiles two windows side by side, the left
// one smaller, so that the find_*_managed_window queries have a known answer.
//
struct angry_tiled_pair
{
    struct window_node root, left, right;
    struct window small, large;
    struct view *view;
    struct window_node *saved_root;
};

static void angry_tiled_pair_begin(struct angry_tiled_pair *pair)
{
    g_mock_active_space = 0x7E571000ULL;
    pair->view = space_manager_find_view(&g_space_manager, g_mock_active_space);
    pair->saved_root = pair->view->root;
    pair->small = (struct window) { .id = 9361 };
    pair->large = (struct window) { .id = 9362 };
    pair->left  = (struct window_node) { .parent = &pair->root, .area = { 0, 0, 100, 100 }, .window_count = 1 };
    pair->right = (struct window_node) { .parent = &pair->root, .area = { 100, 0, 300, 300 }, .window_count = 1 };
    pair->left.window_list[0]  = pair->left.window_order[0]  = pair->small.id;
    pair->right.window_list[0] = pair->right.window_order[0] = pair->large.id;
    pair->root = (struct window_node) { .left = &pair->left, .right = &pair->right, .split = SPLIT_Y };
    pair->view->root = &pair->root;
    table_add(&g_window_manager.window, &pair->small.id, &pair->small);
    table_add(&g_window_manager.window, &pair->large.id, &pair->large);
}

static void angry_tiled_pair_end(struct angry_tiled_pair *pair)
{
    table_remove(&g_window_manager.window, &pair->small.id);
    table_remove(&g_window_manager.window, &pair->large.id);
    pair->view->root = pair->saved_root;
}

ANGRY_TEST(findsFirstManagedWindowOnActiveSpace)
{
    struct angry_tiled_pair pair;
    angry_tiled_pair_begin(&pair);
    struct window *found = window_manager_find_first_managed_window(&g_space_manager, &g_window_manager);
    uint32_t wid = found ? found->id : 0;
    angry_tiled_pair_end(&pair);
    ANGRY_ASSERT(wid == 9361, "expected the first tiled window of the active space");
}

ANGRY_TEST(findsLastManagedWindowOnActiveSpace)
{
    struct angry_tiled_pair pair;
    angry_tiled_pair_begin(&pair);
    struct window *found = window_manager_find_last_managed_window(&g_space_manager, &g_window_manager);
    uint32_t wid = found ? found->id : 0;
    angry_tiled_pair_end(&pair);
    ANGRY_ASSERT(wid == 9362, "expected the last tiled window of the active space");
}

ANGRY_TEST(findsRecentManagedWindowUsingLastWindowId)
{
    g_window_manager.last_window_id = 0;
    struct window *res = window_manager_find_recent_managed_window(&g_window_manager);
    ANGRY_ASSERT(res == NULL, "expected id 0 to yield null recent managed window");
}

ANGRY_TEST(findsPrevWindowInStackReturningNull)
{
    struct window win = { .id = 9323 };
    struct window *res = window_manager_find_prev_window_in_stack(&g_space_manager, &g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null prev window in stack");
}

ANGRY_TEST(findsNextWindowInStackReturningNull)
{
    struct window win = { .id = 9324 };
    struct window *res = window_manager_find_next_window_in_stack(&g_space_manager, &g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null next window in stack");
}

ANGRY_TEST(findsFirstWindowInStackReturningNull)
{
    struct window win = { .id = 9325 };
    struct window *res = window_manager_find_first_window_in_stack(&g_space_manager, &g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null first window in stack");
}

ANGRY_TEST(findsLastWindowInStackReturningNull)
{
    struct window win = { .id = 9326 };
    struct window *res = window_manager_find_last_window_in_stack(&g_space_manager, &g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null last window in stack");
}

ANGRY_TEST(findsRecentWindowInStackReturningNull)
{
    struct window win = { .id = 9327 };
    struct window *res = window_manager_find_recent_window_in_stack(&g_space_manager, &g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null recent window in stack");
}

ANGRY_TEST(findsWindowInStackByIndexReturningNull)
{
    struct window win = { .id = 9328 };
    struct window *res = window_manager_find_window_in_stack(&g_space_manager, &g_window_manager, &win, 1);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to return null window in stack at index");
}

ANGRY_TEST(findsLargestManagedWindowOnActiveSpace)
{
    struct angry_tiled_pair pair;
    angry_tiled_pair_begin(&pair);
    struct window *found = window_manager_find_largest_managed_window(&g_space_manager, &g_window_manager);
    uint32_t wid = found ? found->id : 0;
    angry_tiled_pair_end(&pair);
    ANGRY_ASSERT(wid == 9362, "expected the largest window of the active space");
}

ANGRY_TEST(findsSmallestManagedWindowOnActiveSpace)
{
    struct angry_tiled_pair pair;
    angry_tiled_pair_begin(&pair);
    struct window *found = window_manager_find_smallest_managed_window(&g_space_manager, &g_window_manager);
    uint32_t wid = found ? found->id : 0;
    angry_tiled_pair_end(&pair);
    ANGRY_ASSERT(wid == 9361, "expected the smallest window of the active space");
}

ANGRY_TEST(findsSiblingForManagedWindowReturningNull)
{
    struct window win = { .id = 9329 };
    struct window *res = window_manager_find_sibling_for_managed_window(&g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to have no sibling");
}

ANGRY_TEST(findsFirstNephewForManagedWindowReturningNull)
{
    struct window win = { .id = 9330 };
    struct window *res = window_manager_find_first_nephew_for_managed_window(&g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to have no first nephew");
}

ANGRY_TEST(findsSecondNephewForManagedWindowReturningNull)
{
    struct window win = { .id = 9331 };
    struct window *res = window_manager_find_second_nephew_for_managed_window(&g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to have no second nephew");
}

ANGRY_TEST(findsUncleForManagedWindowReturningNull)
{
    struct window win = { .id = 9332 };
    struct window *res = window_manager_find_uncle_for_managed_window(&g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to have no uncle");
}

ANGRY_TEST(findsFirstCousinForManagedWindowReturningNull)
{
    struct window win = { .id = 9333 };
    struct window *res = window_manager_find_first_cousin_for_managed_window(&g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to have no first cousin");
}

ANGRY_TEST(findsSecondCousinForManagedWindowReturningNull)
{
    struct window win = { .id = 9334 };
    struct window *res = window_manager_find_second_cousin_for_managed_window(&g_window_manager, &win);
    ANGRY_ASSERT(res == NULL, "expected unmanaged window to have no second cousin");
}

ANGRY_TEST(focusesWindowOfAnotherProcessWithoutRaiseByMakingItKey)
{
    ProcessSerialNumber saved = g_window_manager.focused_window_psn;
    g_window_manager.focused_window_psn = (ProcessSerialNumber) { 0, 4242 };
    ProcessSerialNumber psn = { 0, kNoProcess };
    window_manager_focus_window_without_raise(&psn, 9341);
    g_window_manager.focused_window_psn = saved;
    bool made_key = ANGRY_ALL(g_mock_posted_event_count == 2, angry_posted_event_window_id(1) == 9341);
    ANGRY_ASSERT(made_key, "expected only the key-window events for a window of another process");
}

ANGRY_TEST(deactivatesFocusedWindowBeforeFocusingSiblingWithoutRaise)
{
    g_mock_fast_usleep = true;
    ProcessSerialNumber saved_psn = g_window_manager.focused_window_psn;
    uint32_t saved_wid = g_window_manager.focused_window_id;
    ProcessSerialNumber psn = { 0, 4243 };
    g_window_manager.focused_window_psn = psn;
    g_window_manager.focused_window_id = 9342;
    window_manager_focus_window_without_raise(&psn, 9343);
    g_window_manager.focused_window_psn = saved_psn;
    g_window_manager.focused_window_id = saved_wid;
    bool ordered = ANGRY_ALL(angry_posted_event_window_id(0) == 9342, angry_posted_event_window_id(1) == 9343);
    ANGRY_ASSERT(ordered, "expected the focused window to be deactivated before its sibling is activated");
}

ANGRY_TEST(tracksLostFrontSwitchedEventInTable)
{
    window_manager_add_lost_front_switched_event(&g_window_manager, 9335);
    bool found = window_manager_find_lost_front_switched_event(&g_window_manager, 9335);
    window_manager_remove_lost_front_switched_event(&g_window_manager, 9335);
    ANGRY_ASSERT_ALL("expected front switched event to be tracked and removed",
                     found,
                     !window_manager_find_lost_front_switched_event(&g_window_manager, 9335));
}

ANGRY_TEST(tracksLostFocusedEventInTable)
{
    window_manager_add_lost_focused_event(&g_window_manager, 9336);
    bool found = window_manager_find_lost_focused_event(&g_window_manager, 9336);
    window_manager_remove_lost_focused_event(&g_window_manager, 9336);
    ANGRY_ASSERT_ALL("expected focused event to be tracked and removed",
                     found,
                     !window_manager_find_lost_focused_event(&g_window_manager, 9336));
}

ANGRY_TEST(addsAndRemovesWindowFromTable)
{
    struct window win = { .id = 9337 };
    window_manager_add_window(&g_window_manager, &win);
    struct window *found = window_manager_find_window(&g_window_manager, 9337);
    window_manager_remove_window(&g_window_manager, 9337);
    ANGRY_ASSERT_ALL("expected window to be added and removed",
                     found == &win,
                     window_manager_find_window(&g_window_manager, 9337) == NULL);
}

ANGRY_TEST(addsAndRemovesApplicationFromTable)
{
    struct application app = { .pid = 9338 };
    window_manager_add_application(&g_window_manager, &app);
    struct application *found = window_manager_find_application(&g_window_manager, 9338);
    window_manager_remove_application(&g_window_manager, 9338);
    ANGRY_ASSERT_ALL("expected application to be added and removed",
                     found == &app,
                     window_manager_find_application(&g_window_manager, 9338) == NULL);
}

ANGRY_TEST(findsWindowsBelongingToApplication)
{
    struct application app = { .pid = 9339 };
    int count = 0;
    struct window **list = window_manager_find_application_windows(&g_window_manager, &app, &count);
    ANGRY_ASSERT_ALL("expected empty application window list",
                     list != NULL,
                     count == 0);
}

ANGRY_TEST(postsTwoEventsToMakeWindowKey)
{
    ProcessSerialNumber psn = { 0, kNoProcess };
    window_manager_make_key_window(&psn, 9340);
    ANGRY_ASSERT(g_mock_posted_event_count == 2, "expected a key-window change to post a pair of events");
}

ANGRY_TEST(addressesKeyWindowEventsToWindow)
{
    ProcessSerialNumber psn = { 0, kNoProcess };
    window_manager_make_key_window(&psn, 9340);
    bool addressed = ANGRY_ALL(angry_posted_event_window_id(0) == 9340, angry_posted_event_window_id(1) == 9340);
    ANGRY_ASSERT(addressed, "expected both key-window events to carry the window id");
}

ANGRY_TEST(dropsWindowThatCannotBeObserved)
{
    g_mock_ax_cannot_complete = true;
    struct application app = { .pid = getpid(), .name = "test_app" };
    struct window *win = window_manager_create_and_add_window(&g_space_manager, &g_window_manager, &app, NULL, 0, false);
    ANGRY_ASSERT(win == NULL, "expected a window whose notifications can not be observed to be dropped");
}

ANGRY_TEST(addsApplicationWindowsSafely)
{
    struct application app = { .pid = getpid(), .ref = NULL };
    int count = 0;
    struct window **list = window_manager_add_application_windows(&g_space_manager, &g_window_manager, &app, &count);
    ANGRY_ASSERT_ALL("expected null application window list when ref is null",
                     list == NULL,
                     count == 0);
}

ANGRY_TEST(addsExistingApplicationWindowsSafely)
{
    struct application app = { .pid = getpid(), .ref = NULL, .connection = -1 };
    bool added = window_manager_add_existing_application_windows(&g_space_manager, &g_window_manager, &app, -1);
    ANGRY_ASSERT(!added, "expected add_existing_application_windows to return false on null ref");
}

ANGRY_TEST(refusesInsertionPointForWindowOutsideTheLayout)
{
    g_mock_active_space = 0x7E574001ULL;
    space_manager_find_view(&g_space_manager, g_mock_active_space)->layout = VIEW_BSP;
    struct window win = { .id = 9341 };
    enum window_op_error err = window_manager_set_window_insertion(&g_space_manager, &win, DIR_NORTH);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_INVALID_SRC_NODE, "expected no insertion point for a window that is not tiled");
}

ANGRY_TEST(stacksWindowReturningErrorOnSelfStack)
{
    struct window win = { .id = 9342 };
    enum window_op_error err = window_manager_stack_window(&g_space_manager, &g_window_manager, &win, &win);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_SAME_WINDOW, "expected stacking window onto itself to return SAME_WINDOW");
}

ANGRY_TEST(warpsWindowReturningErrorOnSelfWarp)
{
    struct window win = { .id = 9343 };
    enum window_op_error err = window_manager_warp_window(&g_space_manager, &g_window_manager, &win, &win);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_SAME_WINDOW, "expected warping window onto itself to return SAME_WINDOW");
}

ANGRY_TEST(swapsWindowReturningErrorOnSelfSwap)
{
    struct window win = { .id = 9344 };
    enum window_op_error err = window_manager_swap_window(&g_space_manager, &g_window_manager, &win, &win);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_SAME_WINDOW, "expected swapping window onto itself to return SAME_WINDOW");
}

ANGRY_TEST(minimizesWindowCheckingCapabilities)
{
    struct window win = { .id = 9345, .flags = 0 };
    enum window_op_error err = window_manager_minimize_window(&win);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_CANT_MINIMIZE, "expected non-minimizable window to return CANT_MINIMIZE");
}

ANGRY_TEST(deminimizesWindowCheckingMinimizedState)
{
    struct window win = { .id = 9346, .flags = 0 };
    enum window_op_error err = window_manager_deminimize_window(&win);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_NOT_MINIMIZED, "expected non-minimized window to return NOT_MINIMIZED");
}

ANGRY_TEST(closesWindowReturningFailureOnNullButton)
{
    struct window win = { .id = 9347, .ref = NULL };
    bool closed = window_manager_close_window(&win);
    ANGRY_ASSERT(!closed, "expected close_window to fail on null window ref");
}

ANGRY_TEST(appliesGridToWindowHandlingInvalidView)
{
    struct window win = { .id = 9349 };
    enum window_op_error err = window_manager_apply_grid(&g_space_manager, &g_window_manager, &win, 2, 2, 0, 0, 1, 1);
    ANGRY_ASSERT(err == WINDOW_OP_ERROR_INVALID_SRC_VIEW || err == WINDOW_OP_ERROR_SUCCESS, "expected apply_grid to execute");
}

ANGRY_TEST(keepsIneligibleWindowFromFloating)
{
    struct window win = { .id = 9350, .is_root = false };
    window_manager_make_window_floating(&g_space_manager, &g_window_manager, &win, true, false);
    ANGRY_ASSERT(!window_check_flag(&win, WINDOW_FLOAT), "expected a window that is not a root window to stay non-floating");
}

ANGRY_TEST(keepsIneligibleWindowFromBecomingSticky)
{
    struct window win = { .id = 9351, .is_root = false };
    window_manager_make_window_sticky(&g_space_manager, &g_window_manager, &win, true);
    ANGRY_ASSERT(!window_check_flag(&win, WINDOW_STICKY), "expected a window that is not a root window to stay non-sticky");
}

ANGRY_TEST(keepsShadowFlagWhenScriptingAdditionCannotChangeIt)
{
    struct window win = { .id = 9352 };
    window_set_flag(&win, WINDOW_SHADOW);
    window_manager_toggle_window_shadow(&win);
    ANGRY_ASSERT(window_check_flag(&win, WINDOW_SHADOW), "expected the shadow flag to stay while the scripting addition is unreachable");
}

ANGRY_TEST(togglesScratchpadWindowByLabelReturningFalse)
{
    bool toggled = window_manager_toggle_scratchpad_window_by_label(&g_window_manager, "missing_label");
    ANGRY_ASSERT(!toggled, "expected missing scratchpad label to return false");
}

ANGRY_TEST(togglesScratchpadWindowDirectly)
{
    struct application app = { .pid = getpid() };
    struct window win = { .id = 9359, .application = &app, .ref = NULL };
    bool toggled = window_manager_toggle_scratchpad_window(&g_window_manager, &win, 2);
    ANGRY_ASSERT(toggled, "expected forced mode 2 toggle scratchpad to succeed");
}

ANGRY_TEST(setsAndRemovesScratchpadForWindow)
{
    struct window win = { .id = 9360, .is_root = false };
    char *lbl = strdup("test_pad");
    bool set = window_manager_set_scratchpad_for_window(&g_window_manager, &win, lbl);
    bool removed = window_manager_remove_scratchpad_for_window(&g_window_manager, &win, false);
    ANGRY_ASSERT_ALL("expected scratchpad to be set and removed successfully",
                     set,
                     removed);
}

ANGRY_TEST(findsScratchpadWindowByLabel)
{
    struct window *win = window_manager_find_scratchpad_window(&g_window_manager, "nonexistent");
    ANGRY_ASSERT(win == NULL, "expected non-existent scratchpad label to return null");
}

//
// NOTE: two windows tiled on a fake BSP space that is never visible, and
// managed by the window manager, so that the zoom commands have a layout to
// act on and mark the view dirty instead of moving real windows.
//
struct angry_managed_pair
{
    struct application app;
    struct window first, second;
    struct view *view;
};

static void angry_managed_pair_begin(struct angry_managed_pair *pair, uint64_t sid)
{
    pair->view = space_manager_find_view(&g_space_manager, sid);
    pair->view->layout = VIEW_BSP;
    pair->app = (struct application) { .pid = 4400, .name = "zoom_app" };
    pair->first  = (struct window) { .id = 94001, .application = &pair->app, .ref = (AXUIElementRef) CFRetain(CFSTR("dummy")), .frame = {{0,0},{400,400}} };
    pair->second = (struct window) { .id = 94002, .application = &pair->app, .ref = (AXUIElementRef) CFRetain(CFSTR("dummy")), .frame = {{0,0},{400,400}} };
    space_manager_tile_window_on_space(&g_space_manager, &pair->first, sid);
    window_manager_add_managed_window(&g_window_manager, &pair->first, pair->view);
    space_manager_tile_window_on_space(&g_space_manager, &pair->second, sid);
    window_manager_add_managed_window(&g_window_manager, &pair->second, pair->view);
}

static void angry_managed_pair_end(struct angry_managed_pair *pair)
{
    window_manager_remove_managed_window(&g_window_manager, pair->first.id);
    window_manager_remove_managed_window(&g_window_manager, pair->second.id);
    space_manager_untile_window(pair->view, &pair->first);
    space_manager_untile_window(pair->view, &pair->second);
    CFRelease(pair->first.ref);
    CFRelease(pair->second.ref);
}

ANGRY_TEST(zoomsWindowToItsParentNode)
{
    struct angry_managed_pair pair;
    angry_managed_pair_begin(&pair, 0x7E575001ULL);
    window_manager_toggle_window_zoom_parent(&g_window_manager, &pair.second);
    struct window_node *node = view_find_window_node(pair.view, pair.second.id);
    bool zoomed = node->zoom == node->parent;
    angry_managed_pair_end(&pair);
    ANGRY_ASSERT(zoomed, "expected the window to be zoomed to its parent node");
}

ANGRY_TEST(unzoomsWindowFromParentNodeOnSecondToggle)
{
    struct angry_managed_pair pair;
    angry_managed_pair_begin(&pair, 0x7E575002ULL);
    window_manager_toggle_window_zoom_parent(&g_window_manager, &pair.second);
    window_manager_toggle_window_zoom_parent(&g_window_manager, &pair.second);
    struct window_node *zoom = view_find_window_node(pair.view, pair.second.id)->zoom;
    angry_managed_pair_end(&pair);
    ANGRY_ASSERT(zoom == NULL, "expected a second toggle to undo the zoom to the parent node");
}

ANGRY_TEST(zoomsWindowToWholeSpace)
{
    struct angry_managed_pair pair;
    angry_managed_pair_begin(&pair, 0x7E575003ULL);
    window_manager_toggle_window_zoom_fullscreen(&g_window_manager, &pair.second);
    bool zoomed = view_find_window_node(pair.view, pair.second.id)->zoom == pair.view->root;
    angry_managed_pair_end(&pair);
    ANGRY_ASSERT(zoomed, "expected the window to be zoomed to the root of the space");
}

ANGRY_TEST(unzoomsWindowFromWholeSpaceOnSecondToggle)
{
    struct angry_managed_pair pair;
    angry_managed_pair_begin(&pair, 0x7E575004ULL);
    window_manager_toggle_window_zoom_fullscreen(&g_window_manager, &pair.second);
    window_manager_toggle_window_zoom_fullscreen(&g_window_manager, &pair.second);
    struct window_node *zoom = view_find_window_node(pair.view, pair.second.id)->zoom;
    angry_managed_pair_end(&pair);
    ANGRY_ASSERT(zoom == NULL, "expected a second toggle to undo the zoom to the whole space");
}

ANGRY_TEST(marksHiddenSpaceDirtyWhenZoomingWindow)
{
    struct angry_managed_pair pair;
    angry_managed_pair_begin(&pair, 0x7E575005ULL);
    view_clear_flag(pair.view, VIEW_IS_DIRTY);
    window_manager_toggle_window_zoom_fullscreen(&g_window_manager, &pair.second);
    bool dirty = view_check_flag(pair.view, VIEW_IS_DIRTY);
    angry_managed_pair_end(&pair);
    ANGRY_ASSERT(dirty, "expected a zoom on a space that is not visible to be applied when the space is shown");
}

ANGRY_TEST(leavesSoleWindowOfSpaceUnzoomed)
{
    struct view *view = space_manager_find_view(&g_space_manager, 0x7E575006ULL);
    view->layout = VIEW_BSP;
    struct application app = { .pid = 4401, .name = "zoom_app" };
    struct window win = { .id = 94003, .application = &app, .ref = (AXUIElementRef) CFRetain(CFSTR("dummy")), .frame = {{0,0},{400,400}} };
    space_manager_tile_window_on_space(&g_space_manager, &win, view->sid);
    window_manager_add_managed_window(&g_window_manager, &win, view);
    window_manager_toggle_window_zoom_fullscreen(&g_window_manager, &win);
    struct window_node *zoom = view_find_window_node(view, win.id)->zoom;
    window_manager_remove_managed_window(&g_window_manager, win.id);
    space_manager_untile_window(view, &win);
    CFRelease(win.ref);
    ANGRY_ASSERT(zoom == NULL, "expected the only window of a space to have nothing to zoom to");
}

//
// NOTE: the test window, wrapped so that frame changes reach the accessibility
// fake: its element belongs to the test process, which the fake accepts.
//
struct angry_framed_window
{
    struct application app;
    struct window window;
    float saved_duration;
};

static void angry_framed_window_begin(struct angry_framed_window *framed)
{
    framed->saved_duration = g_window_manager.window_animation_duration;
    g_window_manager.window_animation_duration = 0.0f;
    framed->app = (struct application) { .pid = getpid(), .name = "framed_app", .ref = AXUIElementCreateApplication(getpid()) };
    framed->window = (struct window) { .id = angry_test_window(), .application = &framed->app, .ref = AXUIElementCreateApplication(getpid()), .frame = {{10,20},{300,200}} };
}

static void angry_framed_window_end(struct angry_framed_window *framed)
{
    g_window_manager.window_animation_duration = framed->saved_duration;
    CFRelease(framed->window.ref);
    CFRelease(framed->app.ref);
}

ANGRY_TEST(remembersFrameWhenEnteringWindowedFullscreen)
{
    struct angry_framed_window framed;
    angry_framed_window_begin(&framed);
    window_manager_toggle_window_windowed_fullscreen(&framed.window);
    CGRect remembered = framed.window.windowed_frame;
    bool windowed = window_check_flag(&framed.window, WINDOW_WINDOWED);
    angry_framed_window_end(&framed);
    ANGRY_ASSERT_ALL("expected the window to enter windowed fullscreen and remember where it was",
                     windowed,
                     CGRectEqualToRect(remembered, CGRectMake(10, 20, 300, 200)));
}

ANGRY_TEST(fillsDisplayWhenEnteringWindowedFullscreen)
{
    struct angry_framed_window framed;
    angry_framed_window_begin(&framed);
    window_manager_toggle_window_windowed_fullscreen(&framed.window);
    angry_framed_window_end(&framed);
    CGRect bounds = display_bounds_constrained(CGMainDisplayID(), false);
    ANGRY_ASSERT_ALL("expected the window to be given the bounds of its display",
                     CGPointEqualToPoint(g_mock_ax_position, bounds.origin),
                     CGSizeEqualToSize(g_mock_ax_size, bounds.size));
}

ANGRY_TEST(restoresFrameWhenLeavingWindowedFullscreen)
{
    struct angry_framed_window framed;
    angry_framed_window_begin(&framed);
    window_set_flag(&framed.window, WINDOW_WINDOWED);
    framed.window.windowed_frame = CGRectMake(40, 50, 600, 500);
    window_manager_toggle_window_windowed_fullscreen(&framed.window);
    bool windowed = window_check_flag(&framed.window, WINDOW_WINDOWED);
    angry_framed_window_end(&framed);
    ANGRY_ASSERT_ALL("expected the window to leave windowed fullscreen at the frame it had before",
                     !windowed,
                     CGPointEqualToPoint(g_mock_ax_position, CGPointMake(40, 50)),
                     CGSizeEqualToSize(g_mock_ax_size, CGSizeMake(600, 500)));
}

ANGRY_TEST(requestsNativeFullscreenForWindowNotInFullscreen)
{
    g_mock_fast_usleep = true;
    struct angry_framed_window framed;
    angry_framed_window_begin(&framed);
    window_manager_toggle_window_native_fullscreen(&framed.window);
    angry_framed_window_end(&framed);
    ANGRY_ASSERT_ALL("expected the window to be asked to enter native fullscreen",
                     strcmp(g_mock_ax_bool_attribute, "AXFullScreen") == 0,
                     g_mock_ax_bool_value);
}

ANGRY_TEST(focusesWindowBeforeChangingNativeFullscreen)
{
    g_mock_fast_usleep = true;
    struct angry_framed_window framed;
    angry_framed_window_begin(&framed);
    window_manager_toggle_window_native_fullscreen(&framed.window);
    angry_framed_window_end(&framed);
    ANGRY_ASSERT(g_mock_front_process_wid == framed.window.id, "expected the window to be brought to the front first");
}

ANGRY_TEST(bringsWindowToFrontBeforeExpose)
{
    struct application app = { .pid = 4402 };
    struct window win = { .id = 94004, .application = &app };
    window_manager_toggle_window_expose(&win);
    ANGRY_ASSERT(g_mock_front_process_wid == win.id, "expected the window to be brought to the front for expose");
}

ANGRY_TEST(asksDockToExposeWindowsOfFrontApplication)
{
    struct application app = { .pid = 4403 };
    struct window win = { .id = 94005, .application = &app };
    window_manager_toggle_window_expose(&win);
    ANGRY_ASSERT(strcmp(g_mock_dock_notification, "com.apple.expose.front.awake") == 0, "expected the Dock to be asked to expose the windows of the application");
}

//
// NOTE: the picture-in-picture tests make the fake active space the current
// space of the main display, so that its padding is the one applied.
//
static struct view *angry_pip_space(uint64_t sid)
{
    g_mock_active_space = sid;
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    view_clear_flag(view, VIEW_ENABLE_PADDING);
    return view;
}

static CGRect angry_pip_request(struct angry_sa_server *server, uint32_t *wid)
{
    float x, y, w, h;
    angry_sa_unpack(server, 0, wid, sizeof(*wid));
    angry_sa_unpack(server, 4, &x, sizeof(x));
    angry_sa_unpack(server, 8, &y, sizeof(y));
    angry_sa_unpack(server, 12, &w, sizeof(w));
    angry_sa_unpack(server, 16, &h, sizeof(h));
    return CGRectMake(x, y, w, h);
}

ANGRY_TEST(asksScriptingAdditionToScaleWindowForPictureInPicture)
{
    angry_pip_space(0x7E575007ULL);
    struct window win = { .id = angry_test_window() };
    struct angry_sa_server server;
    angry_sa_server_begin(&server);
    window_manager_toggle_window_pip(&g_space_manager, &win);
    angry_sa_server_end(&server);
    uint32_t wid = 0;
    angry_pip_request(&server, &wid);
    ANGRY_ASSERT_ALL("expected a scale request for the window",
                     angry_sa_opcode(&server) == SA_OPCODE_WINDOW_SCALE,
                     wid == win.id);
}

ANGRY_TEST(scalesWindowToDisplayBoundsForPictureInPicture)
{
    angry_pip_space(0x7E575008ULL);
    struct window win = { .id = angry_test_window() };
    struct angry_sa_server server;
    angry_sa_server_begin(&server);
    window_manager_toggle_window_pip(&g_space_manager, &win);
    angry_sa_server_end(&server);
    uint32_t wid = 0;
    CGRect requested = angry_pip_request(&server, &wid);
    ANGRY_ASSERT(CGRectEqualToRect(requested, display_bounds_constrained(CGMainDisplayID(), false)),
                 "expected the window to be scaled to the bounds of its display");
}

ANGRY_TEST(keepsSpacePaddingForPictureInPicture)
{
    struct view *view = angry_pip_space(0x7E575009ULL);
    view_set_flag(view, VIEW_ENABLE_PADDING);
    view->top_padding = 10; view->bottom_padding = 20; view->left_padding = 30; view->right_padding = 40;
    struct window win = { .id = angry_test_window() };
    struct angry_sa_server server;
    angry_sa_server_begin(&server);
    window_manager_toggle_window_pip(&g_space_manager, &win);
    angry_sa_server_end(&server);
    uint32_t wid = 0;
    CGRect requested = angry_pip_request(&server, &wid);
    CGRect b = display_bounds_constrained(CGMainDisplayID(), false);
    CGRect expected = CGRectMake(b.origin.x + 30, b.origin.y + 10, b.size.width - 70, b.size.height - 30);
    ANGRY_ASSERT(CGRectEqualToRect(requested, expected), "expected the padding of the space to stay around the scaled window");
}
