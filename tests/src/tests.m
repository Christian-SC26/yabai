unsigned char __src_osax_payload[1];
unsigned int __src_osax_payload_len;
unsigned char __src_osax_loader[1];
unsigned int __src_osax_loader_len;

#include "../../src/manifest.m"
#include <dlfcn.h>

typedef bool (test_function)(void);

#define TEST_SIG(name) bool test_##name(void)

// Legacy test harness macros for reference compatibility
#define TEST_FUNC(name, code) static TEST_SIG(name) { char *test_name = #name; bool result = true; {code} return result; }
#define TEST_CHECK(r, e) if ((r) != (e)) { printf("                   \e[1;33m%s\e[m\e[1;31m#%d %s == %s\e[m \e[1;31m(%d == %d)\e[m\n", test_name, __LINE__, #r, #e, r, e); result = false; }
#define TEST_ENTRY(name) { #name, test_##name },

// Angry Tests infrastructure (Yegor Bugayenko rules: verb-named, one assertion, short, explicit message)
#define ANGRY_TEST(name) static bool name(void)

#define ANGRY_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("\n    \e[1;31m✖ [ASSERTION FAILED]\e[m %s\n" \
                   "      \e[1;33mCondition:\e[m %s\n" \
                   "      \e[1;33mLocation:\e[m  %s:%d (%s)\n", \
                   (msg), #cond, __FILE__, __LINE__, __func__); \
            return false; \
        } \
        return true; \
    } while (0)

#define ANGRY_ENTRY(func_name) { #func_name, func_name },

CGError CoreDockSendNotification(CFStringRef notification, int unknown)
{
    return kCGErrorSuccess;
}

static uid_t g_mock_uid = (uid_t)-1;
uid_t getuid(void)
{
    if (g_mock_uid != (uid_t)-1) return g_mock_uid;
    typedef uid_t (*real_getuid_fn)(void);
    static real_getuid_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_getuid_fn)dlsym(RTLD_NEXT, "getuid");
    return real_fn ? real_fn() : 501;
}

static int g_mock_csr_config = -1;
int csr_get_active_config(uint32_t *config)
{
    if (g_mock_csr_config != -1) {
        *config = (uint32_t)g_mock_csr_config;
        return 0;
    }
    typedef int (*csr_fn)(uint32_t *);
    static csr_fn real_csr = NULL;
    if (!real_csr) real_csr = (csr_fn)dlsym(RTLD_NEXT, "csr_get_active_config");
    return real_csr ? real_csr(config) : 0;
}

static bool g_mock_system = false;
int system(const char *command)
{
    return 0;
}

sem_t *sem_open(const char *name, int oflag, ...)
{
    char isolated_name[256];
    const char *target_name = name;
    if (name && strstr(name, "yabai_event_loop_semaphore")) {
        snprintf(isolated_name, sizeof(isolated_name), "yabai_test_sem_%d", getpid());
        target_name = isolated_name;
    }
    typedef sem_t *(*real_sem_open_fn)(const char *, int, ...);
    static real_sem_open_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_sem_open_fn)dlsym(RTLD_NEXT, "sem_open");

    va_list ap;
    va_start(ap, oflag);
    mode_t mode = (mode_t)va_arg(ap, int);
    unsigned int value = va_arg(ap, unsigned int);
    va_end(ap);

    return real_fn ? real_fn(target_name, oflag, mode, value) : SEM_FAILED;
}

int sem_unlink(const char *name)
{
    char isolated_name[256];
    const char *target_name = name;
    if (name && strstr(name, "yabai_event_loop_semaphore")) {
        snprintf(isolated_name, sizeof(isolated_name), "yabai_test_sem_%d", getpid());
        target_name = isolated_name;
    }
    typedef int (*real_sem_unlink_fn)(const char *);
    static real_sem_unlink_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_sem_unlink_fn)dlsym(RTLD_NEXT, "sem_unlink");
    return real_fn ? real_fn(target_name) : -1;
}

static void dummy_event_tap_mach_port_cb(CFMachPortRef port, void *msg, CFIndex size, void *info) {}

CFMachPortRef CGEventTapCreate(CGEventTapLocation tap,
                               CGEventTapPlacement place,
                               CGEventTapOptions options,
                               CGEventMask eventsOfInterest,
                               CGEventTapCallBack callback,
                               void *refcon)
{
    CFMachPortContext ctx = { 0, refcon, NULL, NULL, NULL };
    return CFMachPortCreate(kCFAllocatorDefault, dummy_event_tap_mach_port_cb, &ctx, NULL);
}

bool CGEventTapIsEnabled(CFMachPortRef tap)
{
    return tap != NULL;
}

void CGEventTapEnable(CFMachPortRef tap, bool enable)
{
}

int execvp(const char *file, char *const argv[])
{
    _exit(0);
}

CGError CGSetLocalEventsSuppressionInterval(CFTimeInterval seconds)
{
    return kCGErrorSuccess;
}

CGError CGEnableEventStateCombining(boolean_t doCombineState)
{
    return kCGErrorSuccess;
}

CGError SLPSPostEventRecordTo(ProcessSerialNumber *psn, uint8_t *bytes)
{
    return kCGErrorSuccess;
}

static const char *g_mock_home = NULL;
NSString *NSHomeDirectoryForUser(NSString *userName)
{
    if (g_mock_home) return [NSString stringWithUTF8String:g_mock_home];
    typedef NSString *(*real_home_fn)(NSString *);
    static real_home_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_home_fn)dlsym(RTLD_NEXT, "NSHomeDirectoryForUser");
    return real_fn ? real_fn(userName) : @"/tmp";
}

static bool g_mock_posix_spawn = false;
int posix_spawn(pid_t *restrict pid, const char *restrict path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *restrict attrp,
                char *const argv[restrict], char *const envp[restrict])
{
    if (g_mock_posix_spawn) {
        if (pid) *pid = 0;
        return 0;
    }
    typedef int (*real_spawn_fn)(pid_t *, const char *, const posix_spawn_file_actions_t *, const posix_spawnattr_t *, char *const [], char *const []);
    static real_spawn_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_spawn_fn)dlsym(RTLD_NEXT, "posix_spawn");
    return real_fn ? real_fn(pid, path, file_actions, attrp, argv, envp) : 1;
}

static bool g_mock_fast_usleep = false;
int usleep(useconds_t usec)
{
    if (g_mock_fast_usleep) return 0;
    typedef int (*real_fn)(useconds_t);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "usleep");
    return r ? r(usec) : 0;
}

static uint64_t g_mock_active_space = 0;
uint64_t SLSGetActiveSpace(int cid)
{
    if (g_mock_active_space) return g_mock_active_space;
    typedef uint64_t (*real_fn)(int);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "SLSGetActiveSpace");
    return r ? r(cid) : 1;
}

CFArrayRef SLSCopySpacesForWindows(int cid, int selector, CFArrayRef window_list)
{
    if (g_mock_active_space) {
        CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt64Type, &g_mock_active_space);
        CFArrayRef arr = CFArrayCreate(NULL, (const void **)&num, 1, &kCFTypeArrayCallBacks);
        CFRelease(num);
        return arr;
    }
    typedef CFArrayRef (*real_fn)(int, int, CFArrayRef);
    static real_fn r = NULL;
    if (!r) r = (real_fn)dlsym(RTLD_NEXT, "SLSCopySpacesForWindows");
    return r ? r(cid, selector, window_list) : NULL;
}

static bool g_mock_dispatch_after = false;
void dispatch_after(dispatch_time_t when, dispatch_queue_t queue, dispatch_block_t block)
{
    if (g_mock_dispatch_after) {
        block();
        return;
    }
    typedef void (*real_dispatch_after_fn)(dispatch_time_t, dispatch_queue_t, dispatch_block_t);
    static real_dispatch_after_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_dispatch_after_fn)dlsym(RTLD_NEXT, "dispatch_after");
    if (real_fn) real_fn(when, queue, block);
}

static bool g_mock_ax_cannot_complete = false;
AXError AXObserverAddNotification(AXObserverRef observer, AXUIElementRef element, CFStringRef notification, void *refcon)
{
    if (g_mock_ax_cannot_complete) {
        return kAXErrorCannotComplete;
    }
    typedef AXError (*real_ax_fn)(AXObserverRef, AXUIElementRef, CFStringRef, void *);
    static real_ax_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_ax_fn)dlsym(RTLD_NEXT, "AXObserverAddNotification");
    return real_fn ? real_fn(observer, element, notification, refcon) : kAXErrorFailure;
}

static bool g_mock_dock_window_layer18 = false;
CFArrayRef CGWindowListCopyWindowInfo(CGWindowListOption option, CGWindowID relativeToWindow)
{
    if (g_mock_dock_window_layer18) {
        int layer18 = 18;
        CFNumberRef layer_num = CFNumberCreate(NULL, kCFNumberIntType, &layer18);
        CFTypeRef keys[] = { (const void *)kCGWindowOwnerName, (const void *)kCGWindowLayer };
        CFTypeRef values[] = { (const void *)CFSTR("Dock"), (const void *)layer_num };
        CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFArrayRef arr = CFArrayCreate(NULL, (const void **)&dict, 1, &kCFTypeArrayCallBacks);
        CFRelease(layer_num);
        CFRelease(dict);
        return arr;
    }
    typedef CFArrayRef (*real_cg_fn)(CGWindowListOption, CGWindowID);
    static real_cg_fn real_fn = NULL;
    if (!real_fn) real_fn = (real_cg_fn)dlsym(RTLD_NEXT, "CGWindowListCopyWindowInfo");
    return real_fn ? real_fn(option, relativeToWindow) : NULL;
}

@implementation NSRunningApplication (AngryEventLoopMock)
- (BOOL)mock_isFinishedLaunching { return YES; }
- (NSApplicationActivationPolicy)mock_activationPolicy { return NSApplicationActivationPolicyRegular; }
@end

#include "area.c"
#include "structures.c"
#include "angry_tests.c"
#include "angry_misc_tests.c"
#include "angry_rule_tests.c"
#include "angry_event_serialize_tests.c"
#include "angry_signal_tests.c"
#include "angry_application_tests.c"
#include "angry_mouse_tests.c"
#include "angry_process_tests.c"
#include "angry_mission_control_tests.c"
#include "angry_display_space_tests.c"
#include "angry_view_tests.c"
#include "angry_window_tests.c"
#include "angry_display_manager_tests.c"
#include "angry_space_manager_tests.c"
#include "angry_message_tests.c"
#include "angry_layout_store_tests.m"
#include "angry_workspace_tests.m"
#include "angry_sa_tests.m"
#include "angry_event_loop_tests.c"
#include "angry_yabai_tests.c"
#include "angry_window_manager_tests.c"

#define TEST_LIST                                                               \
    ANGRY_ENTRY(allocatesChunkInEmptyMemoryPool)                                \
    ANGRY_ENTRY(preservesStoredPointerAcrossThousandAllocations)                 \
    ANGRY_ENTRY(wrapsMemoryPoolPointerToOriginOnOverflow)                       \
    ANGRY_ENTRY(rehashesTableWithoutDataLossUnderHighLoad)                      \
    ANGRY_ENTRY(removesKeyAndCleansEntryFromTable)                              \
    ANGRY_ENTRY(preservesUnremovedKeysAfterPartialDeletion)                     \
    ANGRY_ENTRY(ignoresDuplicateAddWhenKeyAlreadyExists)                       \
    ANGRY_ENTRY(identifiesWestDisplayAcrossNegativeCoordinates)                 \
    ANGRY_ENTRY(rejectsEastDisplayWhenTargetIsPositionedWest)                   \
    ANGRY_ENTRY(identifiesEastDisplayAcrossPositiveCoordinates)                 \
    ANGRY_ENTRY(rejectsWestDisplayWhenTargetIsPositionedEast)                   \
    ANGRY_ENTRY(findsNearestWestDisplayAmongMultipleMonitors)                   \
    ANGRY_ENTRY(findsNearestEastDisplayAcrossIrregularGaps)                     \
    ANGRY_ENTRY(returnsNegativeIndexWhenNoDisplayInDirection)                   \
    ANGRY_ENTRY(splitsParentAreaEquallyWithSpecifiedGapOnVerticalAxis)          \
    ANGRY_ENTRY(splitsParentAreaAsymmetricallyOnHorizontalAxis)                  \
    ANGRY_ENTRY(preservesTotalDimensionWithinPixelRoundingTolerance)            \
    ANGRY_ENTRY(allocatesMinimumDimensionWhenSplitRatioIsExtremelySmall)         \
    ANGRY_ENTRY(allocatesMinimumDimensionWhenSplitRatioIsExtremelyLarge)         \
    ANGRY_ENTRY(ignoresDropWhenCursorDeltaIsBelowThreshold)                     \
    ANGRY_ENTRY(permitsDropWhenCursorDeltaCrossesThreshold)                     \
    ANGRY_ENTRY(preservesWindowWhenExitingToMatchingSpace)                      \
    ANGRY_ENTRY(guardsAgainstPrematureUntileDuringFullscreenSpaceDestruction)   \
    ANGRY_ENTRY(swapsWindowIdsBetweenTwoBspNodesPreservingTreeStructure)        \
    ANGRY_ENTRY(mirrorsBspTreeHorizontallyInvertingChildOrder)                  \
    ANGRY_ENTRY(mirrorsBspTreeVerticallyInvertingChildOrder)                    \
    ANGRY_ENTRY(promotesSiblingToRootWhenRemovingLeafFromTwoNodeTree)           \
    ANGRY_ENTRY(requiresAllAttributesForLegacyMacOsVersions)                    \
    ANGRY_ENTRY(requiresReducedAttributesForTahoeAndGoldengate)                 \
    ANGRY_ENTRY(verifiesReducedAttributeMaskMatchesSubset)                      \
    ANGRY_ENTRY(defaultsWindowFocusAnimationToDisabled)                         \
    ANGRY_ENTRY(recordsPendingStateOnVerticalGestureBegin)                      \
    ANGRY_ENTRY(updatesLastGestureTimeOnVerticalGestureEnd)                     \
    ANGRY_ENTRY(capturesFrontmostWindowAndSpaceStateOnMissionControlEnter)      \
    ANGRY_ENTRY(maintainsIdempotencyWhenMissionControlStateAlreadyRecorded)     \
    ANGRY_ENTRY(coalescesConsecutiveMouseDraggedEventsInQueue)                  \
    ANGRY_ENTRY(clearsDirtyFlagOnDirectViewFlush)                               \
    ANGRY_ENTRY(defaultsBspLiveResizeToEnabled)                                 \
    ANGRY_ENTRY(coercesInsertDirectionToStackWhenTargetViewIsStacked)           \
    ANGRY_ENTRY(updatesFocusFollowsMouseMode)                                   \
    ANGRY_ENTRY(suppressesRapidGestureToSameSpace)                              \
    ANGRY_ENTRY(rateLimitsRapidGesturesUnderThreshold)                         \
    ANGRY_ENTRY(resolvesActiveSpaceForRunningApplications)                      \
    ANGRY_ENTRY(balancesBspTreeLeafRatiosEquallyForTwoLeaves)                   \
    ANGRY_ENTRY(balancesBspTreeLeafRatiosProportionallyForAsymmetricSubtrees)   \
    ANGRY_ENTRY(rotatesBspNodeNinetyDegreesTogglingSplitAxis)                   \
    ANGRY_ENTRY(rotatesBspNodeOneEightyDegreesInvertingRatioAndChildren)        \
    ANGRY_ENTRY(rotatesBspNodeTwoSeventyDegreesTogglingSplitAxis)               \
    ANGRY_ENTRY(findsNorthFenceAncestorWhenBoundedVertically)                   \
    ANGRY_ENTRY(returnsNullFenceWhenNodeLiesOnDisplayEdge)                      \
    ANGRY_ENTRY(stacksWindowOnLeafNodePlacingItAtFrontOfOrder)                  \
    ANGRY_ENTRY(combinesRuleEffectsOverridingManageProperty)                    \
    ANGRY_ENTRY(combinesRuleEffectsClampingOpacityToUnitInterval)               \
    ANGRY_ENTRY(combinesRuleEffectsPreservingScratchpadIdentifier)              \
    ANGRY_ENTRY(parsesRgbaColorFromHexWithAlphaChannel)                         \
    ANGRY_ENTRY(clampsFloatValueWithinSpecifiedBoundary)                        \
    ANGRY_ENTRY(clampsNegativeFloatValueToMinimumBoundary)                      \
    ANGRY_ENTRY(identifiesPointInsideBoundingBox)                               \
    ANGRY_ENTRY(rejectsPointOutsideBoundingBox)                                 \
    ANGRY_ENTRY(identifiesPointInsideTriangle)                                  \
    ANGRY_ENTRY(rejectsPointOutsideTriangle)                                    \
    ANGRY_ENTRY(escapesSpecialCharactersForJsonSerialization)                   \
    ANGRY_ENTRY(returnsNullWhenStringRequiresNoEscaping)                        \
    ANGRY_ENTRY(matchesValidRegularExpression)                                  \
    ANGRY_ENTRY(rejectsMismatchedRegularExpression)                             \
    ANGRY_ENTRY(allocatesAndPushesElementsIntoStretchBuffer)                    \
    ANGRY_ENTRY(deletesElementFromStretchBufferDecreasingLength)                 \
    ANGRY_ENTRY(allocatesAndPushesElementsIntoTemporaryStretchBuffer)           \
    ANGRY_ENTRY(deletesElementFromTemporaryStretchBufferDecreasingLength)        \
    ANGRY_ENTRY(allocatesAlignedMemoryAcrossSpecificByteBoundaries)              \
    ANGRY_ENTRY(expandsTemporaryStorageBufferInPlace)                           \
    ANGRY_ENTRY(shrinksTemporaryStorageBufferAdjustingUsedCounter)              \
    ANGRY_ENTRY(resetsTemporaryStorageUsedCounterToZero)                        \
    ANGRY_ENTRY(readsNonZeroMonotonicCpuTimerTicks)                             \
    ANGRY_ENTRY(readsPositiveCpuFrequencyValue)                                 \
    ANGRY_ENTRY(recordsProfilerAnchorsDuringExecution)                          \
    ANGRY_ENTRY(suppressesDebugOutputWhenVerboseIsDisabled)                     \
    ANGRY_ENTRY(locatesExportedSymbolFromDynamicLibrary)                        \
    ANGRY_ENTRY(returnsNullWhenSearchingForNonexistentSymbol)                   \
    ANGRY_ENTRY(returnsNullWhenSearchingInNonexistentLibrary)                   \
    ANGRY_ENTRY(populatesCurrentUserLaunchdPlistPath)                           \
    ANGRY_ENTRY(populatesLegacyCurrentUserLaunchdPlistPath)                     \
    ANGRY_ENTRY(executesSimpleBinaryViaSafeExecReturningZero)                   \
    ANGRY_ENTRY(initializesNotificationCenterDelegateAndIcon)                   \
    ANGRY_ENTRY(computesEaseInSineBoundaryValues)                               \
    ANGRY_ENTRY(computesEaseOutSineBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseInOutSineBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInQuadBoundaryValues)                               \
    ANGRY_ENTRY(computesEaseOutQuadBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseInOutQuadBoundaryValues)                            \
    ANGRY_ENTRY(computesEaseInCubicBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseOutCubicBoundaryValues)                             \
    ANGRY_ENTRY(computesEaseInOutCubicBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInQuartBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseOutQuartBoundaryValues)                             \
    ANGRY_ENTRY(computesEaseInOutQuartBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInQuintBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseOutQuintBoundaryValues)                             \
    ANGRY_ENTRY(computesEaseInOutQuintBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInExpoBoundaryValues)                               \
    ANGRY_ENTRY(computesEaseOutExpoBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseInOutExpoBoundaryValues)                           \
    ANGRY_ENTRY(computesEaseInCircBoundaryValues)                               \
    ANGRY_ENTRY(computesEaseOutCircBoundaryValues)                              \
    ANGRY_ENTRY(computesEaseInOutCircBoundaryValues)                            \
    ANGRY_ENTRY(formatsOptionalBoolJsonValues)                                  \
    ANGRY_ENTRY(formatsBooleanJsonValues)                                       \
    ANGRY_ENTRY(determinesRootPrivilegesAccurately)                             \
    ANGRY_ENTRY(copiesCfStringToHeapAllocatedCString)                           \
    ANGRY_ENTRY(copiesHeapStringAccurately)                                     \
    ANGRY_ENTRY(clampsCornerRadiiToHalfDimensionBoundary)                       \
    ANGRY_ENTRY(verifiesDirectoryExistenceAccurately)                           \
    ANGRY_ENTRY(verifiesFileExistenceAccurately)                                \
    ANGRY_ENTRY(verifiesExecutablePermissionsAccurately)                       \
    ANGRY_ENTRY(readsOperatingSystemFrequency)                                  \
    ANGRY_ENTRY(sendsMachMessageSafely)                                         \
    ANGRY_ENTRY(convertsInt32ToCfString)                                        \
    ANGRY_ENTRY(resolvesConfigurationFilePath)                                  \
    ANGRY_ENTRY(executesConfigurationFileSafelyWhenMissing)                      \
    ANGRY_ENTRY(queriesAccessibilityPrivileges)                                 \
    ANGRY_ENTRY(resolvesWindowPidFromAccessibilityElement)                      \
    ANGRY_ENTRY(checksEnhancedUserInterfaceAccessibilityAttribute)             \
    ANGRY_ENTRY(restoresAlphaChannelInQuartzImage)                              \
    ANGRY_ENTRY(formatsDebugMessageIteratingTokens)                             \
    ANGRY_ENTRY(terminatesWithExitFailureOnError)                               \
    ANGRY_ENTRY(terminatesWithExitSuccessOnRequire)                             \
    ANGRY_ENTRY(allowsUserNotificationPresentation)                             \
    ANGRY_ENTRY(profilesExecutionTimerAndPrintsResults)                         \
    ANGRY_ENTRY(populatesLaunchdServicePlist)                                   \
    ANGRY_ENTRY(ensuresDirectoryPathExistsBeforeWriting)                        \
    ANGRY_ENTRY(installsServiceInternalToFile)                                  \
    ANGRY_ENTRY(cleansUpLegacyServiceRegistrationSafely)                        \
    ANGRY_ENTRY(installsLaunchdServiceToMockHomeDirectory)                      \
    ANGRY_ENTRY(uninstallsLaunchdServiceFromMockHomeDirectory)                    \
    ANGRY_ENTRY(startsLaunchdServiceInMockEnvironment)                          \
    ANGRY_ENTRY(restartsLaunchdServiceInMockEnvironment)                         \
    ANGRY_ENTRY(stopsLaunchdServiceInMockEnvironment)                           \
    ANGRY_ENTRY(serializesRuleConfigurationWithLabelAndEscapedStringsToJsonStream) \
    ANGRY_ENTRY(serializesRuleConfigurationWithoutOptionalStringsUsingEmptyDefaults) \
    ANGRY_ENTRY(copiesDisplayIdentifierAndFollowSpaceWhenFollowSpaceFlagSet)   \
    ANGRY_ENTRY(clearsFollowSpaceFlagWhenSourceDisplaySpecifiedWithoutFollowSpaceFlag) \
    ANGRY_ENTRY(copiesSpaceIdentifierAndFollowSpaceWhenFollowSpaceFlagSet)     \
    ANGRY_ENTRY(clearsFollowSpaceFlagWhenSourceSpaceSpecifiedWithoutFollowSpaceFlag) \
    ANGRY_ENTRY(ignoresOpacityOutsideUnitIntervalDuringCombination)             \
    ANGRY_ENTRY(copiesSublayerWhenRuleLayerFlagSet)                             \
    ANGRY_ENTRY(replacesScratchpadStringFreeingPreexistingValue)                \
    ANGRY_ENTRY(preservesDestinationPropertyWhenSourcePropertyIsUndecided)     \
    ANGRY_ENTRY(appliesGridDimensionsWhenBothRowAndColumnAreNonZero)            \
    ANGRY_ENTRY(ignoresGridDimensionsWhenRowOrColumnIsZero)                     \
    ANGRY_ENTRY(addsRuleToWindowManagerRulesBuffer)                             \
    ANGRY_ENTRY(replacesExistingRuleWhenAddingRuleWithIdenticalLabel)           \
    ANGRY_ENTRY(removesRuleByIndexFromWindowManagerBuffer)                      \
    ANGRY_ENTRY(returnsFalseWhenRemovingRuleByInvalidIndex)                     \
    ANGRY_ENTRY(removesRuleByLabelFromWindowManagerBuffer)                      \
    ANGRY_ENTRY(returnsFalseWhenRemovingRuleByNonexistentLabel)                 \
    ANGRY_ENTRY(freesRuleResourcesAndCompiledRegularExpressions)                \
    ANGRY_ENTRY(skipsOneShotRuleWhenReapplyingByIndex)                          \
    ANGRY_ENTRY(returnsFalseWhenReapplyingByNonexistentIndex)                    \
    ANGRY_ENTRY(skipsOneShotRuleWhenReapplyingByLabel)                          \
    ANGRY_ENTRY(returnsFalseWhenReapplyingByNonexistentLabel)                    \
    ANGRY_ENTRY(combinesStickyMffAndFullscreenProperties)                       \
    ANGRY_ENTRY(reappliesRuleByIndexIteratingActiveWindows)                     \
    ANGRY_ENTRY(reappliesRuleByLabelIteratingActiveWindows)                     \
    ANGRY_ENTRY(clearsRuleFlagFromRuleBitmask)                                  \
    ANGRY_ENTRY(setsRuleFlagInRuleBitmask)                                      \
    ANGRY_ENTRY(clearsRuleEffectsFlagFromEffectsBitmask)                         \
    ANGRY_ENTRY(augmentsDockSwipeEventWithRawIoHidPayload)                      \
    ANGRY_ENTRY(augmentsDockSwipeEndedEventWithVelocityPayload)                 \
    ANGRY_ENTRY(replacesPreexistingPayloadWhenAugmentingAlreadyAugmentedEvent)  \
    ANGRY_ENTRY(returnsNullWhenAugmentingNullEvent)                             \
    ANGRY_ENTRY(augmentsBareEventDefaultingMissingFieldsToZero)                 \
    ANGRY_ENTRY(evaluatesRequiresEventAugmentationWithoutCrashing)              \
    ANGRY_ENTRY(rejectsTruncatedEventDataUnderFourBytes)                        \
    ANGRY_ENTRY(rejectsUnsupportedEventDataVersionNumber)                       \
    ANGRY_ENTRY(rejectsTruncatedFieldHeaderInEventData)                         \
    ANGRY_ENTRY(clampsExtremePositiveAndNegativeFixedPointValues)               \
    ANGRY_ENTRY(handlesNullParsedEventDataFreeSafely)                           \
    ANGRY_ENTRY(resolvesSignalTypeFromStringAccurately)                         \
    ANGRY_ENTRY(resolvesSignalTypeForDockDidChangePref)                         \
    ANGRY_ENTRY(returnsUnknownSignalTypeForInvalidString)                       \
    ANGRY_ENTRY(addsSignalToSignalEventBuffer)                                 \
    ANGRY_ENTRY(replacesSignalWhenAddingWithDuplicateLabel)                     \
    ANGRY_ENTRY(removesSignalByIndexAcrossGlobalSignalTypes)                    \
    ANGRY_ENTRY(returnsFalseWhenRemovingSignalByInvalidIndex)                   \
    ANGRY_ENTRY(removesSignalByLabelFromSignalEventBuffer)                      \
    ANGRY_ENTRY(returnsFalseWhenRemovingSignalByNonexistentLabel)               \
    ANGRY_ENTRY(destroysSignalFreeingRegexesAndStrings)                         \
    ANGRY_ENTRY(serializesSignalListToJsonStream)                               \
    ANGRY_ENTRY(serializesEmptySignalListAsEmptyJsonArray)                      \
    ANGRY_ENTRY(filtersSignalWhenApplicationRegexDoesNotMatch)                  \
    ANGRY_ENTRY(permitsSignalWhenApplicationRegexMatches)                       \
    ANGRY_ENTRY(filtersSignalWhenActivePropertyMismatches)                      \
    ANGRY_ENTRY(pushesApplicationSignalIntoStorageWhenSubscriberExists)         \
    ANGRY_ENTRY(skipsSignalPushWhenNoSubscribersExist)                          \
    ANGRY_ENTRY(pushesDisplaySignalIntoStorageWhenSubscriberExists)              \
    ANGRY_ENTRY(pushesSpaceSignalIntoStorageWhenSubscriberExists)                \
    ANGRY_ENTRY(pushesMissionControlSignalIntoStorageWhenSubscriberExists)       \
    ANGRY_ENTRY(pushesSystemWokeSignalIntoStorageWhenSubscriberExists)          \
    ANGRY_ENTRY(pushesApplicationTerminatedSignalIntoStorage)                   \
    ANGRY_ENTRY(pushesApplicationFrontSwitchedSignalIntoStorage)                 \
    ANGRY_ENTRY(pushesApplicationHiddenSignalIntoStorage)                        \
    ANGRY_ENTRY(pushesWindowDestroyedSignalIntoStorage)                         \
    ANGRY_ENTRY(pushesSpaceCreatedAndDestroyedSignalsIntoStorage)               \
    ANGRY_ENTRY(pushesDisplayRemovedAndChangedSignalsIntoStorage)               \
    ANGRY_ENTRY(pushesMissionControlExitSignalIntoStorage)                      \
    ANGRY_ENTRY(flushesSignalStorageImmediatelyWhenEmpty)                       \
    ANGRY_ENTRY(createsApplicationInstanceFromCurrentProcess)                   \
    ANGRY_ENTRY(evaluatesApplicationFrontmostState)                             \
    ANGRY_ENTRY(evaluatesApplicationVisibilityState)                            \
    ANGRY_ENTRY(queriesApplicationWindowList)                                   \
    ANGRY_ENTRY(queriesApplicationMainWindow)                                  \
    ANGRY_ENTRY(queriesApplicationFocusedWindow)                               \
    ANGRY_ENTRY(observesAndUnobservesApplicationNotifications)                  \
    ANGRY_ENTRY(dispatchesNotificationHandlerForCreatedAndFocusedNotifications) \
    ANGRY_ENTRY(dispatchesNotificationHandlerForWindowGeometryChanges)          \
    ANGRY_ENTRY(dispatchesNotificationHandlerForMenuAndStateChanges)           \
    ANGRY_ENTRY(dispatchesNotificationHandlerForWindowDestroyedNotification)    \
    ANGRY_ENTRY(traversesEmptyWindowTableWithoutErrorDuringRuleReapplyAll)      \
    ANGRY_ENTRY(initializesMouseStateWithDefaultParameters)                     \
    ANGRY_ENTRY(parsesMouseModifierFromCommandAndAltFlags)                      \
    ANGRY_ENTRY(parsesMouseModifierFromShiftAndControlFlags)                    \
    ANGRY_ENTRY(populatesMouseWindowInfoCalculatingDeltasAndFlags)              \
    ANGRY_ENTRY(determinesStackDropActionWhenCursorInCenterAndModeIsStack)       \
    ANGRY_ENTRY(determinesSwapDropActionWhenCursorInCenterAndModeIsSwap)        \
    ANGRY_ENTRY(determinesWarpTopDropActionWhenCursorInTopTriangle)             \
    ANGRY_ENTRY(determinesWarpRightDropActionWhenCursorInRightTriangle)         \
    ANGRY_ENTRY(determinesWarpBottomDropActionWhenCursorInBottomTriangle)       \
    ANGRY_ENTRY(determinesWarpLeftDropActionWhenCursorInLeftTriangle)           \
    ANGRY_ENTRY(returnsDropActionNoneWhenCursorIsOutsideWindowBounds)           \
    ANGRY_ENTRY(handlesMouseDropNoTargetOnSameSpaceByFlushingNode)              \
    ANGRY_ENTRY(bypassesBspGridAdjustmentWhenViewLayoutIsNotBsp)                \
    ANGRY_ENTRY(swapsWindowsBetweenNodesUpdatingInsertionPoints)                \
    ANGRY_ENTRY(consumesMouseClickWhenModifiersMatchAndFreesOnRelease)          \
    ANGRY_ENTRY(stacksWindowIntoDestinationNodeOnMouseDropActionStack)          \
    ANGRY_ENTRY(warpsWindowBetweenSiblingsWithMatchingSplitSwappingNodes)       \
    ANGRY_ENTRY(handlesMouseRightMouseDownConsumingWhenModifierMatches)         \
    ANGRY_ENTRY(handlesMouseDraggedEventSettingDragDetectedFlag)                \
    ANGRY_ENTRY(handlesMouseMovedEventPassingThroughWhenModifierMatches)        \
    ANGRY_ENTRY(handlesDockSwipeGestureBeganAndEndedEvents)                     \
    ANGRY_ENTRY(handlesEventTapDisabledTimeoutReenablingHandleSafely)           \
    ANGRY_ENTRY(swapsWindowsAcrossDifferentSpacesUpdatingManagedTable)          \
    ANGRY_ENTRY(warpsWindowBetweenSiblingsWithDifferentSplitModifyingParent)    \
    ANGRY_ENTRY(handlesMouseMovedEventPostingEventWhenModifierDiffers)          \
    ANGRY_ENTRY(handlesZoomedNodeOnMouseDropActionStack)                        \
    ANGRY_ENTRY(hashesProcessSerialNumberUsingLowLong)                          \
    ANGRY_ENTRY(comparesIdenticalProcessSerialNumbersReturningTrue)             \
    ANGRY_ENTRY(comparesDifferentProcessSerialNumbersReturningFalse)            \
    ANGRY_ENTRY(retrievesPidForCurrentProcessSerialNumber)                      \
    ANGRY_ENTRY(createsProcessInstanceForCurrentProcess)                       \
    ANGRY_ENTRY(rejectsInvalidProcessInProcessCreateReturningNull)              \
    ANGRY_ENTRY(evaluatesProcessIsBeingDebuggedViaSysctl)                       \
    ANGRY_ENTRY(findsProcessInProcessManagerTable)                              \
    ANGRY_ENTRY(returnsNullWhenProcessNotFoundInProcessManager)                 \
    ANGRY_ENTRY(destroysProcessReleasingResources)                             \
    ANGRY_ENTRY(handlesProcessLaunchEventInProcessHandler)                      \
    ANGRY_ENTRY(handlesDuplicateProcessLaunchEventDiscardingDupe)               \
    ANGRY_ENTRY(handlesProcessFrontSwitchedEventInProcessHandler)               \
    ANGRY_ENTRY(handlesProcessTerminatedEventMarkingProcessTerminated)         \
    ANGRY_ENTRY(returnsErrorWhenProcessEventLacksProcessIdParameter)            \
    ANGRY_ENTRY(returnsZeroActiveSpaceForInvalidConnectionId)                   \
    ANGRY_ENTRY(addsRunningProcessesAndCachesFinderPsn)                         \
    ANGRY_ENTRY(initializesProcessManagerInstallingCarbonEventHandler)          \
    ANGRY_ENTRY(recordsMissionControlStateCapturingFrontApplicationAndSpace)    \
    ANGRY_ENTRY(preservesMissionControlStateIdempotencyWhenAlreadyRecorded)     \
    ANGRY_ENTRY(dispatchesConnectionHandlerForMissionControlEnter)              \
    ANGRY_ENTRY(dispatchesConnectionHandlerForSpaceCreatedNotification)         \
    ANGRY_ENTRY(dispatchesConnectionHandlerForSpaceDestroyedNotification)       \
    ANGRY_ENTRY(dispatchesConnectionHandlerForWindowOrderedNotification)        \
    ANGRY_ENTRY(dispatchesConnectionHandlerForWindowDestroyedNotification)      \
    ANGRY_ENTRY(updatesLastCmdTabTimeOnConnectionHandlerType1202)               \
    ANGRY_ENTRY(dispatchesMissionControlNotificationHandlerForShowAllWindows)   \
    ANGRY_ENTRY(dispatchesMissionControlNotificationHandlerForShowFrontWindows) \
    ANGRY_ENTRY(dispatchesMissionControlNotificationHandlerForShowDesktop)      \
    ANGRY_ENTRY(dispatchesMissionControlNotificationHandlerForExposeExit)       \
    ANGRY_ENTRY(observesAndUnobservesMissionControlDockEvents)                  \
    ANGRY_ENTRY(evaluatesMissionControlIsActiveReturningBooleanState)           \
    ANGRY_ENTRY(returnsCachedMissionControlStateWhenCalledWithinFiftyMilliseconds) \
    ANGRY_ENTRY(mapsMissionControlModesToExpectedStringRepresentations)         \
    ANGRY_ENTRY(dispatchesDisplayHandlerForDisplayAddedEvent)                    \
    ANGRY_ENTRY(dispatchesDisplayHandlerForDisplayRemovedEvent)                  \
    ANGRY_ENTRY(dispatchesDisplayHandlerForDisplayMovedEvent)                    \
    ANGRY_ENTRY(dispatchesDisplayHandlerForDisplayResizedEvent)                  \
    ANGRY_ENTRY(serializesDisplayPropertiesToJsonStream)                        \
    ANGRY_ENTRY(serializesIndividualDisplayPropertyFields)                      \
    ANGRY_ENTRY(countsSpacesForActiveDisplay)                                   \
    ANGRY_ENTRY(returnsZeroSpacesForInvalidDisplayId)                           \
    ANGRY_ENTRY(resolvesDisplayBoundsConstrainedWithExternalBarPadding)          \
    ANGRY_ENTRY(convertsDisplayIdToUuidAndBack)                                 \
    ANGRY_ENTRY(identifiesSystemSpaceAccurately)                                 \
    ANGRY_ENTRY(queriesSpaceWindowListIncludingMinimizedWindows)                \
    ANGRY_ENTRY(evaluatesSpaceVisibilityForActiveSpace)                          \
    ANGRY_ENTRY(evaluatesSpaceFullscreenStateForActiveSpace)                      \
    ANGRY_ENTRY(evaluatesSpaceUserTypeForActiveSpace)                           \
    ANGRY_ENTRY(identifiesLeftChildInBinaryTreeHierarchy)                       \
    ANGRY_ENTRY(equalizesBspNodeRatioAcrossSpecifiedSplitAxis)                  \
    ANGRY_ENTRY(locatesIndexOfWindowInWindowNodeList)                           \
    ANGRY_ENTRY(clearsZoomNodeRecursivelyFromBspSubtree)                       \
    ANGRY_ENTRY(locatesLastLeafNodeInDeepBspTree)                               \
    ANGRY_ENTRY(locatesPreviousLeafNodeInBspTree)                               \
    ANGRY_ENTRY(returnsNullWhenSearchingDirectionalNodeOnInvalidSpace)          \
    ANGRY_ENTRY(allocatesAndPopulatesWindowListFromLeafNodes)                   \
    ANGRY_ENTRY(evaluatesViewIsInvalidWhenValidFlagIsUnset)                      \
    ANGRY_ENTRY(marksViewDirtyWhenFlushingInvisibleSpace)                       \
    ANGRY_ENTRY(serializesBspViewPropertiesToJsonStream)                        \
    ANGRY_ENTRY(clearsViewRootTreeAndResetsNodeDimensions)                     \
    ANGRY_ENTRY(displaysAndDestroysInsertFeedbackWindow)                        \
    ANGRY_ENTRY(resolvesWindowDisplayUuidAndDisplayId)                           \
    ANGRY_ENTRY(resolvesWindowSpaceAndSpaceList)                                \
    ANGRY_ENTRY(queriesWindowShadowAndOpacity)                                  \
    ANGRY_ENTRY(queriesWindowLevelAndSubLevel)                                  \
    ANGRY_ENTRY(queriesWindowParentAndTags)                                     \
    ANGRY_ENTRY(evaluatesWindowIsStickyAccurately)                              \
    ANGRY_ENTRY(queriesWindowPropertyTitleViaTextStorage)                       \
    ANGRY_ENTRY(serializesNonAxWindowPropertiesToJsonStream)                    \
    ANGRY_ENTRY(serializesWindowStructurePropertiesToJsonStream)                \
    ANGRY_ENTRY(serializesWindowSelectedPropertiesWithoutOptionalAttributes)    \
    ANGRY_ENTRY(formatsWindowTitleUsingTemporaryStorage)                       \
    ANGRY_ENTRY(formatsWindowRoleUsingTemporaryStorage)                         \
    ANGRY_ENTRY(formatsWindowSubroleUsingTemporaryStorage)                      \
    ANGRY_ENTRY(identifiesRealWindowWithStandardRoleAndSubrole)                  \
    ANGRY_ENTRY(identifiesStandardWindowWithStandardSubrole)                    \
    ANGRY_ENTRY(identifiesUnknownSubroleWindow)                                 \
    ANGRY_ENTRY(evaluatesWindowCanMoveAndCanResizeFromFlags)                     \
    ANGRY_ENTRY(evaluatesWindowIsUndersizedBasedOnFrameDimensions)              \
    ANGRY_ENTRY(mapsWindowLevelsToLayerStrings)                                 \
    ANGRY_ENTRY(evaluatesWindowLevelIsStandardMatchingNormalLayer)              \
    ANGRY_ENTRY(evaluatesWindowIsRootWithValidOrDummyElement)                   \
    ANGRY_ENTRY(manipulatesWindowFlagsAndRuleFlagsBitmasks)                     \
    ANGRY_ENTRY(destroysWindowFreeingAllocatedStringsAndReferences)             \
    ANGRY_ENTRY(createsAndObservesWindowWhenAccessibilityWindowExists)          \
    ANGRY_ENTRY(queriesAllActiveDisplaysToJsonStream)                           \
    ANGRY_ENTRY(managesDisplayLabelsSettingAndQueryingAndRemoving)              \
    ANGRY_ENTRY(resolvesMainDisplayUuidMatchingMainDisplayId)                   \
    ANGRY_ENTRY(resolvesDockDisplayUuidAndDockDisplayId)                        \
    ANGRY_ENTRY(resolvesPointDisplayUuidAndPointDisplayId)                      \
    ANGRY_ENTRY(evaluatesDisplayCoordinateComparatorAcrossAxes)                 \
    ANGRY_ENTRY(calculatesDisplayArrangementWithCustomSortOrders)               \
    ANGRY_ENTRY(resolvesArrangementDisplayUuidAndDisplayId)                     \
    ANGRY_ENTRY(resolvesCursorDisplayIdFromCurrentCursorLocation)               \
    ANGRY_ENTRY(evaluatesPrevAndNextDisplayIdNavigation)                        \
    ANGRY_ENTRY(resolvesFirstAndLastDisplayIdsAccurately)                       \
    ANGRY_ENTRY(findsClosestDisplayInCardinalDirections)                        \
    ANGRY_ENTRY(queriesDockOrientationAndDockBoundingRect)                      \
    ANGRY_ENTRY(evaluatesDisplayIsAnimatingStatus)                              \
    ANGRY_ENTRY(locatesElementAtSpecifiedScreenCoordinates)                    \
    ANGRY_ENTRY(focusesDisplayWithWindowAtSpecifiedCoordinates)                \
    ANGRY_ENTRY(returnsSameDisplayErrorWhenSpaceDoesNotBelongToDisplay)         \
    ANGRY_ENTRY(comparesViewsUsingKeyEquality)                                  \
    ANGRY_ENTRY(queriesSpaceToJsonStream)                                       \
    ANGRY_ENTRY(queriesSpacesForWindowToJsonStream)                             \
    ANGRY_ENTRY(queriesSpacesForDisplayToJsonStream)                            \
    ANGRY_ENTRY(queriesSpacesForDisplaysToJsonStream)                           \
    ANGRY_ENTRY(findsAndRefreshesViewForSpace)                                  \
    ANGRY_ENTRY(marksViewInvalidForSpace)                                       \
    ANGRY_ENTRY(untilesWindowFromSpaceView)                                     \
    ANGRY_ENTRY(managesSpaceLabelsSettingAndQueryingAndRemoving)                \
    ANGRY_ENTRY(configuresSpaceLayoutAndValidates)                              \
    ANGRY_ENTRY(configuresGapForSpaceWithAbsAndRelTypes)                        \
    ANGRY_ENTRY(togglesGapForSpaceUpdatingState)                                \
    ANGRY_ENTRY(togglesMissionControlAndShowDesktopSafely)                      \
    ANGRY_ENTRY(configuresLayoutForAllSpaces)                                   \
    ANGRY_ENTRY(configuresWindowGapForAllSpaces)                                 \
    ANGRY_ENTRY(configuresAllPaddingDimensionsForAllSpaces)                     \
    ANGRY_ENTRY(configuresSplitTypeAndAutoBalanceForAllSpaces)                  \
    ANGRY_ENTRY(configuresPaddingForSpaceWithAbsAndRelTypes)                     \
    ANGRY_ENTRY(togglesPaddingForSpaceUpdatingState)                            \
    ANGRY_ENTRY(rotatesAndMirrorsSpaceSubtree)                                  \
    ANGRY_ENTRY(equalizesAndBalancesSpaceSubtree)                               \
    ANGRY_ENTRY(tilesWindowOnSpaceWithInsertionPoint)                           \
    ANGRY_ENTRY(togglesWindowSplitDirectionInBspSubtree)                        \
    ANGRY_ENTRY(resolvesMissionControlSpaceByDesktopIndex)                      \
    ANGRY_ENTRY(resolvesCursorSpaceMatchingCurrentCursor)                      \
    ANGRY_ENTRY(resolvesPreviousAndNextSpaceFromCurrent)                        \
    ANGRY_ENTRY(resolvesFirstAndLastSpaceAcrossDisplays)                       \
    ANGRY_ENTRY(movesWindowListAndSingleWindowToSpace)                          \
    ANGRY_ENTRY(assignsProcessToSpaceAndAllSpaces)                              \
    ANGRY_ENTRY(evaluatesWindowIsOnActiveSpaceAndSpecifiedSpace)                 \
    ANGRY_ENTRY(swapsSpaceWithSpaceOnSameAndCrossDisplay)                       \
    ANGRY_ENTRY(movesSpaceToSpaceDetectingErrors)                               \
    ANGRY_ENTRY(movesSpaceToDisplayDetectingErrors)                             \
    ANGRY_ENTRY(switchesSpaceDetectingSameSpace)                                \
    ANGRY_ENTRY(destroysSpaceDetectingMissingAndInvalidSrc)                      \
    ANGRY_ENTRY(addsSpaceDetectingMissingSrc)                                   \
    ANGRY_ENTRY(marksSpacesInvalidForDisplayAndAllDisplays)                     \
    ANGRY_ENTRY(refreshesApplicationWindowsReturningStatus)                     \
    ANGRY_ENTRY(handlesDisplayAddUpdatingSpaceViews)                            \
    ANGRY_ENTRY(evaluatesIsSpaceLastUserSpaceAndFindFirstUserSpace)            \
    ANGRY_ENTRY(advancesTokenPointerAndExtractsTokenText)                       \
    ANGRY_ENTRY(evaluatesTokenPrefixMatchingStartOfString)                      \
    ANGRY_ENTRY(evaluatesTokenEqualsAgainstExactStrings)                        \
    ANGRY_ENTRY(evaluatesTokenIsValidCheckingLengthAndPointer)                  \
    ANGRY_ENTRY(evaluatesTokenIsPositiveIntegerForValidAndInvalidStrings)       \
    ANGRY_ENTRY(evaluatesTokenIsHexadecimalForPrefixedNumbers)                  \
    ANGRY_ENTRY(evaluatesTokenIsFloatForValidFloatingPointNumbers)              \
    ANGRY_ENTRY(convertsTokenToTypedTokenValue)                                 \
    ANGRY_ENTRY(parsesKeyValuePairExtractingKeyAndValueAndExclusion)            \
    ANGRY_ENTRY(parsesValueTypeForAbsAndRelModifiers)                          \
    ANGRY_ENTRY(parsesResizeHandleForCardinalAndCornerDirections)              \
    ANGRY_ENTRY(parsesLabelValidatingReservedIdentifiers)                      \
    ANGRY_ENTRY(parsesPropertiesFromCommaSeparatedToken)                        \
    ANGRY_ENTRY(parsesDisplaySelectorForIndexAndDirections)                    \
    ANGRY_ENTRY(parsesSpaceSelectorForIndexAndRelativeSelectors)                \
    ANGRY_ENTRY(parsesWindowSelectorForKeywordsAndWindowIds)                    \
    ANGRY_ENTRY(parsesInsertSelectorForCardinalDirectionsAndStack)              \
    ANGRY_ENTRY(parsesRuleSpecifyingAppFilterAndManageEffect)                   \
    ANGRY_ENTRY(dispatchesConfigDomainModifyingRuntimeSettings)                 \
    ANGRY_ENTRY(dispatchesDisplayDomainExecutingDisplayCommands)                \
    ANGRY_ENTRY(dispatchesSpaceDomainExecutingSpaceCommands)                    \
    ANGRY_ENTRY(dispatchesWindowDomainExecutingWindowCommands)                  \
    ANGRY_ENTRY(dispatchesQueryDomainEmittingFormattedJson)                     \
    ANGRY_ENTRY(dispatchesRuleDomainListingActiveRules)                         \
    ANGRY_ENTRY(dispatchesSignalDomainListingRegisteredSignals)                 \
    ANGRY_ENTRY(dispatchesMessageHandlingForInvalidDomain)                      \
    ANGRY_ENTRY(formatsDaemonFailureMessageToJsonStream)                        \
    ANGRY_ENTRY(formatsDaemonDeprecatedWarningToJsonStream)                     \
    ANGRY_ENTRY(terminatesMessageLoopImmediatelyWhenNotRunning)                 \
    ANGRY_ENTRY(initializesAndTerminatesMessageLoopSocket)                     \
    ANGRY_ENTRY(resolvesLayoutStorePathForNullAndCustomPaths)                   \
    ANGRY_ENTRY(resolvesLayoutStoreSessionPathForNullAndCustomPaths)            \
    ANGRY_ENTRY(ensuresDirectoryExistsForTargetFilePath)                       \
    ANGRY_ENTRY(serializesLeafAndBranchWindowNodesToDictionary)                 \
    ANGRY_ENTRY(collectsAppNamesRecursivelyFromSerializedNodeDictionary)        \
    ANGRY_ENTRY(reconstructsWindowNodeHierarchyFromSerializedDictionary)        \
    ANGRY_ENTRY(buildsSessionDictionaryCapturingDisplaysAndSpaces)              \
    ANGRY_ENTRY(savesSpaceLayoutToFileHandlingErrors)                           \
    ANGRY_ENTRY(dumpsSpaceLayoutToJsonStreamHandlingErrors)                     \
    ANGRY_ENTRY(restoresSpaceLayoutFromFileHandlingMissingFile)                 \
    ANGRY_ENTRY(savesSessionStateToFileHandlingErrors)                          \
    ANGRY_ENTRY(dumpsSessionStateToJsonStreamHandlingErrors)                    \
    ANGRY_ENTRY(restoresSessionStateFromFileHandlingMissingFile)                \
    ANGRY_ENTRY(observesFinishedLaunchingForValidAndNullApplications)           \
    ANGRY_ENTRY(observesActivationPolicyForValidAndNullApplications)            \
    ANGRY_ENTRY(determinesApplicationObservableForRegularAndNullPolicies)       \
    ANGRY_ENTRY(evaluatesApplicationFinishedLaunchingState)                     \
    ANGRY_ENTRY(measuresDisplayNotchHeightForBuiltinAndExternalDisplays)        \
    ANGRY_ENTRY(deallocatesWorkspaceContextProperly)                            \
    ANGRY_ENTRY(processesActivationPolicyChangeObservation)                     \
    ANGRY_ENTRY(processesFinishedLaunchingChangeObservation)                    \
    ANGRY_ENTRY(handlesWorkspaceWakeNotificationSafely)                         \
    ANGRY_ENTRY(handlesMenuBarHidingChangeNotificationSafely)                   \
    ANGRY_ENTRY(handlesDockRestartNotificationSafely)                           \
    ANGRY_ENTRY(handlesDockPreferenceChangeNotificationSafely)                  \
    ANGRY_ENTRY(handlesActiveDisplayChangeNotificationSafely)                   \
    ANGRY_ENTRY(handlesActiveSpaceChangeNotificationSafely)                     \
    ANGRY_ENTRY(handlesApplicationHideAndUnhideNotificationsSafely)             \
    ANGRY_ENTRY(configuresOsaxFilesystemPathsProperly)                          \
    ANGRY_ENTRY(failsToCreateOsaxDirectoryWithoutRootPrivileges)                \
    ANGRY_ENTRY(writesAndRemovesTemporaryOsaxFile)                              \
    ANGRY_ENTRY(preparesOsaxBinariesPermissionsAndSignatures)                   \
    ANGRY_ENTRY(restartsDockApplicationSafelyWithSwizzledTerminate)             \
    ANGRY_ENTRY(configuresSocketPathWhenRunningUnderSudo)                       \
    ANGRY_ENTRY(evaluatesOsaxInstallationAndVersionStatus)                      \
    ANGRY_ENTRY(removesOsaxInstallationReturningStatusCode)                     \
    ANGRY_ENTRY(handlesOsaxInstallationFailureGracefully)                       \
    ANGRY_ENTRY(failsHandshakeRequestWhenSocketServerIsAbsent)                  \
    ANGRY_ENTRY(returnsExpectedAttributesForOperatingSystemVersion)             \
    ANGRY_ENTRY(performsValidationFailingWhenSocketIsAbsent)                    \
    ANGRY_ENTRY(evaluatesSystemIntegrityProtectionCompatibility)                \
    ANGRY_ENTRY(evaluatesArm64ePreviewAbiStatus)                                \
    ANGRY_ENTRY(attemptsMachLoaderPayloadInjection)                             \
    ANGRY_ENTRY(uninstallsScriptingAdditionWithPermissionChecks)                \
    ANGRY_ENTRY(loadsScriptingAdditionVerifyingRootAndSipEnforcement)             \
    ANGRY_ENTRY(failsSendingRawBytesWhenSocketUnavailable)                      \
    ANGRY_ENTRY(dispatchesSpaceFocusAndCreationCommandsSafely)                  \
    ANGRY_ENTRY(dispatchesSpaceMoveToDisplayCommandSafely)                      \
    ANGRY_ENTRY(dispatchesWindowMoveCommandSafely)                              \
    ANGRY_ENTRY(dispatchesWindowOpacityCommandsForInstantAndFade)               \
    ANGRY_ENTRY(dispatchesWindowStickyAndFocusAndScaleCommandsSafely)           \
    ANGRY_ENTRY(dispatchesWindowSwapProxyInAndOutCommandsSafely)                \
    ANGRY_ENTRY(dispatchesWindowOrderInCommandSafely)                           \
    ANGRY_ENTRY(updatesWindowNotificationsAcrossWindowManagerWindows)           \
    ANGRY_ENTRY(updatesApplicationOpenMenusCounterAndAdjustsFfm)                \
    ANGRY_ENTRY(processesWindowDidReceiveFocusUpdatingWindowManagerState)       \
    ANGRY_ENTRY(initializesAndRunsEventLoopThreadProcessingEvents)              \
    ANGRY_ENTRY(handlesApplicationLaunchedEventForTerminatedAndRunningProcess)  \
    ANGRY_ENTRY(triggersAxRetryBlockInApplicationLaunchedEvent)                 \
    ANGRY_ENTRY(handlesApplicationTerminatedEventSafely)                        \
    ANGRY_ENTRY(handlesApplicationFrontSwitchedEventSafely)                     \
    ANGRY_ENTRY(handlesApplicationVisibilityEventsSafely)                      \
    ANGRY_ENTRY(handlesWindowCreatedEventSafely)                                \
    ANGRY_ENTRY(handlesWindowDestroyedAndFocusedEventsSafely)                   \
    ANGRY_ENTRY(handlesWindowMovedAndResizedEventsSafely)                       \
    ANGRY_ENTRY(handlesWindowMinimizedAndDeminimizedEventsSafely)               \
    ANGRY_ENTRY(handlesWindowTitleChangedEventSafely)                          \
    ANGRY_ENTRY(handlesSlsWindowOrderedAndDestroyedEventsSafely)                \
    ANGRY_ENTRY(handlesSlsSpaceCreatedAndDestroyedEventsSafely)                 \
    ANGRY_ENTRY(handlesSpaceChangedEventSafely)                                 \
    ANGRY_ENTRY(handlesDisplayLifecycleAndGeometryChangeEventsSafely)          \
    ANGRY_ENTRY(handlesMouseInteractionEventsSafely)                            \
    ANGRY_ENTRY(handlesMissionControlPresentationModeEventsSafely)              \
    ANGRY_ENTRY(handlesMissionControlEnterWithDispatchBlocks)                   \
    ANGRY_ENTRY(handlesMissionControlCheckForExitWithLegacyDispatchBlocks)      \
    ANGRY_ENTRY(handlesMissionControlExitWithFocusRestorationDispatchBlocks)    \
    ANGRY_ENTRY(handlesDockDidRestartEventSafely)                               \
    ANGRY_ENTRY(handlesMenuAndSystemStateNotificationEventsSafely)             \
    ANGRY_ENTRY(handlesDaemonSocketMessageProcessingPayload)                    \
    ANGRY_ENTRY(abortsWhenClientSendMessageReceivesNoArguments)                 \
    ANGRY_ENTRY(printsVersionAndExitsInParseArguments)                          \
    ANGRY_ENTRY(initializesSettingsAndAttemptsLockAcquisition)                  \
    ANGRY_ENTRY(validatesWindowEligibilityForManagedRules)                      \
    ANGRY_ENTRY(queriesWindowsForSpaceListAsJsonStream)                         \
    ANGRY_ENTRY(queriesWindowsForSingleDisplayAsJsonStream)                     \
    ANGRY_ENTRY(queriesWindowsForDisplaysAsJsonStream)                         \
    ANGRY_ENTRY(matchesWindowAgainstRegexRule)                                  \
    ANGRY_ENTRY(appliesManageRuleEffectsToWindow)                               \
    ANGRY_ENTRY(appliesGeneralRuleEffectsToWindow)                              \
    ANGRY_ENTRY(evaluatesManageRulesAcrossWindowManagers)                       \
    ANGRY_ENTRY(evaluatesGeneralRulesAcrossWindowManagers)                      \
    ANGRY_ENTRY(togglesGlobalWindowOpacityState)                                \
    ANGRY_ENTRY(evaluatesWhetherWindowShouldBeManaged)                          \
    ANGRY_ENTRY(adjustsWindowRatioReturningErrorOnUnmanagedWindow)              \
    ANGRY_ENTRY(movesWindowRelativelyForUnmanagedWindow)                        \
    ANGRY_ENTRY(resizesWindowRelativeInternalWithAnimation)                     \
    ANGRY_ENTRY(resizesWindowRelativeHandlingUnmanagedWindow)                   \
    ANGRY_ENTRY(movesWindowDirectlyWithNullReference)                           \
    ANGRY_ENTRY(resizesWindowDirectlyWithNullReference)                          \
    ANGRY_ENTRY(animatesWindowListAsynchronouslyWithEmptyList)                  \
    ANGRY_ENTRY(setsWindowPurifyModeAndIteratesWindows)                         \
    ANGRY_ENTRY(setsWindowOpacityDirectly)                                      \
    ANGRY_ENTRY(setsMenubarOpacityDirectly)                                     \
    ANGRY_ENTRY(setsActiveWindowOpacityDirectly)                                \
    ANGRY_ENTRY(setsNormalWindowOpacityDirectly)                                \
    ANGRY_ENTRY(setsWindowLayerAndAssociatedSublayers)                         \
    ANGRY_ENTRY(findsRankOfWindowInWindowList)                                  \
    ANGRY_ENTRY(findsWindowAtPointFilteringSpecifiedWindow)                     \
    ANGRY_ENTRY(notifiesJankyBordersSafely)                                     \
    ANGRY_ENTRY(createsWindowProxySkippingNullImage)                            \
    ANGRY_ENTRY(destroysWindowProxyCleaningUpResources)                         \
    ANGRY_ENTRY(buildsWindowProxyInThreadProcSafely)                            \
    ANGRY_ENTRY(processesWindowAnimationListStepInThreadProc)                  \
    ANGRY_ENTRY(findsClosestManagedWindowInDirection)                          \
    ANGRY_ENTRY(findsPrevManagedWindowOnActiveSpace)                            \
    ANGRY_ENTRY(findsNextManagedWindowOnActiveSpace)                            \
    ANGRY_ENTRY(findsFirstManagedWindowOnActiveSpace)                           \
    ANGRY_ENTRY(findsLastManagedWindowOnActiveSpace)                            \
    ANGRY_ENTRY(findsRecentManagedWindowUsingLastWindowId)                      \
    ANGRY_ENTRY(findsPrevWindowInStackReturningNull)                            \
    ANGRY_ENTRY(findsNextWindowInStackReturningNull)                            \
    ANGRY_ENTRY(findsFirstWindowInStackReturningNull)                           \
    ANGRY_ENTRY(findsLastWindowInStackReturningNull)                            \
    ANGRY_ENTRY(findsRecentWindowInStackReturningNull)                          \
    ANGRY_ENTRY(findsWindowInStackByIndexReturningNull)                         \
    ANGRY_ENTRY(findsLargestManagedWindowOnActiveSpace)                          \
    ANGRY_ENTRY(findsSmallestManagedWindowOnActiveSpace)                         \
    ANGRY_ENTRY(findsSiblingForManagedWindowReturningNull)                      \
    ANGRY_ENTRY(findsFirstNephewForManagedWindowReturningNull)                  \
    ANGRY_ENTRY(findsSecondNephewForManagedWindowReturningNull)                 \
    ANGRY_ENTRY(findsUncleForManagedWindowReturningNull)                        \
    ANGRY_ENTRY(findsFirstCousinForManagedWindowReturningNull)                  \
    ANGRY_ENTRY(findsSecondCousinForManagedWindowReturningNull)                 \
    ANGRY_ENTRY(focusesWindowWithoutRaiseSynthesizingEvents)                    \
    ANGRY_ENTRY(focusesWindowWithRaiseActivatingProcess)                        \
    ANGRY_ENTRY(tracksLostFrontSwitchedEventInTable)                            \
    ANGRY_ENTRY(tracksLostFocusedEventInTable)                                  \
    ANGRY_ENTRY(addsAndRemovesWindowFromTable)                                  \
    ANGRY_ENTRY(addsAndRemovesApplicationFromTable)                             \
    ANGRY_ENTRY(findsWindowsBelongingToApplication)                             \
    ANGRY_ENTRY(makesKeyWindowPostingEventRecords)                              \
    ANGRY_ENTRY(retrievesExistingApplicationWindowList)                         \
    ANGRY_ENTRY(createsAndAddsWindowHandlingUnknownWindow)                      \
    ANGRY_ENTRY(addsApplicationWindowsSafely)                                   \
    ANGRY_ENTRY(addsExistingApplicationWindowsSafely)                           \
    ANGRY_ENTRY(setsWindowInsertionPointDirection)                             \
    ANGRY_ENTRY(stacksWindowReturningErrorOnSelfStack)                          \
    ANGRY_ENTRY(warpsWindowReturningErrorOnSelfWarp)                            \
    ANGRY_ENTRY(swapsWindowReturningErrorOnSelfSwap)                            \
    ANGRY_ENTRY(minimizesWindowCheckingCapabilities)                            \
    ANGRY_ENTRY(deminimizesWindowCheckingMinimizedState)                        \
    ANGRY_ENTRY(closesWindowReturningFailureOnNullButton)                       \
    ANGRY_ENTRY(sendsWindowToSpaceHandlingSameSpace)                            \
    ANGRY_ENTRY(appliesGridToWindowHandlingInvalidView)                         \
    ANGRY_ENTRY(makesWindowFloatingHandlingIneligibleWindow)                    \
    ANGRY_ENTRY(makesWindowStickyHandlingIneligibleWindow)                      \
    ANGRY_ENTRY(togglesWindowShadowDirectly)                                    \
    ANGRY_ENTRY(waitsForNativeFullscreenTransitionOnWindow)                    \
    ANGRY_ENTRY(togglesWindowNativeFullscreenSafely)                            \
    ANGRY_ENTRY(togglesWindowZoomParentOnWindow)                                \
    ANGRY_ENTRY(togglesWindowZoomFullscreenOnWindow)                            \
    ANGRY_ENTRY(togglesWindowWindowedFullscreenOnWindow)                        \
    ANGRY_ENTRY(togglesWindowExposeDirectly)                                    \
    ANGRY_ENTRY(togglesWindowPictureInPictureDirectly)                          \
    ANGRY_ENTRY(togglesScratchpadWindowByLabelReturningFalse)                   \
    ANGRY_ENTRY(togglesScratchpadWindowDirectly)                                \
    ANGRY_ENTRY(setsAndRemovesScratchpadForWindow)                              \
    ANGRY_ENTRY(findsScratchpadWindowByLabel)                                   \
    ANGRY_ENTRY(recoversScratchpadWindowsSafely)                                \
    ANGRY_ENTRY(correctsForMissionControlChangesSafely)                         \
    ANGRY_ENTRY(beginsWindowManagerLifecyleObservingWindows)

static struct {
    char *name;
    test_function *func;
} tests[] = {
    TEST_LIST
};

int main(int argc, char **argv)
{
    ts_init(MEGABYTES(8));
    g_connection = SLSMainConnectionID();
    CGSGetConnectionPortById = macho_find_symbol("/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/SkyLight", "_CGSGetConnectionPortById");
    g_layer_below_window_level  = CGWindowLevelForKey(LAYER_BELOW);
    g_layer_normal_window_level = CGWindowLevelForKey(LAYER_NORMAL);
    g_layer_above_window_level  = CGWindowLevelForKey(LAYER_ABOVE);
    workspace_event_handler_begin(&g_workspace_context);
    mouse_state_init(&g_mouse_state);
    window_manager_init(&g_window_manager);
    display_manager_begin(&g_display_manager);
    space_manager_begin(&g_space_manager);
    memory_pool_init(&g_signal_storage, KILOBYTES(256));
    memory_pool_init(&g_event_loop.pool, KILOBYTES(512));
    g_event_loop.semaphore = sem_open("yabai_event_loop_test_sem", O_CREAT, 0600, 0);
    sem_unlink("yabai_event_loop_test_sem");
    g_event_loop.head = memory_pool_push(&g_event_loop.pool, sizeof(struct event));
    g_event_loop.head->next = NULL;
    g_event_loop.tail = g_event_loop.head;

    int succeeded = 0;
    int failed = 0;
    int slow = 0;
    int total = array_count(tests);
    printf("\e[1;34m -- Running %d Angry Tests (<50ms/test standard)\e[m\n\n", total);

    uint64_t cpu_freq  = read_cpu_freq();
    uint64_t begin_tsc = read_cpu_timer();

    for (int i = 0; i < total; ++i) {
        uint64_t tsc = read_cpu_timer();
        bool result = tests[i].func();
        double ms_elapsed = 1000.0 * (double)(read_cpu_timer() - tsc) / (double)cpu_freq;

        bool is_slow = ms_elapsed > 50.0;
        if (is_slow) ++slow;

        printf("(%0.4fms) %s \e[1;33m%s\e[m%s\n",
               ms_elapsed,
               result ? "\e[1;32msuccess\e[m" : " \e[1;31mfailed\e[m",
               tests[i].name,
               is_slow ? " \e[1;35m[SLOW > 50ms]\e[m" : "");
        if (result) ++succeeded; else ++failed;
    }

    double ms_elapsed = 1000.0 * (double)(read_cpu_timer() - begin_tsc) / (double)cpu_freq;
    printf("\n\e[1;34m -- Completed (%0.4fms)\e[m\n", ms_elapsed);
    printf("\t%d \e[1;32msucceeded\e[m\n", succeeded);
    printf("\t%d \e[1;31mfailed\e[m\n", failed);
    if (slow > 0) {
        printf("\t%d \e[1;35mslow (>50ms)\e[m\n", slow);
    }
    printf("\t%d \e[1;33mtotal\e[m\n", total);

    return total == succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
}
