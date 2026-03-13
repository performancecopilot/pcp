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
#include <unistd.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/sds.h>

static void test_urlencode(char *input, char *expected)
{
    int sts;
    char *output;

    sts = __pmUrlEncode(input, strlen(input), &output);
    assert_return_code(sts, 0);
    assert_string_equal(output, expected);
    free(output);
}

static void test_urldecode(char *input, char *expected)
{
    int sts;
    char *output;

    sts = __pmUrlDecode(input, strlen(input), &output);
    assert_return_code(sts, 0);
    assert_string_equal(output, expected);
    free(output);
}

static void test_urlencode_and_decode(char *text, char *encoded)
{
    test_urlencode(text, encoded);
    test_urldecode(encoded, text);
}

static void test_urlencoding_basic(void **state)
{
    test_urlencode_and_decode("and&question?equal=space .", "and%26question%3Fequal%3Dspace+.");
}

static void test_urlencoding_empty(void **state)
{
    test_urlencode_and_decode("", "");
}

static void test_urlencoding_nonascii(void **state)
{
    test_urlencode_and_decode("umlautö.tildeñ.", "umlaut%C3%B6.tilde%C3%B1.");
    test_urlencode_and_decode("ä", "%C3%A4");
}

static void test_urldecode_invalid(void **state)
{
    int sts;
    char *output;

    sts = __pmUrlDecode("and%", strlen("and%"), &output);
    assert_int_equal(sts, -EINVAL);
    sts = __pmUrlDecode("and%2", strlen("and%2"), &output);
    assert_int_equal(sts, -EINVAL);
    sts = __pmUrlDecode("and%XX", strlen("and%XX"), &output);
    assert_int_equal(sts, -EINVAL);
}

extern sds base64_decode(const char *, size_t);

static void test_base64_decode(char *input, char *expected)
{
    sds output;

    output = base64_decode(input, strlen(input));
    assert_string_equal(output, expected);
    assert_int_equal(strlen(output), sdslen(output));
    sdsfree(output);
}

static void test_base64_basic(void **state)
{
    test_base64_decode("Ym9iOnk=", "bob:y");
    test_base64_decode("SQ==", "I");
    test_base64_decode("QU0=", "AM");
    test_base64_decode("VEpN", "TJM");
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_urlencoding_basic),
        cmocka_unit_test(test_urlencoding_empty),
        cmocka_unit_test(test_urlencoding_nonascii),
        cmocka_unit_test(test_urldecode_invalid),
        cmocka_unit_test(test_base64_basic),
    };

    /* some versions of libcmocka mix stdout and stderr ... whack that */
    dup2(STDOUT_FILENO, STDERR_FILENO);

    return cmocka_run_group_tests(tests, NULL, NULL);
}
