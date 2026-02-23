/*
 * Unit tests for SMC format conversion functions
 *
 * Tests the conversion of SMC fixed-point formats to human-readable values:
 * - sp78: signed fixed-point (÷256) for temperature in Celsius
 * - fpe2: unsigned fixed-point (÷4) for fan speed in RPM
 *
 * Build: cc -o test_smc_format test_smc_format.c -I../../
 * Run:   ./test_smc_format
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Color output for readability */
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define RESET "\033[0m"

/*
 * SMC format conversion functions (will be implemented in smc.c)
 * These are the interfaces we're testing against
 */

/* Convert SMC sp78 format to Celsius (signed fixed-point, divide by 256) */
float smc_sp78_to_celsius(uint16_t raw) {
    int16_t signed_raw = (int16_t)raw;
    return (float)signed_raw / 256.0f;
}

/* Convert SMC fpe2 format to RPM (unsigned fixed-point, divide by 4) */
float smc_fpe2_to_rpm(uint16_t raw) {
    return (float)raw / 4.0f;
}

/*
 * Test helper macros
 */
#define TEST_START(name) \
    do { \
        tests_run++; \
        printf("Test %d: %s ... ", tests_run, name); \
    } while(0)

#define TEST_PASS() \
    do { \
        tests_passed++; \
        printf(GREEN "PASS" RESET "\n"); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        tests_failed++; \
        printf(RED "FAIL" RESET ": %s\n", msg); \
    } while(0)

#define ASSERT_FLOAT_EQ(expected, actual, tolerance) \
    do { \
        if (fabs((expected) - (actual)) > (tolerance)) { \
            char buf[256]; \
            snprintf(buf, sizeof(buf), "Expected %.6f, got %.6f", expected, actual); \
            TEST_FAIL(buf); \
            return; \
        } \
    } while(0)

/*
 * Test cases for sp78 (temperature) conversion
 */

void test_sp78_zero(void) {
    TEST_START("sp78: zero temperature");
    float result = smc_sp78_to_celsius(0x0000);
    ASSERT_FLOAT_EQ(0.0f, result, 0.001f);
    TEST_PASS();
}

void test_sp78_room_temp(void) {
    TEST_START("sp78: room temperature (25°C)");
    /* 25 * 256 = 6400 = 0x1900 */
    float result = smc_sp78_to_celsius(0x1900);
    ASSERT_FLOAT_EQ(25.0f, result, 0.001f);
    TEST_PASS();
}

void test_sp78_hot_cpu(void) {
    TEST_START("sp78: hot CPU (85°C)");
    /* 85 * 256 = 21760 = 0x5500 */
    float result = smc_sp78_to_celsius(0x5500);
    ASSERT_FLOAT_EQ(85.0f, result, 0.001f);
    TEST_PASS();
}

void test_sp78_fractional(void) {
    TEST_START("sp78: fractional temperature (37.5°C)");
    /* 37.5 * 256 = 9600 = 0x2580 */
    float result = smc_sp78_to_celsius(0x2580);
    ASSERT_FLOAT_EQ(37.5f, result, 0.001f);
    TEST_PASS();
}

void test_sp78_negative(void) {
    TEST_START("sp78: negative temperature (-10°C)");
    /* -10 * 256 = -2560 = 0xF600 (two's complement) */
    float result = smc_sp78_to_celsius(0xF600);
    ASSERT_FLOAT_EQ(-10.0f, result, 0.001f);
    TEST_PASS();
}

void test_sp78_small_negative(void) {
    TEST_START("sp78: small negative (-0.5°C)");
    /* -0.5 * 256 = -128 = 0xFF80 */
    float result = smc_sp78_to_celsius(0xFF80);
    ASSERT_FLOAT_EQ(-0.5f, result, 0.001f);
    TEST_PASS();
}

void test_sp78_max_positive(void) {
    TEST_START("sp78: maximum positive (127.996°C)");
    /* Max signed 16-bit / 256 = 32767 / 256 = 127.996 */
    float result = smc_sp78_to_celsius(0x7FFF);
    ASSERT_FLOAT_EQ(127.996f, result, 0.01f);
    TEST_PASS();
}

void test_sp78_max_negative(void) {
    TEST_START("sp78: maximum negative (-128°C)");
    /* Min signed 16-bit / 256 = -32768 / 256 = -128 */
    float result = smc_sp78_to_celsius(0x8000);
    ASSERT_FLOAT_EQ(-128.0f, result, 0.001f);
    TEST_PASS();
}

/*
 * Test cases for fpe2 (fan RPM) conversion
 */

void test_fpe2_zero(void) {
    TEST_START("fpe2: zero RPM (fan stopped)");
    float result = smc_fpe2_to_rpm(0x0000);
    ASSERT_FLOAT_EQ(0.0f, result, 0.001f);
    TEST_PASS();
}

void test_fpe2_idle_fan(void) {
    TEST_START("fpe2: idle fan (800 RPM)");
    /* 800 * 4 = 3200 = 0x0C80 */
    float result = smc_fpe2_to_rpm(0x0C80);
    ASSERT_FLOAT_EQ(800.0f, result, 0.001f);
    TEST_PASS();
}

void test_fpe2_typical_fan(void) {
    TEST_START("fpe2: typical fan (2000 RPM)");
    /* 2000 * 4 = 8000 = 0x1F40 */
    float result = smc_fpe2_to_rpm(0x1F40);
    ASSERT_FLOAT_EQ(2000.0f, result, 0.001f);
    TEST_PASS();
}

void test_fpe2_high_speed(void) {
    TEST_START("fpe2: high speed fan (5000 RPM)");
    /* 5000 * 4 = 20000 = 0x4E20 */
    float result = smc_fpe2_to_rpm(0x4E20);
    ASSERT_FLOAT_EQ(5000.0f, result, 0.001f);
    TEST_PASS();
}

void test_fpe2_fractional(void) {
    TEST_START("fpe2: fractional RPM (1500.25 RPM)");
    /* 1500.25 * 4 = 6001 = 0x1771 */
    float result = smc_fpe2_to_rpm(0x1771);
    ASSERT_FLOAT_EQ(1500.25f, result, 0.001f);
    TEST_PASS();
}

void test_fpe2_max(void) {
    TEST_START("fpe2: maximum RPM (16383.75)");
    /* Max 16-bit / 4 = 65535 / 4 = 16383.75 */
    float result = smc_fpe2_to_rpm(0xFFFF);
    ASSERT_FLOAT_EQ(16383.75f, result, 0.01f);
    TEST_PASS();
}

void test_fpe2_one_rpm(void) {
    TEST_START("fpe2: 1 RPM precision");
    /* 1 * 4 = 4 = 0x0004 */
    float result = smc_fpe2_to_rpm(0x0004);
    ASSERT_FLOAT_EQ(1.0f, result, 0.001f);
    TEST_PASS();
}

void test_fpe2_quarter_rpm(void) {
    TEST_START("fpe2: 0.25 RPM precision");
    /* 0.25 * 4 = 1 = 0x0001 */
    float result = smc_fpe2_to_rpm(0x0001);
    ASSERT_FLOAT_EQ(0.25f, result, 0.001f);
    TEST_PASS();
}

/*
 * Main test runner
 */
int main(void) {
    printf("========================================\n");
    printf("SMC Format Conversion Unit Tests\n");
    printf("========================================\n\n");

    /* sp78 temperature tests */
    printf("--- sp78 Temperature Tests ---\n");
    test_sp78_zero();
    test_sp78_room_temp();
    test_sp78_hot_cpu();
    test_sp78_fractional();
    test_sp78_negative();
    test_sp78_small_negative();
    test_sp78_max_positive();
    test_sp78_max_negative();
    printf("\n");

    /* fpe2 fan RPM tests */
    printf("--- fpe2 Fan RPM Tests ---\n");
    test_fpe2_zero();
    test_fpe2_idle_fan();
    test_fpe2_typical_fan();
    test_fpe2_high_speed();
    test_fpe2_fractional();
    test_fpe2_max();
    test_fpe2_one_rpm();
    test_fpe2_quarter_rpm();
    printf("\n");

    /* Summary */
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: " GREEN "%d" RESET "\n", tests_passed);
    printf("Tests failed: " RED "%d" RESET "\n", tests_failed);
    printf("\n");

    if (tests_failed == 0) {
        printf(GREEN "✓ All tests passed!" RESET "\n");
        return 0;
    } else {
        printf(RED "✗ Some tests failed" RESET "\n");
        return 1;
    }
}
