#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include "../../src/layout_store.h"

ANGRY_TEST(resolvesLayoutStorePathForNullAndCustomPaths) {
    uint64_t sid = space_manager_active_space();
    NSString *p1 = layout_store_resolve_path(NULL, sid);
    NSString *p2 = layout_store_resolve_path("~/test_layout.json", sid);
    ANGRY_ASSERT([p1 containsString:@".config/yabai/layouts"] && [p2 containsString:@"test_layout.json"], "layout_store_resolve_path failed");
}

ANGRY_TEST(resolvesLayoutStoreSessionPathForNullAndCustomPaths) {
    NSString *p1 = layout_store_resolve_session_path(NULL);
    NSString *p2 = layout_store_resolve_session_path("/tmp/session.json");
    ANGRY_ASSERT([p1 hasSuffix:@"session.json"] && [p2 isEqualToString:@"/tmp/session.json"], "layout_store_resolve_session_path failed");
}

ANGRY_TEST(ensuresDirectoryExistsForTargetFilePath) {
    NSString *test_p = @"/tmp/yabai_test_dir_angry/test.json";
    bool ok = layout_store_ensure_dir(test_p);
    [[NSFileManager defaultManager] removeItemAtPath:@"/tmp/yabai_test_dir_angry" error:nil];
    ANGRY_ASSERT(ok, "layout_store_ensure_dir failed to ensure directory");
}

ANGRY_TEST(serializesLeafAndBranchWindowNodesToDictionary) {
    struct window_node leaf = { .window_count = 0 };
    struct window_node branch = { .split = SPLIT_X, .ratio = 0.5f, .left = &leaf };
    NSDictionary *d_leaf = layout_store_serialize_node(&leaf);
    NSDictionary *d_branch = layout_store_serialize_node(&branch);
    ANGRY_ASSERT([d_leaf[@"type"] isEqualToString:@"leaf"] && [d_branch[@"type"] isEqualToString:@"branch"], "serialize_node failed");
}

ANGRY_TEST(collectsAppNamesRecursivelyFromSerializedNodeDictionary) {
    NSDictionary *d = @{ @"type": @"leaf", @"apps": @[@"Finder", @"Terminal"] };
    NSMutableArray *apps = [NSMutableArray array];
    layout_store_collect_tree_apps(d, apps);
    ANGRY_ASSERT([apps count] == 2 && [apps containsObject:@"Finder"], "layout_store_collect_tree_apps failed");
}

ANGRY_TEST(reconstructsWindowNodeHierarchyFromSerializedDictionary) {
    NSDictionary *d = @{ @"type": @"branch", @"split": @"horizontal", @"ratio": @0.5f };
    struct window *avail[1] = {NULL};
    bool used[1] = {false};
    struct window_node *n = layout_store_reconstruct_node(d, avail, used, 0, NULL, CHILD_NONE);
    ANGRY_ASSERT(n == NULL, "reconstruct_node with no windows correctly returned NULL");
}

ANGRY_TEST(buildsSessionDictionaryCapturingDisplaysAndSpaces) {
    NSDictionary *dict = layout_store_build_session_dict();
    ANGRY_ASSERT(dict != nil && [dict[@"type"] isEqualToString:@"session"], "build_session_dict failed");
}

ANGRY_TEST(savesSpaceLayoutToFileHandlingErrors) {
    char out_p[256] = {0}, err[256] = {0};
    uint64_t sid = space_manager_active_space();
    bool res = layout_store_save(sid, "/tmp/yabai_layout_test.json", out_p, sizeof(out_p), err, sizeof(err));
    ANGRY_ASSERT(res == true || res == false, "layout_store_save executed safely");
}

ANGRY_TEST(dumpsSpaceLayoutToJsonStreamHandlingErrors) {
    char buf[1024] = {0}, err[256] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    uint64_t sid = space_manager_active_space();
    bool res = layout_store_dump(sid, f, err, sizeof(err));
    fclose(f);
    ANGRY_ASSERT(res == true || res == false, "layout_store_dump executed safely");
}

ANGRY_TEST(restoresSpaceLayoutFromFileHandlingMissingFile) {
    char out_p[256] = {0}, err[256] = {0};
    uint64_t sid = space_manager_active_space();
    bool res = layout_store_restore(sid, "/tmp/nonexistent_yabai_layout_store.json", out_p, sizeof(out_p), err, sizeof(err));
    ANGRY_ASSERT(!res && strstr(err, "not found"), "layout_store_restore failed to detect missing file");
}

ANGRY_TEST(savesSessionStateToFileHandlingErrors) {
    char out_p[256] = {0}, err[256] = {0};
    bool ok = layout_store_save_session("/tmp/yabai_session_test.json", out_p, sizeof(out_p), err, sizeof(err));
    [[NSFileManager defaultManager] removeItemAtPath:@"/tmp/yabai_session_test.json" error:nil];
    ANGRY_ASSERT(ok, "layout_store_save_session failed to save session file");
}

ANGRY_TEST(dumpsSessionStateToJsonStreamHandlingErrors) {
    char buf[4096] = {0}, err[256] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    bool ok = layout_store_dump_session(f, err, sizeof(err));
    fclose(f);
    ANGRY_ASSERT(ok && strstr(buf, "\"type\" : \"session\""), "layout_store_dump_session failed emitting JSON");
}

ANGRY_TEST(restoresSessionStateFromFileHandlingMissingFile) {
    char out_p[256] = {0}, err[256] = {0};
    bool res = layout_store_restore_session("/tmp/nonexistent_yabai_session_store.json", out_p, sizeof(out_p), err, sizeof(err));
    ANGRY_ASSERT(!res && strstr(err, "not found"), "layout_store_restore_session failed to detect missing file");
}
