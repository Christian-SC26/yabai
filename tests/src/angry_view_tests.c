#include <ApplicationServices/ApplicationServices.h>
#include "../../src/view.h"

ANGRY_TEST(identifiesLeftChildInBinaryTreeHierarchy) {
    struct window_node parent = {0};
    struct window_node child = { .parent = &parent };
    parent.left = &child;
    bool left = window_node_is_left_child(&child);
    ANGRY_ASSERT(left, "window_node_is_left_child returned false for left child");
}

ANGRY_TEST(equalizesBspNodeRatioAcrossSpecifiedSplitAxis) {
    struct window_node node = { .split = SPLIT_Y, .ratio = 0.8f };
    g_space_manager.split_ratio = 0.5f;
    window_node_equalize(&node, SPLIT_Y);
    ANGRY_ASSERT(node.ratio == 0.5f, "window_node_equalize failed to reset node ratio");
}

ANGRY_TEST(locatesIndexOfWindowInWindowNodeList) {
    struct window_node node = { .window_count = 3, .window_list = { 101, 202, 303 } };
    int idx = window_node_index_of_window(&node, 202);
    ANGRY_ASSERT(idx == 1, "window_node_index_of_window returned incorrect index");
}

ANGRY_TEST(clearsZoomNodeRecursivelyFromBspSubtree) {
    struct window_node zoom_n = {0};
    struct window_node node = { .zoom = &zoom_n };
    window_node_clear_zoom(&node);
    ANGRY_ASSERT(node.zoom == NULL, "window_node_clear_zoom failed to clear zoom pointer");
}

ANGRY_TEST(locatesLastLeafNodeInDeepBspTree) {
    struct window_node left = {0}, right = {0};
    struct window_node root = { .left = &left, .right = &right };
    struct window_node *last = window_node_find_last_leaf(&root);
    ANGRY_ASSERT(last == &right, "window_node_find_last_leaf failed to return rightmost leaf");
}

ANGRY_TEST(locatesPreviousLeafNodeInBspTree) {
    struct window_node left = {0}, right = {0};
    struct window_node root = { .left = &left, .right = &right };
    left.parent = &root; right.parent = &root;
    struct window_node *prev = window_node_find_prev_leaf(&right);
    ANGRY_ASSERT(prev == &left, "window_node_find_prev_leaf failed to return left sibling");
}

ANGRY_TEST(returnsNullWhenSearchingDirectionalNodeOnInvalidSpace) {
    struct view v = { .sid = 0 };
    struct window_node src = {0};
    struct window_node *target = view_find_window_node_in_direction(&v, &src, DIR_EAST);
    ANGRY_ASSERT(target == NULL, "view_find_window_node_in_direction should return NULL for invalid space");
}

ANGRY_TEST(allocatesAndPopulatesWindowListFromLeafNodes) {
    struct window_node node = { .window_count = 2, .window_list = { 401, 402 } };
    struct view v = { .root = &node };
    int count = 0;
    uint32_t *list = view_find_window_list(&v, &count);
    ANGRY_ASSERT_ALL("view_find_window_list failed to populate window list",
                     count == 2,
                     list[0] == 401,
                     list[1] == 402);
}

ANGRY_TEST(evaluatesViewIsInvalidWhenValidFlagIsUnset) {
    struct view v = { .flags = 0 };
    bool inv = view_is_invalid(&v);
    ANGRY_ASSERT(inv == true, "view_is_invalid must return true when VIEW_IS_VALID flag is missing");
}

ANGRY_TEST(marksViewDirtyWhenFlushingInvisibleSpace) {
    struct view v = { .sid = 0 };
    view_flush(&v);
    bool dirty = view_is_dirty(&v);
    ANGRY_ASSERT(dirty, "view_flush failed to mark invisible space view as dirty");
}

ANGRY_TEST(serializesBspViewPropertiesToJsonStream) {
    struct view v = { .sid = 501, .layout = VIEW_BSP, .uuid = CFSTR("view-test-uuid") };
    char buf[2048] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    view_serialize(f, &v, 0);
    fclose(f);
    ANGRY_ASSERT_ALL("view_serialize failed to emit complete JSON",
                     strstr(buf, "\"id\":501"),
                     strstr(buf, "\"type\":\"bsp\""));
}

ANGRY_TEST(clearsViewRootTreeAndResetsNodeDimensions) {
    struct window_node root = {0};
    struct view v = { .sid = space_manager_active_space(), .root = &root };
    view_clear(&v);
    ANGRY_ASSERT(v.root->window_count == 0, "view_clear failed to reset root node");
}

ANGRY_TEST(displaysAndDestroysInsertFeedbackWindow) {
    struct window_node node = { .area = {{ 100, 100 }, { 300, 300 }}, .window_order = { 1 }, .insert_dir = STACK };
    insert_feedback_show(&node);
    bool showed = node.feedback_window.id != 0;
    insert_feedback_destroy(&node);
    bool destroyed = node.feedback_window.id == 0;
    ANGRY_ASSERT_ALL("insert_feedback failed to create and destroy feedback window",
                     showed,
                     destroyed);
}

ANGRY_TEST(movesInsertFeedbackEntryToNewFrontWindowOfStack) {
    struct window_node node = { .area = {{ 100, 100 }, { 300, 300 }}, .window_count = 1, .window_list = { 7101 }, .window_order = { 7101 }, .insert_dir = STACK };
    insert_feedback_show(&node);
    struct window w = { .id = 7102 };
    view_stack_window_node(&node, &w);
    uint32_t old_front = 7101, new_front = 7102;
    bool moved = table_find(&g_window_manager.insert_feedback, &new_front) == &node;
    bool cleared = table_find(&g_window_manager.insert_feedback, &old_front) == NULL;
    insert_feedback_destroy(&node);
    bool removed = table_find(&g_window_manager.insert_feedback, &new_front) == NULL;
    ANGRY_ASSERT_ALL("insert_feedback entry did not follow the new front window of the stack",
                     moved,
                     cleared,
                     removed);
}

ANGRY_TEST(handsInsertFeedbackEntryToParentWhenSiblingIsRemoved) {
    struct view *view = view_create(998);
    view->layout = VIEW_BSP;
    struct window w1 = { .id = 7201 };
    struct window w2 = { .id = 7202 };
    view_add_window_node(view, &w1);
    view_add_window_node(view, &w2);
    struct window_node *node = view_find_window_node(view, w2.id);
    node->insert_dir = STACK;
    insert_feedback_show(node);
    view_remove_window_node(view, &w1);
    bool handed = table_find(&g_window_manager.insert_feedback, &w2.id) == view->root;
    insert_feedback_destroy(view->root);
    bool removed = table_find(&g_window_manager.insert_feedback, &w2.id) == NULL;
    view_destroy(view);
    ANGRY_ASSERT_ALL("insert_feedback entry still pointed at the freed sibling node",
                     handed,
                     removed);
}

//
// NOTE: a node keeps its windows in arrays of NODE_MAX_WINDOW_COUNT entries.
// A space with a stack layout is a single node, so the window after the last
// one that fits used to be written past the end of those arrays.
//
ANGRY_TEST(leavesFullStackLayoutUntouchedWhenOneMoreWindowArrives)
{
    struct view *view = view_create(999);
    view->layout = VIEW_STACK;
    struct window windows[NODE_MAX_WINDOW_COUNT + 1];
    for (int i = 0; i <= NODE_MAX_WINDOW_COUNT; ++i) windows[i] = (struct window) { .id = 5000 + i };
    for (int i = 0; i < NODE_MAX_WINDOW_COUNT; ++i) view_add_window_node(view, &windows[i]);
    struct window *extra = &windows[NODE_MAX_WINDOW_COUNT];
    struct window_node *node = view_add_window_node(view, extra);
    bool kept = ANGRY_ALL((node == NULL),
                          (view->root->window_count == NODE_MAX_WINDOW_COUNT),
                          (!window_node_contains_window(view->root, extra->id)),
                          (view->root->window_order[0] == windows[NODE_MAX_WINDOW_COUNT - 1].id));
    view_destroy(view);
    ANGRY_ASSERT(kept, "a window past the capacity of a stack layout must not be added to its node");
}

ANGRY_TEST(tilesWindowBesideFullStackInsteadOfStackingOntoIt)
{
    struct view *view = view_create(999);
    view->layout = VIEW_BSP;
    struct window windows[NODE_MAX_WINDOW_COUNT];
    for (int i = 0; i < NODE_MAX_WINDOW_COUNT; ++i) windows[i] = (struct window) { .id = 6000 + i };
    view_add_window_node(view, &windows[0]);
    for (int i = 1; i < NODE_MAX_WINDOW_COUNT; ++i) view_stack_window_node(view->root, &windows[i]);
    view->root->insert_dir = STACK;
    view->insertion_point = windows[0].id;
    struct window extra = { .id = 6100 };
    view_add_window_node(view, &extra);
    struct window_node *stack = view_find_window_node(view, windows[0].id);
    struct window_node *tile = view_find_window_node(view, extra.id);
    bool beside = ANGRY_ALL((stack != NULL),
                            (tile != NULL),
                            (stack != tile),
                            (stack->window_count == NODE_MAX_WINDOW_COUNT),
                            (tile->window_count == 1));
    view_destroy(view);
    ANGRY_ASSERT(beside, "a window meant for a full stack must get a tile of its own next to it");
}

ANGRY_TEST(leavesWindowOutOfLayoutWhenTilingOnFullStackSpace)
{
    uint64_t sid = 0xBE5C0001;
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    view->layout = VIEW_STACK;
    struct application app = { .pid = 4400, .name = "stack_app" };
    struct window windows[NODE_MAX_WINDOW_COUNT + 1];
    for (int i = 0; i <= NODE_MAX_WINDOW_COUNT; ++i) windows[i] = (struct window) { .id = 7000 + i, .application = &app };
    for (int i = 0; i < NODE_MAX_WINDOW_COUNT; ++i) space_manager_tile_window_on_space(&g_space_manager, &windows[i], sid);
    struct window *extra = &windows[NODE_MAX_WINDOW_COUNT];
    struct view *result = space_manager_tile_window_on_space(&g_space_manager, extra, sid);
    bool left_out = ANGRY_ALL((result == view),
                              (view_find_window_node(view, extra->id) == NULL),
                              (view->root->window_count == NODE_MAX_WINDOW_COUNT));
    for (int i = NODE_MAX_WINDOW_COUNT - 1; i >= 0; --i) space_manager_untile_window(view, &windows[i]);
    ANGRY_ASSERT(left_out, "tiling a window on a full stack space must leave it out of the layout instead of failing");
}

ANGRY_TEST(keepsWholeWindowListOfFullStackOutOfNextTemporaryAllocation) {
    struct view *view = view_create(997);
    view->layout = VIEW_STACK;
    struct window windows[NODE_MAX_WINDOW_COUNT];
    for (int i = 0; i < NODE_MAX_WINDOW_COUNT; ++i) {
        windows[i] = (struct window) { .id = 7300 + i };
        view_add_window_node(view, &windows[i]);
    }
    int count = 0;
    uint32_t *list = view_find_window_list(view, &count);
    uint32_t *next = ts_alloc_list(uint32_t, NODE_MAX_WINDOW_COUNT);
    memset(next, 0xff, sizeof(uint32_t) * NODE_MAX_WINDOW_COUNT);
    bool intact = count == NODE_MAX_WINDOW_COUNT;
    for (int i = 0; intact && i < count; ++i) intact = list[i] != 0xffffffff;
    view_destroy(view);
    ANGRY_ASSERT(intact, "the next temporary allocation overwrote the window list of a full stack");
}
