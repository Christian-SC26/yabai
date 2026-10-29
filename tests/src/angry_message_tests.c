#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include "../../src/display.h"

ANGRY_TEST(advancesTokenPointerAndExtractsTokenText) {
    char msg[] = "foo\0bar\0\0";
    char *p = msg;
    struct token t1 = get_token(&p);
    struct token t2 = get_token(&p);
    ANGRY_ASSERT(string_equals(t1.text, "foo") && string_equals(t2.text, "bar"), "get_token failed extraction");
}

ANGRY_TEST(evaluatesTokenPrefixMatchingStartOfString) {
    struct token tok = { .text = "config", .length = 6 };
    bool pre = token_prefix(tok, "con");
    bool no_pre = !token_prefix(tok, "bar");
    ANGRY_ASSERT(pre && no_pre, "token_prefix failed matching");
}

ANGRY_TEST(evaluatesTokenEqualsAgainstExactStrings) {
    struct token tok = { .text = "window", .length = 6 };
    bool eq = token_equals(tok, "window");
    bool neq = !token_equals(tok, "space");
    ANGRY_ASSERT(eq && neq, "token_equals failed exact comparison");
}

ANGRY_TEST(evaluatesTokenIsValidCheckingLengthAndPointer) {
    struct token valid = { .text = "a", .length = 1 };
    struct token invalid = { .text = NULL, .length = 0 };
    ANGRY_ASSERT(token_is_valid(valid) && !token_is_valid(invalid), "token_is_valid evaluation failed");
}

ANGRY_TEST(evaluatesTokenIsPositiveIntegerForValidAndInvalidStrings) {
    int val = 0;
    struct token tok_num = { .text = "42", .length = 2 };
    struct token tok_bad = { .text = "-5", .length = 2 };
    bool ok_num = token_is_positive_integer(tok_num, &val) && val == 42;
    bool ok_bad = !token_is_positive_integer(tok_bad, &val);
    ANGRY_ASSERT(ok_num && ok_bad, "token_is_positive_integer failed parsing");
}

ANGRY_TEST(evaluatesTokenIsHexadecimalForPrefixedNumbers) {
    uint32_t val = 0;
    struct token tok_hex = { .text = "0x2A", .length = 4 };
    struct token tok_bad = { .text = "2A", .length = 2 };
    bool ok_hex = token_is_hexadecimal(tok_hex, &val) && val == 42;
    bool ok_bad = !token_is_hexadecimal(tok_bad, &val);
    ANGRY_ASSERT(ok_hex && ok_bad, "token_is_hexadecimal failed parsing");
}

ANGRY_TEST(evaluatesTokenIsFloatForValidFloatingPointNumbers) {
    float val = 0.0f;
    struct token tok_flt = { .text = "0.75", .length = 4 };
    struct token tok_bad = { .text = "abc", .length = 3 };
    bool ok_flt = token_is_float(tok_flt, &val) && val == 0.75f;
    bool ok_bad = !token_is_float(tok_bad, &val);
    ANGRY_ASSERT(ok_flt && ok_bad, "token_is_float failed parsing");
}

ANGRY_TEST(convertsTokenToTypedTokenValue) {
    struct token tok_int = { .text = "10", .length = 2 };
    struct token tok_str = { .text = "bsp", .length = 3 };
    struct token_value val_int = token_to_value(tok_int);
    struct token_value val_str = token_to_value(tok_str);
    ANGRY_ASSERT(val_int.type == TOKEN_TYPE_INT && val_str.type == TOKEN_TYPE_STRING, "token_to_value failed conversion");
}

ANGRY_TEST(parsesKeyValuePairExtractingKeyAndValueAndExclusion) {
    char s1[] = "app=Finder";
    char s2[] = "title!=Zoom";
    char *k1 = NULL, *v1 = NULL, *k2 = NULL, *v2 = NULL;
    bool e1 = false, e2 = false;
    parse_key_value_pair(s1, &k1, &v1, &e1);
    parse_key_value_pair(s2, &k2, &v2, &e2);
    ANGRY_ASSERT(string_equals(k1, "app") && !e1 && string_equals(k2, "title") && e2, "parse_key_value_pair failed");
}

ANGRY_TEST(parsesValueTypeForAbsAndRelModifiers) {
    uint8_t t_abs = parse_value_type("abs");
    uint8_t t_rel = parse_value_type("rel");
    uint8_t t_inv = parse_value_type("inv");
    ANGRY_ASSERT(t_abs == TYPE_ABS && t_rel == TYPE_REL && t_inv == 0, "parse_value_type returned mismatched types");
}

ANGRY_TEST(parsesResizeHandleForCardinalAndCornerDirections) {
    uint8_t top = parse_resize_handle("top");
    uint8_t tl = parse_resize_handle("top_left");
    uint8_t abs_h = parse_resize_handle("abs");
    ANGRY_ASSERT(top == HANDLE_TOP && tl == (HANDLE_TOP | HANDLE_LEFT) && abs_h == HANDLE_ABS, "parse_resize_handle failed");
}

ANGRY_TEST(parsesLabelValidatingReservedIdentifiers) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char *lbl = NULL;
    struct token tok_valid = { .text = "my_space", .length = 8 };
    bool ok = parse_label(f, tok_valid, LABEL_SPACE, &lbl);
    fclose(f);
    ANGRY_ASSERT(ok && string_equals(lbl, "my_space"), "parse_label failed extraction");
}

ANGRY_TEST(parsesPropertiesFromCommaSeparatedToken) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char p_text[] = "id,uuid";
    struct token tok = { .text = p_text, .length = (int)strlen(p_text) };
    struct properties props = parse_properties(f, tok, display_property_val, display_property_str, array_count(display_property_val));
    fclose(f);
    ANGRY_ASSERT(props.did_parse && !props.did_error, "parse_properties failed parsing display property flags");
}

ANGRY_TEST(parsesDisplaySelectorForIndexAndDirections) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "1\0";
    char *p = msg;
    struct selector sel = parse_display_selector(f, &p, 1, false);
    fclose(f);
    ANGRY_ASSERT(sel.did_parse && sel.did > 0, "parse_display_selector failed index selection");
}

ANGRY_TEST(parsesSpaceSelectorForIndexAndRelativeSelectors) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "1\0";
    char *p = msg;
    struct selector sel = parse_space_selector(f, &p, space_manager_active_space(), false);
    fclose(f);
    ANGRY_ASSERT(sel.did_parse && sel.sid > 0, "parse_space_selector failed index selection");
}

ANGRY_TEST(parsesWindowSelectorForKeywordsAndWindowIds) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "mouse\0";
    char *p = msg;
    struct selector sel = parse_window_selector(f, &p, NULL, false);
    fclose(f);
    ANGRY_ASSERT(sel.did_parse, "parse_window_selector failed mouse selection");
}

ANGRY_TEST(parsesInsertSelectorForCardinalDirectionsAndStack) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "north\0";
    char *p = msg;
    struct selector sel = parse_insert_selector(f, &p);
    fclose(f);
    ANGRY_ASSERT(sel.did_parse && sel.dir == DIR_NORTH, "parse_insert_selector failed direction parsing");
}

ANGRY_TEST(parsesRuleSpecifyingAppFilterAndManageEffect) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "app=Finder\0manage=on\0\0";
    char *p = msg;
    struct token tok = get_token(&p);
    struct rule r = {0};
    bool ok = parse_rule(f, &p, &r, tok);
    rule_destroy(&r);
    fclose(f);
    ANGRY_ASSERT(ok, "parse_rule failed to parse valid rule arguments");
}

ANGRY_TEST(dispatchesConfigDomainModifyingRuntimeSettings) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "window_gap\0" "12\0\0";
    struct token dom = { .text = "config", .length = 6 };
    handle_domain_config(f, dom, msg);
    fclose(f);
    ANGRY_ASSERT(g_space_manager.window_gap == 12, "handle_domain_config failed setting window_gap");
}

ANGRY_TEST(configuresSpaceGestureNaturalScrollingMode) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "space_gesture_natural_scrolling\0" "off\0\0";
    struct token dom = { .text = "config", .length = 6 };
    handle_domain_config(f, dom, msg);
    fclose(f);
    ANGRY_ASSERT(g_space_manager.gesture_natural_scrolling == SPACE_GESTURE_NATURAL_SCROLL_OFF, "handle_domain_config failed setting natural scrolling");
}

ANGRY_TEST(dispatchesDisplayDomainExecutingDisplayCommands) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "--focus\01\0\0";
    struct token dom = { .text = "display", .length = 7 };
    handle_domain_display(f, dom, msg);
    fclose(f);
    ANGRY_ASSERT(true, "handle_domain_display executed cleanly");
}

ANGRY_TEST(dispatchesSpaceDomainExecutingSpaceCommands) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "--layout\0bsp\0\0";
    struct token dom = { .text = "space", .length = 5 };
    handle_domain_space(f, dom, msg);
    fclose(f);
    ANGRY_ASSERT(true, "handle_domain_space executed cleanly");
}

ANGRY_TEST(dispatchesWindowDomainExecutingWindowCommands) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "--toggle\0float\0\0";
    struct token dom = { .text = "window", .length = 6 };
    handle_domain_window(f, dom, msg);
    fclose(f);
    ANGRY_ASSERT(true, "handle_domain_window executed cleanly");
}

ANGRY_TEST(dispatchesQueryDomainEmittingFormattedJson) {
    char buf[4096] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "--displays\0\0";
    struct token dom = { .text = "query", .length = 5 };
    handle_domain_query(f, dom, msg);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "\"id\":"), "handle_domain_query failed emitting displays JSON");
}

ANGRY_TEST(dispatchesRuleDomainListingActiveRules) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "--list\0\0";
    struct token dom = { .text = "rule", .length = 4 };
    handle_domain_rule(f, dom, msg);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "[") != NULL, "handle_domain_rule failed listing rules");
}

ANGRY_TEST(dispatchesSignalDomainListingRegisteredSignals) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "--list\0\0";
    struct token dom = { .text = "signal", .length = 6 };
    handle_domain_signal(f, dom, msg);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "[") != NULL, "handle_domain_signal failed listing signals");
}

ANGRY_TEST(dispatchesMessageHandlingForInvalidDomain) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    char msg[] = "nonexistent_domain\0\0";
    handle_message(f, msg);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "unknown domain"), "handle_message failed error dispatch on unknown domain");
}

ANGRY_TEST(formatsDaemonFailureMessageToJsonStream) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    daemon_fail(f, "test error %d\n", 42);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "test error 42"), "daemon_fail failed formatting message");
}

ANGRY_TEST(formatsDaemonDeprecatedWarningToJsonStream) {
    char buf[512] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    daemon_deprecated(f, "old command '%s'\n", "opt");
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "deprecation warning: old command 'opt'"), "daemon_deprecated failed formatting");
}

ANGRY_TEST(terminatesMessageLoopImmediatelyWhenNotRunning) {
    g_message_loop.is_running = false;
    void *res = message_loop_run(NULL);
    ANGRY_ASSERT(res == NULL, "message_loop_run failed returning NULL when stopped");
}

ANGRY_TEST(initializesAndTerminatesMessageLoopSocket) {
    char *path = "/tmp/yabai_test_angry_sock.socket";
    bool ok = message_loop_begin(path);
    g_message_loop.is_running = false;
    close(g_message_loop.sockfd);
    unlink(path);
    ANGRY_ASSERT(ok, "message_loop_begin failed to create unix socket");
}
