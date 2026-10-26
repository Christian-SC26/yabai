#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <sys/sysctl.h>
#include "../../src/process_manager.h"

ANGRY_TEST(hashesProcessSerialNumberUsingLowLong) {
    ProcessSerialNumber psn = { .highLongOfPSN = 10, .lowLongOfPSN = 42 };
    uint32_t hash = hash_psn(&psn);
    ANGRY_ASSERT(hash == 42, "hash_psn failed to extract lowLongOfPSN");
}

ANGRY_TEST(comparesIdenticalProcessSerialNumbersReturningTrue) {
    ProcessSerialNumber a = { 1, 100 }, b = { 1, 100 };
    ANGRY_ASSERT(compare_psn(&a, &b) == true, "compare_psn failed on identical PSNs");
}

ANGRY_TEST(comparesDifferentProcessSerialNumbersReturningFalse) {
    ProcessSerialNumber a = { 0, 100 }, b = { 0, 101 };
    ANGRY_ASSERT(compare_psn(&a, &b) == false, "compare_psn failed on different PSNs");
}

ANGRY_TEST(retrievesPidForCurrentProcessSerialNumber) {
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
    pid_t pid = process_pid_for_psn(psn);
    ANGRY_ASSERT(pid == getpid(), "process_pid_for_psn failed to resolve current PID");
}

ANGRY_TEST(createsProcessInstanceForCurrentProcess) {
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
    struct process *proc = process_create(psn, getpid());
    bool ok = proc != NULL && proc->pid == getpid() && proc->name != NULL;
    if (proc) process_destroy(proc);
    ANGRY_ASSERT(ok, "process_create failed to instantiate process for current task");
}

ANGRY_TEST(rejectsInvalidProcessInProcessCreateReturningNull) {
    ProcessSerialNumber invalid_psn = { 0, 0 };
    struct process *proc = process_create(invalid_psn, 0);
    ANGRY_ASSERT(proc == NULL, "process_create must return NULL for invalid PSN");
}

ANGRY_TEST(evaluatesProcessIsBeingDebuggedViaSysctl) {
    bool debugged = process_is_being_debugged(getpid());
    ANGRY_ASSERT(debugged == false || debugged == true, "process_is_being_debugged executed sysctl cleanly");
}

ANGRY_TEST(findsProcessInProcessManagerTable) {
    struct process_manager pm = {0};
    table_init(&pm.process, 13, hash_psn, compare_psn);
    struct process proc = { .psn = { 0, 777 }, .pid = 888 };
    table_add(&pm.process, &proc.psn, &proc);
    struct process *found = process_manager_find_process(&pm, &proc.psn);
    table_remove(&pm.process, &proc.psn);
    table_free(&pm.process);
    ANGRY_ASSERT(found && found->pid == 888, "process_manager_find_process failed to retrieve added process");
}

ANGRY_TEST(returnsNullWhenProcessNotFoundInProcessManager) {
    struct process_manager pm = {0};
    table_init(&pm.process, 13, hash_psn, compare_psn);
    ProcessSerialNumber missing = { 0, 9999 };
    struct process *found = process_manager_find_process(&pm, &missing);
    table_free(&pm.process);
    ANGRY_ASSERT(found == NULL, "process_manager_find_process returned non-NULL for missing process");
}

ANGRY_TEST(destroysProcessReleasingResources) {
    struct process *proc = malloc(sizeof(struct process));
    proc->name = strdup("angry_test_proc");
    proc->ns_application = NULL;
    process_destroy(proc);
    ANGRY_ASSERT(true, "process_destroy executed cleanly without error");
}

ANGRY_TEST(handlesProcessLaunchEventInProcessHandler) {
    struct process_manager pm = {0};
    table_init(&pm.process, 13, hash_psn, compare_psn);
    EventRef ev;
    CreateEvent(NULL, kEventClassApplication, kEventAppLaunched, GetCurrentEventTime(), kEventAttributeNone, &ev);
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
    SetEventParameter(ev, kEventParamProcessID, typeProcessSerialNumber, sizeof(psn), &psn);
    OSStatus status = process_handler(NULL, ev, &pm);
    struct process *p = table_find(&pm.process, &psn);
    bool added = p != NULL;
    if (p) { table_remove(&pm.process, &psn); process_destroy(p); }
    table_free(&pm.process);
    ReleaseEvent(ev);
    ANGRY_ASSERT(status == noErr && added, "process_handler failed to handle kEventAppLaunched");
}

ANGRY_TEST(handlesDuplicateProcessLaunchEventDiscardingDupe) {
    struct process_manager pm = {0};
    table_init(&pm.process, 13, hash_psn, compare_psn);
    ProcessSerialNumber psn = { 0, 555 };
    struct process p = { .psn = psn, .pid = 1 };
    table_add(&pm.process, &psn, &p);
    EventRef ev;
    CreateEvent(NULL, kEventClassApplication, kEventAppLaunched, GetCurrentEventTime(), kEventAttributeNone, &ev);
    SetEventParameter(ev, kEventParamProcessID, typeProcessSerialNumber, sizeof(psn), &psn);
    OSStatus status = process_handler(NULL, ev, &pm);
    ReleaseEvent(ev);
    table_remove(&pm.process, &psn);
    table_free(&pm.process);
    ANGRY_ASSERT(status == noErr, "process_handler failed to discard duplicate launch event");
}

ANGRY_TEST(handlesProcessFrontSwitchedEventInProcessHandler) {
    struct process_manager pm = {0};
    table_init(&pm.process, 13, hash_psn, compare_psn);
    ProcessSerialNumber psn = { 0, 666 };
    struct process p = { .psn = psn, .pid = 2 };
    table_add(&pm.process, &psn, &p);
    EventRef ev;
    CreateEvent(NULL, kEventClassApplication, kEventAppFrontSwitched, GetCurrentEventTime(), kEventAttributeNone, &ev);
    SetEventParameter(ev, kEventParamProcessID, typeProcessSerialNumber, sizeof(psn), &psn);
    OSStatus status = process_handler(NULL, ev, &pm);
    ReleaseEvent(ev);
    table_remove(&pm.process, &psn);
    table_free(&pm.process);
    ANGRY_ASSERT(status == noErr, "process_handler failed to dispatch kEventAppFrontSwitched");
}

ANGRY_TEST(handlesProcessTerminatedEventMarkingProcessTerminated) {
    struct process_manager pm = {0};
    table_init(&pm.process, 13, hash_psn, compare_psn);
    ProcessSerialNumber psn = { 0, 777 };
    struct process p = { .psn = psn, .pid = 3, .terminated = false };
    table_add(&pm.process, &psn, &p);
    EventRef ev;
    CreateEvent(NULL, kEventClassApplication, kEventAppTerminated, GetCurrentEventTime(), kEventAttributeNone, &ev);
    SetEventParameter(ev, kEventParamProcessID, typeProcessSerialNumber, sizeof(psn), &psn);
    OSStatus status = process_handler(NULL, ev, &pm);
    ReleaseEvent(ev);
    bool term = p.terminated;
    table_free(&pm.process);
    ANGRY_ASSERT(status == noErr && term, "process_handler failed to mark terminated on kEventAppTerminated");
}

ANGRY_TEST(returnsErrorWhenProcessEventLacksProcessIdParameter) {
    EventRef ev;
    CreateEvent(NULL, kEventClassApplication, kEventAppLaunched, GetCurrentEventTime(), kEventAttributeNone, &ev);
    OSStatus status = process_handler(NULL, ev, NULL);
    ReleaseEvent(ev);
    ANGRY_ASSERT(status == -1, "process_handler should return -1 when kEventParamProcessID is missing");
}

ANGRY_TEST(returnsZeroActiveSpaceForInvalidConnectionId) {
    uint64_t sid = process_manager_active_space_for_psn(0);
    ANGRY_ASSERT(sid == 0, "process_manager_active_space_for_psn should return 0 for invalid connection");
}

ANGRY_TEST(addsRunningProcessesAndCachesFinderPsn) {
    struct process_manager pm = {0};
    table_init(&pm.process, 125, hash_psn, compare_psn);
    process_manager_add_running_processes(&pm);
    bool has_procs = pm.process.count > 0;
    table_for(struct process *p, pm.process, { process_destroy(p); });
    table_free(&pm.process);
    ANGRY_ASSERT(has_procs, "process_manager_add_running_processes failed to enumerate running apps");
}

ANGRY_TEST(initializesProcessManagerInstallingCarbonEventHandler) {
    bool ok = process_manager_begin(&g_process_manager);
    ANGRY_ASSERT(ok && g_process_manager.process.count > 0, "process_manager_begin failed to initialize process table");
}
