#ifndef SERVICE_H
#define SERVICE_H

#define _PATH_LAUNCHCTL   "/bin/launchctl"
#define _NAME_YABAI_PLIST "com.asmvik.yabai"
#define _LEGACY_NAME_YABAI_PLIST "com.koekeishiya.yabai"
#define _PATH_YABAI_PLIST "%s/Library/LaunchAgents/"_NAME_YABAI_PLIST".plist"
#define _PATH_LEGACY_YABAI_PLIST "%s/Library/LaunchAgents/"_LEGACY_NAME_YABAI_PLIST".plist"

#define _YABAI_PLIST \
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n" \
    "<plist version=\"1.0\">\n" \
    "<dict>\n" \
    "    <key>Label</key>\n" \
    "    <string>"_NAME_YABAI_PLIST"</string>\n" \
    "    <key>ProgramArguments</key>\n" \
    "    <array>\n" \
    "        <string>%s</string>\n" \
    "    </array>\n" \
    "    <key>EnvironmentVariables</key>\n" \
    "    <dict>\n" \
    "        <key>PATH</key>\n" \
    "        <string>%s</string>\n" \
    "    </dict>\n" \
    "    <key>RunAtLoad</key>\n" \
    "    <true/>\n" \
    "    <key>KeepAlive</key>\n" \
    "    <dict>\n" \
    "        <key>SuccessfulExit</key>\n" \
    " 	     <false/>\n" \
    " 	     <key>Crashed</key>\n" \
    " 	     <true/>\n" \
    "    </dict>\n" \
    "    <key>StandardOutPath</key>\n" \
    "    <string>/tmp/yabai_%s.out.log</string>\n" \
    "    <key>StandardErrorPath</key>\n" \
    "    <string>/tmp/yabai_%s.err.log</string>\n" \
    "    <key>ProcessType</key>\n" \
    "    <string>Interactive</string>\n" \
    "    <key>Nice</key>\n" \
    "    <integer>-20</integer>\n" \
    "</dict>\n" \
    "</plist>"

//
// NOTE(asmvik): A launchd service has the following states:
//
//          1. Installed / Uninstalled
//          2. Active (Enable / Disable)
//          3. Bootstrapped (Load / Unload)
//          4. Running (Start / Stop)
//

static int safe_exec(char *const argv[], bool suppress_output)
{
    pid_t pid;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    if (suppress_output) {
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY|O_APPEND, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY|O_APPEND, 0);
    }

    int status = posix_spawn(&pid, argv[0], &actions, NULL, argv, NULL);
    if (status) return 1;

    while ((waitpid(pid, &status, 0) == -1) && (errno == EINTR)) {
        usleep(1000);
    }

    if (WIFSIGNALED(status)) {
        return 1;
    } else if (WIFSTOPPED(status)) {
        return 1;
    } else {
        return WEXITSTATUS(status);
    }
}

static char *populate_plist_path(void)
{
    CFStringRef home_ref = (__bridge CFStringRef) NSHomeDirectoryForUser(NULL);
    char *home = home_ref ? cfstring_copy(home_ref) : NULL;

    if (!home) {
        error("yabai: unable to retrieve home directory! abort..\n");
    }

    int size = strlen(_PATH_YABAI_PLIST)-2 + strlen(home) + 1;
    char *result = malloc(size);
    if (!result) {
        error("yabai: could not allocate memory for plist path! abort..\n");
    }

    memset(result, 0, size);
    snprintf(result, size, _PATH_YABAI_PLIST, home);

    return result;
}

static void service_cleanup_legacy(void)
{
    CFStringRef home_ref = (__bridge CFStringRef) NSHomeDirectoryForUser(NULL);
    char *home = home_ref ? cfstring_copy(home_ref) : NULL;
    if (!home) return;

    int size = strlen(_PATH_LEGACY_YABAI_PLIST)-2 + strlen(home) + 1;
    char *legacy_plist_path = malloc(size);
    if (!legacy_plist_path) {
        return;
    }

    memset(legacy_plist_path, 0, size);
    snprintf(legacy_plist_path, size, _PATH_LEGACY_YABAI_PLIST, home);

    char legacy_target[MAXLEN];
    snprintf(legacy_target, sizeof(legacy_target), "gui/%d/%s", getuid(), _LEGACY_NAME_YABAI_PLIST);

    char domain_target[MAXLEN];
    snprintf(domain_target, sizeof(domain_target), "gui/%d", getuid());

    bool has_file = file_exists(legacy_plist_path);

    const char *const args_print[] = { _PATH_LAUNCHCTL, "print", legacy_target, NULL };
    bool is_bootstrapped = (safe_exec((char *const*)args_print, true) == 0);

    if (has_file || is_bootstrapped) {
        warn("yabai: detected legacy service '%s', removing and disabling..\n", _LEGACY_NAME_YABAI_PLIST);

        const char *const args_bootout[] = { _PATH_LAUNCHCTL, "bootout", domain_target, legacy_plist_path, NULL };
        safe_exec((char *const*)args_bootout, true);

        const char *const args_disable[] = { _PATH_LAUNCHCTL, "disable", legacy_target, NULL };
        safe_exec((char *const*)args_disable, true);

        if (has_file) {
            unlink(legacy_plist_path);
        }
    }

    free(legacy_plist_path);
}


static char *populate_plist(int *length)
{
    char *user = getenv("USER");
    if (!user) {
        error("yabai: 'env USER' not set! abort..\n");
    }

    char *path_env = getenv("PATH");
    if (!path_env) {
        error("yabai: 'env PATH' not set! abort..\n");
    }

    char exe_path[4096];
    unsigned int exe_path_size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &exe_path_size) < 0) {
        error("yabai: unable to retrieve path of executable! abort..\n");
    }

    int size = strlen(_YABAI_PLIST)-8 + strlen(exe_path) + strlen(path_env) + (2*strlen(user)) + 1;
    char *result = malloc(size);
    if (!result) {
        error("yabai: could not allocate memory for plist contents! abort..\n");
    }

    memset(result, 0, size);
    snprintf(result, size, _YABAI_PLIST, exe_path, path_env, user, user);
    *length = size-1;

    return result;
}

static inline void ensure_directory_exists(char *yabai_plist_path)
{
    //
    // NOTE(asmvik): Temporarily remove filename.
    // We know the filepath will contain a slash, as
    // it is controlled by us, so don't bother checking
    // the result..
    //

    char *last_slash = strrchr(yabai_plist_path, '/');
    *last_slash = '\0';

    if (!directory_exists(yabai_plist_path)) {
        mkdir(yabai_plist_path, 0755);
    }

    //
    // NOTE(asmvik): Restore original filename.
    //

    *last_slash = '/';
}

static int service_install_internal(char *yabai_plist_path)
{
    int yabai_plist_length;
    char *yabai_plist = populate_plist(&yabai_plist_length);
    ensure_directory_exists(yabai_plist_path);

    FILE *handle = fopen(yabai_plist_path, "w");
    if (!handle) return 1;

    size_t bytes = fwrite(yabai_plist, yabai_plist_length, 1, handle);
    int result = bytes == 1 ? 0 : 1;
    fclose(handle);

    return result;
}

static int service_install(void)
{
    service_cleanup_legacy();
    char *yabai_plist_path = populate_plist_path();

    if (file_exists(yabai_plist_path)) {
        error("yabai: service file '%s' is already installed! abort..\n", yabai_plist_path);
    }

    return service_install_internal(yabai_plist_path);
}

static int service_uninstall(void)
{
    char *yabai_plist_path = populate_plist_path();

    if (!file_exists(yabai_plist_path)) {
        error("yabai: service file '%s' is not installed! abort..\n", yabai_plist_path);
    }

    return unlink(yabai_plist_path) == 0 ? 0 : 1;
}

static int service_start(void)
{
    service_cleanup_legacy();
    char *yabai_plist_path = populate_plist_path();
    if (!file_exists(yabai_plist_path)) {
        warn("yabai: service file '%s' is not installed! attempting installation..\n", yabai_plist_path);

        int result = service_install_internal(yabai_plist_path);
        if (result) {
            error("yabai: service file '%s' could not be installed! abort..\n", yabai_plist_path);
        }
    }

    char service_target[MAXLEN];
    snprintf(service_target, sizeof(service_target), "gui/%d/%s", getuid(), _NAME_YABAI_PLIST);

    char domain_target[MAXLEN];
    snprintf(domain_target, sizeof(domain_target), "gui/%d", getuid());

    //
    // NOTE(asmvik): Check if service is bootstrapped
    //

    const char *const args[] = { _PATH_LAUNCHCTL, "print", service_target, NULL };
    int is_bootstrapped = safe_exec((char *const*)args, true);

    if (is_bootstrapped != 0) {

        //
        // NOTE(asmvik): Service is not bootstrapped and could be disabled.
        // There is no way to query if the service is disabled, and we cannot
        // bootstrap a disabled service. Try to enable the service. This will be
        // a no-op if the service is already enabled.
        //

        const char *const args[] = { _PATH_LAUNCHCTL, "enable", service_target, NULL };
        safe_exec((char *const*)args, false);

        //
        // NOTE(asmvik): Bootstrap service into the target domain.
        // This will also start the program **iff* RunAtLoad is set to true.
        //

        const char *const args2[] = { _PATH_LAUNCHCTL, "bootstrap", domain_target, yabai_plist_path, NULL };
        return safe_exec((char *const*)args2, false);
    } else {

        //
        // NOTE(asmvik): The service has already been bootstrapped.
        // Tell the bootstrapped service to launch immediately; it is an
        // error to bootstrap a service that has already been bootstrapped.
        //

        const char *const args[] = { _PATH_LAUNCHCTL, "kickstart", service_target, NULL };
        return safe_exec((char *const*)args, false);
    }
}

static int service_restart(void)
{
    service_cleanup_legacy();
    char *yabai_plist_path = populate_plist_path();
    if (!file_exists(yabai_plist_path)) {
        error("yabai: service file '%s' is not installed! abort..\n", yabai_plist_path);
    }

    char service_target[MAXLEN];
    snprintf(service_target, sizeof(service_target), "gui/%d/%s", getuid(), _NAME_YABAI_PLIST);

    const char *const args[] = { _PATH_LAUNCHCTL, "kickstart", "-k", service_target, NULL };
    return safe_exec((char *const*)args, false);
}

static int service_stop(void)
{
    service_cleanup_legacy();
    char *yabai_plist_path = populate_plist_path();
    if (!file_exists(yabai_plist_path)) {
        error("yabai: service file '%s' is not installed! abort..\n", yabai_plist_path);
    }

    char service_target[MAXLEN];
    snprintf(service_target, sizeof(service_target), "gui/%d/%s", getuid(), _NAME_YABAI_PLIST);

    char domain_target[MAXLEN];
    snprintf(domain_target, sizeof(domain_target), "gui/%d", getuid());

    //
    // NOTE(asmvik): Check if service is bootstrapped
    //

    const char *const args[] = { _PATH_LAUNCHCTL, "print", service_target, NULL };
    int is_bootstrapped = safe_exec((char *const*)args, true);

    if (is_bootstrapped != 0) {

        //
        // NOTE(asmvik): Service is not bootstrapped, but the program
        // could still be running an instance that was started **while the service
        // was bootstrapped**, so we tell it to stop said service.
        //

        const char *const args[] = { _PATH_LAUNCHCTL, "kill", "SIGTERM", service_target, NULL };
        return safe_exec((char *const*)args, false);
    } else {

        //
        // NOTE(asmvik): Service is bootstrapped; we stop a potentially
        // running instance of the program and unload the service, making it
        // not trigger automatically in the future.
        //
        // This is NOT the same as disabling the service, which will prevent
        // it from being boostrapped in the future (without explicitly re-enabling
        // it first).
        //

        const char *const args[] = { _PATH_LAUNCHCTL, "bootout", domain_target, yabai_plist_path, NULL };
        safe_exec((char *const*)args, false);

        const char *const args2[] = { _PATH_LAUNCHCTL, "disable", service_target, NULL };
        return safe_exec((char *const*)args2, false);
    }
}

#endif
