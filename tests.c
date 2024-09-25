#include <glib.h>
#include "helper.h"

void test_strval();
int main(int argc, char **argv);

void test_strval() {
    char* empty = "";
    char* not_empty = "string";
    int not_empty_n = 6;

    // test empty string
    int expectation  = 0;
    int ret = strval(empty);
    g_assert_cmpint(ret, ==, expectation);

    // test not empty string
    expectation = not_empty_n;
    ret = strval(not_empty);
    g_assert_cmpint(ret, ==, expectation);

    // test NULL
    g_assert_cmpint(strval(NULL), ==, 0);
}

int main(int argc, char **argv) {
    // ?
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/helper/strval test", test_strval);

    return g_test_run();
}