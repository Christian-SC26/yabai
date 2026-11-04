#include <ApplicationServices/ApplicationServices.h>
#include "../../src/display.h"
#include "../../src/space.h"

ANGRY_TEST(postsDisplayAddedEventCarryingDisplayId) {
    display_handler(7001, kCGDisplayAddFlag, NULL);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT(e->type == DISPLAY_ADDED && (uint32_t)(intptr_t)e->context == 7001, "expected a DISPLAY_ADDED event for the display to be posted");
}

ANGRY_TEST(postsDisplayRemovedEventCarryingDisplayId) {
    display_handler(7002, kCGDisplayRemoveFlag, NULL);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT(e->type == DISPLAY_REMOVED && (uint32_t)(intptr_t)e->context == 7002, "expected a DISPLAY_REMOVED event for the display to be posted");
}

ANGRY_TEST(postsDisplayMovedEventCarryingDisplayId) {
    display_handler(7003, kCGDisplayMovedFlag, NULL);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT(e->type == DISPLAY_MOVED && (uint32_t)(intptr_t)e->context == 7003, "expected a DISPLAY_MOVED event for the display to be posted");
}

ANGRY_TEST(postsDisplayResizedEventCarryingDisplayId) {
    display_handler(7004, kCGDisplayDesktopShapeChangedFlag, NULL);
    struct event *e = angry_last_posted_event();
    ANGRY_ASSERT(e->type == DISPLAY_RESIZED && (uint32_t)(intptr_t)e->context == 7004, "expected a DISPLAY_RESIZED event for the display to be posted");
}

ANGRY_TEST(serializesDisplayPropertiesToJsonStream) {
    char buf[4096] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    uint32_t did = display_manager_main_display_id();
    display_serialize(f, did, 0);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "\"id\":") && strstr(buf, "\"uuid\":") && strstr(buf, "\"frame\":"), "display_serialize failed to emit complete JSON");
}

ANGRY_TEST(serializesIndividualDisplayPropertyFields) {
    char buf[1024] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    uint32_t did = display_manager_main_display_id();
    display_serialize(f, did, DISPLAY_PROPERTY_ID | DISPLAY_PROPERTY_FRAME | DISPLAY_PROPERTY_HAS_FOCUS);
    fclose(f);
    ANGRY_ASSERT(strstr(buf, "\"id\":") && strstr(buf, "\"has-focus\":"), "display_serialize failed to emit selected fields");
}

ANGRY_TEST(countsSpacesForActiveDisplay) {
    uint32_t did = display_manager_main_display_id();
    int count = display_space_count(did);
    ANGRY_ASSERT(count > 0, "display_space_count failed to count spaces on active display");
}

ANGRY_TEST(returnsZeroSpacesForInvalidDisplayId) {
    int count = display_space_count(0xFFFFFFFF);
    ANGRY_ASSERT(count == 0, "display_space_count returned non-zero for invalid display");
}

ANGRY_TEST(resolvesDisplayBoundsConstrainedWithExternalBarPadding) {
    uint32_t did = display_manager_main_display_id();
    g_display_manager.mode = EXTERNAL_BAR_MAIN;
    g_display_manager.top_padding = 20;
    g_display_manager.bottom_padding = 10;
    CGRect r = display_bounds_constrained(did, false);
    g_display_manager.mode = EXTERNAL_BAR_OFF;
    g_display_manager.top_padding = 0;
    g_display_manager.bottom_padding = 0;
    ANGRY_ASSERT(r.size.width > 0 && r.size.height > 0, "display_bounds_constrained failed with external bar");
}

ANGRY_TEST(convertsDisplayIdToUuidAndBack) {
    uint32_t did = display_manager_main_display_id();
    CFStringRef uuid = display_uuid(did);
    uint32_t resolved_did = display_id(uuid);
    CFRelease(uuid);
    ANGRY_ASSERT(resolved_did == did, "display_id failed to resolve did from uuid");
}

ANGRY_TEST(identifiesSystemSpaceAccurately) {
    bool sys = space_is_system(0);
    ANGRY_ASSERT(sys == false, "space_is_system returned true for space 0");
}

ANGRY_TEST(evaluatesSpaceVisibilityForActiveSpace) {
    uint64_t sid = space_manager_active_space();
    bool vis = space_is_visible(sid);
    ANGRY_ASSERT(vis == true, "active space must be reported as visible");
}
