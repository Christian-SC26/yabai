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
