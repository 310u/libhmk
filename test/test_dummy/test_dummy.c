#include <unity.h>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_dummy_true(void) {
    TEST_ASSERT_TRUE(1);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_dummy_true);
    UNITY_END();
    return 0;
}
