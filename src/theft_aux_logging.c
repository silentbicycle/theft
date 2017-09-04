#include "theft_types_internal.h"

#include <assert.h>

int theft_get_loglevel(struct theft *t) {
    assert(t);
    return t->log.level;
}

void theft_set_loglevel(struct theft *t, int level) { 
    assert(t);
    t->log.level = level;
}

FILE *theft_get_logfile(struct theft *t) {
    assert(t);
    return t->log.file;
}

void theft_set_logfile(struct theft *t, FILE *f) {
    assert(t);
    t->log.file = f;
}

void theft_log_printed(struct theft *t) {
    assert(t);
    t->log.printed = true;
}
