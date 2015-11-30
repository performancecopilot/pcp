/*
 * Copyright (c) 2015 Red Hat.  GPL2+.
 *
 * pmFetchGroup* testing
 */

/* More entertaining to run with env PCP_DEBUG=56 */

#include <pcp/pmapi.h>
#include <assert.h>
#include <stdio.h>


void
__pcp_assert(int sts, const char *FILE, int LINE)
{
    if (sts < 0) {
        fprintf(stderr, "ERROR %s:%d: sts %d (%s)\n",
                FILE, LINE, sts, pmErrStr(sts));
        exit (1);
    }
}
#define pcp_assert(s) __pcp_assert((s),__FILE__,__LINE__)


void
test_indoms ()
{
    int sts;
    pmFG fg;
    enum { almost_bins = 5 }; /* fewer than the sample.*.bin instances */
    pmAtomValue values[almost_bins];
    int values_stss[almost_bins];
    int values_inst_codes[almost_bins];
    char *values_inst_names[almost_bins];
    int values_sts;
    int i;
    
    sts = pmCreateFetchGroup(& fg);
    pcp_assert(sts);
    assert (fg != NULL);

    sts = pmExtendFetchGroup_indom(fg, "sample.bogus_bin", "rate",
                                   values_inst_codes, values_inst_names,
                                   values, PM_TYPE_32,
                                   values_stss, almost_bins, NULL,
                                   &values_sts);
    pcp_assert(sts);

    for (i=0; i<3; i++) {
        int j;
        sts = pmFetchGroup(fg);
        pcp_assert(sts);
        assert (values_sts == PM_ERR_TOOBIG); /* 5 < 9 */
        for (j=0; j<almost_bins; j++) {
            pcp_assert((i>0) ^ (values_stss == 0));
            if (i>0) assert(values[j].l == 0);
            assert (values_inst_codes[j] >= 0);
            /* validate bogus indom names */
            if (values_inst_codes[j] % 100)
                assert (values_inst_names[j] == NULL);
            else
                /* bin-XXX */
                assert (atoi(values_inst_names[j]+4) ==
                        values_inst_codes[j]);
        }
    }
    
    sts = pmDestroyFetchGroup(fg);
    pcp_assert(sts);
}



void
test_counter ()
{
    int sts;
    pmFG fg;
    pmAtomValue rapid_counter, rapid_counter2, rapid_counter3;
    int rapid_counter_sts, rapid_counter2_sts;
    pmAtomValue constant_rate_counter_values[2];
    int constant_rate_counter_codes[2];
    char *constant_rate_counter_names[2];
    int constant_rate_counter_stss[2];
    int constant_rate_counter_sts;
    unsigned constant_rate_counter_count;
    unsigned i;
    
    sts = pmCreateFetchGroup(& fg);
    pcp_assert(sts);
    assert (fg != NULL);

    sts = pmExtendFetchGroup_item(fg, "sample.rapid", NULL, NULL,
                                  &rapid_counter, PM_TYPE_FLOAT, &rapid_counter_sts);
    pcp_assert(sts);
    sts = pmExtendFetchGroup_item(fg, "sample.rapid", NULL, "instant",
                                  &rapid_counter2, PM_TYPE_U32, &rapid_counter2_sts);
    pcp_assert(sts);
    sts = pmExtendFetchGroup_item(fg, "sample.rapid", NULL, "instant",
                                  &rapid_counter3, PM_TYPE_DOUBLE, NULL);
    pcp_assert(sts);

    sts = pmExtendFetchGroup_indom(fg, "sample.const_rate.value", "count/24 hours",
                                   constant_rate_counter_codes, constant_rate_counter_names,
                                   constant_rate_counter_values, PM_TYPE_64,
                                   constant_rate_counter_stss, 2, &constant_rate_counter_count,
                                   &constant_rate_counter_sts);
    pcp_assert(sts);

    for (i=0; i<3; i++) {
        sts = pmFetchGroup(fg);
        pcp_assert (sts);
        assert (rapid_counter2_sts == 0);
        assert (constant_rate_counter_count == 1); /* only one instance */
        assert ((i > 0) ^ (rapid_counter_sts < 0));
        assert ((i > 0) ^ (constant_rate_counter_stss[0] < 0));
        assert ((constant_rate_counter_stss[1] < 0));
        /* ticking at 10Hz, expect 864000-ish "count/24 hours" */
        if (i > 0) assert (constant_rate_counter_values[0].ll >  800000 &&
                           constant_rate_counter_values[0].ll < 1000000 );
        /* doubles can exactly represent full 32-bit uint range */
        assert (rapid_counter2.ul == rapid_counter3.d);
        /* assert (rapid_counter.f > 0.0); ... but wraparound can make rate < 0.0 */
        sleep (3);
    }
    
    sts = pmDestroyFetchGroup(fg);
    pcp_assert(sts);
}





int
main()
{
    int ctx, sts;
    
    ctx = pmNewContext(PM_CONTEXT_HOST, "local:");
    pcp_assert(ctx);
    test_counter();
    test_indoms();
    sts = pmDestroyContext(ctx); 
    pcp_assert(sts);

#if 0
    ctx = pmNewContext(PM_CONTEXT_ARCHIVE, "archives/kenj-pc-1");
    pcp_assert (ctx);
    test ();
    sts = pmDestroyContext (ctx); 
    pcp_assert (sts);
#endif

    printf("complete\n");

    return 0;
}
