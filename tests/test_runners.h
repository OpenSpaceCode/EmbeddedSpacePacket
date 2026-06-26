#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

typedef struct
{
    int passed;
    int total;
} test_result_t;

test_result_t test_space_packet_run_all(void);

#endif /* TEST_RUNNERS_H */
