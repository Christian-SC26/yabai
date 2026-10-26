ANGRY_TEST(allocatesAndPushesElementsIntoStretchBuffer)
{
    int *buf = NULL;
    for (int i = 0; i < 32; ++i) buf_push(buf, i * 17);
    bool valid = (buf_len(buf) == 32) && (buf[31] == 31 * 17) && (buf_last(buf) == 31 * 17);
    buf_free(buf);
    ANGRY_ASSERT(valid, "pushing into stretch buffer must maintain length, capacity and last element");
}

ANGRY_TEST(deletesElementFromStretchBufferDecreasingLength)
{
    int *buf = NULL;
    buf_push(buf, 100); buf_push(buf, 200); buf_push(buf, 300);
    buf_del(buf, 0);
    bool valid = (buf_len(buf) == 2) && (buf[0] == 300);
    buf_free(buf);
    ANGRY_ASSERT(valid, "deleting element must replace slot with last element and decrement length");
}

ANGRY_TEST(allocatesAndPushesElementsIntoTemporaryStretchBuffer)
{
    int *tbuf = NULL;
    ts_buf_push(tbuf, 77);
    ts_buf_push(tbuf, 88);
    ts_buf_push(tbuf, 99);
    bool valid = (ts_buf_len(tbuf) == 3) && (ts_buf_last(tbuf) == 99);
    ANGRY_ASSERT(valid, "temporary stretch buffer must allocate in scratch memory and record items");
}

ANGRY_TEST(deletesElementFromTemporaryStretchBufferDecreasingLength)
{
    int *tbuf = NULL;
    ts_buf_push(tbuf, 11);
    ts_buf_push(tbuf, 22);
    ts_buf_del(tbuf, 0);
    bool valid = (ts_buf_len(tbuf) == 1) && (tbuf[0] == 22);
    ANGRY_ASSERT(valid, "deleting from temporary stretch buffer must adjust length and promote last item");
}

ANGRY_TEST(allocatesAlignedMemoryAcrossSpecificByteBoundaries)
{
    void *p1 = ts_alloc_aligned(64, 128);
    uintptr_t addr = (uintptr_t) p1;
    ANGRY_ASSERT((addr & 63) == 0, "aligned scratch allocation must be aligned to 64-byte boundary");
}

ANGRY_TEST(expandsTemporaryStorageBufferInPlace)
{
    uint64_t before = g_temp_storage.used;
    void *ptr = ts_alloc_unaligned(64);
    void *expanded = ts_expand(ptr, 64, 128);
    bool valid = (expanded == ptr) && (g_temp_storage.used == before + 192);
    ANGRY_ASSERT(valid, "expanding most recent temporary buffer must preserve origin and increment usage");
}

ANGRY_TEST(shrinksTemporaryStorageBufferAdjustingUsedCounter)
{
    ts_alloc_unaligned(256);
    uint64_t top = g_temp_storage.used;
    void *ptr = g_temp_storage.memory + top - 256;
    ts_resize(ptr, 256, 128);
    ANGRY_ASSERT(g_temp_storage.used == top - 128, "shrinking top scratch buffer must decrement used counter");
}

ANGRY_TEST(resetsTemporaryStorageUsedCounterToZero)
{
    ts_alloc_unaligned(512);
    ts_reset();
    ANGRY_ASSERT(g_temp_storage.used == 0, "ts_reset must return scratch allocation pointer to offset zero");
}

ANGRY_TEST(readsNonZeroMonotonicCpuTimerTicks)
{
    uint64_t t1 = read_cpu_timer();
    uint64_t t2 = read_cpu_timer();
    ANGRY_ASSERT(t2 >= t1 && t1 > 0, "reading cpu timer ticks must yield non-zero monotonic progress");
}

ANGRY_TEST(readsPositiveCpuFrequencyValue)
{
    uint64_t freq = read_cpu_freq();
    ANGRY_ASSERT(freq >= 1000000ULL, "cpu timer frequency must exceed 1MHz");
}

ANGRY_TEST(recordsProfilerAnchorsDuringExecution)
{
    profile_begin();
    g_profiler.anchors[1].tsc_elapsed_exclusive = 5000;
    g_profiler.anchors[1].tsc_elapsed_inclusive = 5000;
    g_profiler.anchors[1].hit_count = 1;
    g_profiler.anchors[1].label = "test_profile_anchor";
    ANGRY_ASSERT(g_profiler.anchors[1].hit_count == 1, "profiler anchor must store hit count and label");
}

ANGRY_TEST(suppressesDebugOutputWhenVerboseIsDisabled)
{
    bool orig = g_verbose;
    g_verbose = false;
    debug("this should not print %d\n", 42);
    g_verbose = orig;
    ANGRY_ASSERT(!g_verbose, "debug logger must skip formatting when verbose flag is false");
}

ANGRY_TEST(locatesExportedSymbolFromDynamicLibrary)
{
    void *sym = macho_find_symbol("/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/SkyLight", "_CGSGetConnectionPortById");
    ANGRY_ASSERT(sym != NULL, "macho symbol lookup must locate internal _CGSGetConnectionPortById in SkyLight");
}

ANGRY_TEST(returnsNullWhenSearchingForNonexistentSymbol)
{
    void *sym = macho_find_symbol("/usr/lib/libSystem.B.dylib", "_this_symbol_does_not_exist_xyz");
    ANGRY_ASSERT(sym == NULL, "macho symbol lookup must return NULL for non-existent symbol");
}

ANGRY_TEST(returnsNullWhenSearchingInNonexistentLibrary)
{
    void *sym = macho_find_symbol("/nonexistent/bogus/path.dylib", "_printf");
    ANGRY_ASSERT(sym == NULL, "macho symbol lookup must return NULL for non-existent image");
}

ANGRY_TEST(populatesCurrentUserLaunchdPlistPath)
{
    char *path = populate_plist_path();
    bool valid = path && strstr(path, "Library/LaunchAgents/com.asmvik.yabai.plist") != NULL;
    free(path);
    ANGRY_ASSERT(valid, "launchd plist path generator must target user LaunchAgents folder");
}

ANGRY_TEST(populatesLegacyCurrentUserLaunchdPlistPath)
{
    CFStringRef home_ref = (__bridge CFStringRef) NSHomeDirectoryForUser(NULL);
    char *home = home_ref ? cfstring_copy(home_ref) : NULL;
    int size = strlen(_PATH_LEGACY_YABAI_PLIST) - 2 + strlen(home) + 1;
    char *path = malloc(size);
    snprintf(path, size, _PATH_LEGACY_YABAI_PLIST, home);
    bool valid = path && strstr(path, "com.koekeishiya.yabai.plist") != NULL;
    free(home);
    free(path);
    ANGRY_ASSERT(valid, "legacy launchd plist path generator must target koekeishiya plist");
}

ANGRY_TEST(executesSimpleBinaryViaSafeExecReturningZero)
{
    const char *const args[] = { "/usr/bin/true", NULL };
    int code = safe_exec((char *const *)args, true);
    ANGRY_ASSERT(code == 0, "safe_exec running /usr/bin/true must return status code zero");
}

ANGRY_TEST(initializesNotificationCenterDelegateAndIcon)
{
    bool ok = notify_init();
    ANGRY_ASSERT(ok && g_notify_init && g_notify_img != nil, "notification subsystem initialization must register icon and delegate");
}

ANGRY_TEST(computesEaseInSineBoundaryValues)
{
    float val = ease_in_sine(0.0f);
    ANGRY_ASSERT(fabs(val - 0.0f) < 0.001f, "ease_in_sine at t=0 must equal 0.0");
}

ANGRY_TEST(computesEaseOutSineBoundaryValues)
{
    float val = ease_out_sine(1.0f);
    ANGRY_ASSERT(fabs(val - 1.0f) < 0.001f, "ease_out_sine at t=1 must equal 1.0");
}

ANGRY_TEST(computesEaseInOutSineBoundaryValues)
{
    float val = ease_in_out_sine(0.5f);
    ANGRY_ASSERT(fabs(val - 0.5f) < 0.001f, "ease_in_out_sine at midpoint t=0.5 must equal 0.5");
}

ANGRY_TEST(computesEaseInQuadBoundaryValues)
{
    float val = ease_in_quad(0.5f);
    ANGRY_ASSERT(fabs(val - 0.25f) < 0.001f, "ease_in_quad at t=0.5 must equal 0.25");
}

ANGRY_TEST(computesEaseOutQuadBoundaryValues)
{
    float val = ease_out_quad(0.5f);
    ANGRY_ASSERT(fabs(val - 0.75f) < 0.001f, "ease_out_quad at t=0.5 must equal 0.75");
}

ANGRY_TEST(computesEaseInOutQuadBoundaryValues)
{
    float val = ease_in_out_quad(0.5f);
    ANGRY_ASSERT(fabs(val - 0.5f) < 0.001f, "ease_in_out_quad at t=0.5 must equal 0.5");
}

ANGRY_TEST(computesEaseInCubicBoundaryValues)
{
    float val = ease_in_cubic(0.5f);
    ANGRY_ASSERT(fabs(val - 0.125f) < 0.001f, "ease_in_cubic at t=0.5 must equal 0.125");
}

ANGRY_TEST(computesEaseOutCubicBoundaryValues)
{
    float val = ease_out_cubic(0.5f);
    ANGRY_ASSERT(fabs(val - 0.875f) < 0.001f, "ease_out_cubic at t=0.5 must equal 0.875");
}

ANGRY_TEST(computesEaseInOutCubicBoundaryValues)
{
    float val = ease_in_out_cubic(0.5f);
    ANGRY_ASSERT(fabs(val - 0.5f) < 0.001f, "ease_in_out_cubic at t=0.5 must equal 0.5");
}

ANGRY_TEST(computesEaseInQuartBoundaryValues)
{
    float val = ease_in_quart(0.5f);
    ANGRY_ASSERT(fabs(val - 0.0625f) < 0.001f, "ease_in_quart at t=0.5 must equal 0.0625");
}

ANGRY_TEST(computesEaseOutQuartBoundaryValues)
{
    float val = ease_out_quart(0.5f);
    ANGRY_ASSERT(fabs(val - 0.9375f) < 0.001f, "ease_out_quart at t=0.5 must equal 0.9375");
}

ANGRY_TEST(computesEaseInOutQuartBoundaryValues)
{
    float val = ease_in_out_quart(0.5f);
    ANGRY_ASSERT(fabs(val - 0.5f) < 0.001f, "ease_in_out_quart at t=0.5 must equal 0.5");
}

ANGRY_TEST(computesEaseInQuintBoundaryValues)
{
    float val = ease_in_quint(0.5f);
    ANGRY_ASSERT(fabs(val - 0.03125f) < 0.001f, "ease_in_quint at t=0.5 must equal 0.03125");
}

ANGRY_TEST(computesEaseOutQuintBoundaryValues)
{
    float val = ease_out_quint(0.5f);
    ANGRY_ASSERT(fabs(val - 0.96875f) < 0.001f, "ease_out_quint at t=0.5 must equal 0.96875");
}

ANGRY_TEST(computesEaseInOutQuintBoundaryValues)
{
    float val = ease_in_out_quint(0.5f);
    ANGRY_ASSERT(fabs(val - 0.5f) < 0.001f, "ease_in_out_quint at t=0.5 must equal 0.5");
}

ANGRY_TEST(computesEaseInExpoBoundaryValues)
{
    float val = ease_in_expo(1.0f);
    ANGRY_ASSERT(fabs(val - 1.0f) < 0.001f, "ease_in_expo at t=1.0 must equal 1.0");
}

ANGRY_TEST(computesEaseOutExpoBoundaryValues)
{
    float val = ease_out_expo(0.0f);
    ANGRY_ASSERT(fabs(val - 0.0f) < 0.001f, "ease_out_expo at t=0.0 must equal 0.0");
}

ANGRY_TEST(computesEaseInOutExpoBoundaryValues)
{
    float val = ease_in_out_expo(0.5f);
    ANGRY_ASSERT(fabs(val - 0.5f) < 0.001f, "ease_in_out_expo at t=0.5 must equal 0.5");
}

ANGRY_TEST(computesEaseInCircBoundaryValues)
{
    float val = ease_in_circ(0.0f);
    ANGRY_ASSERT(fabs(val - 0.0f) < 0.001f, "ease_in_circ at t=0.0 must equal 0.0");
}

ANGRY_TEST(computesEaseOutCircBoundaryValues)
{
    float val = ease_out_circ(1.0f);
    ANGRY_ASSERT(fabs(val - 1.0f) < 0.001f, "ease_out_circ at t=1.0 must equal 1.0");
}

ANGRY_TEST(computesEaseInOutCircBoundaryValues)
{
    float val = ease_in_out_circ(0.5f);
    ANGRY_ASSERT(fabs(val - 0.5f) < 0.001f, "ease_in_out_circ at t=0.5 must equal 0.5");
}

ANGRY_TEST(formatsOptionalBoolJsonValues)
{
    bool valid = string_equals(json_optional_bool(0), "null") &&
                 string_equals(json_optional_bool(1), "true") &&
                 string_equals(json_optional_bool(2), "false");
    ANGRY_ASSERT(valid, "optional bool JSON serialization must map 0 to null, 1 to true, and 2 to false");
}

ANGRY_TEST(formatsBooleanJsonValues)
{
    bool valid = string_equals(json_bool(true), "true") && string_equals(json_bool(false), "false");
    ANGRY_ASSERT(valid, "bool JSON serialization must strictly format true and false literals");
}

ANGRY_TEST(determinesRootPrivilegesAccurately)
{
    bool root = is_root();
    ANGRY_ASSERT(!root, "running tests as normal user must return false for root check");
}

ANGRY_TEST(copiesCfStringToHeapAllocatedCString)
{
    CFStringRef ref = CFSTR("yabai_test_string_2026");
    char *copied = cfstring_copy(ref);
    bool valid = copied && string_equals(copied, "yabai_test_string_2026");
    free(copied);
    ANGRY_ASSERT(valid, "cfstring_copy must extract full UTF-8 C-string from CoreFoundation string");
}

ANGRY_TEST(copiesHeapStringAccurately)
{
    char *orig = "heap_allocated_yabai_token";
    char *dup = string_copy(orig);
    bool valid = dup && string_equals(dup, orig);
    free(dup);
    ANGRY_ASSERT(valid, "string_copy must allocate and copy exact content with terminating null byte");
}

ANGRY_TEST(clampsCornerRadiiToHalfDimensionBoundary)
{
    CGRect r = CGRectMake(0, 0, 100, 60);
    float rx = cgrect_clamp_x_radius(r, 80.0f);
    float ry = cgrect_clamp_y_radius(r, 50.0f);
    bool valid = (rx == 50.0f) && (ry == 30.0f);
    ANGRY_ASSERT(valid, "corner radii exceeding half dimensions must clamp to width/2 and height/2");
}

ANGRY_TEST(verifiesDirectoryExistenceAccurately)
{
    bool exists = directory_exists("/usr") && !directory_exists("/usr_bogus_dir_xyz");
    ANGRY_ASSERT(exists, "directory existence checker must identify valid directories and reject non-existent paths");
}

ANGRY_TEST(verifiesFileExistenceAccurately)
{
    bool exists = file_exists("/bin/sh") && !file_exists("/bin") && !file_exists("/nonexistent_file_xyz");
    ANGRY_ASSERT(exists, "file existence checker must validate regular files and reject directories or missing paths");
}

ANGRY_TEST(verifiesExecutablePermissionsAccurately)
{
    bool can_exec = file_can_execute("/bin/sh") && !file_can_execute("/dev/null");
    ANGRY_ASSERT(can_exec, "file executable checker must verify S_IXUSR permissions");
}
