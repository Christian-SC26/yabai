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
