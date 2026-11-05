//
// NOTE: in-process benchmarks for the code yabai runs on every event and
// message: layout trees, rules, signals, the message parser, the event queue,
// the containers and the SkyLight reads behind queries. The file pulls in the
// test suite for its fakes, so nothing measured here can move, focus or hide a
// real window; the reads of displays and spaces go to the real WindowServer,
// which is what they cost in the daemon too.
//
// For every benchmark it reports wall and thread CPU time per operation over
// several calibrated samples, allocations and allocated bytes per operation
// (counted through malloc_logger in a separate pass, so the counting does not
// slow the timed samples), and the heap bytes an operation leaves behind.
//

#define ANGRY_BENCH
#include "../../tests/src/tests.m"
#include <mach/mach_time.h>
#include <malloc/malloc.h>
#include <sys/utsname.h>

#ifndef BENCH_GIT
#define BENCH_GIT "unknown"
#endif

extern void (*malloc_logger)(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t skip);

#define MALLOC_LOG_ALLOCATE   0x02
#define MALLOC_LOG_DEALLOCATE 0x04

static volatile uint64_t g_bench_allocs;
static volatile uint64_t g_bench_alloc_bytes;

static void bench_malloc_logger(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t skip)
{
    if (!(type & MALLOC_LOG_ALLOCATE)) return;
    __atomic_fetch_add(&g_bench_allocs, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_bench_alloc_bytes, (type & MALLOC_LOG_DEALLOCATE) ? arg3 : arg2, __ATOMIC_RELAXED);
}

static uint64_t bench_now_ns(void)
{
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

static uint64_t bench_cpu_ns(void)
{
    return clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID);
}

static uint64_t bench_heap_in_use(void)
{
    malloc_statistics_t stats;
    malloc_zone_statistics(NULL, &stats);
    return stats.size_in_use;
}

static void bench_footprint(uint64_t *now, uint64_t *peak)
{
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &info, &count) == KERN_SUCCESS) {
        *now = info.phys_footprint;
        *peak = info.ledger_phys_footprint_peak;
    } else {
        *now = *peak = 0;
    }
}

static void *volatile g_bench_sink;
static FILE *g_bench_null;

//
// NOTE: a benchmark runs its operation n times per call. Setup builds the
// fixture once, outside every measurement, and teardown releases it.
//
struct bench
{
    const char *name;
    void *(*setup)(void);
    void (*run)(void *ctx, uint64_t n);
    void (*teardown)(void *ctx);
};

// ---------------------------------------------------------------------------
// fixtures
// ---------------------------------------------------------------------------

static uint64_t bench_sid(void)
{
    static uint64_t sid;
    if (!sid) sid = display_space_id(display_manager_active_display_id());
    return sid;
}

struct bench_view
{
    struct application app;
    struct window *windows;
    int count;
    struct view *view;
    bool registered;
};

static struct bench_view *bench_view_create(int count, enum view_type layout, bool fill)
{
    struct bench_view *bv = calloc(1, sizeof(struct bench_view));
    bv->app = (struct application) { .pid = 4400, .name = "BenchApp" };
    bv->count = count;
    bv->windows = calloc(count, sizeof(struct window));
    for (int i = 0; i < count; ++i) {
        bv->windows[i] = (struct window) {
            .id = 0xBE000000 + i + 1,
            .application = &bv->app,
            .title = CFSTR("bench document"),
            .frame = {{0, 0}, {400, 300}}
        };
    }

    bv->view = view_create(bench_sid());
    bv->view->layout = layout;

    if (fill) {
        for (int i = 0; i < count; ++i) view_add_window_node(bv->view, &bv->windows[i]);
        view_update(bv->view);
    }

    return bv;
}

static void bench_view_register(struct bench_view *bv)
{
    for (int i = 0; i < bv->count; ++i) table_add(&g_window_manager.window, &bv->windows[i].id, &bv->windows[i]);
    bv->registered = true;
}

static void bench_view_destroy(void *ctx)
{
    struct bench_view *bv = ctx;
    if (bv->registered) {
        for (int i = 0; i < bv->count; ++i) table_remove(&g_window_manager.window, &bv->windows[i].id);
    }
    view_destroy(bv->view);
    free(bv->view);
    free(bv->windows);
    free(bv);
}

//
// NOTE: a message the way the client sends it: arguments separated by a nul
// byte and the list closed by another one.
//
struct bench_message
{
    char *buf;
    int len;
};

static struct bench_message bench_message_make(int argc, const char **argv)
{
    int len = 1;
    for (int i = 0; i < argc; ++i) len += strlen(argv[i]) + 1;
    char *buf = malloc(len);
    char *cursor = buf;
    for (int i = 0; i < argc; ++i) {
        size_t l = strlen(argv[i]);
        memcpy(cursor, argv[i], l);
        cursor += l;
        *cursor++ = '\0';
    }
    *cursor = '\0';
    return (struct bench_message) { buf, len };
}

//
// NOTE: handle_message tokenizes the buffer in place, so every call gets a
// fresh copy of the original message.
//
static void bench_send(struct bench_message *msg)
{
    char copy[1024];
    memcpy(copy, msg->buf, msg->len);
    handle_message(g_bench_null, copy);
    ts_reset();
}

#define BENCH_SEND(...) \
    do { \
        const char *argv[] = { __VA_ARGS__ }; \
        struct bench_message m = bench_message_make(array_count(argv), argv); \
        bench_send(&m); \
        free(m.buf); \
    } while (0)

// ---------------------------------------------------------------------------
// view: the layout tree behind every tiled space
// ---------------------------------------------------------------------------

static void run_view_bsp_build(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) {
        for (int i = 0; i < bv->count; ++i) view_add_window_node(bv->view, &bv->windows[i]);
        for (int i = bv->count - 1; i >= 0; --i) view_remove_window_node(bv->view, &bv->windows[i]);
    }
}

static void run_view_update(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) view_update(bv->view);
}

static void run_view_find_window_node(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) {
        g_bench_sink = view_find_window_node(bv->view, bv->windows[k % bv->count].id);
    }
}

static void run_view_find_direction(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    int dirs[] = { DIR_EAST, DIR_WEST, DIR_NORTH, DIR_SOUTH };
    for (uint64_t k = 0; k < n; ++k) {
        struct window_node *node = view_find_window_node(bv->view, bv->windows[k % bv->count].id);
        g_bench_sink = view_find_window_node_in_direction(bv->view, node, dirs[k & 3]);
    }
}

static void run_view_window_list(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) {
        int count;
        g_bench_sink = view_find_window_list(bv->view, &count);
        ts_reset();
    }
}

static void run_view_rotate(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) window_node_rotate(bv->view->root, 90);
}

static void run_view_mirror(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) window_node_mirror(bv->view->root, (k & 1) ? SPLIT_X : SPLIT_Y);
}

static void run_view_balance(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) window_node_balance(bv->view->root, SPLIT_X | SPLIT_Y);
}

static void run_view_equalize(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) window_node_equalize(bv->view->root, SPLIT_X | SPLIT_Y);
}

static void run_view_capture(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) {
        struct window_capture *list = NULL;
        window_node_capture_windows(bv->view->root, &list);
        g_bench_sink = list;
        ts_reset();
    }
}

static void run_view_serialize(void *ctx, uint64_t n)
{
    struct bench_view *bv = ctx;
    for (uint64_t k = 0; k < n; ++k) {
        view_serialize(g_bench_null, bv->view, ~0ULL);
        ts_reset();
    }
}

#define BENCH_VIEW_SETUPS(N) \
    static void *setup_view_empty_##N(void) { return bench_view_create(N, VIEW_BSP, false); } \
    static void *setup_view_bsp_##N(void)   { return bench_view_create(N, VIEW_BSP, true); } \
    static void *setup_view_managed_##N(void) { struct bench_view *bv = bench_view_create(N, VIEW_BSP, true); bench_view_register(bv); return bv; }

BENCH_VIEW_SETUPS(4)
BENCH_VIEW_SETUPS(16)
BENCH_VIEW_SETUPS(64)

//
// NOTE: a stack holds at most NODE_MAX_WINDOW_COUNT windows in one node, so
// the largest stack measured is a full one.
//
static void *setup_view_stack_4(void)  { return bench_view_create(4,  VIEW_STACK, true); }
static void *setup_view_stack_16(void) { return bench_view_create(16, VIEW_STACK, true); }
static void *setup_view_stack_32(void) { return bench_view_create(NODE_MAX_WINDOW_COUNT, VIEW_STACK, true); }

// ---------------------------------------------------------------------------
// layout store: dumping a space's tree as json
// ---------------------------------------------------------------------------

#define BENCH_LAYOUT_SID 0xBE7C0001ULL

struct bench_layout
{
    struct application app;
    struct window windows[16];
    struct view *view;
};

static void *setup_layout_16(void)
{
    struct bench_layout *bl = calloc(1, sizeof(struct bench_layout));
    bl->app = (struct application) { .pid = 4400, .name = "BenchApp" };
    bl->view = space_manager_find_view(&g_space_manager, BENCH_LAYOUT_SID);
    bl->view->layout = VIEW_BSP;
    for (int i = 0; i < 16; ++i) {
        bl->windows[i] = (struct window) { .id = 0xBE100000 + i + 1, .application = &bl->app, .title = CFSTR("bench document"), .frame = {{0, 0}, {400, 300}} };
        table_add(&g_window_manager.window, &bl->windows[i].id, &bl->windows[i]);
        view_add_window_node(bl->view, &bl->windows[i]);
    }
    return bl;
}

static void teardown_layout_16(void *ctx)
{
    struct bench_layout *bl = ctx;
    for (int i = 15; i >= 0; --i) {
        view_remove_window_node(bl->view, &bl->windows[i]);
        table_remove(&g_window_manager.window, &bl->windows[i].id);
    }
    free(bl);
}

static void run_layout_dump(void *ctx, uint64_t n)
{
    char err[256];
    for (uint64_t k = 0; k < n; ++k) {
        layout_store_dump(BENCH_LAYOUT_SID, g_bench_null, err, sizeof(err));
        ts_reset();
    }
}

// ---------------------------------------------------------------------------
// rules: matching a new window against the rule list
// ---------------------------------------------------------------------------

struct bench_rules
{
    struct application app;
    struct window window;
    int count;
};

static void bench_rules_add(int count)
{
    for (int i = 0; i < count; ++i) {
        char app[64], label[64];
        snprintf(app, sizeof(app), "app=^BenchApp%d$", i);
        snprintf(label, sizeof(label), "label=bench_rule_%d", i);
        BENCH_SEND("rule", "--add", app, "title=^(Preferences|Settings).*", "manage=off", label);
    }
}

static void bench_rules_remove(int count)
{
    for (int i = 0; i < count; ++i) {
        char label[64];
        snprintf(label, sizeof(label), "bench_rule_%d", i);
        BENCH_SEND("rule", "--remove", label);
    }
}

static void *bench_rules_create(int count)
{
    struct bench_rules *br = calloc(1, sizeof(struct bench_rules));
    br->app = (struct application) { .pid = 4400, .name = "BenchApp7" };
    br->window = (struct window) { .id = 0xBE200001, .application = &br->app, .title = CFSTR("Settings — General") };
    br->count = count;
    bench_rules_add(count);
    return br;
}

static void *setup_rules_1(void)  { return bench_rules_create(1); }
static void *setup_rules_50(void) { return bench_rules_create(50); }

static void teardown_rules(void *ctx)
{
    struct bench_rules *br = ctx;
    bench_rules_remove(br->count);
    free(br);
}

static void run_rules_match(void *ctx, uint64_t n)
{
    struct bench_rules *br = ctx;
    int count = buf_len(g_window_manager.rules);
    for (uint64_t k = 0; k < n; ++k) {
        int matched = 0;
        for (int i = 0; i < count; ++i) {
            matched += window_manager_rule_matches_window(&g_window_manager.rules[i], &br->window, "Settings — General", "AXWindow", "AXStandardWindow");
        }
        g_bench_sink = (void *)(intptr_t) matched;
    }
}

static void run_rules_add_remove(void *ctx, uint64_t n)
{
    for (uint64_t k = 0; k < n; ++k) {
        BENCH_SEND("rule", "--add", "app=^BenchChurn$", "title=^Bench.*", "manage=off", "label=bench_churn");
        BENCH_SEND("rule", "--remove", "bench_churn");
    }
}

// ---------------------------------------------------------------------------
// signals: deciding which subscribers an event wakes up
// ---------------------------------------------------------------------------

static void *setup_signals_50(void)
{
    for (int i = 0; i < 50; ++i) {
        char app[64], label[64];
        snprintf(app, sizeof(app), "app=^BenchApp%d$", i);
        snprintf(label, sizeof(label), "label=bench_signal_%d", i);
        BENCH_SEND("signal", "--add", "event=window_focused", app, "title=document", "action=true", label);
    }
    return NULL;
}

static void teardown_signals_50(void *ctx)
{
    for (int i = 0; i < 50; ++i) {
        char label[64];
        snprintf(label, sizeof(label), "bench_signal_%d", i);
        BENCH_SEND("signal", "--remove", label);
    }
}

static void run_signals_filter(void *ctx, uint64_t n)
{
    struct event_signal es = { .type = SIGNAL_WINDOW_FOCUSED, .app = "BenchApp7", .title = "bench document", .active = true };
    struct signal *list = g_signal_event[SIGNAL_WINDOW_FOCUSED];
    int count = buf_len(list);
    for (uint64_t k = 0; k < n; ++k) {
        int woken = 0;
        for (int i = 0; i < count; ++i) woken += !event_signal_filter(&es, &list[i]);
        g_bench_sink = (void *)(intptr_t) woken;
    }
}

static void run_signals_add_remove(void *ctx, uint64_t n)
{
    for (uint64_t k = 0; k < n; ++k) {
        BENCH_SEND("signal", "--add", "event=window_created", "app=^BenchChurn$", "action=true", "label=bench_churn");
        BENCH_SEND("signal", "--remove", "bench_churn");
    }
}

// ---------------------------------------------------------------------------
// messages: what the daemon does for one `yabai -m` call, minus the socket
// ---------------------------------------------------------------------------

struct bench_msg_ctx
{
    struct bench_message msg;
};

//
// NOTE: a message that the daemon answers with a failure measures the error
// path instead of the command, so the first answer is checked and a failure
// is reported next to the numbers.
//
static void *bench_msg_create(int argc, const char **argv, bool expect_failure)
{
    struct bench_msg_ctx *mc = malloc(sizeof(struct bench_msg_ctx));
    mc->msg = bench_message_make(argc, argv);

    char copy[1024], answer[256] = {0};
    memcpy(copy, mc->msg.buf, mc->msg.len);
    FILE *rsp = fmemopen(answer, sizeof(answer) - 1, "w");
    handle_message(rsp, copy);
    fclose(rsp);
    ts_reset();

    bool failed = answer[0] == FAILURE_MESSAGE[0];
    if (failed != expect_failure) {
        char *nl = strchr(answer, '\n');
        if (nl) *nl = '\0';
        fprintf(stderr, "  ! %s %s: daemon answered %s\n", argv[0], argc > 1 ? argv[1] : "", failed ? answer + 1 : "without the expected failure");
    }

    return mc;
}

static void teardown_msg(void *ctx)
{
    struct bench_msg_ctx *mc = ctx;
    free(mc->msg.buf);
    free(mc);
}

static void run_msg(void *ctx, uint64_t n)
{
    struct bench_msg_ctx *mc = ctx;
    for (uint64_t k = 0; k < n; ++k) bench_send(&mc->msg);
}

#define BENCH_MSG_SETUP_EXPECT(name, failure, ...) \
    static void *setup_msg_##name(void) { const char *argv[] = { __VA_ARGS__ }; return bench_msg_create(array_count(argv), argv, failure); }
#define BENCH_MSG_SETUP(name, ...) BENCH_MSG_SETUP_EXPECT(name, false, __VA_ARGS__)

BENCH_MSG_SETUP(query_displays, "query", "--displays")
BENCH_MSG_SETUP(query_spaces,   "query", "--spaces")
BENCH_MSG_SETUP(query_space,    "query", "--spaces", "--space")
BENCH_MSG_SETUP(query_windows,  "query", "--windows")
BENCH_MSG_SETUP(query_window_ids, "query", "--windows", "id,app,space")
BENCH_MSG_SETUP(config_get,     "config", "layout")
BENCH_MSG_SETUP(config_set,     "config", "split_ratio", "0.5")
BENCH_MSG_SETUP(rule_list,      "rule", "--list")
BENCH_MSG_SETUP(signal_list,    "signal", "--list")
BENCH_MSG_SETUP_EXPECT(unknown_domain, true, "bench_unknown_domain")

// ---------------------------------------------------------------------------
// event loop: the queue every system event goes through
// ---------------------------------------------------------------------------

struct bench_loop
{
    struct event_loop loop;
};

static void *setup_event_loop(void)
{
    struct bench_loop *bl = calloc(1, sizeof(struct bench_loop));
    memory_pool_init(&bl->loop.pool, KILOBYTES(512));
    bl->loop.semaphore = sem_open("yabai_bench_event_loop_sem", O_CREAT, 0600, 0);
    sem_unlink("yabai_bench_event_loop_sem");
    bl->loop.head = memory_pool_push(&bl->loop.pool, sizeof(struct event));
    bl->loop.head->next = NULL;
    bl->loop.tail = bl->loop.head;
    return bl;
}

static void teardown_event_loop(void *ctx)
{
    struct bench_loop *bl = ctx;
    sem_close(bl->loop.semaphore);
    munmap(bl->loop.pool.memory, bl->loop.pool.size + getpagesize());
    free(bl);
}

//
// NOTE: one operation is a post and the matching take off the queue, the
// work the posting thread and the event loop thread do for every event.
//
static void run_event_loop_post_take(void *ctx, uint64_t n)
{
    struct bench_loop *bl = ctx;
    for (uint64_t k = 0; k < n; ++k) {
        event_loop_post(&bl->loop, MOUSE_MOVED, NULL, (int) k);
        sem_trywait(bl->loop.semaphore);
        struct event *head = bl->loop.head;
        struct event *next = head->next;
        if (next) bl->loop.head = next;
        g_bench_sink = next;
    }
}

// ---------------------------------------------------------------------------
// containers and allocators
// ---------------------------------------------------------------------------

static TABLE_HASH_FUNC(bench_hash_u32) { return *(uint32_t *) key; }
static TABLE_COMPARE_FUNC(bench_compare_u32) { return *(uint32_t *) key_a == *(uint32_t *) key_b; }

struct bench_table
{
    struct table table;
    uint32_t keys[1024];
};

static void *setup_table_1024(void)
{
    struct bench_table *bt = calloc(1, sizeof(struct bench_table));
    table_init(&bt->table, 150, bench_hash_u32, bench_compare_u32);
    for (int i = 0; i < 1024; ++i) bt->keys[i] = 0xBE000000 + i * 7;
    for (int i = 0; i < 1024; ++i) table_add(&bt->table, &bt->keys[i], &bt->keys[i]);
    return bt;
}

static void teardown_table(void *ctx)
{
    struct bench_table *bt = ctx;
    table_free(&bt->table);
    free(bt);
}

static void run_table_find(void *ctx, uint64_t n)
{
    struct bench_table *bt = ctx;
    for (uint64_t k = 0; k < n; ++k) g_bench_sink = table_find(&bt->table, &bt->keys[(k * 31) & 1023]);
}

static void run_table_add_remove(void *ctx, uint64_t n)
{
    struct bench_table *bt = ctx;
    uint32_t key = 0xBEEF0000;
    for (uint64_t k = 0; k < n; ++k) {
        table_add(&bt->table, &key, &key);
        table_remove(&bt->table, &key);
    }
}

static void run_ts_alloc(void *ctx, uint64_t n)
{
    for (uint64_t k = 0; k < n; ++k) {
        for (int i = 0; i < 16; ++i) g_bench_sink = ts_alloc_unaligned(64);
        ts_reset();
    }
}

// ---------------------------------------------------------------------------
// system: the WindowServer reads behind most commands
// ---------------------------------------------------------------------------

static void run_sys_active_display(void *ctx, uint64_t n)
{
    for (uint64_t k = 0; k < n; ++k) g_bench_sink = (void *)(uintptr_t) display_manager_active_display_id();
}

static void run_sys_display_space(void *ctx, uint64_t n)
{
    uint32_t did = display_manager_active_display_id();
    for (uint64_t k = 0; k < n; ++k) g_bench_sink = (void *)(uintptr_t) display_space_id(did);
}

static void run_sys_display_bounds(void *ctx, uint64_t n)
{
    uint32_t did = display_manager_active_display_id();
    for (uint64_t k = 0; k < n; ++k) {
        CGRect r = display_bounds_constrained(did, false);
        g_bench_sink = (void *)(uintptr_t) r.size.width;
    }
}

static void run_sys_space_window_list(void *ctx, uint64_t n)
{
    uint64_t sid = bench_sid();
    for (uint64_t k = 0; k < n; ++k) {
        int count;
        g_bench_sink = space_window_list(sid, &count, true);
        ts_reset();
    }
}

static void run_sys_space_type(void *ctx, uint64_t n)
{
    uint64_t sid = bench_sid();
    for (uint64_t k = 0; k < n; ++k) g_bench_sink = (void *)(uintptr_t) space_is_user(sid);
}

static void run_sys_cursor_space(void *ctx, uint64_t n)
{
    for (uint64_t k = 0; k < n; ++k) g_bench_sink = (void *)(uintptr_t) space_manager_cursor_space();
}

// ---------------------------------------------------------------------------
// registry
// ---------------------------------------------------------------------------

#define BENCH_VIEW_ENTRIES(N) \
    { "view/bsp_build/" #N,      setup_view_empty_##N,   run_view_bsp_build,        bench_view_destroy }, \
    { "view/bsp_update/" #N,     setup_view_bsp_##N,     run_view_update,           bench_view_destroy }, \
    { "view/find_node/" #N,      setup_view_bsp_##N,     run_view_find_window_node, bench_view_destroy }, \
    { "view/find_direction/" #N, setup_view_bsp_##N,     run_view_find_direction,   bench_view_destroy }, \
    { "view/window_list/" #N,    setup_view_bsp_##N,     run_view_window_list,      bench_view_destroy }, \
    { "view/rotate/" #N,         setup_view_bsp_##N,     run_view_rotate,           bench_view_destroy }, \
    { "view/mirror/" #N,         setup_view_bsp_##N,     run_view_mirror,           bench_view_destroy }, \
    { "view/balance/" #N,        setup_view_bsp_##N,     run_view_balance,          bench_view_destroy }, \
    { "view/equalize/" #N,       setup_view_bsp_##N,     run_view_equalize,         bench_view_destroy }, \
    { "view/capture/" #N,        setup_view_managed_##N, run_view_capture,          bench_view_destroy }, \
    { "view/serialize/" #N,      setup_view_bsp_##N,     run_view_serialize,        bench_view_destroy },

static struct bench g_benches[] = {
    BENCH_VIEW_ENTRIES(4)
    BENCH_VIEW_ENTRIES(16)
    BENCH_VIEW_ENTRIES(64)
    { "view/stack_update/4",      setup_view_stack_4,   run_view_update,          bench_view_destroy },
    { "view/stack_update/16",     setup_view_stack_16,  run_view_update,          bench_view_destroy },
    { "view/stack_update/32",     setup_view_stack_32,  run_view_update,          bench_view_destroy },
    { "layout/dump/16",           setup_layout_16,      run_layout_dump,          teardown_layout_16 },
    { "rule/match/1",             setup_rules_1,        run_rules_match,          teardown_rules },
    { "rule/match/50",            setup_rules_50,       run_rules_match,          teardown_rules },
    { "rule/add_remove",          NULL,                 run_rules_add_remove,     NULL },
    { "signal/filter/50",         setup_signals_50,     run_signals_filter,       teardown_signals_50 },
    { "signal/add_remove",        NULL,                 run_signals_add_remove,   NULL },
    { "message/query_displays",   setup_msg_query_displays,   run_msg, teardown_msg },
    { "message/query_spaces",     setup_msg_query_spaces,     run_msg, teardown_msg },
    { "message/query_space",      setup_msg_query_space,      run_msg, teardown_msg },
    { "message/query_windows",    setup_msg_query_windows,    run_msg, teardown_msg },
    { "message/query_window_ids", setup_msg_query_window_ids, run_msg, teardown_msg },
    { "message/config_get",       setup_msg_config_get,       run_msg, teardown_msg },
    { "message/config_set",       setup_msg_config_set,       run_msg, teardown_msg },
    { "message/rule_list",        setup_msg_rule_list,        run_msg, teardown_msg },
    { "message/signal_list",      setup_msg_signal_list,      run_msg, teardown_msg },
    { "message/unknown_domain",   setup_msg_unknown_domain,   run_msg, teardown_msg },
    { "event_loop/post_take",     setup_event_loop,     run_event_loop_post_take, teardown_event_loop },
    { "table/find/1024",          setup_table_1024,     run_table_find,           teardown_table },
    { "table/add_remove/1024",    setup_table_1024,     run_table_add_remove,     teardown_table },
    { "ts/alloc_reset/16",        NULL,                 run_ts_alloc,             NULL },
    { "sys/active_display",       NULL,                 run_sys_active_display,   NULL },
    { "sys/display_space",        NULL,                 run_sys_display_space,    NULL },
    { "sys/display_bounds",       NULL,                 run_sys_display_bounds,   NULL },
    { "sys/space_window_list",    NULL,                 run_sys_space_window_list, NULL },
    { "sys/space_is_user",        NULL,                 run_sys_space_type,       NULL },
    { "sys/cursor_space",         NULL,                 run_sys_cursor_space,     NULL },
};

// ---------------------------------------------------------------------------
// runner
// ---------------------------------------------------------------------------

struct bench_result
{
    const char *name;
    uint64_t iters;
    int samples;
    double ns_min, ns_median, ns_p90, ns_mean, cv;
    double cpu_ns;
    double allocs, alloc_bytes, retained_bytes;
};

static int bench_compare_double(const void *a, const void *b)
{
    double x = *(const double *) a, y = *(const double *) b;
    return (x > y) - (x < y);
}

static double bench_quantile(double *sorted, int count, double q)
{
    double pos = q * (count - 1);
    int lo = (int) pos;
    int hi = lo + 1 < count ? lo + 1 : lo;
    return sorted[lo] + (sorted[hi] - sorted[lo]) * (pos - lo);
}

static void bench_measure(struct bench *b, int samples, double sample_ms, struct bench_result *r)
{
    void *ctx = b->setup ? b->setup() : NULL;

    // NOTE: warm caches and lazy state, then grow n until one call fills a sample.
    b->run(ctx, 1);
    uint64_t n = 1;
    uint64_t target_ns = (uint64_t)(sample_ms * 1e6);
    for (;;) {
        uint64_t t0 = bench_now_ns();
        b->run(ctx, n);
        uint64_t dt = bench_now_ns() - t0;
        if (dt >= target_ns || n >= (1ULL << 30)) break;
        uint64_t next = dt ? (uint64_t)((double) n * (double) target_ns * 1.2 / (double) dt) : n * 100;
        if (next <= n) next = n * 2;
        if (next > n * 100) next = n * 100;
        n = next;
    }

    double ns[samples];
    uint64_t cpu_total = 0;
    for (int s = 0; s < samples; ++s) {
        uint64_t c0 = bench_cpu_ns();
        uint64_t t0 = bench_now_ns();
        b->run(ctx, n);
        uint64_t dt = bench_now_ns() - t0;
        cpu_total += bench_cpu_ns() - c0;
        ns[s] = (double) dt / (double) n;
    }

    // NOTE: the counting pass runs on its own, so the logger never slows a timed sample.
    uint64_t heap_before = bench_heap_in_use();
    g_bench_allocs = g_bench_alloc_bytes = 0;
    malloc_logger = bench_malloc_logger;
    b->run(ctx, n);
    malloc_logger = NULL;
    uint64_t heap_after = bench_heap_in_use();

    if (b->teardown) b->teardown(ctx);

    double sum = 0, sq = 0;
    for (int s = 0; s < samples; ++s) sum += ns[s];
    double mean = sum / samples;
    for (int s = 0; s < samples; ++s) sq += (ns[s] - mean) * (ns[s] - mean);
    qsort(ns, samples, sizeof(double), bench_compare_double);

    *r = (struct bench_result) {
        .name = b->name,
        .iters = n,
        .samples = samples,
        .ns_min = ns[0],
        .ns_median = bench_quantile(ns, samples, 0.5),
        .ns_p90 = bench_quantile(ns, samples, 0.9),
        .ns_mean = mean,
        .cv = mean > 0 ? 100.0 * sqrt(sq / samples) / mean : 0,
        .cpu_ns = (double) cpu_total / (double)(n * samples),
        .allocs = (double) g_bench_allocs / (double) n,
        .alloc_bytes = (double) g_bench_alloc_bytes / (double) n,
        .retained_bytes = heap_after > heap_before ? (double)(heap_after - heap_before) / (double) n : 0,
    };
}

static void bench_format_ns(char *buf, size_t size, double ns)
{
    if (ns < 1e3)      snprintf(buf, size, "%.1f ns", ns);
    else if (ns < 1e6) snprintf(buf, size, "%.2f µs", ns / 1e3);
    else               snprintf(buf, size, "%.2f ms", ns / 1e6);
}

static void bench_print_result(struct bench_result *r)
{
    char median[32], p90[32], cpu[32];
    bench_format_ns(median, sizeof(median), r->ns_median);
    bench_format_ns(p90, sizeof(p90), r->ns_p90);
    bench_format_ns(cpu, sizeof(cpu), r->cpu_ns);
    printf("%-30s %12s %12s %6.1f%% %12s %9.1f %11.0f %9.1f\n",
           r->name, median, p90, r->cv, cpu, r->allocs, r->alloc_bytes, r->retained_bytes);
    fflush(stdout);
}

static void bench_write_json(const char *path, struct bench_result *results, int count, uint64_t footprint, uint64_t peak)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "bench: cannot write %s\n", path); return; }

    struct utsname u;
    uname(&u);
    NSOperatingSystemVersion os = [[NSProcessInfo processInfo] operatingSystemVersion];

    fprintf(f, "{\n  \"meta\": {\"kind\": \"micro\", \"git\": \"%s\", \"time\": %ld, \"machine\": \"%s\", \"os\": \"%ld.%ld.%ld\", \"footprint_bytes\": %llu, \"footprint_peak_bytes\": %llu},\n",
            BENCH_GIT, (long) time(NULL), u.machine, (long) os.majorVersion, (long) os.minorVersion, (long) os.patchVersion, footprint, peak);
    fprintf(f, "  \"results\": [\n");
    for (int i = 0; i < count; ++i) {
        struct bench_result *r = &results[i];
        fprintf(f, "    {\"name\": \"%s\", \"ns_op\": %.3f, \"ns_min\": %.3f, \"ns_p90\": %.3f, \"ns_mean\": %.3f, \"cv_pct\": %.2f, \"cpu_ns_op\": %.3f, \"allocs_op\": %.3f, \"bytes_op\": %.1f, \"retained_bytes_op\": %.1f, \"iters\": %llu, \"samples\": %d}%s\n",
                r->name, r->ns_median, r->ns_min, r->ns_p90, r->ns_mean, r->cv, r->cpu_ns, r->allocs, r->alloc_bytes, r->retained_bytes, r->iters, r->samples, i + 1 < count ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

static void bench_usage(void)
{
    printf("usage: bench [--filter text] [--samples k] [--sample-ms ms] [--json path] [--list]\n"
           "  --filter     run only benchmarks whose name contains text (repeatable)\n"
           "  --samples    timed samples per benchmark (default 10)\n"
           "  --sample-ms  target length of one sample in ms (default 20)\n"
           "  --json       also write the results as json, for bench/compare.py\n"
           "  --list       print the benchmark names and exit\n");
}

int main(int argc, char **argv)
{
    const char *filters[32];
    int filter_count = 0;
    int samples = 10;
    double sample_ms = 20;
    const char *json = NULL;
    bool list = false;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--filter") && i + 1 < argc && filter_count < 32) filters[filter_count++] = argv[++i];
        else if (!strcmp(argv[i], "--samples") && i + 1 < argc) samples = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--sample-ms") && i + 1 < argc) sample_ms = atof(argv[++i]);
        else if (!strcmp(argv[i], "--json") && i + 1 < argc) json = argv[++i];
        else if (!strcmp(argv[i], "--list")) list = true;
        else { bench_usage(); return strcmp(argv[i], "--help") ? EXIT_FAILURE : EXIT_SUCCESS; }
    }
    if (samples < 1) samples = 1;

    int total = array_count(g_benches);
    bool selected[total];
    int selected_count = 0;
    for (int i = 0; i < total; ++i) {
        selected[i] = filter_count == 0;
        for (int f = 0; f < filter_count; ++f) if (strstr(g_benches[i].name, filters[f])) selected[i] = true;
        selected_count += selected[i];
    }

    if (list) {
        for (int i = 0; i < total; ++i) if (selected[i]) printf("%s\n", g_benches[i].name);
        return EXIT_SUCCESS;
    }

    tests_init_process();
    g_bench_null = fopen("/dev/null", "w");

    printf("yabai micro benchmarks (%s) — %d of %d, %d samples × ~%.0fms\n\n", BENCH_GIT, selected_count, total, samples, sample_ms);
    printf("%-30s %12s %12s %7s %12s %9s %11s %9s\n", "benchmark", "time/op", "p90/op", "cv", "cpu/op", "allocs/op", "bytes/op", "kept B/op");

    struct bench_result *results = calloc(selected_count ? selected_count : 1, sizeof(struct bench_result));
    int done = 0;
    char *last_group = NULL;
    for (int i = 0; i < total; ++i) {
        if (!selected[i]) continue;
        const char *slash = strchr(g_benches[i].name, '/');
        size_t glen = slash ? (size_t)(slash - g_benches[i].name) : strlen(g_benches[i].name);
        if (!last_group || strncmp(last_group, g_benches[i].name, glen) || last_group[glen] != '\0') {
            free(last_group);
            last_group = strndup(g_benches[i].name, glen);
            printf("\n");
        }
        bench_measure(&g_benches[i], samples, sample_ms, &results[done]);
        bench_print_result(&results[done]);
        ++done;
    }

    uint64_t footprint, peak;
    bench_footprint(&footprint, &peak);
    printf("\nprocess footprint %.1f MB, peak %.1f MB\n", footprint / 1048576.0, peak / 1048576.0);

    if (json) {
        bench_write_json(json, results, done, footprint, peak);
        printf("results written to %s\n", json);
    }

    return EXIT_SUCCESS;
}
