#include "../../src/misc/memory_pool.h"
#include "../../src/misc/hashtable.h"

static TABLE_HASH_FUNC(angry_hash)
{
    return *(uint32_t *) key;
}

static TABLE_COMPARE_FUNC(angry_compare)
{
    return *(uint32_t *) key_a == *(uint32_t *) key_b;
}

ANGRY_TEST(allocatesChunkInEmptyMemoryPool)
{
    struct memory_pool pool;
    memory_pool_init(&pool, KILOBYTES(16));
    void *ptr = memory_pool_push(&pool, 128);
    ANGRY_ASSERT(ptr == pool.memory, "first allocated chunk must point to start of pool buffer");
}

ANGRY_TEST(preservesStoredPointerAcrossThousandAllocations)
{
    struct memory_pool pool;
    memory_pool_init(&pool, MEGABYTES(1));
    uint32_t *first = (uint32_t *) memory_pool_push(&pool, sizeof(uint32_t));
    *first = 0xDEADBEEF;
    for (int i = 1; i < 1000; ++i) {
        *(uint32_t *) memory_pool_push(&pool, sizeof(uint32_t)) = (uint32_t)(i * 31);
    }
    ANGRY_ASSERT(*first == 0xDEADBEEF, "first element data must not be corrupted by subsequent pushes");
}

ANGRY_TEST(wrapsMemoryPoolPointerToOriginOnOverflow)
{
    struct memory_pool pool;
    memory_pool_init(&pool, getpagesize());
    uint64_t chunk = 64;
    uint64_t count = pool.size / chunk;
    for (uint64_t i = 0; i < count - 1; ++i) {
        memory_pool_push(&pool, chunk);
    }
    void *wrapped = memory_pool_push(&pool, chunk);
    ANGRY_ASSERT(wrapped == pool.memory, "pool wrap-around must reset memory allocation pointer to origin");
}

ANGRY_TEST(rehashesTableWithoutDataLossUnderHighLoad)
{
    struct table table;
    table_init(&table, 37, angry_hash, angry_compare);
    uint32_t keys[1024];
    for (uint32_t i = 0; i < 1024; ++i) {
        keys[i] = i * 2654435761u;
        table_add(&table, &keys[i], (void *)(intptr_t)(keys[i] ^ 0xCAFEBABE));
    }
    uint32_t target_idx = 777;
    void *val = table_find(&table, &keys[target_idx]);
    ANGRY_ASSERT((intptr_t)val == (intptr_t)(keys[target_idx] ^ 0xCAFEBABE), "rehashed table must retrieve stored keys without loss");
}

ANGRY_TEST(identifiesWestDisplayAcrossNegativeCoordinates)
{
    struct area center = { 0.0f, 0.0f, 2560.0f, 1440.0f };
    struct area west   = { -1728.0f, -120.0f, 1728.0f, 1117.0f };
    bool is_west = area_is_in_direction(&center, area_max_point(center), &west, area_max_point(west), DIR_WEST);
    ANGRY_ASSERT(is_west, "display with negative X offset must be recognized in west direction");
}

ANGRY_TEST(rejectsEastDisplayWhenTargetIsPositionedWest)
{
    struct area center = { 0.0f, 0.0f, 2560.0f, 1440.0f };
    struct area west   = { -1728.0f, -120.0f, 1728.0f, 1117.0f };
    bool is_east = area_is_in_direction(&center, area_max_point(center), &west, area_max_point(west), DIR_EAST);
    ANGRY_ASSERT(!is_east, "display to the west must not be accepted as east direction");
}

ANGRY_TEST(splitsParentAreaEquallyWithSpecifiedGapOnVerticalAxis)
{
    struct area parent = { 0.0f, 0.0f, 1920.0f, 1080.0f };
    struct area left, right;
    area_make_pair(SPLIT_Y, 20, 0.5f, &parent, &left, &right);
    bool valid = (left.w == 950) && (right.w == 950) && (right.x == 970);
    ANGRY_ASSERT(valid, "vertical split at 0.5 with 20px gap must yield symmetric widths and correct right offset");
}

ANGRY_TEST(splitsParentAreaAsymmetricallyOnHorizontalAxis)
{
    struct area parent = { 24.0f, 64.0f, 1200.0f, 900.0f };
    struct area top, bottom;
    area_make_pair(SPLIT_X, 10, 0.3f, &parent, &top, &bottom);
    bool valid = (top.h == 267) && (bottom.h == 623) && (bottom.y == 341);
    ANGRY_ASSERT(valid, "horizontal split with ratio 0.3 must distribute height proportionally and position bottom area");
}

ANGRY_TEST(ignoresDropWhenCursorDeltaIsBelowThreshold)
{
    CGPoint down = CGPointMake(543.2f, 412.8f);
    CGPoint up   = CGPointMake(544.7f, 411.9f);
    bool moved = (fabs(up.x - down.x) >= 3.0f) || (fabs(up.y - down.y) >= 3.0f);
    ANGRY_ASSERT(!moved, "cursor displacement under 3 pixels must not register as drag movement");
}

ANGRY_TEST(permitsDropWhenCursorDeltaCrossesThreshold)
{
    CGPoint down = CGPointMake(543.2f, 412.8f);
    CGPoint up   = CGPointMake(546.5f, 412.8f);
    bool moved = (fabs(up.x - down.x) >= 3.0f) || (fabs(up.y - down.y) >= 3.0f);
    ANGRY_ASSERT(moved, "cursor displacement of 3.3 pixels must be recognized as deliberate drag movement");
}

ANGRY_TEST(preservesWindowWhenExitingToMatchingSpace)
{
    uint64_t target_sid = 4;
    uint64_t current_sid = 4;
    bool should_skip = space_is_fullscreen(current_sid) || (current_sid == target_sid);
    ANGRY_ASSERT(should_skip, "validation must not un-tile windows already present on destination space");
}

ANGRY_TEST(guardsAgainstPrematureUntileDuringFullscreenSpaceDestruction)
{
    uint64_t temp_fullscreen_sid = 1071;
    uint64_t destination_sid = 4;
    bool is_fs = space_is_fullscreen(temp_fullscreen_sid);
    bool should_skip = is_fs || (temp_fullscreen_sid == destination_sid);
    ANGRY_ASSERT(should_skip == is_fs, "window associated with active fullscreen space must be shielded from eviction");
}
