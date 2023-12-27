#pragma once

#include <stdio.h>

/***
 * Simple Unit test framework.
 *
 * The goal is to avoid significant dependencies, and have the entire
 * framework be accessible with a single include. It provides the
 * minimum functionality for a unit testing framework:
 *
 * - execute tests,
 * - provide a means to check for and trigger test failures,
 * - eager print out a lot of information about failures,
 * - track the number of successes, report them at the end of execution.
 *
 * The downsides of the simplicity of this library include:
 *
 * - `SUNIT_ASSERT` must be used in the test function itself, or with
 *    the, somewhat awkward, `SUNIT_CHECK_FAIL` on the return value
 *    from another function that uses `SUNIT_ASSERT`.
 * - A fault in the middle of execution will prevent previous
 *    successes from being reported.
 * - The macros for test construction are a little unintuitive.
 *
 * An example usage:
 *
 * ```c
 * struct sunit_test tests[] = {
 *	SUNIT_TEST("allocation test", test_malloc),
 *	SUNIT_TEST("free test", test_free),
 *	SUNIT_TEST_TERM
 * };
 * sunit_execute("addition", tests);
 * ```
 *
 * Where the test functions look similar to this:
 *
 * ```c
 * sunit_ret_t
 * test_calloc(void)
 * {
 *      char *str = calloc(1, 1);
 *      SUNIT_ASSERT("calloc", str != NULL);
 *      // ...
 *      return SUNIT_SUCCESS;
 * }
 *
 * sunit_ret_t
 * test_malloc(void)
 * {
 * 	char *str;
 *
 * 	str = malloc(10);
 * 	SUNIT_ASSERT("string allocation", str != NULL);
 *      // return failure from called function, or continue executing
 *      SUNIT_CHECK_FAIL(test_calloc());
 *      // ...
 *      return SUNIT_SUCCESS;
 * }
 * ```
 */

/*
 * Convoluted and esoteric C macro logic here: We need to pass the
 * __LINE__ and condition through 2 macros to get it to expand to a
 * constant string.
 */
#define SUNIT_STRX(x) #x
#define SUNIT_STR(x) SUNIT_STRX(x)

/*
 * This is architected to avoid dynamic allocation of the error
 * string, and ensuring that it is static, read-only memory. The
 * `return` is meant to return from the test function, which means
 * that you should *only use this assertion from within the test
 * function*.
 */
#define SUNIT_ASSERT(description, condition)				\
	do {								\
		if (!(condition)) {					\
			char *desc = description " at " __FILE__ ":"	\
				SUNIT_STR(__LINE__) " when "		\
				"evaluating " SUNIT_STR(condition);	\
			return desc;					\
	}								\
} while (0)

/*
 * The return value type from unit test functions, and the constant for
 * returning "success".
 */
typedef char *sunit_ret_t;
#define SUNIT_SUCCESS NULL

/*
 * If we are going to call a function within our test function, and it
 * could `SUNIT_ASSERT` and fail, we need to propagate that failure to
 * return from the test function. This macro does that.
 */
#define SUNIT_CHECK_FAIL(expression)			\
	do {						\
		sunit_ret_t r = expression;		\
		if (r != SUNIT_SUCCESS) return r;	\
	} while (0)

typedef sunit_ret_t (*sunit_test_fn_t)(void);

struct sunit_test {
	char *description;
	sunit_test_fn_t fn;
	sunit_ret_t ret;
};

#define SUNIT_TEST(desc, test_fn) { .description = desc, .fn = test_fn }
#define SUNIT_TEST_TERM { .fn = NULL }

/**
 * `sunit_execute` is the function to execute a batch of tests, print
 * out failures, and report how many of them were successes. It
 * focuses on specifying the tests in a relatively straightforward
 * way, and the above macros are to make it easier.
 *
 * Arguments:
 *
 * - `@description` - Textual description of the set of tests to be
 *   performed. Printed out on success.
 * - `@tests` - An array of tests to execute in order. Each test
 *   structure has a description (to be printed on failure), and the
 *   test function. The last test in the array must have `.fn == NULL`
 *   to convey the end of the tests. The `SUNIT_TEST` and
 *   `SUNIT_TEST_TERM` macros help construct this array.
 */
static void
sunit_execute(char *description, struct sunit_test *tests)
{
	int i;
	int successes = 0;

	for (i = 0; tests[i].fn; i++) {
		tests[i].ret = tests[i].fn();
		/* print out any failures immediately */
		if (tests[i].ret != SUNIT_SUCCESS) {
			printf("FAILURE: %s -- %s\n", tests[i].description, tests[i].ret);
		} else {
			successes++;
		}
	}
	/* `i`, here, is the count of tests */
	printf("SUCCESS (%d of %d): %s\n", successes, i, description);

	return;
}
