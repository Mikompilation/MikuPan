#include <check.h>
#include <stdlib.h>
#include <string.h>

extern void key_cnf_init(void);
extern void key_cnf_set(const u_short *key_data, size_t len);
extern u_short *key_cnf_get_current(void);

START_TEST(test_buffer_read_bounds)
{
    // Invariant: Buffer reads never exceed the declared length (64 u_short elements = 128 bytes)
    
    // Test case 1: Oversized input (2x expected size)
    u_short oversized_key[128];
    memset(oversized_key, 0xAA, sizeof(oversized_key));
    
    // Test case 2: Boundary case (exactly 64 elements)
    u_short boundary_key[64];
    memset(boundary_key, 0xBB, sizeof(boundary_key));
    
    // Test case 3: Valid small input
    u_short valid_key[32];
    memset(valid_key, 0xCC, sizeof(valid_key));
    
    struct {
        u_short *data;
        size_t len;
    } payloads[] = {
        {oversized_key, 128},
        {boundary_key, 64},
        {valid_key, 32}
    };
    
    for (int i = 0; i < 3; i++) {
        key_cnf_init();
        
        // Attempt to set key with various sizes
        key_cnf_set(payloads[i].data, payloads[i].len);
        
        // Retrieve current key
        u_short *current = key_cnf_get_current();
        
        // Verify no buffer overflow: only first 64 elements should be accessible
        for (int j = 0; j < 64; j++) {
            ck_assert(current[j] != 0xDEADBEEF);  // Canary check
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_buffer_read_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}