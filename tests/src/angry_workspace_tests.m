ANGRY_TEST(observesFinishedLaunchingOfApplication)
{
    struct process proc = { .pid = getpid(), .name = "self_app" };
    proc.ns_application = [NSRunningApplication currentApplication];
    workspace_application_observe_finished_launching(g_workspace_context, &proc);
    bool observed = true;
    @try { [(NSRunningApplication *)proc.ns_application removeObserver:(id)g_workspace_context forKeyPath:@"finishedLaunching" context:&proc]; } @catch (id ex) { observed = false; }
    ANGRY_ASSERT(observed, "expected the workspace to observe finishedLaunching of the application");
}

ANGRY_TEST(observesActivationPolicyOfApplication)
{
    struct process proc = { .pid = getpid(), .name = "self_app" };
    proc.ns_application = [NSRunningApplication currentApplication];
    workspace_application_observe_activation_policy(g_workspace_context, &proc);
    bool observed = true;
    @try { [(NSRunningApplication *)proc.ns_application removeObserver:(id)g_workspace_context forKeyPath:@"activationPolicy" context:&proc]; } @catch (id ex) { observed = false; }
    ANGRY_ASSERT(observed, "expected the workspace to observe activationPolicy of the application");
}

ANGRY_TEST(treatsProcessWithoutApplicationAsUnobservable)
{
    struct process proc = { .pid = 99997, .ns_application = NULL };
    bool observable = workspace_application_is_observable(&proc);
    ANGRY_ASSERT_ALL("expected a process without an application to be unobservable with a prohibited policy",
                     !observable,
                     proc.policy == NSApplicationActivationPolicyProhibited);
}

ANGRY_TEST(evaluatesApplicationFinishedLaunchingState)
{
    struct process proc_null = { .pid = 99996, .ns_application = NULL };
    bool null_finished = workspace_application_is_finished_launching(&proc_null);
    struct process proc_dock = { .pid = workspace_get_dock_pid() };
    proc_dock.ns_application = workspace_application_create_running_ns_application(&proc_dock);
    bool dock_finished = workspace_application_is_finished_launching(&proc_dock);
    if (proc_dock.ns_application) [(id)proc_dock.ns_application release];
    ANGRY_ASSERT_ALL("expected dock process to report finished launching",
                     !null_finished,
                     dock_finished);
}

ANGRY_TEST(reportsNoNotchForUnknownDisplay)
{
    ANGRY_ASSERT(workspace_display_notch_height(999999) == 0, "expected a display that does not exist to have no notch");
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
    ANGRY_ASSERT_ALL("expected activation policy change to update process policy",
                     proc.policy == NSApplicationActivationPolicyRegular,
                     term_proc.policy == NSApplicationActivationPolicyProhibited);
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

ANGRY_TEST(postsSystemWokeEventOnWake)
{
    [(workspace_context *)g_workspace_context didWake:nil];
    ANGRY_ASSERT(angry_last_posted_event()->type == SYSTEM_WOKE, "expected a system woke event to be posted");
}

ANGRY_TEST(postsMenuBarHiddenChangedEventOnMenuBarHidingChange)
{
    [(workspace_context *)g_workspace_context didChangeMenuBarHiding:nil];
    ANGRY_ASSERT(angry_last_posted_event()->type == MENU_BAR_HIDDEN_CHANGED, "expected a menu bar hidden changed event to be posted");
}

ANGRY_TEST(postsDockDidRestartEventOnDockRestart)
{
    [(workspace_context *)g_workspace_context didRestartDock:nil];
    ANGRY_ASSERT(angry_last_posted_event()->type == DOCK_DID_RESTART, "expected a dock did restart event to be posted");
}

ANGRY_TEST(postsDockDidChangePrefEventOnDockPreferenceChange)
{
    [(workspace_context *)g_workspace_context didChangeDockPref:nil];
    ANGRY_ASSERT(angry_last_posted_event()->type == DOCK_DID_CHANGE_PREF, "expected a dock did change pref event to be posted");
}

ANGRY_TEST(postsDisplayChangedEventOnActiveDisplayChange)
{
    [(workspace_context *)g_workspace_context activeDisplayDidChange:nil];
    ANGRY_ASSERT(angry_last_posted_event()->type == DISPLAY_CHANGED, "expected a display changed event to be posted");
}

ANGRY_TEST(postsSpaceChangedEventOnActiveSpaceChange)
{
    [(workspace_context *)g_workspace_context activeSpaceDidChange:nil];
    ANGRY_ASSERT(angry_last_posted_event()->type == SPACE_CHANGED, "expected a space changed event to be posted");
}

ANGRY_TEST(postsApplicationHiddenEventCarryingPid)
{
    NSNotification *notif = [NSNotification notificationWithName:@"test_app" object:nil userInfo:@{NSWorkspaceApplicationKey: [NSRunningApplication currentApplication]}];
    [(workspace_context *)g_workspace_context didHideApplication:notif];
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT_ALL("expected an application hidden event for the application",
                     e->type == APPLICATION_HIDDEN,
                     (pid_t)(intptr_t)e->context == getpid());
}

ANGRY_TEST(postsApplicationVisibleEventCarryingPid)
{
    NSNotification *notif = [NSNotification notificationWithName:@"test_app" object:nil userInfo:@{NSWorkspaceApplicationKey: [NSRunningApplication currentApplication]}];
    [(workspace_context *)g_workspace_context didUnhideApplication:notif];
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT_ALL("expected an application visible event for the application",
                     e->type == APPLICATION_VISIBLE,
                     (pid_t)(intptr_t)e->context == getpid());
}
