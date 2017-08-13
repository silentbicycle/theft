#include "test_theft.h"
#include "theft_bloom.h"

TEST all_marked_should_remain_marked(size_t limit) {
    struct theft_bloom *b = theft_bloom_init(NULL);

    char buf[32];
    for (size_t i = 0; i < limit; i++) {
        size_t used = snprintf(buf, sizeof(buf), "key%zd\n", i);
        assert(used < sizeof(buf));
        ASSERTm("marking should not fail",
            theft_bloom_mark(b, (uint8_t *)buf, used));
    }

    for (size_t i = 0; i < limit; i++) {
        size_t used = snprintf(buf, sizeof(buf), "key%zd\n", i);
        assert(used < sizeof(buf));
        ASSERTm("marked became unmarked",
            theft_bloom_check(b, (uint8_t *)buf, used));
    }

    theft_bloom_free(b);
    PASS();
}

SUITE(bloom) {
    RUN_TESTp(all_marked_should_remain_marked, 10);
    RUN_TESTp(all_marked_should_remain_marked, 1000);
    RUN_TESTp(all_marked_should_remain_marked, 100000);
}
