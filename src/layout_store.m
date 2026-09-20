#include "layout_store.h"

static NSString *layout_store_resolve_path(const char *name_or_path, uint64_t sid)
{
    NSString *home = NSHomeDirectory();
    NSString *dir = [home stringByAppendingPathComponent:@".config/yabai/layouts"];

    if (!name_or_path || !*name_or_path) {
        int index = space_manager_mission_control_index(sid);
        if (index > 0) {
            return [dir stringByAppendingPathComponent:[NSString stringWithFormat:@"space_%d.json", index]];
        }
        return [dir stringByAppendingPathComponent:@"default.json"];
    }

    NSString *str = [NSString stringWithUTF8String:name_or_path];
    if ([str hasPrefix:@"~/"]) {
        return [home stringByAppendingPathComponent:[str substringFromIndex:2]];
    } else if ([str hasPrefix:@"/"] || [str hasPrefix:@"."]) {
        return str;
    } else {
        if (![str hasSuffix:@".json"]) {
            str = [str stringByAppendingString:@".json"];
        }
        return [dir stringByAppendingPathComponent:str];
    }
}

static NSString *layout_store_resolve_session_path(const char *name_or_path)
{
    NSString *home = NSHomeDirectory();
    NSString *dir = [home stringByAppendingPathComponent:@".config/yabai/layouts"];

    if (!name_or_path || !*name_or_path) {
        return [dir stringByAppendingPathComponent:@"session.json"];
    }

    NSString *str = [NSString stringWithUTF8String:name_or_path];
    if ([str hasPrefix:@"~/"]) {
        return [home stringByAppendingPathComponent:[str substringFromIndex:2]];
    } else if ([str hasPrefix:@"/"] || [str hasPrefix:@"."]) {
        return str;
    } else {
        if (![str hasSuffix:@".json"]) {
            str = [str stringByAppendingString:@".json"];
        }
        return [dir stringByAppendingPathComponent:str];
    }
}

static bool layout_store_ensure_dir(NSString *path)
{
    NSString *dir = [path stringByDeletingLastPathComponent];
    NSFileManager *fm = [NSFileManager defaultManager];
    BOOL isDir = NO;
    if (![fm fileExistsAtPath:dir isDirectory:&isDir] || !isDir) {
        return [fm createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    }
    return true;
}

static NSDictionary *layout_store_serialize_node(struct window_node *node)
{
    if (!node) return nil;
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];

    if (window_node_is_leaf(node)) {
        [dict setObject:@"leaf" forKey:@"type"];
        NSMutableArray *apps = [NSMutableArray array];
        NSMutableArray *titles = [NSMutableArray array];
        for (int i = 0; i < node->window_count; ++i) {
            struct window *w = window_manager_find_window(&g_window_manager, node->window_list[i]);
            if (w && w->application && w->application->name) {
                [apps addObject:[NSString stringWithUTF8String:w->application->name]];
                char *title = window_title_ts(w);
                [titles addObject:title ? [NSString stringWithUTF8String:title] : @""];
            }
        }
        [dict setObject:apps forKey:@"apps"];
        [dict setObject:titles forKey:@"titles"];
    } else {
        [dict setObject:@"branch" forKey:@"type"];
        [dict setObject:(node->split == SPLIT_X ? @"horizontal" : @"vertical") forKey:@"split"];
        [dict setObject:@(node->ratio) forKey:@"ratio"];
        if (node->left) {
            NSDictionary *left_dict = layout_store_serialize_node(node->left);
            if (left_dict) [dict setObject:left_dict forKey:@"left"];
        }
        if (node->right) {
            NSDictionary *right_dict = layout_store_serialize_node(node->right);
            if (right_dict) [dict setObject:right_dict forKey:@"right"];
        }
    }

    return dict;
}

static void layout_store_collect_tree_apps(NSDictionary *node_dict, NSMutableArray *apps)
{
    if (!node_dict || ![node_dict isKindOfClass:[NSDictionary class]]) return;
    NSString *type = node_dict[@"type"];
    if ([type isEqualToString:@"leaf"]) {
        NSArray *node_apps = node_dict[@"apps"];
        if (node_apps && [node_apps isKindOfClass:[NSArray class]]) {
            for (NSString *a in node_apps) {
                [apps addObject:a];
            }
        }
    } else {
        layout_store_collect_tree_apps(node_dict[@"left"], apps);
        layout_store_collect_tree_apps(node_dict[@"right"], apps);
    }
}

bool layout_store_save(uint64_t sid, const char *name_or_path, char *out_path, size_t out_path_size, char *err_msg, size_t err_size)
{
    @autoreleasepool {
        struct view *view = space_manager_find_view(&g_space_manager, sid);
        if (!view) {
            snprintf(err_msg, err_size, "could not locate view for space %llu", sid);
            return false;
        }

        if (view->layout != VIEW_BSP) {
            snprintf(err_msg, err_size, "can only save layout for BSP spaces (current is %s)", view_type_str[view->layout]);
            return false;
        }

        if (!view->root || (window_node_is_leaf(view->root) && !window_node_is_occupied(view->root))) {
            snprintf(err_msg, err_size, "space has no tiled windows to save");
            return false;
        }

        NSString *path = layout_store_resolve_path(name_or_path, sid);
        if (!layout_store_ensure_dir(path)) {
            snprintf(err_msg, err_size, "could not create directory for '%s'", [path UTF8String]);
            return false;
        }

        NSDictionary *tree_dict = layout_store_serialize_node(view->root);
        if (!tree_dict) {
            snprintf(err_msg, err_size, "failed to serialize window tree");
            return false;
        }

        NSDictionary *root_dict = @{
            @"version": @1,
            @"layout": @"bsp",
            @"top_padding": @(view->top_padding),
            @"bottom_padding": @(view->bottom_padding),
            @"left_padding": @(view->left_padding),
            @"right_padding": @(view->right_padding),
            @"window_gap": @(view->window_gap),
            @"tree": tree_dict
        };

        NSError *err = nil;
        NSData *data = [NSJSONSerialization dataWithJSONObject:root_dict options:NSJSONWritingPrettyPrinted error:&err];
        if (!data || err) {
            snprintf(err_msg, err_size, "JSON serialization error: %s", err ? [[err localizedDescription] UTF8String] : "unknown");
            return false;
        }

        if (![data writeToFile:path atomically:YES]) {
            snprintf(err_msg, err_size, "could not write file to '%s'", [path UTF8String]);
            return false;
        }

        if (out_path && out_path_size > 0) {
            snprintf(out_path, out_path_size, "%s", [path UTF8String]);
        }

        return true;
    }
}

bool layout_store_dump(uint64_t sid, FILE *rsp, char *err_msg, size_t err_size)
{
    @autoreleasepool {
        struct view *view = space_manager_find_view(&g_space_manager, sid);
        if (!view) {
            snprintf(err_msg, err_size, "could not locate view for space %llu", sid);
            return false;
        }

        if (view->layout != VIEW_BSP) {
            snprintf(err_msg, err_size, "can only dump layout for BSP spaces");
            return false;
        }

        if (!view->root || (window_node_is_leaf(view->root) && !window_node_is_occupied(view->root))) {
            snprintf(err_msg, err_size, "space has no tiled windows to dump");
            return false;
        }

        NSDictionary *tree_dict = layout_store_serialize_node(view->root);
        if (!tree_dict) {
            snprintf(err_msg, err_size, "failed to serialize window tree");
            return false;
        }

        NSDictionary *root_dict = @{
            @"version": @1,
            @"layout": @"bsp",
            @"top_padding": @(view->top_padding),
            @"bottom_padding": @(view->bottom_padding),
            @"left_padding": @(view->left_padding),
            @"right_padding": @(view->right_padding),
            @"window_gap": @(view->window_gap),
            @"tree": tree_dict
        };

        NSError *err = nil;
        NSData *data = [NSJSONSerialization dataWithJSONObject:root_dict options:NSJSONWritingPrettyPrinted error:&err];
        if (!data || err) {
            snprintf(err_msg, err_size, "JSON serialization error");
            return false;
        }

        NSString *json_str = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        if (json_str) {
            fprintf(rsp, "%s\n", [json_str UTF8String]);
            [json_str release];
        }

        return true;
    }
}

static struct window_node *layout_store_reconstruct_node(NSDictionary *dict, struct window **avail, bool *used, int avail_count, struct window_node *parent, enum window_node_child child)
{
    if (!dict || ![dict isKindOfClass:[NSDictionary class]]) return NULL;

    NSString *type = dict[@"type"];
    if ([type isEqualToString:@"leaf"]) {
        struct window *matched = NULL;
        NSArray *apps = dict[@"apps"];
        if (apps && [apps isKindOfClass:[NSArray class]]) {
            for (NSString *desired_app in apps) {
                for (int i = 0; i < avail_count; ++i) {
                    if (!used[i] && avail[i]->application && avail[i]->application->name) {
                        if ([desired_app isEqualToString:[NSString stringWithUTF8String:avail[i]->application->name]]) {
                            matched = avail[i];
                            used[i] = true;
                            break;
                        }
                    }
                }
                if (matched) break;
            }
        }

        if (!matched) {
            for (int i = 0; i < avail_count; ++i) {
                if (!used[i]) {
                    matched = avail[i];
                    used[i] = true;
                    break;
                }
            }
        }

        if (!matched) return NULL;

        struct window_node *leaf = malloc(sizeof(struct window_node));
        memset(leaf, 0, sizeof(struct window_node));
        leaf->parent = parent;
        leaf->child = child;
        leaf->ratio = 0.5f;
        leaf->window_list[0] = matched->id;
        leaf->window_order[0] = matched->id;
        leaf->window_count = 1;
        return leaf;
    } else if ([type isEqualToString:@"branch"]) {
        struct window_node *branch = malloc(sizeof(struct window_node));
        memset(branch, 0, sizeof(struct window_node));
        branch->parent = parent;
        branch->child = child;
        branch->split = [dict[@"split"] isEqualToString:@"horizontal"] ? SPLIT_X : SPLIT_Y;
        branch->ratio = [dict[@"ratio"] floatValue];
        if (branch->ratio < 0.05f || branch->ratio > 0.95f) branch->ratio = 0.5f;

        branch->left = layout_store_reconstruct_node(dict[@"left"], avail, used, avail_count, branch, CHILD_FIRST);
        branch->right = layout_store_reconstruct_node(dict[@"right"], avail, used, avail_count, branch, CHILD_SECOND);

        if (!branch->left && !branch->right) {
            free(branch);
            return NULL;
        }
        if (!branch->left) {
            struct window_node *r = branch->right;
            r->parent = parent;
            r->child = child;
            free(branch);
            return r;
        }
        if (!branch->right) {
            struct window_node *l = branch->left;
            l->parent = parent;
            l->child = child;
            free(branch);
            return l;
        }

        return branch;
    }

    return NULL;
}

bool layout_store_restore(uint64_t sid, const char *name_or_path, char *out_path, size_t out_path_size, char *err_msg, size_t err_size)
{
    @autoreleasepool {
        struct view *view = space_manager_find_view(&g_space_manager, sid);
        if (!view) {
            snprintf(err_msg, err_size, "could not locate view for space %llu", sid);
            return false;
        }

        NSString *path = layout_store_resolve_path(name_or_path, sid);
        NSData *data = [NSData dataWithContentsOfFile:path];
        if (!data) {
            snprintf(err_msg, err_size, "layout file not found or unreadable: '%s'", [path UTF8String]);
            return false;
        }

        NSError *err = nil;
        NSDictionary *root_dict = [NSJSONSerialization JSONObjectWithData:data options:0 error:&err];
        if (!root_dict || ![root_dict isKindOfClass:[NSDictionary class]] || err) {
            snprintf(err_msg, err_size, "invalid JSON format in '%s'", [path UTF8String]);
            return false;
        }

        if (root_dict[@"spaces"] && [root_dict[@"spaces"] isKindOfClass:[NSArray class]]) {
            return layout_store_restore_session(name_or_path, out_path, out_path_size, err_msg, err_size);
        }

        NSDictionary *tree_dict = root_dict[@"tree"];
        if (!tree_dict || ![tree_dict isKindOfClass:[NSDictionary class]]) {
            snprintf(err_msg, err_size, "missing 'tree' key in layout file '%s'", [path UTF8String]);
            return false;
        }

        int total_windows = 0;
        uint32_t *raw_list = space_window_list(sid, &total_windows, false);
        struct window *avail[256];
        bool used[256] = {false};
        int avail_count = 0;

        for (int i = 0; i < total_windows && avail_count < 256; ++i) {
            struct window *w = window_manager_find_window(&g_window_manager, raw_list[i]);
            if (w && window_manager_is_window_eligible(w) && !window_check_flag(w, WINDOW_FLOAT)) {
                avail[avail_count++] = w;
            }
        }

        if (avail_count == 0) {
            snprintf(err_msg, err_size, "no eligible managed windows found on space %llu to apply layout", sid);
            return false;
        }

        struct window_node *new_root = layout_store_reconstruct_node(tree_dict, avail, used, avail_count, NULL, CHILD_NONE);
        if (!new_root) {
            snprintf(err_msg, err_size, "failed to reconstruct tree from '%s'", [path UTF8String]);
            return false;
        }

        view->layout = VIEW_BSP;

        if (view->root) {
            insert_feedback_destroy(view->root);
            window_node_destroy(view->root);
        }
        view->root = new_root;

        for (int i = 0; i < avail_count; ++i) {
            if (!used[i]) {
                view_add_window_node_with_insertion_point(view, avail[i], 0);
            }
            window_manager_add_managed_window(&g_window_manager, avail[i], view);
        }

        if (root_dict[@"top_padding"])    view->top_padding    = [root_dict[@"top_padding"] intValue];
        if (root_dict[@"bottom_padding"]) view->bottom_padding = [root_dict[@"bottom_padding"] intValue];
        if (root_dict[@"left_padding"])   view->left_padding   = [root_dict[@"left_padding"] intValue];
        if (root_dict[@"right_padding"])  view->right_padding  = [root_dict[@"right_padding"] intValue];
        if (root_dict[@"window_gap"])     view->window_gap     = [root_dict[@"window_gap"] intValue];

        view_update(view);
        view_flush(view);

        if (out_path && out_path_size > 0) {
            snprintf(out_path, out_path_size, "%s", [path UTF8String]);
        }

        return true;
    }
}

static NSDictionary *layout_store_build_session_dict(void)
{
    int display_count = 0;
    uint32_t *display_list = display_manager_active_display_list(&display_count);
    if (!display_list || display_count == 0) return nil;

    NSMutableArray *spaces_arr = [NSMutableArray array];

    for (int i = 0; i < display_count; ++i) {
        int space_count = 0;
        uint64_t *space_list = display_space_list(display_list[i], &space_count);
        if (!space_list) continue;

        for (int j = 0; j < space_count; ++j) {
            uint64_t sid = space_list[j];
            if (!space_is_user(sid)) continue;

            int mci = space_manager_mission_control_index(sid);
            struct view *view = space_manager_find_view(&g_space_manager, sid);
            if (!view) continue;

            NSMutableDictionary *s_dict = [NSMutableDictionary dictionary];
            [s_dict setObject:@(mci) forKey:@"index"];
            [s_dict setObject:@(i + 1) forKey:@"display"];
            [s_dict setObject:[NSString stringWithUTF8String:view_type_str[view->layout]] forKey:@"layout"];
            [s_dict setObject:@(view->top_padding) forKey:@"top_padding"];
            [s_dict setObject:@(view->bottom_padding) forKey:@"bottom_padding"];
            [s_dict setObject:@(view->left_padding) forKey:@"left_padding"];
            [s_dict setObject:@(view->right_padding) forKey:@"right_padding"];
            [s_dict setObject:@(view->window_gap) forKey:@"window_gap"];

            if (view->layout == VIEW_BSP && view->root && (!window_node_is_leaf(view->root) || window_node_is_occupied(view->root))) {
                NSDictionary *tree_dict = layout_store_serialize_node(view->root);
                if (tree_dict) [s_dict setObject:tree_dict forKey:@"tree"];
            }

            int w_count = 0;
            uint32_t *w_list = space_window_list(sid, &w_count, false);
            NSMutableArray *win_arr = [NSMutableArray array];
            for (int k = 0; k < w_count; ++k) {
                struct window *w = window_manager_find_window(&g_window_manager, w_list[k]);
                if (w && w->application && w->application->name) {
                    NSMutableDictionary *w_dict = [NSMutableDictionary dictionary];
                    [w_dict setObject:[NSString stringWithUTF8String:w->application->name] forKey:@"app"];
                    char *title = window_title_ts(w);
                    [w_dict setObject:(title ? [NSString stringWithUTF8String:title] : @"") forKey:@"title"];
                    [win_arr addObject:w_dict];
                }
            }
            [s_dict setObject:win_arr forKey:@"windows"];

            [spaces_arr addObject:s_dict];
        }
    }

    return @{
        @"version": @1,
        @"type": @"session",
        @"spaces": spaces_arr
    };
}

bool layout_store_save_session(const char *name_or_path, char *out_path, size_t out_path_size, char *err_msg, size_t err_size)
{
    @autoreleasepool {
        NSDictionary *root_dict = layout_store_build_session_dict();
        if (!root_dict) {
            snprintf(err_msg, err_size, "failed to collect space session state");
            return false;
        }

        NSString *path = layout_store_resolve_session_path(name_or_path);
        if (!layout_store_ensure_dir(path)) {
            snprintf(err_msg, err_size, "could not create directory for '%s'", [path UTF8String]);
            return false;
        }

        NSError *err = nil;
        NSData *data = [NSJSONSerialization dataWithJSONObject:root_dict options:NSJSONWritingPrettyPrinted error:&err];
        if (!data || err) {
            snprintf(err_msg, err_size, "JSON serialization error: %s", err ? [[err localizedDescription] UTF8String] : "unknown");
            return false;
        }

        if (![data writeToFile:path atomically:YES]) {
            snprintf(err_msg, err_size, "could not write session file to '%s'", [path UTF8String]);
            return false;
        }

        if (out_path && out_path_size > 0) {
            snprintf(out_path, out_path_size, "%s", [path UTF8String]);
        }

        return true;
    }
}

bool layout_store_dump_session(FILE *rsp, char *err_msg, size_t err_size)
{
    @autoreleasepool {
        NSDictionary *root_dict = layout_store_build_session_dict();
        if (!root_dict) {
            snprintf(err_msg, err_size, "failed to collect space session state");
            return false;
        }

        NSError *err = nil;
        NSData *data = [NSJSONSerialization dataWithJSONObject:root_dict options:NSJSONWritingPrettyPrinted error:&err];
        if (!data || err) {
            snprintf(err_msg, err_size, "JSON serialization error");
            return false;
        }

        NSString *json_str = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        if (json_str) {
            fprintf(rsp, "%s\n", [json_str UTF8String]);
            [json_str release];
        }

        return true;
    }
}

bool layout_store_restore_session(const char *name_or_path, char *out_path, size_t out_path_size, char *err_msg, size_t err_size)
{
    @autoreleasepool {
        NSString *path = layout_store_resolve_session_path(name_or_path);
        NSData *data = [NSData dataWithContentsOfFile:path];
        if (!data) {
            snprintf(err_msg, err_size, "session file not found or unreadable: '%s'", [path UTF8String]);
            return false;
        }

        NSError *err = nil;
        NSDictionary *root_dict = [NSJSONSerialization JSONObjectWithData:data options:0 error:&err];
        if (!root_dict || ![root_dict isKindOfClass:[NSDictionary class]] || err) {
            snprintf(err_msg, err_size, "invalid JSON format in '%s'", [path UTF8String]);
            return false;
        }

        NSArray *spaces_arr = root_dict[@"spaces"];
        if (!spaces_arr || ![spaces_arr isKindOfClass:[NSArray class]]) {
            snprintf(err_msg, err_size, "missing 'spaces' array in session file '%s'", [path UTF8String]);
            return false;
        }

        int display_count = 0;
        uint32_t *display_list = display_manager_active_display_list(&display_count);
        struct window *all_windows[512];
        uint64_t window_current_sid[512];
        bool window_used[512] = {false};
        int all_count = 0;

        for (int i = 0; i < display_count && all_count < 512; ++i) {
            int space_count = 0;
            uint64_t *space_list = display_space_list(display_list[i], &space_count);
            if (!space_list) continue;

            for (int j = 0; j < space_count && all_count < 512; ++j) {
                uint64_t sid = space_list[j];
                if (!space_is_user(sid)) continue;

                int w_count = 0;
                uint32_t *w_list = space_window_list(sid, &w_count, false);
                for (int k = 0; k < w_count && all_count < 512; ++k) {
                    struct window *w = window_manager_find_window(&g_window_manager, w_list[k]);
                    if (w && window_manager_is_window_eligible(w) && !window_check_flag(w, WINDOW_FLOAT)) {
                        all_windows[all_count] = w;
                        window_current_sid[all_count] = sid;
                        all_count++;
                    }
                }
            }
        }

        for (NSDictionary *s_dict in spaces_arr) {
            if (![s_dict isKindOfClass:[NSDictionary class]]) continue;

            int mci = [s_dict[@"index"] intValue];
            if (mci <= 0) continue;

            uint64_t sid = space_manager_mission_control_space(mci);
            if (!sid) continue;

            struct view *view = space_manager_find_view(&g_space_manager, sid);
            if (!view) continue;

            struct window *space_windows[256];
            bool space_used[256] = {false};
            int space_win_count = 0;

            NSMutableArray *expected_apps = [NSMutableArray array];
            if (s_dict[@"windows"] && [s_dict[@"windows"] isKindOfClass:[NSArray class]]) {
                for (NSDictionary *w_dict in s_dict[@"windows"]) {
                    if (w_dict[@"app"]) [expected_apps addObject:w_dict[@"app"]];
                }
            } else if (s_dict[@"tree"] && [s_dict[@"tree"] isKindOfClass:[NSDictionary class]]) {
                layout_store_collect_tree_apps(s_dict[@"tree"], expected_apps);
            }

            for (NSString *exp_app in expected_apps) {
                for (int m = 0; m < all_count && space_win_count < 256; ++m) {
                    if (!window_used[m] && all_windows[m]->application && all_windows[m]->application->name) {
                        if ([exp_app isEqualToString:[NSString stringWithUTF8String:all_windows[m]->application->name]]) {
                            window_used[m] = true;
                            space_windows[space_win_count++] = all_windows[m];
                            break;
                        }
                    }
                }
            }

            for (int m = 0; m < all_count && space_win_count < 256; ++m) {
                if (!window_used[m] && window_current_sid[m] == sid) {
                    window_used[m] = true;
                    space_windows[space_win_count++] = all_windows[m];
                }
            }

            for (int m = 0; m < space_win_count; ++m) {
                struct window *w = space_windows[m];
                struct view *src_view = window_manager_find_managed_window(&g_window_manager, w);
                if (src_view && src_view->sid != sid) {
                    space_manager_untile_window(src_view, w);
                    window_manager_remove_managed_window(&g_window_manager, w->id);
                    space_manager_move_window_to_space(sid, w);
                } else if (!src_view) {
                    space_manager_move_window_to_space(sid, w);
                }
            }

            NSString *layout_str = s_dict[@"layout"];
            enum view_type new_layout = VIEW_BSP;
            if ([layout_str isEqualToString:@"stack"]) new_layout = VIEW_STACK;
            else if ([layout_str isEqualToString:@"float"]) new_layout = VIEW_FLOAT;
            view->layout = new_layout;

            if (s_dict[@"top_padding"])    view->top_padding    = [s_dict[@"top_padding"] intValue];
            if (s_dict[@"bottom_padding"]) view->bottom_padding = [s_dict[@"bottom_padding"] intValue];
            if (s_dict[@"left_padding"])   view->left_padding   = [s_dict[@"left_padding"] intValue];
            if (s_dict[@"right_padding"])  view->right_padding  = [s_dict[@"right_padding"] intValue];
            if (s_dict[@"window_gap"])     view->window_gap     = [s_dict[@"window_gap"] intValue];

            if (view->layout == VIEW_BSP) {
                NSDictionary *tree_dict = s_dict[@"tree"];
                if (tree_dict && [tree_dict isKindOfClass:[NSDictionary class]] && space_win_count > 0) {
                    struct window_node *new_root = layout_store_reconstruct_node(tree_dict, space_windows, space_used, space_win_count, NULL, CHILD_NONE);
                    if (new_root) {
                        if (view->root) {
                            insert_feedback_destroy(view->root);
                            window_node_destroy(view->root);
                        }
                        view->root = new_root;

                        for (int k = 0; k < space_win_count; ++k) {
                            if (!space_used[k]) {
                                view_add_window_node_with_insertion_point(view, space_windows[k], 0);
                            }
                            window_manager_add_managed_window(&g_window_manager, space_windows[k], view);
                        }
                    }
                } else {
                    if (view->root) {
                        insert_feedback_destroy(view->root);
                        window_node_destroy(view->root);
                    }
                    view->root = malloc(sizeof(struct window_node));
                    memset(view->root, 0, sizeof(struct window_node));
                    for (int k = 0; k < space_win_count; ++k) {
                        view_add_window_node_with_insertion_point(view, space_windows[k], 0);
                        window_manager_add_managed_window(&g_window_manager, space_windows[k], view);
                    }
                }
                view_update(view);
                view_flush(view);
            } else if (view->layout == VIEW_STACK) {
                if (view->root) {
                    insert_feedback_destroy(view->root);
                    window_node_destroy(view->root);
                }
                view->root = malloc(sizeof(struct window_node));
                memset(view->root, 0, sizeof(struct window_node));
                for (int k = 0; k < space_win_count; ++k) {
                    view_add_window_node(view, space_windows[k]);
                    window_manager_adjust_layer(space_windows[k], LAYER_BELOW);
                    window_manager_add_managed_window(&g_window_manager, space_windows[k], view);
                }
                view_update(view);
                view_flush(view);
            } else if (view->layout == VIEW_FLOAT) {
                view_clear(view);
                view_update(view);
                view_flush(view);
            }
        }

        if (out_path && out_path_size > 0) {
            snprintf(out_path, out_path_size, "%s", [path UTF8String]);
        }

        return true;
    }
}
