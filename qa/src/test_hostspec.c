/*
 * Copyright (c) 2021 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

static void test_hostspec_unparse(void **state)
{
    __pmHostSpec	hosts[] = {{.name = "pcphost"}};
    __pmHashCtl		attrs;
    char		buf[512];
    int bytes;

    __pmHashInit(&attrs);
    __pmHashAdd(PCP_ATTR_USERNAME, strdup("max"), &attrs);
    __pmHashAdd(PCP_ATTR_PASSWORD, strdup("very$secret&complex?passwÖrd="), &attrs);
    bytes = __pmUnparseHostAttrsSpec(hosts, 1, &attrs, buf, sizeof(buf));
    assert_return_code(bytes, 0);
    assert_string_equal(buf, "pcphost?password=very%24secret%26complex%3Fpassw%C3%96rd%3D&username=max");
    __pmFreeAttrsSpec(&attrs);
    __pmHashClear(&attrs);
}

static void test_hostspec_parse(void **state)
{
    __pmHostSpec	*hosts = NULL;
    int			nhosts;
    __pmHashCtl		attrs;
    __pmHashNode	*node;
    int			sts;
    char		*errmsg;

    __pmHashInit(&attrs);
    sts = __pmParseHostAttrsSpec("pcphost?password=very%24secret%26complex%3Fpassw%C3%96rd%3D&username=max", &hosts, &nhosts, &attrs, &errmsg);
    assert_return_code(sts, 0);

    node = __pmHashSearch(PCP_ATTR_PASSWORD, &attrs);
    assert_non_null(node);
    assert_string_equal(node->data, "very$secret&complex?passwÖrd=");

    __pmFreeHostAttrsSpec(hosts, nhosts, &attrs);
    __pmHashClear(&attrs);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_hostspec_unparse),
        cmocka_unit_test(test_hostspec_parse),
    };

    /* some versions of libcmocka mix stdout and stderr ... whack that */
    dup2(STDOUT_FILENO, STDERR_FILENO);

    return cmocka_run_group_tests(tests, NULL, NULL);
}
