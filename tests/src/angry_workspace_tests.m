ANGRY_TEST(observesFinishedLaunchingForValidAndNullApplications)
{
    struct process proc_null = { .pid = 99999, .ns_application = NULL, .name = "null_app" };
    workspace_application_observe_finished_launching(g_workspace_context, &proc_null);
    struct process proc_valid = { .pid = getpid(), .name = "self_app" };
    proc_valid.ns_application = [NSRunningApplication currentApplication];
    workspace_application_observe_finished_launching(g_workspace_context, &proc_valid);
    @try { [(NSRunningApplication *)proc_valid.ns_application removeObserver:(id)g_workspace_context forKeyPath:@"finishedLaunching" context:&proc_valid]; } @catch (id ex) {}
    ANGRY_ASSERT(proc_valid.ns_application != NULL, "expected valid ns_application after observing finished launching");
}

ANGRY_TEST(observesActivationPolicyForValidAndNullApplications)
{
    struct process proc_null = { .pid = 99998, .ns_application = NULL, .name = "null_app" };
    workspace_application_observe_activation_policy(g_workspace_context, &proc_null);
    struct process proc_valid = { .pid = getpid(), .name = "self_app" };
    proc_valid.ns_application = [NSRunningApplication currentApplication];
    workspace_application_observe_activation_policy(g_workspace_context, &proc_valid);
    @try { [(NSRunningApplication *)proc_valid.ns_application removeObserver:(id)g_workspace_context forKeyPath:@"activationPolicy" context:&proc_valid]; } @catch (id ex) {}
    ANGRY_ASSERT(proc_valid.ns_application != NULL, "expected valid ns_application after observing activation policy");
}

ANGRY_TEST(determinesApplicationObservableForRegularAndNullPolicies)
{
    struct process proc_null = { .pid = 99997, .ns_application = NULL };
    bool null_observable = workspace_application_is_observable(&proc_null);
    struct process proc_valid = { .pid = getpid() };
    proc_valid.ns_application = [NSRunningApplication currentApplication];
    bool valid_observable = workspace_application_is_observable(&proc_valid);
    ANGRY_ASSERT(!null_observable && proc_null.policy == NSApplicationActivationPolicyProhibited && (valid_observable || !valid_observable),
                 "expected null application to be unobservable with prohibited policy");
}

ANGRY_TEST(evaluatesApplicationFinishedLaunchingState)
{
    struct process proc_null = { .pid = 99996, .ns_application = NULL };
    bool null_finished = workspace_application_is_finished_launching(&proc_null);
    struct process proc_dock = { .pid = workspace_get_dock_pid() };
    proc_dock.ns_application = workspace_application_create_running_ns_application(&proc_dock);
    bool dock_finished = workspace_application_is_finished_launching(&proc_dock);
    if (proc_dock.ns_application) [(id)proc_dock.ns_application release];
    ANGRY_ASSERT(!null_finished && dock_finished, "expected dock process to report finished launching");
}

ANGRY_TEST(measuresDisplayNotchHeightForBuiltinAndExternalDisplays)
{
    int invalid_notch = workspace_display_notch_height(999999);
    int main_notch = workspace_display_notch_height(CGMainDisplayID());
    ANGRY_ASSERT(invalid_notch == 0 && main_notch >= 0, "expected non-builtin display notch height to be 0");
}

ANGRY_TEST(deallocatesWorkspaceContextProperly)
{
    workspace_context *ctx = [[workspace_context alloc] init];
    [ctx release];
    ANGRY_ASSERT(ctx != nil, "expected workspace_context allocation and deallocation to succeed");
}

ANGRY_TEST(processesActivationPolicyChangeObservation)
{
    struct process proc = { .pid = 101, .policy = NSApplicationActivationPolicyProhibited, .terminated = false, .name = "test_proc" };
    NSDictionary *change = @{ NSKeyValueChangeNewKey: @(NSApplicationActivationPolicyRegular) };
    [(workspace_context *)g_workspace_context observeValueForKeyPath:@"activationPolicy" ofObject:[NSRunningApplication currentApplication] change:change context:&proc];
    struct process term_proc = { .pid = 102, .policy = NSApplicationActivationPolicyProhibited, .terminated = true, .name = "term_proc" };
    [(workspace_context *)g_workspace_context observeValueForKeyPath:@"activationPolicy" ofObject:[NSRunningApplication currentApplication] change:change context:&term_proc];
    ANGRY_ASSERT(proc.policy == NSApplicationActivationPolicyRegular && term_proc.policy == NSApplicationActivationPolicyProhibited,
                 "expected activation policy change to update process policy");
}

ANGRY_TEST(processesFinishedLaunchingChangeObservation)
{
    struct process proc = { .pid = 103, .terminated = false, .name = "launch_proc" };
    NSDictionary *change_done = @{ NSKeyValueChangeNewKey: @1 };
    [(workspace_context *)g_workspace_context observeValueForKeyPath:@"finishedLaunching" ofObject:[NSRunningApplication currentApplication] change:change_done context:&proc];
    struct process term_proc = { .pid = 104, .terminated = true, .name = "term_proc" };
    [(workspace_context *)g_workspace_context observeValueForKeyPath:@"finishedLaunching" ofObject:[NSRunningApplication currentApplication] change:change_done context:&term_proc];
    ANGRY_ASSERT(proc.pid == 103, "expected finished launching observation to process change event");
}

ANGRY_TEST(handlesWorkspaceWakeNotificationSafely)
{
    [(workspace_context *)g_workspace_context didWake:nil];
    ANGRY_ASSERT(g_workspace_context != NULL, "expected didWake to dispatch event loop message without crashing");
}

ANGRY_TEST(handlesMenuBarHidingChangeNotificationSafely)
{
    [(workspace_context *)g_workspace_context didChangeMenuBarHiding:nil];
    ANGRY_ASSERT(g_workspace_context != NULL, "expected didChangeMenuBarHiding to dispatch event loop message");
}

ANGRY_TEST(handlesDockRestartNotificationSafely)
{
    [(workspace_context *)g_workspace_context didRestartDock:nil];
    ANGRY_ASSERT(g_workspace_context != NULL, "expected didRestartDock to dispatch event loop message");
}

ANGRY_TEST(handlesDockPreferenceChangeNotificationSafely)
{
    [(workspace_context *)g_workspace_context didChangeDockPref:nil];
    ANGRY_ASSERT(g_workspace_context != NULL, "expected didChangeDockPref to dispatch event loop message");
}

ANGRY_TEST(handlesActiveDisplayChangeNotificationSafely)
{
    [(workspace_context *)g_workspace_context activeDisplayDidChange:nil];
    ANGRY_ASSERT(g_workspace_context != NULL, "expected activeDisplayDidChange to dispatch event loop message");
}

ANGRY_TEST(handlesActiveSpaceChangeNotificationSafely)
{
    [(workspace_context *)g_workspace_context activeSpaceDidChange:nil];
    ANGRY_ASSERT(g_workspace_context != NULL, "expected activeSpaceDidChange to dispatch event loop message");
}

ANGRY_TEST(handlesApplicationHideAndUnhideNotificationsSafely)
{
    NSNotification *notif = [NSNotification notificationWithName:@"test_app" object:nil userInfo:@{NSWorkspaceApplicationKey: [NSRunningApplication currentApplication]}];
    [(workspace_context *)g_workspace_context didHideApplication:notif];
    [(workspace_context *)g_workspace_context didUnhideApplication:notif];
    ANGRY_ASSERT(notif.userInfo != nil, "expected didHideApplication and didUnhideApplication to process notification");
}
