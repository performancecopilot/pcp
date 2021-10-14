#include "jsonsl.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include "all-tests.h"


/* "actual" must have at least all the same bits set as "expected" */
static void
check_flags (int actual, int expected)
{
    int i;

    for (i = 0; i < (int)(sizeof actual); i++) {
        if (expected & (1 << i)) {
            if (!(actual & (1 << i))) {
                fprintf(stderr, "bit %d not set in special_flags\n", i);
                abort();
            }
        }
    }
}


static void
special_flags_test_pop_callback (jsonsl_t jsn,
                                 jsonsl_action_t action,
                                 struct jsonsl_state_st *state,
                                 const char *buf)
{
    jsonsl_special_t flags = (jsonsl_special_t) jsn->data;
    if (state->type == JSONSL_T_SPECIAL) {
        check_flags (state->special_flags, flags);
    }
}


int
main (int argc, char **argv)
{
    typedef struct {
       const char *value;
       jsonsl_special_t expected_special_flags;
    } special_flags_test_t;

    special_flags_test_t tests[] = {
       { "1", JSONSL_SPECIALf_UNSIGNED },
       { "1.0", JSONSL_SPECIALf_FLOAT|JSONSL_SPECIALf_UNSIGNED },
       { "0", JSONSL_SPECIALf_UNSIGNED },
       { "0.0", JSONSL_SPECIALf_FLOAT|JSONSL_SPECIALf_UNSIGNED },
       { "-0.0", JSONSL_SPECIALf_FLOAT|JSONSL_SPECIALf_SIGNED },
       { NULL }
    };

    special_flags_test_t *test;
    jsonsl_t jsn;
    char name[512];
    char formatted_json[512];
    int formatted_len;

    for (test = tests; test->value; test++) {
        snprintf (name, sizeof name, "parse \"%s\"", test->value);
        fprintf (stderr, "==== %-40s ====\n", name);
        formatted_len = snprintf (formatted_json,
                                  sizeof formatted_json,
                                  "{\"x\": %s}",
                                  test->value);

        jsn = jsonsl_new(0x2000);
        jsn->data = (void *) test->expected_special_flags;
        jsn->action_callback_POP = special_flags_test_pop_callback;
        jsonsl_enable_all_callbacks (jsn);

        jsonsl_feed (jsn, formatted_json, (size_t) formatted_len);
        jsonsl_destroy (jsn);
    }

    return 0;
}
