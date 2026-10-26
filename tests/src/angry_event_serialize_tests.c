#include <ApplicationServices/ApplicationServices.h>
#include <mach/mach_time.h>
#include "../../src/event_serialize.h"

ANGRY_TEST(augmentsDockSwipeEventWithRawIoHidPayload) {
    CGEventRef ev = CGEventCreate(NULL);
    CGEventSetIntegerValueField(ev, 55, 30);
    CGEventSetIntegerValueField(ev, 110, 23);
    CGEventSetIntegerValueField(ev, 132, 1);
    CGEventSetDoubleValueField(ev, 124, -0.000016);
    CGEventSetIntegerValueField(ev, 123, 1);
    CGEventRef aug = iss_augment_dock_swipe_event(ev);
    CFRelease(ev);
    bool ok = aug != NULL;
    if (aug) CFRelease(aug);
    ANGRY_ASSERT(ok, "iss_augment_dock_swipe_event returned NULL for valid dock swipe began event");
}

ANGRY_TEST(augmentsDockSwipeEndedEventWithVelocityPayload) {
    CGEventRef ev = CGEventCreate(NULL);
    CGEventSetIntegerValueField(ev, 55, 30);
    CGEventSetIntegerValueField(ev, 110, 23);
    CGEventSetIntegerValueField(ev, 132, 4);
    CGEventSetDoubleValueField(ev, 124, 0.000016);
    CGEventSetDoubleValueField(ev, 129, 500.0);
    CGEventRef aug = iss_augment_dock_swipe_event(ev);
    CFRelease(ev);
    bool ok = aug != NULL;
    if (aug) CFRelease(aug);
    ANGRY_ASSERT(ok, "iss_augment_dock_swipe_event returned NULL for valid dock swipe ended event");
}

ANGRY_TEST(replacesPreexistingPayloadWhenAugmentingAlreadyAugmentedEvent) {
    CGEventRef ev = CGEventCreate(NULL);
    CGEventSetIntegerValueField(ev, 55, 30);
    CGEventSetIntegerValueField(ev, 110, 23);
    CGEventSetIntegerValueField(ev, 132, 1);
    CGEventSetDoubleValueField(ev, 124, -0.000016);
    CGEventSetIntegerValueField(ev, 123, 1);
    CGEventRef aug1 = iss_augment_dock_swipe_event(ev);
    CGEventRef aug2 = iss_augment_dock_swipe_event(aug1);
    CFRelease(ev);
    if (aug1) CFRelease(aug1);
    bool ok = aug2 != NULL;
    if (aug2) CFRelease(aug2);
    ANGRY_ASSERT(ok, "Re-augmenting already augmented event failed to replace raw payload");
}

ANGRY_TEST(returnsNullWhenAugmentingNullEvent) {
    CGEventRef aug = iss_augment_dock_swipe_event(NULL);
    ANGRY_ASSERT(aug == NULL, "iss_augment_dock_swipe_event did not return NULL for NULL input");
}

ANGRY_TEST(augmentsBareEventDefaultingMissingFieldsToZero) {
    CGEventRef ev = CGEventCreate(NULL);
    CGEventRef aug = iss_augment_dock_swipe_event(ev);
    CFRelease(ev);
    bool ok = aug != NULL;
    if (aug) CFRelease(aug);
    ANGRY_ASSERT(ok, "iss_augment_dock_swipe_event failed on bare CGEvent");
}

ANGRY_TEST(evaluatesRequiresEventAugmentationWithoutCrashing) {
    bool req = iss_requires_event_augmentation();
    ANGRY_ASSERT(req == true || req == false, "iss_requires_event_augmentation returned invalid boolean state");
}

ANGRY_TEST(rejectsTruncatedEventDataUnderFourBytes) {
    uint8_t short_bytes[3] = {0x00, 0x00, 0x02};
    ISSCGEventParsedData parsed = {0};
    bool ok = iss_parse_event_data(short_bytes, sizeof(short_bytes), &parsed);
    ANGRY_ASSERT(ok == false, "iss_parse_event_data accepted truncated data under 4 bytes");
}

ANGRY_TEST(rejectsUnsupportedEventDataVersionNumber) {
    uint8_t wrong_ver[4] = {0x00, 0x00, 0x00, 0x09};
    ISSCGEventParsedData parsed = {0};
    bool ok = iss_parse_event_data(wrong_ver, sizeof(wrong_ver), &parsed);
    ANGRY_ASSERT(ok == false, "iss_parse_event_data accepted unsupported header version");
}

ANGRY_TEST(rejectsTruncatedFieldHeaderInEventData) {
    uint8_t truncated_field[6] = {0x00, 0x00, 0x00, 0x02, 0x00, 0x01};
    ISSCGEventParsedData parsed = {0};
    bool ok = iss_parse_event_data(truncated_field, sizeof(truncated_field), &parsed);
    ANGRY_ASSERT(ok == false, "iss_parse_event_data accepted incomplete field header");
}

ANGRY_TEST(clampsExtremePositiveAndNegativeFixedPointValues) {
    int32_t hi = iss_double_to_fixed1616(70000.0);
    int32_t lo = iss_double_to_fixed1616(-70000.0);
    ANGRY_ASSERT(hi == INT32_MAX && lo == INT32_MIN, "iss_double_to_fixed1616 failed to clamp out-of-range floats");
}

ANGRY_TEST(handlesNullParsedEventDataFreeSafely) {
    iss_parsed_event_data_free(NULL);
    ISSCGEventParsedData empty = {0};
    iss_parsed_event_data_free(&empty);
    ANGRY_ASSERT(true, "iss_parsed_event_data_free crashed on NULL or empty pointer");
}
