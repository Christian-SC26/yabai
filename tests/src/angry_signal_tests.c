#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "../../src/event_signal.h"

ANGRY_TEST(resolvesSignalTypeFromStringAccurately) {
    enum signal_type t = signal_type_from_string("window_created");
    ANGRY_ASSERT(t == SIGNAL_WINDOW_CREATED, "signal_type_from_string returned wrong enum for window_created");
}

ANGRY_TEST(resolvesSignalTypeForDockDidChangePref) {
    enum signal_type t = signal_type_from_string("dock_did_change_pref");
    ANGRY_ASSERT(t == SIGNAL_DOCK_DID_CHANGE_PREF, "signal_type_from_string returned wrong enum for dock_did_change_pref");
}

ANGRY_TEST(returnsUnknownSignalTypeForInvalidString) {
    enum signal_type t = signal_type_from_string("invalid_nonexistent_signal");
    ANGRY_ASSERT(t == SIGNAL_TYPE_UNKNOWN, "signal_type_from_string should return unknown for invalid string");
}

ANGRY_TEST(addsSignalToSignalEventBuffer) {
    buf_free(g_signal_event[SIGNAL_WINDOW_CREATED]);
    g_signal_event[SIGNAL_WINDOW_CREATED] = NULL;
    struct signal s = { .command = string_copy("echo hello") };
    event_signal_add(SIGNAL_WINDOW_CREATED, &s);
    int len = buf_len(g_signal_event[SIGNAL_WINDOW_CREATED]);
    buf_free(g_signal_event[SIGNAL_WINDOW_CREATED]);
    g_signal_event[SIGNAL_WINDOW_CREATED] = NULL;
    ANGRY_ASSERT(len == 1, "event_signal_add failed to insert signal into buffer");
}

ANGRY_TEST(replacesSignalWhenAddingWithDuplicateLabel) {
    buf_free(g_signal_event[SIGNAL_WINDOW_CREATED]);
    g_signal_event[SIGNAL_WINDOW_CREATED] = NULL;
    struct signal s1 = { .label = string_copy("sig-dup"), .command = string_copy("cmd1") };
    struct signal s2 = { .label = string_copy("sig-dup"), .command = string_copy("cmd2") };
    event_signal_add(SIGNAL_WINDOW_CREATED, &s1);
    event_signal_add(SIGNAL_WINDOW_CREATED, &s2);
    bool ok = buf_len(g_signal_event[SIGNAL_WINDOW_CREATED]) == 1 && strcmp(g_signal_event[SIGNAL_WINDOW_CREATED][0].command, "cmd2") == 0;
    buf_free(g_signal_event[SIGNAL_WINDOW_CREATED]);
    g_signal_event[SIGNAL_WINDOW_CREATED] = NULL;
    ANGRY_ASSERT(ok, "event_signal_add failed to replace duplicate label signal");
}

ANGRY_TEST(removesSignalByIndexAcrossGlobalSignalTypes) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo remove") };
    event_signal_add(SIGNAL_APPLICATION_LAUNCHED, &s);
    bool ok = event_signal_remove_by_index(0);
    ANGRY_ASSERT(ok && buf_len(g_signal_event[SIGNAL_APPLICATION_LAUNCHED]) == 0, "event_signal_remove_by_index failed to remove signal");
}

ANGRY_TEST(returnsFalseWhenRemovingSignalByInvalidIndex) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    bool ok = event_signal_remove_by_index(8888);
    ANGRY_ASSERT(ok == false, "event_signal_remove_by_index succeeded on out of bounds index");
}

ANGRY_TEST(removesSignalByLabelFromSignalEventBuffer) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .label = string_copy("sig-lbl-target"), .command = string_copy("cmd") };
    event_signal_add(SIGNAL_SPACE_CHANGED, &s);
    bool ok = event_signal_remove("sig-lbl-target");
    ANGRY_ASSERT(ok && buf_len(g_signal_event[SIGNAL_SPACE_CHANGED]) == 0, "event_signal_remove failed to remove by label");
}

ANGRY_TEST(returnsFalseWhenRemovingSignalByNonexistentLabel) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    bool ok = event_signal_remove("missing-lbl-404");
    ANGRY_ASSERT(ok == false, "event_signal_remove succeeded on nonexistent label");
}

ANGRY_TEST(destroysSignalFreeingRegexesAndStrings) {
    struct signal s = {0};
    s.app_regex_valid = true;
    regcomp(&s.app_regex, "^Slack$", REG_EXTENDED);
    s.title_regex_valid = true;
    regcomp(&s.title_regex, ".*", REG_EXTENDED);
    s.label = string_copy("lbl");
    s.app = string_copy("Slack");
    s.title = string_copy("General");
    s.command = string_copy("notify");
    event_signal_destroy(&s);
    ANGRY_ASSERT(true, "event_signal_destroy failed to clean up signal");
}

ANGRY_TEST(serializesSignalListToJsonStream) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .label = string_copy("json_sig"), .command = string_copy("notify.sh") };
    event_signal_add(SIGNAL_WINDOW_CREATED, &s);
    char *buf = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buf, &size);
    event_signal_list(stream);
    fclose(stream);
    bool ok = buf && strstr(buf, "\"event\":\"window_created\"") && strstr(buf, "\"label\":\"json_sig\"");
    free(buf);
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_list JSON serialization mismatch");
}

ANGRY_TEST(serializesEmptySignalListAsEmptyJsonArray) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    char *buf = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buf, &size);
    event_signal_list(stream);
    fclose(stream);
    bool ok = buf && strcmp(buf, "[]\n") == 0;
    free(buf);
    ANGRY_ASSERT(ok, "event_signal_list on empty table did not produce []\\n");
}

ANGRY_TEST(filtersSignalWhenApplicationRegexDoesNotMatch) {
    struct signal s = { .app_regex_valid = true };
    regcomp(&s.app_regex, "^Safari$", REG_EXTENDED);
    struct event_signal es = { .type = SIGNAL_APPLICATION_LAUNCHED, .app = "Ghostty" };
    bool filtered = event_signal_filter(&es, &s);
    regfree(&s.app_regex);
    ANGRY_ASSERT(filtered == true, "event_signal_filter did not filter out mismatched application regex");
}

ANGRY_TEST(permitsSignalWhenApplicationRegexMatches) {
    struct signal s = { .app_regex_valid = true };
    regcomp(&s.app_regex, "^Ghostty$", REG_EXTENDED);
    struct event_signal es = { .type = SIGNAL_APPLICATION_LAUNCHED, .app = "Ghostty" };
    bool filtered = event_signal_filter(&es, &s);
    regfree(&s.app_regex);
    ANGRY_ASSERT(filtered == false, "event_signal_filter erroneously filtered matching application regex");
}

ANGRY_TEST(filtersSignalWhenActivePropertyMismatches) {
    struct signal s = { .active = SIGNAL_PROP_YES };
    struct event_signal es = { .type = SIGNAL_WINDOW_DESTROYED, .active = 0 };
    bool filtered = event_signal_filter(&es, &s);
    ANGRY_ASSERT(filtered == true, "event_signal_filter failed to filter out mismatched active property");
}

ANGRY_TEST(pushesApplicationSignalIntoStorageWhenSubscriberExists) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo launch") };
    event_signal_add(SIGNAL_APPLICATION_LAUNCHED, &s);
    struct application app = { .pid = 4321, .name = "TestLauncherApp" };
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_APPLICATION_LAUNCHED, &app);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push did not write application signal to storage");
}

ANGRY_TEST(skipsSignalPushWhenNoSubscribersExist) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_DISPLAY_REMOVED, (void *)1);
    ANGRY_ASSERT(g_signal_storage.used == 0, "event_signal_push wrote to storage without subscribers");
}

ANGRY_TEST(pushesDisplaySignalIntoStorageWhenSubscriberExists) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo disp") };
    event_signal_add(SIGNAL_DISPLAY_ADDED, &s);
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_DISPLAY_ADDED, (void *)(intptr_t)4);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record display signal");
}

ANGRY_TEST(pushesSpaceSignalIntoStorageWhenSubscriberExists) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo spc") };
    event_signal_add(SIGNAL_SPACE_CHANGED, &s);
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_SPACE_CHANGED, (void *)(intptr_t)7);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record space signal");
}

ANGRY_TEST(pushesMissionControlSignalIntoStorageWhenSubscriberExists) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo mc") };
    event_signal_add(SIGNAL_MISSION_CONTROL_ENTER, &s);
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_MISSION_CONTROL_ENTER, NULL);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record mission control signal");
}

ANGRY_TEST(pushesSystemWokeSignalIntoStorageWhenSubscriberExists) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo wake") };
    event_signal_add(SIGNAL_SYSTEM_WOKE, &s);
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_SYSTEM_WOKE, NULL);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record system woke signal");
}

ANGRY_TEST(pushesApplicationTerminatedSignalIntoStorage) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo term") };
    event_signal_add(SIGNAL_APPLICATION_TERMINATED, &s);
    struct application app = { .pid = 7712, .name = "TermApp" };
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_APPLICATION_TERMINATED, &app);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record application terminated signal");
}

ANGRY_TEST(pushesApplicationFrontSwitchedSignalIntoStorage) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo front") };
    event_signal_add(SIGNAL_APPLICATION_FRONT_SWITCHED, &s);
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_APPLICATION_FRONT_SWITCHED, NULL);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record front switched signal");
}

ANGRY_TEST(pushesApplicationHiddenSignalIntoStorage) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo hide") };
    event_signal_add(SIGNAL_APPLICATION_HIDDEN, &s);
    struct application app = { .pid = 8812, .name = "HideApp" };
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_APPLICATION_HIDDEN, &app);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record hidden application signal");
}

ANGRY_TEST(pushesWindowDestroyedSignalIntoStorage) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo win_destroy") };
    event_signal_add(SIGNAL_WINDOW_DESTROYED, &s);
    struct application app = { .pid = 991, .name = "DestroyApp" };
    struct window win = { .id = 404, .application = &app };
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_WINDOW_DESTROYED, &win);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record window destroyed signal");
}

ANGRY_TEST(pushesSpaceCreatedAndDestroyedSignalsIntoStorage) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s1 = { .command = string_copy("echo spc_create") };
    struct signal s2 = { .command = string_copy("echo spc_destroy") };
    event_signal_add(SIGNAL_SPACE_CREATED, &s1);
    event_signal_add(SIGNAL_SPACE_DESTROYED, &s2);
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_SPACE_CREATED, (void *)(intptr_t)12);
    event_signal_push(SIGNAL_SPACE_DESTROYED, (void *)(intptr_t)14);
    bool ok = g_signal_storage.used >= 2 * sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record space created or destroyed signals");
}

ANGRY_TEST(pushesDisplayRemovedAndChangedSignalsIntoStorage) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s1 = { .command = string_copy("echo disp_rem") };
    struct signal s2 = { .command = string_copy("echo disp_chg") };
    event_signal_add(SIGNAL_DISPLAY_REMOVED, &s1);
    event_signal_add(SIGNAL_DISPLAY_CHANGED, &s2);
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_DISPLAY_REMOVED, (void *)(intptr_t)5);
    event_signal_push(SIGNAL_DISPLAY_CHANGED, NULL);
    bool ok = g_signal_storage.used >= 2 * sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record display removed or changed signals");
}

ANGRY_TEST(pushesMissionControlExitSignalIntoStorage) {
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    struct signal s = { .command = string_copy("echo mc_exit") };
    event_signal_add(SIGNAL_MISSION_CONTROL_EXIT, &s);
    g_signal_storage.used = 0;
    event_signal_push(SIGNAL_MISSION_CONTROL_EXIT, (void *)(intptr_t)MISSION_CONTROL_MODE_SHOW_DESKTOP);
    bool ok = g_signal_storage.used >= sizeof(struct event_signal);
    g_signal_storage.used = 0;
    for (int i = 0; i < SIGNAL_TYPE_COUNT; ++i) {
        buf_free(g_signal_event[i]);
        g_signal_event[i] = NULL;
    }
    ANGRY_ASSERT(ok, "event_signal_push failed to record mission control exit signal");
}

ANGRY_TEST(flushesSignalStorageImmediatelyWhenEmpty) {
    g_signal_storage.used = 0;
    event_signal_flush();
    ANGRY_ASSERT(g_signal_storage.used == 0, "event_signal_flush should do nothing when storage is empty");
}
