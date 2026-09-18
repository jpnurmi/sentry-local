/*
 * Exercises server-side processing of the native event and minidump produced
 * for an uncaught C++ exception. The build enables the C++ integration with
 * `SENTRY_INTEGRATION_CPP`; the app throws a `std::runtime_error` and uses
 * `SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP` so Relay and Sentry can be
 * developed against both payloads.
 *
 * Build this test against a sentry-native checkout containing the C++
 * integration.
 *
 * The event has the `test.case=cpp-exception` tag. The native event contains an
 * exception with:
 *
 * - `type`: a platform-specific name ending in `runtime_error`
 * - `value`: `something went wrong`
 * - `mechanism.type`: `cpp_exception`
 * - `mechanism.handled`: `false`
 *
 * Relay and Sentry should preserve this metadata while using the minidump for
 * the exception stack.
 */

#include "sentry.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

int
main()
{
    const char *dsn = std::getenv("SENTRY_DSN");
    if (!dsn || !*dsn) {
        std::fputs("Set SENTRY_DSN for the crash report.\n", stderr);
        return 1;
    }

    sentry_options_t *options = sentry_options_new();
    sentry_options_set_crash_reporting_mode(
        options, SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP);
    if (sentry_init(options) != 0) {
        return 1;
    }
    sentry_set_tag("test.case", "cpp-exception");

    std::puts("Throwing an uncaught std::runtime_error.");
    std::fflush(stdout);
    throw std::runtime_error("something went wrong");
}
