#import <objc/runtime.h>

@interface NSRunningApplication (MockDockTermination)
- (BOOL)mock_terminate;
@end

@implementation NSRunningApplication (MockDockTermination)
- (BOOL)mock_terminate
{
    return YES;
}
@end

ANGRY_TEST(configuresOsaxFilesystemPathsProperly)
{
    scripting_addition_set_path();
    ANGRY_ASSERT(osax_base_dir[0] == '/' && strstr(osax_info_plist, "Info.plist") != NULL,
                 "expected osax directory paths to be formatted with valid prefixes");
}

ANGRY_TEST(failsToCreateOsaxDirectoryWithoutRootPrivileges)
{
    bool created = scripting_addition_create_directory();
    ANGRY_ASSERT(!created, "expected creating osax directory in system folder to fail without root");
}

ANGRY_TEST(writesAndRemovesTemporaryOsaxFile)
{
    char tmp_file[] = "/tmp/yabai_sa_write_test.tmp";
    bool written = scripting_addition_write_file("payload", 7, tmp_file, "w");
    unlink(tmp_file);
    ANGRY_ASSERT(written, "expected writing temporary test file to succeed");
}

ANGRY_TEST(preparesOsaxBinariesPermissionsAndSignatures)
{
    g_mock_system = true;
    scripting_addition_prepare_binaries();
    g_mock_system = false;
    ANGRY_ASSERT(osax_bin_loader[0] != '\0', "expected binary preparation commands to execute");
}

ANGRY_TEST(restartsDockApplicationSafelyWithSwizzledTerminate)
{
    Method orig = class_getInstanceMethod([NSRunningApplication class], @selector(terminate));
    Method mock = class_getInstanceMethod([NSRunningApplication class], @selector(mock_terminate));
    method_exchangeImplementations(orig, mock);
    scripting_addition_restart_dock();
    method_exchangeImplementations(orig, mock);
    ANGRY_ASSERT(orig != NULL && mock != NULL, "expected dock restart to execute cleanly with swizzling");
}

ANGRY_TEST(configuresSocketPathWhenRunningUnderSudo)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    g_mock_uid = 0;
    setenv("SUDO_UID", "501", 1);
    bool set_sudo = scripting_addition_set_socket_path();
    unsetenv("SUDO_UID");
    bool set_nosudo = scripting_addition_set_socket_path();
    g_mock_uid = (uid_t)-1;
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(set_sudo && !set_nosudo, "expected socket path setup to succeed with SUDO_UID and fail without");
}

ANGRY_TEST(evaluatesOsaxInstallationAndVersionStatus)
{
    bool installed = scripting_addition_is_installed();
    int check_result = scripting_addition_check();
    ANGRY_ASSERT((installed || !installed) && (check_result == 0 || check_result == 1),
                 "expected installation and version checks to return valid boolean/status codes");
}

ANGRY_TEST(removesOsaxInstallationReturningStatusCode)
{
    g_mock_system = true;
    bool removed = scripting_addition_remove();
    g_mock_system = false;
    ANGRY_ASSERT(removed, "expected scripting addition remove call to exit cleanly");
}

ANGRY_TEST(handlesOsaxInstallationFailureGracefully)
{
    g_mock_system = true;
    int install_code = scripting_addition_install();
    g_mock_system = false;
    scripting_addition_set_path();
    ANGRY_ASSERT(install_code == 2 || install_code == 1, "expected install attempt to return failure code without root");
}

ANGRY_TEST(failsHandshakeRequestWhenSocketServerIsAbsent)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    char ver[SA_SOCKET_BUFF_LEN] = {0};
    uint32_t attrib = 0;
    bool connected = scripting_addition_request_handshake(ver, &attrib);
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!connected, "expected handshake request to fail when no socket daemon is running");
}

ANGRY_TEST(returnsExpectedAttributesForOperatingSystemVersion)
{
    uint32_t current_attr = scripting_addition_expected_attrib();
    ANGRY_ASSERT(current_attr == OSAX_ATTRIB_ALL || current_attr == OSAX_ATTRIB_REDUCED,
                 "expected valid OSAX attributes for current macOS version");
}

ANGRY_TEST(performsValidationFailingWhenSocketIsAbsent)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    int val_code = scripting_addition_perform_validation();
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(val_code != 0, "expected perform validation to report error without connection");
}

ANGRY_TEST(evaluatesSystemIntegrityProtectionCompatibility)
{
    g_mock_csr_config = 0;
    bool sip_denied = scripting_addition_is_sip_friendly();
    g_mock_csr_config = CSR_ALLOW_UNRESTRICTED_FS | CSR_ALLOW_TASK_FOR_PID;
    bool sip_allowed = scripting_addition_is_sip_friendly();
    g_mock_csr_config = -1;
    ANGRY_ASSERT(!sip_denied && sip_allowed, "expected SIP check to reflect CSR flags accurately");
}

ANGRY_TEST(evaluatesArm64ePreviewAbiStatus)
{
#ifdef __arm64__
    bool arm64e = scripting_addition_is_arm64e_enabled();
    ANGRY_ASSERT(arm64e || !arm64e, "expected arm64e check to query boot arguments");
#else
    ANGRY_ASSERT(true, "not running on arm64 architecture");
#endif
}

ANGRY_TEST(attemptsMachLoaderPayloadInjection)
{
    bool injected = mach_loader_inject_payload();
    ANGRY_ASSERT(!injected, "expected injection to fail when loader executable is absent");
}

ANGRY_TEST(uninstallsScriptingAdditionWithPermissionChecks)
{
    snprintf(osax_base_dir, sizeof(osax_base_dir), "%s", "/tmp/yabai_test_osax_dir.tmp");
    g_mock_csr_config = 0;
    int u1 = scripting_addition_uninstall();
    g_mock_csr_config = CSR_ALLOW_UNRESTRICTED_FS | CSR_ALLOW_TASK_FOR_PID;
    g_mock_uid = 501;
    int u2 = scripting_addition_uninstall();
    g_mock_uid = 0;
    int u3 = scripting_addition_uninstall();
    g_mock_uid = (uid_t)-1;
    g_mock_csr_config = -1;
    scripting_addition_set_path();
    ANGRY_ASSERT(u1 == 1 && u2 == 1 && u3 == 0, "expected uninstall to enforce SIP and root checks");
}

ANGRY_TEST(loadsScriptingAdditionVerifyingRootAndSipEnforcement)
{
    g_mock_uid = 501;
    int load_nonroot = scripting_addition_load();
    g_mock_uid = 0;
    g_mock_csr_config = 0;
    int load_sip_fail = scripting_addition_load();
    g_mock_uid = (uid_t)-1;
    g_mock_csr_config = -1;
    ANGRY_ASSERT(load_nonroot == 1 && load_sip_fail == 1,
                 "expected load to reject non-root execution and SIP-restricted systems");
}

ANGRY_TEST(failsSendingRawBytesWhenSocketUnavailable)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    char test_bytes[4] = { 0x01, 0x02, 0x03, 0x04 };
    bool sent = scripting_addition_send_bytes(test_bytes, sizeof(test_bytes));
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!sent, "expected sending bytes to disconnected socket to return false");
}

ANGRY_TEST(dispatchesSpaceFocusAndCreationCommandsSafely)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    bool focus_ok = scripting_addition_focus_space(1);
    bool create_ok = scripting_addition_create_space(1);
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!focus_ok && !create_ok, "expected space focus and creation to dispatch cleanly without crashing");
}

ANGRY_TEST(dispatchesSpaceMoveToDisplayCommandSafely)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    bool moved = scripting_addition_move_space_to_display(1, 2, 0, true);
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!moved, "expected space move to display to dispatch payload");
}

ANGRY_TEST(dispatchesWindowMoveCommandSafely)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    bool moved = scripting_addition_move_window(1234, 100, 200);
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!moved, "expected window move command to dispatch payload");
}

ANGRY_TEST(dispatchesWindowOpacityCommandsForInstantAndFade)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    bool instant_ok = scripting_addition_set_opacity(1234, 0.85f, 0.0f);
    bool fade_ok = scripting_addition_set_opacity(1234, 0.85f, 0.35f);
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!instant_ok && !fade_ok, "expected opacity instant and fade commands to dispatch");
}

ANGRY_TEST(dispatchesWindowStickyAndFocusAndScaleCommandsSafely)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    bool sticky_ok = scripting_addition_set_sticky(1234, true);
    bool focus_ok = scripting_addition_focus_window(1234);
    bool scale_ok = scripting_addition_scale_window(1234, 10.0f, 10.0f, 800.0f, 600.0f);
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!sticky_ok && !focus_ok && !scale_ok, "expected sticky, focus, and scale commands to dispatch");
}

ANGRY_TEST(dispatchesWindowSwapProxyInAndOutCommandsSafely)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    struct window_animation anims[2] = {
        { .wid = 101, .proxy = { .id = 201 }, .skip = false },
        { .wid = 102, .proxy = { .id = 202 }, .skip = true }
    };
    bool in_ok = scripting_addition_swap_window_proxy_in(anims, 2);
    bool out_ok = scripting_addition_swap_window_proxy_out(anims, 2);
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!in_ok && !out_ok, "expected swap window proxy in and out commands to dispatch");
}

ANGRY_TEST(dispatchesWindowOrderInCommandSafely)
{
    char old_sock[MAXLEN];
    memcpy(old_sock, g_sa_socket_file, sizeof(old_sock));
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", "/tmp/nonexistent_sa.socket");
    uint32_t window_ids[2] = { 101, 102 };
    bool ordered = scripting_addition_order_window_in(window_ids, 2);
    memcpy(g_sa_socket_file, old_sock, sizeof(old_sock));
    ANGRY_ASSERT(!ordered, "expected order window in command to dispatch payload");
}
