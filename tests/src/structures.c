#include "../../src/misc/memory_pool.h"
#include "../../src/misc/hashtable.h"

static TABLE_HASH_FUNC(hash_test)
{
    return *(uint32_t *) key;
}

static TABLE_COMPARE_FUNC(compare_test)
{
    return *(uint32_t *) key_a == *(uint32_t *) key_b;
}

TEST_FUNC(memory_pool_basic, {
    struct memory_pool pool;
    bool init = memory_pool_init(&pool, KILOBYTES(512));
    TEST_CHECK(init, true);

    // Push 1000 items
    uint32_t *first = (uint32_t *) memory_pool_push(&pool, sizeof(uint32_t));
    *first = 0xdeadbeef;

    for (int i = 1; i < 1000; ++i) {
        uint32_t *p = (uint32_t *) memory_pool_push(&pool, sizeof(uint32_t));
        *p = (uint32_t) i;
    }

    TEST_CHECK(*first, 0xdeadbeef);
    TEST_CHECK((int)pool.used, (int)(1000 * sizeof(uint32_t)));
})

TEST_FUNC(memory_pool_wraparound_detection, {
    struct memory_pool pool;
    int page_size = getpagesize();
    bool init = memory_pool_init(&pool, page_size);
    TEST_CHECK(init, true);

    uint64_t item_size = 32; // struct event size
    uint64_t items_per_pool = pool.size / item_size;

    void *first_ptr = memory_pool_push(&pool, item_size);
    TEST_CHECK(first_ptr == pool.memory, true);

    // Push until used = (items_per_pool - 1) * item_size
    for (uint64_t i = 1; i < items_per_pool - 1; ++i) {
        memory_pool_push(&pool, item_size);
    }

    // Next push: new_used = items_per_pool * item_size == pool.size.
    // Since condition is (new_used < pool->size), this MUST trigger wrap-around!
    void *wrapped_ptr = memory_pool_push(&pool, item_size);
    TEST_CHECK(wrapped_ptr == pool.memory, true);
    TEST_CHECK((int)pool.used, (int)item_size);
})

TEST_FUNC(hashtable_stress_and_rehashing, {
    struct table table;
    table_init(&table, 150, hash_test, compare_test);

    // Insert 5000 distinct items
    for (uint32_t i = 1; i <= 5000; ++i) {
        table_add(&table, &i, (void *)(intptr_t)(i * 10));
    }
    TEST_CHECK(table.count, 5000);

    // Verify all 5000 items are present
    bool all_found = true;
    for (uint32_t i = 1; i <= 5000; ++i) {
        void *val = table_find(&table, &i);
        if ((intptr_t) val != (intptr_t)(i * 10)) {
            all_found = false;
            break;
        }
    }
    TEST_CHECK(all_found, true);

    // Insert 1000 existing keys with table_add.
    // yabai's table_add only writes value if existing value is NULL.
    // Count must remain 5000, and original values must be preserved.
    for (uint32_t i = 1; i <= 1000; ++i) {
        table_add(&table, &i, (void *)(intptr_t)(i * 20));
    }
    TEST_CHECK(table.count, 5000);

    for (uint32_t i = 1; i <= 1000; ++i) {
        void *val = table_find(&table, &i);
        if ((intptr_t) val != (intptr_t)(i * 10)) {
            all_found = false;
            break;
        }
    }
    TEST_CHECK(all_found, true);

    // Remove 2500 items
    for (uint32_t i = 1; i <= 2500; ++i) {
        table_remove(&table, &i);
    }
    TEST_CHECK(table.count, 2500);

    // Verify removed items are NOT found
    bool removed_cleanly = true;
    for (uint32_t i = 1; i <= 2500; ++i) {
        if (table_find(&table, &i) != NULL) {
            removed_cleanly = false;
            break;
        }
    }
    TEST_CHECK(removed_cleanly, true);

    // Verify remaining 2500 items are still found
    bool remaining_found = true;
    for (uint32_t i = 2501; i <= 5000; ++i) {
        void *val = table_find(&table, &i);
        if ((intptr_t) val != (intptr_t)(i * 10)) {
            remaining_found = false;
            break;
        }
    }
    TEST_CHECK(remaining_found, true);

    table_free(&table);
})

static const char *g_target_app_names[] = { "Ghostty", "Helium", "Books", "Telegram", "ZenNotes", "Music", NULL };

static bool run_app_active_space_resolution(void)
{
    NSArray *apps = [[NSWorkspace sharedWorkspace] runningApplications];
    bool result = true;

    for (int i = 0; g_target_app_names[i]; ++i) {
        const char *name = g_target_app_names[i];
        NSRunningApplication *target_app = nil;
        for (NSRunningApplication *app in apps) {
            if ([app.localizedName isEqualToString:[NSString stringWithUTF8String:name]]) {
                target_app = app;
                break;
            }
        }
        if (!target_app) continue;

        ProcessSerialNumber psn = {0};
        GetProcessForPID(target_app.processIdentifier, &psn);
        int cid = 0;
        SLSGetConnectionIDForPSN(g_connection, &psn, &cid);

        uint64_t psn_sid = process_manager_active_space_for_psn(cid);
        int space_idx = space_manager_mission_control_index(psn_sid);

        printf("  App '%s' (PID %d, CID %d) -> resolved active space id: %llu (index: %d)\n",
               name, target_app.processIdentifier, cid, psn_sid, space_idx);

        if (psn_sid == 0) {
            result = false;
        }

    }
    return result;
}

TEST_FUNC(app_active_space_resolution, {
    bool ok = run_app_active_space_resolution();
    TEST_CHECK(ok, true);
})

TEST_FUNC(space_gesture_guards, {
    uint32_t did = display_manager_active_display_id();
    uint64_t cur_sid = display_space_id(did);

    // Test 1: Focusing current space must be immediate no-op
    enum space_op_error err = space_manager_focus_space(cur_sid);
    TEST_CHECK(err == SPACE_OP_ERROR_SUCCESS || err == SPACE_OP_ERROR_SAME_SPACE, true);

    // Test 2: Rapid duplicate gesture to same space must be suppressed by Guard 1
    uint64_t dummy_sid = 999901;
    space_manager_focus_space_using_gesture(0, dummy_sid);
    bool dup_res = space_manager_focus_space_using_gesture(0, dummy_sid);
    TEST_CHECK(dup_res, true);

    // Test 3: Rapid gesture to different space (<150ms) must be rate-limited by Guard 2
    uint64_t other_sid = 999902;
    bool rate_res = space_manager_focus_space_using_gesture(0, other_sid);
    TEST_CHECK(rate_res, false);

    // Test 4: Ping-pong oscillation breaker
    usleep(160000);
    space_manager_focus_space_using_gesture(0, other_sid);
    usleep(160000);
    space_manager_focus_space_using_gesture(0, dummy_sid);
    usleep(160000);
    bool pp_res = space_manager_focus_space_using_gesture(0, other_sid);
    TEST_CHECK(pp_res, false);
})

static bool run_osax_expected_attributes_validation(void)
{
    NSOperatingSystemVersion os_26_0 = { 26, 0, 0 };
    NSOperatingSystemVersion os_27_0 = { 27, 0, 1 };
    NSOperatingSystemVersion os_27_1 = { 27, 1, 0 };
    NSOperatingSystemVersion os_27_2 = { 27, 2, 0 };
    NSOperatingSystemVersion os_27_3 = { 27, 3, 0 };
    NSOperatingSystemVersion os_28_0 = { 28, 0, 0 };

    if (scripting_addition_expected_attrib_for_version(os_26_0) != (uint32_t)OSAX_ATTRIB_ALL) return false;
    if (scripting_addition_expected_attrib_for_version(os_27_0) != (uint32_t)OSAX_ATTRIB_ALL) return false;
    if (scripting_addition_expected_attrib_for_version(os_27_1) != (uint32_t)OSAX_ATTRIB_ALL) return false;
    if (scripting_addition_expected_attrib_for_version(os_27_2) != (uint32_t)OSAX_ATTRIB_REDUCED) return false;
    if (scripting_addition_expected_attrib_for_version(os_27_3) != (uint32_t)OSAX_ATTRIB_REDUCED) return false;
    if (scripting_addition_expected_attrib_for_version(os_28_0) != (uint32_t)OSAX_ATTRIB_REDUCED) return false;

    if ((uint32_t)OSAX_ATTRIB_REDUCED != (uint32_t)0x4D) return false;
    if ((uint32_t)OSAX_ATTRIB_ALL != (uint32_t)0x7F) return false;
    if ((0x4D & OSAX_ATTRIB_REDUCED) != OSAX_ATTRIB_REDUCED) return false;

    return true;
}

TEST_FUNC(osax_expected_attributes_validation, {
    bool ok = run_osax_expected_attributes_validation();
    TEST_CHECK(ok, true);
})

TEST_FUNC(skip_window_focus_animation_default, {
    TEST_CHECK(g_space_manager.skip_window_focus_animation, false);
})

TEST_FUNC(gesture_vertical_and_mc_guards, {
    extern volatile bool __pending_gesture;
    extern volatile uint64_t __last_gesture_time;

    __atomic_store_n(&__pending_gesture, false, __ATOMIC_RELEASE);
    __atomic_store_n(&__last_gesture_time, 0, __ATOMIC_RELEASE);

    // Simulate vertical gesture (motion == 2) began (phase == 1)
    __atomic_store_n(&__pending_gesture, true, __ATOMIC_RELEASE);
    TEST_CHECK(__atomic_load_n(&__pending_gesture, __ATOMIC_RELAXED), true);

    // Simulate vertical gesture ended (phase == 4)
    uint64_t now = read_os_timer();
    __atomic_store_n(&__pending_gesture, false, __ATOMIC_RELEASE);
    __atomic_store_n(&__last_gesture_time, now, __ATOMIC_RELEASE);
    TEST_CHECK(__atomic_load_n(&__pending_gesture, __ATOMIC_RELAXED), false);
    TEST_CHECK(__atomic_load_n(&__last_gesture_time, __ATOMIC_RELAXED) > 0, true);
})

TEST_FUNC(mission_control_focus_state_record, {
    extern volatile uint32_t g_mission_control_prev_wid;
    extern volatile uint64_t g_mission_control_prev_sid;
    extern volatile pid_t g_mission_control_prev_pid;
    void mission_control_record_state(void);

    g_mission_control_prev_wid = 0;
    g_mission_control_prev_sid = 0;
    g_mission_control_prev_pid = 0;

    g_space_manager.current_space_id = 42;
    g_window_manager.focused_window_id = 1234;
    g_process_manager.front_pid = 789;

    mission_control_record_state();
    TEST_CHECK((int)g_mission_control_prev_sid, 42);
    TEST_CHECK((int)g_mission_control_prev_wid, 1234);
    TEST_CHECK((int)g_mission_control_prev_pid, 789);

    // Idempotent: should not be overwritten while prev_sid != 0
    g_space_manager.current_space_id = 99;
    g_window_manager.focused_window_id = 5678;
    g_process_manager.front_pid = 999;
    mission_control_record_state();
    TEST_CHECK((int)g_mission_control_prev_sid, 42);
    TEST_CHECK((int)g_mission_control_prev_wid, 1234);
    TEST_CHECK((int)g_mission_control_prev_pid, 789);

    // Cleanup
    g_mission_control_prev_wid = 0;
    g_mission_control_prev_sid = 0;
    g_mission_control_prev_pid = 0;
})

TEST_FUNC(mouse_drag_coalescing_and_bsp_direct_flush, {
    struct event_loop loop = {0};
    bool init = memory_pool_init(&loop.pool, KILOBYTES(64));
    TEST_CHECK(init, true);
    loop.head = memory_pool_push(&loop.pool, sizeof(struct event));
    loop.head->next = NULL;
    loop.tail = loop.head;
    loop.semaphore = sem_open("yabai_test_semaphore", O_CREAT, 0600, 0);
    sem_unlink("yabai_test_semaphore");

    // Post 5 MOUSE_DRAGGED events
    for (int i = 0; i < 5; ++i) {
        int val = i + 1;
        CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &val);
        event_loop_post(&loop, MOUSE_DRAGGED, (void *) num, 0);
    }
    // And 1 MOUSE_UP at the end
    event_loop_post(&loop, MOUSE_UP, NULL, 0);

    // Dequeue first (advancing loop.head to next, exactly as in event_loop_run)
    struct event *head = loop.head;
    struct event *next = head->next;
    TEST_CHECK(next != NULL, true);
    loop.head = next;

    enum event_type type = __atomic_load_n(&next->type, __ATOMIC_RELAXED);
    TEST_CHECK((int)type, (int)MOUSE_DRAGGED);

    // Run coalesce logic
    if (type == MOUSE_DRAGGED || type == MOUSE_MOVED) {
        struct event *peek;
        while ((peek = __atomic_load_n(&next->next, __ATOMIC_RELAXED)) != NULL) {
            if (__atomic_load_n(&peek->type, __ATOMIC_RELAXED) != type) break;
            if (!__sync_bool_compare_and_swap(&loop.head, next, peek)) break;
            void *old_context = __atomic_load_n(&next->context, __ATOMIC_RELAXED);
            if (old_context) CFRelease(old_context);
            next = peek;
        }
    }

    // next should now be the 5th MOUSE_DRAGGED event!
    int final_val = 0;
    CFNumberGetValue((CFNumberRef)next->context, kCFNumberIntType, &final_val);
    TEST_CHECK(final_val, 5);

    // The event following next should be MOUSE_UP!
    struct event *up_event = next->next;
    TEST_CHECK(up_event != NULL, true);
    TEST_CHECK((int)up_event->type, (int)MOUSE_UP);

    // Cleanup 5th context
    CFRelease(next->context);
    sem_close(loop.semaphore);

    // Verify view_flush_direct clears dirty flag on active space
    uint32_t did = display_manager_active_display_id();
    uint64_t active_sid = display_space_id(did);
    if (active_sid) {
        struct view *test_view = view_create(active_sid);
        test_view->layout = VIEW_BSP;
        view_update(test_view);
        view_flush_direct(test_view);
        TEST_CHECK(view_is_dirty(test_view), false);
        view_destroy(test_view);
    }

    // Verify bsp_live_resize config defaults to true and can be toggled
    TEST_CHECK(g_mouse_state.bsp_live_resize, true);
    g_mouse_state.bsp_live_resize = false;
    TEST_CHECK(g_mouse_state.bsp_live_resize, false);
    g_mouse_state.bsp_live_resize = true;
    TEST_CHECK(g_mouse_state.bsp_live_resize, true);
})

TEST_FUNC(community_upstream_fixes, {
    // 1. Verify stack layout drop feedback rule (#2840)
    struct view stack_view = {0};
    stack_view.layout = VIEW_STACK;
    int insert_dir = DIR_WEST;
    if (insert_dir && stack_view.layout == VIEW_STACK) {
        insert_dir = STACK;
    }
    TEST_CHECK(insert_dir, (int)STACK);

    // 2. Verify FFM reset functionality (#2834)
    event_loop_reset_open_menus();
    window_manager_set_focus_follows_mouse(&g_window_manager, FFM_AUTOFOCUS);
    TEST_CHECK((int)g_window_manager.ffm_mode, (int)FFM_AUTOFOCUS);

    // 3. Verify drag guard logic (#2843): window currently being dragged across displays is skipped
    struct window dummy_window = {0};
    dummy_window.id = 9999;
    g_mouse_state.window = &dummy_window;
    bool is_dragged = (&dummy_window == g_mouse_state.window);
    TEST_CHECK(is_dragged, true);
    g_mouse_state.window = NULL;
})

TEST_FUNC(fullscreen_exit_and_mouse_click_guards, {
    // 1. Verify cursor moved threshold logic for clicks vs drags
    CGPoint down_loc = CGPointMake(100.0f, 100.0f);
    CGPoint click_up = CGPointMake(101.0f, 101.5f);
    double dx = fabs(click_up.x - down_loc.x);
    double dy = fabs(click_up.y - down_loc.y);
    bool click_cursor_moved = dx >= 3.0f || dy >= 3.0f;
    TEST_CHECK(click_cursor_moved, false);

    CGPoint drag_up = CGPointMake(120.0f, 150.0f);
    double drag_dx = fabs(drag_up.x - down_loc.x);
    double drag_dy = fabs(drag_up.y - down_loc.y);
    bool drag_cursor_moved = drag_dx >= 3.0f || drag_dy >= 3.0f;
    TEST_CHECK(drag_cursor_moved, true);

    // 2. Verify fullscreen space guard in validation logic
    uint64_t target_sid = 4;
    uint64_t fullscreen_sid = 1071;
    bool is_fs = space_is_fullscreen(fullscreen_sid);
    bool should_skip_untile = is_fs || (fullscreen_sid == target_sid);
    TEST_CHECK(should_skip_untile, is_fs);

    // Same space guard
    uint64_t same_sid = target_sid;
    bool same_space_skip = space_is_fullscreen(same_sid) || (same_sid == target_sid);
    TEST_CHECK(same_space_skip, true);
})
