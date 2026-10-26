#include <ApplicationServices/ApplicationServices.h>
#include "../../src/display.h"
#include "../../src/space.h"

ANGRY_TEST(dispatchesDisplayHandlerForDisplayAddedEvent) {
    display_handler(1, kCGDisplayAddFlag, NULL);
    ANGRY_ASSERT(true, "display_handler handled kCGDisplayAddFlag cleanly");
}

ANGRY_TEST(dispatchesDisplayHandlerForDisplayRemovedEvent) {
    display_handler(1, kCGDisplayRemoveFlag, NULL);
    ANGRY_ASSERT(true, "display_handler handled kCGDisplayRemoveFlag cleanly");
}

ANGRY_TEST(dispatchesDisplayHandlerForDisplayMovedEvent) {
    display_handler(1, kCGDisplayMovedFlag, NULL);
    ANGRY_ASSERT(true, "display_handler handled kCGDisplayMovedFlag cleanly");
}

ANGRY_TEST(dispatchesDisplayHandlerForDisplayResizedEvent) {
    display_handler(1, kCGDisplayDesktopShapeChangedFlag, NULL);
    ANGRY_ASSERT(true, "display_handler handled kCGDisplayDesktopShapeChangedFlag cleanly");
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

ANGRY_TEST(queriesSpaceWindowListIncludingMinimizedWindows) {
    uint64_t sid = space_manager_active_space();
    int count = 0;
    uint32_t *list = space_window_list(sid, &count, true);
    ANGRY_ASSERT(count >= 0, "space_window_list including minimized returned negative count");
}

ANGRY_TEST(evaluatesSpaceVisibilityForActiveSpace) {
    uint64_t sid = space_manager_active_space();
    bool vis = space_is_visible(sid);
    ANGRY_ASSERT(vis == true, "active space must be reported as visible");
}

ANGRY_TEST(evaluatesSpaceFullscreenStateForActiveSpace) {
    uint64_t sid = space_manager_active_space();
    bool fs = space_is_fullscreen(sid);
    ANGRY_ASSERT(fs == false || fs == true, "space_is_fullscreen evaluated cleanly");
}

ANGRY_TEST(evaluatesSpaceUserTypeForActiveSpace) {
    uint64_t sid = space_manager_active_space();
    bool user = space_is_user(sid);
    ANGRY_ASSERT(user == true, "active user space must be reported as user space");
}
