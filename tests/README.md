# Testing STASIS

## Unit tests

Rules:

* Use the boilerplate `_test_boilerplate.c`
  * ```shell
    cp _test_boilerplate.c test_mynewtest.c
    ```
* Test file names start with `test_` (`test_file.c`)
* Test functions start with `test_` (`void test_function()`)
* Test suites can be skipped by returning `127` from `main()`
* PASS, FAIL, and SKIP conditions are set by the assertion macros `STASIS_{ASSERT,ASSERT_FATAL,SKIP_IF}`
* Parametrized tests should implement an array of `struct testcase`
  * The `testcase` structure should be local to each `test_` function
  * The `testcase` variables are freeform
  * Example:
    ```c
    void test_function() {
        struct testcase {
            int your;
            int variables;
            int here;
        };
        struct testcase tc[] = {
            {.your = 0, .variables = 1, .here = 2},
            {.your = 3, .variables = 4, .here = 5},
            // ...
        };
        for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
            // STASIS_ASSERT()s here
        }
    }
    ```
    
## Regression test (RT)

Rules:

* Regression tests are shell scripts (BASH)
* Use the boilerplate `_rt_boilerplate.sh`
  * ```shell
    cp _rt_boilerplate.sh rt_mynewtest.sh
    ```
* Test file names start with `rt_` (`rt_file.sh`)
* Test function names are freeform, however they must be executed using `run_command()` (`run_command test_function`)
* Test suites can be skipped by returning `127` from the shell script