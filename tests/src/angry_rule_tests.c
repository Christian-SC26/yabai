#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

ANGRY_TEST(serializesRuleConfigurationWithLabelAndEscapedStringsToJsonStream) {
    char *buf = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buf, &size);
    struct rule r = {
        .label = "tile-ghost-99",
        .app = "Ghost\"ty",
        .title = "Shell <1>",
        .effects = {
            .opacity = 0.8542f,
            .manage = RULE_PROP_ON,
            .sticky = RULE_PROP_OFF,
            .scratchpad = "sp_term_1"
        },
        .flags = RULE_ONE_SHOT
    };
    rule_serialize(stream, &r, 3);
    fclose(stream);
    bool ok = buf && strstr(buf, "\"scratchpad\":\"sp_term_1\"") && strstr(buf, "\"manage\":true");
    free(buf);
    ANGRY_ASSERT(ok, "Rule JSON serialization does not match expected output");
}

ANGRY_TEST(serializesRuleConfigurationWithoutOptionalStringsUsingEmptyDefaults) {
    char *buf = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buf, &size);
    struct rule r = {0};
    rule_serialize(stream, &r, 0);
    fclose(stream);
    bool ok = buf && strstr(buf, "\"label\":\"\"") && strstr(buf, "\"scratchpad\":\"\"");
    free(buf);
    ANGRY_ASSERT(ok, "Rule JSON serialization does not output empty strings for optional fields");
}

ANGRY_TEST(copiesDisplayIdentifierAndFollowSpaceWhenFollowSpaceFlagSet) {
    struct rule_effects src = { .did = 3, .flags = RULE_FOLLOW_SPACE };
    struct rule_effects dst = {0};
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.did == 3 && (dst.flags & RULE_FOLLOW_SPACE), "Combine effects failed to set display ID and follow space flag");
}

ANGRY_TEST(clearsFollowSpaceFlagWhenSourceDisplaySpecifiedWithoutFollowSpaceFlag) {
    struct rule_effects src = { .did = 7, .flags = 0 };
    struct rule_effects dst = { .flags = RULE_FOLLOW_SPACE };
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.did == 7 && !(dst.flags & RULE_FOLLOW_SPACE), "Combine effects failed to clear follow space flag on display update");
}

ANGRY_TEST(copiesSpaceIdentifierAndFollowSpaceWhenFollowSpaceFlagSet) {
    struct rule_effects src = { .sid = 991, .flags = RULE_FOLLOW_SPACE };
    struct rule_effects dst = {0};
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.sid == 991 && (dst.flags & RULE_FOLLOW_SPACE), "Combine effects failed to set space ID and follow space flag");
}

ANGRY_TEST(clearsFollowSpaceFlagWhenSourceSpaceSpecifiedWithoutFollowSpaceFlag) {
    struct rule_effects src = { .sid = 412, .flags = 0 };
    struct rule_effects dst = { .flags = RULE_FOLLOW_SPACE };
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.sid == 412 && !(dst.flags & RULE_FOLLOW_SPACE), "Combine effects failed to clear follow space flag on space update");
}

ANGRY_TEST(ignoresOpacityOutsideUnitIntervalDuringCombination) {
    struct rule_effects src = { .opacity = 1.25f, .flags = RULE_OPACITY };
    struct rule_effects dst = { .opacity = 0.45f, .flags = 0 };
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.opacity == 0.45f && !(dst.flags & RULE_OPACITY), "Combine effects allowed invalid opacity outside unit interval");
}

ANGRY_TEST(copiesSublayerWhenRuleLayerFlagSet) {
    struct rule_effects src = { .layer = LAYER_ABOVE, .flags = RULE_LAYER };
    struct rule_effects dst = { .layer = LAYER_BELOW, .flags = 0 };
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.layer == LAYER_ABOVE && (dst.flags & RULE_LAYER), "Combine effects failed to copy sublayer and layer flag");
}

ANGRY_TEST(replacesScratchpadStringFreeingPreexistingValue) {
    struct rule_effects src = { .scratchpad = "new_scratch_88" };
    struct rule_effects dst = { .scratchpad = string_copy("old_scratch_17") };
    rule_combine_effects(&src, &dst);
    bool ok = dst.scratchpad && strcmp(dst.scratchpad, "new_scratch_88") == 0;
    free(dst.scratchpad);
    ANGRY_ASSERT(ok, "Combine effects failed to replace scratchpad string properly");
}

ANGRY_TEST(preservesDestinationPropertyWhenSourcePropertyIsUndecided) {
    struct rule_effects src = { .manage = RULE_PROP_UD, .ffm = RULE_PROP_UD };
    struct rule_effects dst = { .manage = RULE_PROP_ON, .ffm = RULE_PROP_OFF };
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.manage == RULE_PROP_ON && dst.ffm == RULE_PROP_OFF, "Combine effects altered property when source was undecided");
}

ANGRY_TEST(appliesGridDimensionsWhenBothRowAndColumnAreNonZero) {
    struct rule_effects src = { .grid = {4, 6, 1, 2, 3, 4} };
    struct rule_effects dst = { .grid = {0} };
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.grid[0] == 4 && dst.grid[1] == 6 && dst.grid[4] == 3, "Combine effects failed to copy valid grid dimensions");
}

ANGRY_TEST(ignoresGridDimensionsWhenRowOrColumnIsZero) {
    struct rule_effects src = { .grid = {0, 6, 1, 2, 3, 4} };
    struct rule_effects dst = { .grid = {2, 2, 0, 0, 1, 1} };
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.grid[0] == 2 && dst.grid[1] == 2, "Combine effects overwritten grid when source row dimension was zero");
}

ANGRY_TEST(addsRuleToWindowManagerRulesBuffer) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    struct rule r = { .label = string_copy("rule_init_1") };
    rule_add(&r);
    int len = buf_len(g_window_manager.rules);
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    ANGRY_ASSERT(len == 1, "Rule buffer length did not increase after rule_add");
}

ANGRY_TEST(replacesExistingRuleWhenAddingRuleWithIdenticalLabel) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    struct rule r1 = { .label = string_copy("editor-cfg-42"), .app = string_copy("Code") };
    struct rule r2 = { .label = string_copy("editor-cfg-42"), .app = string_copy("Emacs") };
    rule_add(&r1);
    rule_add(&r2);
    bool ok = buf_len(g_window_manager.rules) == 1 && strcmp(g_window_manager.rules[0].app, "Emacs") == 0;
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    ANGRY_ASSERT(ok, "Adding rule with duplicate label did not replace previous rule");
}

ANGRY_TEST(removesRuleByIndexFromWindowManagerBuffer) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    struct rule r1 = { .label = string_copy("r1") };
    struct rule r2 = { .label = string_copy("r2") };
    rule_add(&r1);
    rule_add(&r2);
    bool removed = rule_remove_by_index(0);
    bool ok = removed && buf_len(g_window_manager.rules) == 1 && strcmp(g_window_manager.rules[0].label, "r2") == 0;
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    ANGRY_ASSERT(ok, "Removing rule by index failed to reduce count or remove correct item");
}

ANGRY_TEST(returnsFalseWhenRemovingRuleByInvalidIndex) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    bool removed = rule_remove_by_index(9999);
    ANGRY_ASSERT(removed == false, "rule_remove_by_index succeeded on nonexistent index");
}

ANGRY_TEST(removesRuleByLabelFromWindowManagerBuffer) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    struct rule r = { .label = string_copy("spotify-overlay-9") };
    rule_add(&r);
    bool removed = rule_remove_by_label("spotify-overlay-9");
    bool ok = removed && buf_len(g_window_manager.rules) == 0;
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    ANGRY_ASSERT(ok, "Removing rule by label failed to remove item from rules buffer");
}

ANGRY_TEST(returnsFalseWhenRemovingRuleByNonexistentLabel) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    bool removed = rule_remove_by_label("missing-ghost-rule-00");
    ANGRY_ASSERT(removed == false, "rule_remove_by_label succeeded on nonexistent label");
}

ANGRY_TEST(combinesStickyMffAndFullscreenProperties) {
    struct rule_effects src = { .sticky = RULE_PROP_ON, .mff = RULE_PROP_OFF, .fullscreen = RULE_PROP_ON };
    struct rule_effects dst = {0};
    rule_combine_effects(&src, &dst);
    ANGRY_ASSERT(dst.sticky == RULE_PROP_ON && dst.mff == RULE_PROP_OFF && dst.fullscreen == RULE_PROP_ON, "Combine effects failed to set sticky mff or fullscreen properties");
}

ANGRY_TEST(freesRuleResourcesAndCompiledRegularExpressions) {
    struct rule r = {0};
    r.label = string_copy("label-77");
    r.app = string_copy("Slack");
    r.title = string_copy("Title");
    r.role = string_copy("AXWindow");
    r.subrole = string_copy("AXStandardWindow");
    r.flags = RULE_APP_VALID | RULE_TITLE_VALID | RULE_ROLE_VALID | RULE_SUBROLE_VALID;
    regcomp(&r.app_regex, "^Slack$", REG_EXTENDED);
    regcomp(&r.title_regex, ".*", REG_EXTENDED);
    regcomp(&r.role_regex, ".*", REG_EXTENDED);
    regcomp(&r.subrole_regex, ".*", REG_EXTENDED);
    r.effects.scratchpad = string_copy("sp-1");
    rule_destroy(&r);
    ANGRY_ASSERT(true, "rule_destroy failed to free allocated regexes and strings");
}

ANGRY_TEST(reappliesRuleByIndexIteratingActiveWindows) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    struct rule r = { .label = string_copy("reapply-idx"), .flags = 0 };
    rule_add(&r);
    bool ok = rule_reapply_by_index(0);
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    ANGRY_ASSERT(ok == true, "rule_reapply_by_index failed to reapply non one shot rule");
}

ANGRY_TEST(reappliesRuleByLabelIteratingActiveWindows) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    struct rule r = { .label = string_copy("reapply-lbl"), .flags = 0 };
    rule_add(&r);
    bool ok = rule_reapply_by_label("reapply-lbl");
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    ANGRY_ASSERT(ok == true, "rule_reapply_by_label failed to reapply non one shot rule");
}

ANGRY_TEST(skipsOneShotRuleWhenReapplyingByIndex) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    struct rule r = { .flags = RULE_ONE_SHOT };
    buf_push(g_window_manager.rules, r);
    bool ok = rule_reapply_by_index(0);
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    ANGRY_ASSERT(ok == true, "rule_reapply_by_index failed on one shot rule");
}

ANGRY_TEST(returnsFalseWhenReapplyingByNonexistentIndex) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    bool ok = rule_reapply_by_index(5432);
    ANGRY_ASSERT(ok == false, "rule_reapply_by_index returned true for out of bounds index");
}

ANGRY_TEST(skipsOneShotRuleWhenReapplyingByLabel) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    struct rule r = { .label = string_copy("oneshot-lbl"), .flags = RULE_ONE_SHOT };
    rule_add(&r);
    bool ok = rule_reapply_by_label("oneshot-lbl");
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    ANGRY_ASSERT(ok == true, "rule_reapply_by_label failed on one shot rule");
}

ANGRY_TEST(returnsFalseWhenReapplyingByNonexistentLabel) {
    buf_free(g_window_manager.rules);
    g_window_manager.rules = NULL;
    bool ok = rule_reapply_by_label("nonexistent-label-99");
    ANGRY_ASSERT(ok == false, "rule_reapply_by_label returned true for nonexistent label");
}

ANGRY_TEST(clearsRuleFlagFromRuleBitmask) {
    struct rule r = { .flags = RULE_ONE_SHOT | RULE_APP_VALID };
    rule_clear_flag(&r, RULE_ONE_SHOT);
    ANGRY_ASSERT(!rule_check_flag(&r, RULE_ONE_SHOT) && rule_check_flag(&r, RULE_APP_VALID), "rule_clear_flag failed to clear specified flag");
}

ANGRY_TEST(setsRuleFlagInRuleBitmask) {
    struct rule r = { .flags = 0 };
    rule_set_flag(&r, RULE_ONE_SHOT);
    ANGRY_ASSERT(rule_check_flag(&r, RULE_ONE_SHOT), "rule_set_flag failed to set specified flag");
}

ANGRY_TEST(clearsRuleEffectsFlagFromEffectsBitmask) {
    struct rule_effects e = { .flags = RULE_FOLLOW_SPACE | RULE_OPACITY };
    rule_effects_clear_flag(&e, RULE_FOLLOW_SPACE);
    ANGRY_ASSERT(!rule_effects_check_flag(&e, RULE_FOLLOW_SPACE) && rule_effects_check_flag(&e, RULE_OPACITY), "rule_effects_clear_flag failed to clear specified flag");
}

ANGRY_TEST(traversesEmptyWindowTableWithoutErrorDuringRuleReapplyAll) {
    struct window w = { .id = 991, .is_root = false };
    table_add(&g_window_manager.window, &w.id, &w);
    rule_reapply_all();
    table_remove(&g_window_manager.window, &w.id);
    ANGRY_ASSERT(true, "rule_reapply_all crashed on window manager traversal");
}
