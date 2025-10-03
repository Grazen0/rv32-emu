#include "str.h"
#include <unity.h>

void test_string_lifecycle(void)
{
    String s = String_new();

    TEST_ASSERT_EQUAL(0, s.size);
    TEST_ASSERT_EQUAL_STRING("", s.data);

    String_destroy(&s);

    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL(0, s.size);
    TEST_ASSERT_EQUAL(0, s.capacity);
}

void test_string_with_capacity(void)
{
    String s1 = String_with_capacity(8);
    String s2 = String_with_capacity(12);
    String s3 = String_with_capacity(0);

    TEST_ASSERT_EQUAL(0, s1.size);
    TEST_ASSERT_EQUAL_STRING("", s1.data);
    TEST_ASSERT_GREATER_OR_EQUAL(8, s1.capacity);

    TEST_ASSERT_EQUAL(0, s2.size);
    TEST_ASSERT_EQUAL_STRING("", s2.data);
    TEST_ASSERT_GREATER_OR_EQUAL(12, s2.capacity);

    TEST_ASSERT_EQUAL(0, s3.size);
    TEST_ASSERT_EQUAL_STRING("", s3.data);
    TEST_ASSERT_GREATER_OR_EQUAL(0, s3.capacity);

    String_destroy(&s1);
    String_destroy(&s2);
    String_destroy(&s3);
}

void test_string_from(void)
{
    String s1 = String_from("foo");

    TEST_ASSERT_EQUAL(3, s1.size);
    TEST_ASSERT_GREATER_OR_EQUAL(3, s1.capacity);
    TEST_ASSERT_EQUAL_STRING("foo", s1.data);

    String s2 = String_from("");

    TEST_ASSERT_EQUAL(0, s2.size);
    TEST_ASSERT_GREATER_OR_EQUAL(0, s2.capacity);
    TEST_ASSERT_EQUAL_STRING("", s2.data);

    String s3 = String_from("    hello world       ");

    TEST_ASSERT_EQUAL(22, s3.size);
    TEST_ASSERT_GREATER_OR_EQUAL(22, s3.capacity);
    TEST_ASSERT_EQUAL_STRING("    hello world       ", s3.data);

    String_destroy(&s1);
    String_destroy(&s2);
    String_destroy(&s3);
}

void test_string_concatenation(void)
{
    String s1 = String_from("foo");
    String_push(&s1, ' ');

    TEST_ASSERT_EQUAL(4, s1.size);
    TEST_ASSERT_GREATER_OR_EQUAL(4, s1.capacity);
    TEST_ASSERT_EQUAL_STRING("foo ", s1.data);

    String s2 = String_from("bar");
    String_push_str(&s1, s2);

    TEST_ASSERT_EQUAL(7, s1.size);
    TEST_ASSERT_GREATER_OR_EQUAL(7, s1.capacity);
    TEST_ASSERT_EQUAL_STRING("foo bar", s1.data);

    String_destroy(&s1);
    String_destroy(&s2);
}

void test_string_clear(void)
{
    String s = String_from("foo");
    String_clear(&s);

    TEST_ASSERT_EQUAL(0, s.size);
    TEST_ASSERT_EQUAL_STRING("", s.data);

    String_destroy(&s);
}
