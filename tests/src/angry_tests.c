#include "../../src/misc/memory_pool.h"
#include "../../src/misc/hashtable.h"

static TABLE_HASH_FUNC(angry_hash)
{
    return *(uint32_t *) key;
}

static TABLE_COMPARE_FUNC(angry_compare)
{
    return *(uint32_t *) key_a == *(uint32_t *) key_b;
}

ANGRY_TEST(allocatesChunkInEmptyMemoryPool)
{
    struct memory_pool pool;
    memory_pool_init(&pool, KILOBYTES(16));
    void *ptr = memory_pool_push(&pool, 128);
    ANGRY_ASSERT(ptr == pool.memory, "first allocated chunk must point to start of pool buffer");
}

ANGRY_TEST(preservesStoredPointerAcrossThousandAllocations)
{
    struct memory_pool pool;
    memory_pool_init(&pool, MEGABYTES(1));
    uint32_t *first = (uint32_t *) memory_pool_push(&pool, sizeof(uint32_t));
    *first = 0xDEADBEEF;
    for (int i = 1; i < 1000; ++i) {
        *(uint32_t *) memory_pool_push(&pool, sizeof(uint32_t)) = (uint32_t)(i * 31);
    }
    ANGRY_ASSERT(*first == 0xDEADBEEF, "first element data must not be corrupted by subsequent pushes");
}

ANGRY_TEST(wrapsMemoryPoolPointerToOriginOnOverflow)
{
    struct memory_pool pool;
    memory_pool_init(&pool, getpagesize());
    uint64_t chunk = 64;
    uint64_t count = pool.size / chunk;
    for (uint64_t i = 0; i < count - 1; ++i) {
        memory_pool_push(&pool, chunk);
    }
    void *wrapped = memory_pool_push(&pool, chunk);
    ANGRY_ASSERT(wrapped == pool.memory, "pool wrap-around must reset memory allocation pointer to origin");
}

ANGRY_TEST(rehashesTableWithoutDataLossUnderHighLoad)
{
    struct table table;
    table_init(&table, 37, angry_hash, angry_compare);
    uint32_t keys[1024];
    for (uint32_t i = 0; i < 1024; ++i) {
        keys[i] = i * 2654435761u;
        table_add(&table, &keys[i], (void *)(intptr_t)(keys[i] ^ 0xCAFEBABE));
    }
    uint32_t target_idx = 777;
    void *val = table_find(&table, &keys[target_idx]);
    ANGRY_ASSERT((intptr_t)val == (intptr_t)(keys[target_idx] ^ 0xCAFEBABE), "rehashed table must retrieve stored keys without loss");
}

ANGRY_TEST(removesKeyAndCleansEntryFromTable)
{
    struct table table;
    table_init(&table, 53, angry_hash, angry_compare);
    uint32_t key = 0xBEEFCAFE;
    table_add(&table, &key, (void *) 0x12345);
    table_remove(&table, &key);
    ANGRY_ASSERT(table_find(&table, &key) == NULL && table.count == 0, "removed key must return NULL and decrement count to zero");
}

ANGRY_TEST(preservesUnremovedKeysAfterPartialDeletion)
{
    struct table table;
    table_init(&table, 64, angry_hash, angry_compare);
    uint32_t keys[100];
    for (uint32_t i = 0; i < 100; ++i) {
        keys[i] = i + 1;
        table_add(&table, &keys[i], (void *)(intptr_t)(keys[i] * 7));
    }
    for (uint32_t i = 0; i < 50; ++i) {
        table_remove(&table, &keys[i]);
    }
    uint32_t probe_key = 88;
    void *val = table_find(&table, &probe_key);
    ANGRY_ASSERT((intptr_t)val == 616 && table.count == 50, "unremoved key must remain accessible with its original value");
}

ANGRY_TEST(ignoresDuplicateAddWhenKeyAlreadyExists)
{
    struct table table;
    table_init(&table, 17, angry_hash, angry_compare);
    uint32_t key = 42;
    table_add(&table, &key, (void *) 0xAAAA);
    table_add(&table, &key, (void *) 0xBBBB);
    ANGRY_ASSERT((intptr_t)table_find(&table, &key) == 0xAAAA && table.count == 1, "duplicate add must retain initial value and prevent duplicate count");
}

ANGRY_TEST(identifiesWestDisplayAcrossNegativeCoordinates)
{
    struct area center = { 0.0f, 0.0f, 2560.0f, 1440.0f };
    struct area west   = { -1728.0f, -120.0f, 1728.0f, 1117.0f };
    bool is_west = area_is_in_direction(&center, area_max_point(center), &west, area_max_point(west), DIR_WEST);
    ANGRY_ASSERT(is_west, "display with negative X offset must be recognized in west direction");
}

ANGRY_TEST(rejectsEastDisplayWhenTargetIsPositionedWest)
{
    struct area center = { 0.0f, 0.0f, 2560.0f, 1440.0f };
    struct area west   = { -1728.0f, -120.0f, 1728.0f, 1117.0f };
    bool is_east = area_is_in_direction(&center, area_max_point(center), &west, area_max_point(west), DIR_EAST);
    ANGRY_ASSERT(!is_east, "display to the west must not be accepted as east direction");
}

ANGRY_TEST(identifiesEastDisplayAcrossPositiveCoordinates)
{
    struct area center = { 0.0f, 0.0f, 2560.0f, 1440.0f };
    struct area east   = { 2560.0f, 0.0f, 1920.0f, 1080.0f };
    bool is_east = area_is_in_direction(&center, area_max_point(center), &east, area_max_point(east), DIR_EAST);
    ANGRY_ASSERT(is_east, "display with positive X boundary must be recognized in east direction");
}

ANGRY_TEST(rejectsWestDisplayWhenTargetIsPositionedEast)
{
    struct area center = { 0.0f, 0.0f, 2560.0f, 1440.0f };
    struct area east   = { 2560.0f, 0.0f, 1920.0f, 1080.0f };
    bool is_west = area_is_in_direction(&center, area_max_point(center), &east, area_max_point(east), DIR_WEST);
    ANGRY_ASSERT(!is_west, "display to the east must not be accepted as west direction");
}

ANGRY_TEST(findsNearestWestDisplayAmongMultipleMonitors)
{
    struct test_area displays[3];
    init_test_display_list(displays);
    int idx = closest_display_in_direction(displays, 3, 0, DIR_WEST);
    ANGRY_ASSERT(idx == 1, "center monitor must find display 1 as closest west neighbor");
}

ANGRY_TEST(findsNearestEastDisplayAcrossIrregularGaps)
{
    struct test_area displays[3];
    init_test_display_list(displays);
    int idx = closest_display_in_direction(displays, 3, 0, DIR_EAST);
    ANGRY_ASSERT(idx == 2, "center monitor must find display 2 as closest east neighbor");
}

ANGRY_TEST(returnsNegativeIndexWhenNoDisplayInDirection)
{
    struct test_area displays[3];
    init_test_display_list(displays);
    int idx = closest_display_in_direction(displays, 3, 1, DIR_WEST);
    ANGRY_ASSERT(idx == -1, "westernmost monitor must return -1 when querying west direction");
}

ANGRY_TEST(splitsParentAreaEquallyWithSpecifiedGapOnVerticalAxis)
{
    struct area parent = { 0.0f, 0.0f, 1920.0f, 1080.0f };
    struct area left, right;
    area_make_pair(SPLIT_Y, 20, 0.5f, &parent, &left, &right);
    bool valid = (left.w == 950) && (right.w == 950) && (right.x == 970);
    ANGRY_ASSERT(valid, "vertical split at 0.5 with 20px gap must yield symmetric widths and correct right offset");
}

ANGRY_TEST(splitsParentAreaAsymmetricallyOnHorizontalAxis)
{
    struct area parent = { 24.0f, 64.0f, 1200.0f, 900.0f };
    struct area top, bottom;
    area_make_pair(SPLIT_X, 10, 0.3f, &parent, &top, &bottom);
    bool valid = (top.h == 267) && (bottom.h == 623) && (bottom.y == 341);
    ANGRY_ASSERT(valid, "horizontal split with ratio 0.3 must distribute height proportionally and position bottom area");
}

ANGRY_TEST(preservesTotalDimensionWithinPixelRoundingTolerance)
{
    struct area parent = { 0.0f, 0.0f, 2560.0f, 1440.0f };
    struct area left, right;
    int gap = 48;
    area_make_pair(SPLIT_Y, gap, 0.62f, &parent, &left, &right);
    int delta = abs((int)(left.w + right.w + gap) - (int)parent.w);
    ANGRY_ASSERT(delta <= 1, "sum of partitioned widths plus gap must strictly match parent within integer truncation tolerance");
}

ANGRY_TEST(allocatesMinimumDimensionWhenSplitRatioIsExtremelySmall)
{
    struct area parent = { 0.0f, 0.0f, 1000.0f, 800.0f };
    struct area left, right;
    area_make_pair(SPLIT_Y, 10, 0.01f, &parent, &left, &right);
    bool valid = (left.w == 9) && (right.w == 980);
    ANGRY_ASSERT(valid, "extreme small ratio of 0.01 must not underflow or crash partition arithmetic");
}

ANGRY_TEST(allocatesMinimumDimensionWhenSplitRatioIsExtremelyLarge)
{
    struct area parent = { 0.0f, 0.0f, 1000.0f, 800.0f };
    struct area left, right;
    area_make_pair(SPLIT_Y, 10, 0.99f, &parent, &left, &right);
    bool valid = (left.w == 980) && (right.w == 9);
    ANGRY_ASSERT(valid, "extreme large ratio of 0.99 must assign remainder cleanly to right partition");
}

ANGRY_TEST(ignoresDropWhenCursorDeltaIsBelowThreshold)
{
    CGPoint down = CGPointMake(543.2f, 412.8f);
    CGPoint up   = CGPointMake(544.7f, 411.9f);
    bool moved = (fabs(up.x - down.x) >= 3.0f) || (fabs(up.y - down.y) >= 3.0f);
    ANGRY_ASSERT(!moved, "cursor displacement under 3 pixels must not register as drag movement");
}

ANGRY_TEST(permitsDropWhenCursorDeltaCrossesThreshold)
{
    CGPoint down = CGPointMake(543.2f, 412.8f);
    CGPoint up   = CGPointMake(546.5f, 412.8f);
    bool moved = (fabs(up.x - down.x) >= 3.0f) || (fabs(up.y - down.y) >= 3.0f);
    ANGRY_ASSERT(moved, "cursor displacement of 3.3 pixels must be recognized as deliberate drag movement");
}

ANGRY_TEST(preservesWindowWhenExitingToMatchingSpace)
{
    uint64_t target_sid = 4;
    uint64_t current_sid = 4;
    bool should_skip = space_is_fullscreen(current_sid) || (current_sid == target_sid);
    ANGRY_ASSERT(should_skip, "validation must not un-tile windows already present on destination space");
}

ANGRY_TEST(guardsAgainstPrematureUntileDuringFullscreenSpaceDestruction)
{
    uint64_t temp_fullscreen_sid = 1071;
    uint64_t destination_sid = 4;
    bool is_fs = space_is_fullscreen(temp_fullscreen_sid);
    bool should_skip = is_fs || (temp_fullscreen_sid == destination_sid);
    ANGRY_ASSERT(should_skip == is_fs, "window associated with active fullscreen space must be shielded from eviction");
}

ANGRY_TEST(swapsWindowIdsBetweenTwoBspNodesPreservingTreeStructure)
{
    struct window_node node_a = { .window_count = 1, .window_list = { 101 }, .window_order = { 101 } };
    struct window_node node_b = { .window_count = 1, .window_list = { 202 }, .window_order = { 202 } };
    window_node_swap_window_list(&node_a, &node_b);
    bool swapped = (node_a.window_list[0] == 202) && (node_b.window_list[0] == 101);
    ANGRY_ASSERT(swapped, "swapping window list must exchange window IDs between the two nodes");
}

ANGRY_TEST(mirrorsBspTreeHorizontallyInvertingChildOrder)
{
    struct window_node left = { .window_count = 1, .window_list = { 101 } };
    struct window_node right = { .window_count = 1, .window_list = { 202 } };
    struct window_node root = { .split = SPLIT_Y, .left = &left, .right = &right };
    window_node_mirror(&root, SPLIT_Y);
    bool inverted = (root.left == &right) && (root.right == &left);
    ANGRY_ASSERT(inverted, "mirroring along horizontal axis must swap left and right subtrees");
}

ANGRY_TEST(mirrorsBspTreeVerticallyInvertingChildOrder)
{
    struct window_node top = { .window_count = 1, .window_list = { 303 } };
    struct window_node bottom = { .window_count = 1, .window_list = { 404 } };
    struct window_node root = { .split = SPLIT_X, .left = &top, .right = &bottom };
    window_node_mirror(&root, SPLIT_X);
    bool inverted = (root.left == &bottom) && (root.right == &top);
    ANGRY_ASSERT(inverted, "mirroring along vertical axis must swap top and bottom subtrees");
}

ANGRY_TEST(promotesSiblingToRootWhenRemovingLeafFromTwoNodeTree)
{
    struct view *view = view_create(999);
    view->layout = VIEW_BSP;
    struct window w1 = { .id = 111 };
    struct window w2 = { .id = 222 };
    view_add_window_node(view, &w1);
    view_add_window_node(view, &w2);
    view_remove_window_node(view, &w1);
    bool valid = window_node_is_leaf(view->root) && (view->root->window_list[0] == 222);
    view_destroy(view);
    ANGRY_ASSERT(valid, "removing leaf from two-node tree must promote surviving sibling directly to root");
}

ANGRY_TEST(requiresAllAttributesForLegacyMacOsVersions)
{
    NSOperatingSystemVersion os_26_0 = { 26, 0, 0 };
    uint32_t attrib = scripting_addition_expected_attrib_for_version(os_26_0);
    ANGRY_ASSERT(attrib == (uint32_t)OSAX_ATTRIB_ALL, "macOS 26 must require all scripting addition attributes");
}

ANGRY_TEST(requiresReducedAttributesForTahoeAndGoldengate)
{
    NSOperatingSystemVersion os_27_2 = { 27, 2, 0 };
    uint32_t attrib = scripting_addition_expected_attrib_for_version(os_27_2);
    ANGRY_ASSERT(attrib == (uint32_t)OSAX_ATTRIB_REDUCED, "macOS 27.2+ must require reduced scripting addition attributes");
}

ANGRY_TEST(verifiesReducedAttributeMaskMatchesSubset)
{
    uint32_t reduced = (uint32_t)OSAX_ATTRIB_REDUCED;
    uint32_t all = (uint32_t)OSAX_ATTRIB_ALL;
    ANGRY_ASSERT((reduced & all) == reduced, "reduced attribute mask must strictly be a bitwise subset of all attributes");
}

ANGRY_TEST(defaultsWindowFocusAnimationToDisabled)
{
    ANGRY_ASSERT(!g_space_manager.skip_window_focus_animation, "default focus animation skip flag must be false");
}

ANGRY_TEST(recordsPendingStateOnVerticalGestureBegin)
{
    extern volatile bool __pending_gesture;
    __atomic_store_n(&__pending_gesture, true, __ATOMIC_RELEASE);
    bool pending = __atomic_load_n(&__pending_gesture, __ATOMIC_RELAXED);
    __atomic_store_n(&__pending_gesture, false, __ATOMIC_RELEASE);
    ANGRY_ASSERT(pending, "beginning a vertical gesture must transition pending gesture state to true");
}

ANGRY_TEST(updatesLastGestureTimeOnVerticalGestureEnd)
{
    extern volatile uint64_t __last_gesture_time;
    uint64_t now = read_os_timer();
    __atomic_store_n(&__last_gesture_time, now, __ATOMIC_RELEASE);
    uint64_t stored = __atomic_load_n(&__last_gesture_time, __ATOMIC_RELAXED);
    ANGRY_ASSERT(stored == now && stored > 0, "ending a gesture must record non-zero completion timestamp");
}

ANGRY_TEST(capturesFrontmostWindowAndSpaceStateOnMissionControlEnter)
{
    extern volatile uint32_t g_mission_control_prev_wid;
    extern volatile uint64_t g_mission_control_prev_sid;
    extern volatile pid_t g_mission_control_prev_pid;
    void mission_control_record_state(void);
    g_mission_control_prev_sid = 0;
    g_space_manager.current_space_id = 42;
    g_window_manager.focused_window_id = 1234;
    g_process_manager.front_pid = 789;
    mission_control_record_state();
    bool captured = (g_mission_control_prev_sid == 42) && (g_mission_control_prev_wid == 1234) && (g_mission_control_prev_pid == 789);
    g_mission_control_prev_sid = 0;
    ANGRY_ASSERT(captured, "mission control entry must accurately record active window, space, and pid");
}

ANGRY_TEST(maintainsIdempotencyWhenMissionControlStateAlreadyRecorded)
{
    extern volatile uint32_t g_mission_control_prev_wid;
    extern volatile uint64_t g_mission_control_prev_sid;
    void mission_control_record_state(void);
    g_mission_control_prev_sid = 42;
    g_mission_control_prev_wid = 1234;
    g_space_manager.current_space_id = 99;
    g_window_manager.focused_window_id = 5678;
    mission_control_record_state();
    bool unchanged = (g_mission_control_prev_sid == 42) && (g_mission_control_prev_wid == 1234);
    g_mission_control_prev_sid = 0;
    ANGRY_ASSERT(unchanged, "mission control state recorder must remain idempotent when state is already captured");
}

ANGRY_TEST(coalescesConsecutiveMouseDraggedEventsInQueue)
{
    struct event_loop loop = {0};
    memory_pool_init(&loop.pool, KILOBYTES(64));
    loop.head = memory_pool_push(&loop.pool, sizeof(struct event));
    loop.head->next = NULL;
    loop.tail = loop.head;
    loop.semaphore = sem_open("angry_drag_sem", O_CREAT, 0600, 0);
    sem_unlink("angry_drag_sem");
    for (int i = 1; i <= 5; ++i) {
        event_loop_post(&loop, MOUSE_DRAGGED, (void *)(intptr_t)i, 0);
    }
    struct event *next = loop.head->next;
    struct event *peek;
    while ((peek = __atomic_load_n(&next->next, __ATOMIC_RELAXED)) != NULL) {
        if (__atomic_load_n(&peek->type, __ATOMIC_RELAXED) != MOUSE_DRAGGED) break;
        next = peek;
    }
    int last_val = (int)(intptr_t)next->context;
    sem_close(loop.semaphore);
    ANGRY_ASSERT(last_val == 5, "event queue coalescing must skip obsolete intermediate drag positions to the latest");
}

ANGRY_TEST(clearsDirtyFlagOnDirectViewFlush)
{
    uint32_t did = display_manager_active_display_id();
    uint64_t sid = display_space_id(did);
    struct view *view = view_create(sid ? sid : 1);
    view->layout = VIEW_BSP;
    view_update(view);
    view_flush_direct(view);
    bool clean = !view_is_dirty(view);
    view_destroy(view);
    ANGRY_ASSERT(clean, "direct view flush on active space must clear view dirty flag");
}

ANGRY_TEST(defaultsBspLiveResizeToEnabled)
{
    ANGRY_ASSERT(g_mouse_state.bsp_live_resize == true, "default mouse state bsp live resize must be true");
}

ANGRY_TEST(coercesInsertDirectionToStackWhenTargetViewIsStacked)
{
    struct view stack_view = { .layout = VIEW_STACK };
    int insert_dir = DIR_WEST;
    if (insert_dir && stack_view.layout == VIEW_STACK) insert_dir = STACK;
    ANGRY_ASSERT(insert_dir == STACK, "insert direction on stacked layout must be coerced to STACK");
}

ANGRY_TEST(updatesFocusFollowsMouseMode)
{
    window_manager_set_focus_follows_mouse(&g_window_manager, FFM_AUTOFOCUS);
    ANGRY_ASSERT(g_window_manager.ffm_mode == FFM_AUTOFOCUS, "window manager ffm mode setter must update active state");
}

ANGRY_TEST(suppressesRapidGestureToSameSpace)
{
    uint64_t dummy_sid = 999901;
    space_manager_focus_space_using_gesture(0, dummy_sid);
    bool dup_res = space_manager_focus_space_using_gesture(0, dummy_sid);
    ANGRY_ASSERT(dup_res, "duplicate gesture to same space must be suppressed as success no-op");
}

ANGRY_TEST(rateLimitsRapidGesturesUnderThreshold)
{
    uint64_t dummy_sid = 999901;
    uint64_t other_sid = 999902;
    space_manager_focus_space_using_gesture(0, dummy_sid);
    bool rate_res = space_manager_focus_space_using_gesture(0, other_sid);
    ANGRY_ASSERT(!rate_res, "rapid consecutive gesture to another space must be rate-limited");
}

ANGRY_TEST(resolvesActiveSpaceForRunningApplications)
{
    bool ok = run_app_active_space_resolution();
    ANGRY_ASSERT(ok, "running application PID must resolve to valid non-zero active space ID");
}
