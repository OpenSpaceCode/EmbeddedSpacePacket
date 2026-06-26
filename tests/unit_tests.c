#include "test_runners.h"

#include <stdio.h>

#define REPORT(label, r) printf("  %-14s Passed %d/%d\n\n", label ":", (r).passed, (r).total)

int main(void)
{
    test_result_t r;
    int total_passed = 0;
    int total_tests = 0;

    r = test_space_packet_run_all();
    REPORT("space_packet", r);
    total_passed += r.passed;
    total_tests += r.total;

    printf("  ------------------------------\n");
    printf("  %-14s Passed %d/%d\n", "All UT:", total_passed, total_tests);

    return (total_passed == total_tests) ? 0 : 1;
}
