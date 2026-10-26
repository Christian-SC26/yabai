unsigned char __src_osax_payload[1];
unsigned int __src_osax_payload_len;
unsigned char __src_osax_loader[1];
unsigned int __src_osax_loader_len;

#include "../../src/manifest.m"

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

#include "area.c"
#include "structures.c"
#include "angry_tests.c"

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
    ANGRY_ENTRY(resolvesActiveSpaceForRunningApplications)

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
    mouse_state_init(&g_mouse_state);
    window_manager_init(&g_window_manager);
    display_manager_begin(&g_display_manager);
    space_manager_begin(&g_space_manager);

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
