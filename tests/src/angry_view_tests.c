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
    ANGRY_ASSERT(count == 2 && list[0] == 401 && list[1] == 402, "view_find_window_list failed to populate window list");
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
    ANGRY_ASSERT(strstr(buf, "\"id\":501") && strstr(buf, "\"type\":\"bsp\""), "view_serialize failed to emit complete JSON");
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
    ANGRY_ASSERT(showed && destroyed, "insert_feedback failed to create and destroy feedback window");
}
