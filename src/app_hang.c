/*
 * Watchdog-as-crasher reproduction for:
 * https://github.com/getsentry/sentry/issues/97529
 *
 * A main-thread hang triggers a watchdog crash. An ordinary crash at the same
 * site provides a comparison with a responsive main thread. `make app-hang`
 * builds, uploads debug files, and captures two events:
 *
 * - `crash`: the watchdog crashes while the main thread keeps sending
 *   heartbeats.
 * - `wait-condition`: the main thread hangs in `wait_for_condition`.
 *
 * Make continues after each deliberate crash. The watchdog calls
 * `sentry_crash()` from the same location in both runs. The console prints both
 * thread IDs, and the `test.case` tag identifies the case.
 *
 * The static build uses the SDK's internal thread, synchronization, and
 * thread-ID helpers on Windows, Linux, and macOS.
 *
 * The SDK's native backend captures and uploads one crash event per run in
 * minidump mode, so the server derives stacks from the dump. The dump identifies
 * the watchdog as the exception thread. Only the hang case installs `on_crash`:
 * it adds an `AppHang` exception whose `thread_id` references the main thread and
 * sets both threads' `crashed` flags to false. The callback uses `level: error`
 * and `handled: true`, matching the SDK's hang reports, and supplies no stack
 * trace. The ordinary crash keeps the SDK's event unchanged.
 *
 * On an unmodified server, check whether both events group into one issue using
 * the watchdog stack. With the thread-selection fixes, expect two issues: the
 * ordinary crash uses the watchdog stack, and the hang uses the main-thread
 * stack. Inspect Event Grouping Information as well as the thread stacks.
 */

#include "sentry_boot.h"

#include "sentry_app_hang_latch.h"
#include "sentry_sync.h"
#include "sentry_utils.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#    define NOINLINE __declspec(noinline)
#else
#    define NOINLINE __attribute__((noinline))
#endif

static sentry_mutex_t mutex;
static sentry_cond_t heartbeat;
static sentry_cond_t condition;
static sentry_threadid_t watchdog_thread;
static bool started;
static uint64_t main_id;

static sentry_value_t
on_crash(const sentry_ucontext_t *uctx, sentry_value_t event, void *user_data)
{
    (void)uctx;
    (void)user_data;

    sentry_value_set_by_key(event, "level", sentry_value_new_string("error"));
    sentry_value_t exception = sentry_value_new_exception("AppHang", NULL);
    sentry_value_set_by_key(
        exception, "thread_id", sentry_value_new_uint64(main_id));
    sentry_value_t mechanism = sentry_value_new_object();
    sentry_value_set_by_key(
        mechanism, "type", sentry_value_new_string("AppHang"));
    sentry_value_set_by_key(mechanism, "handled", sentry_value_new_bool(1));
    sentry_value_set_by_key(exception, "mechanism", mechanism);
    sentry_event_add_exception(event, exception);

    sentry_value_t thread = sentry_value_new_thread(main_id, NULL);
    sentry_value_set_by_key(thread, "crashed", sentry_value_new_bool(0));
    sentry_event_add_thread(event, thread);
    thread = sentry_value_new_thread(sentry__app_hang_current_tid(), NULL);
    sentry_value_set_by_key(thread, "crashed", sentry_value_new_bool(0));
    sentry_event_add_thread(event, thread);
    return event;
}

SENTRY_THREAD_FN
watchdog(void *arg)
{
    bool crash = *(bool *)arg;
    sentry__mutex_lock(&mutex);
    while (!started) {
        sentry__cond_wait(&heartbeat, &mutex);
    }
    uint64_t deadline = sentry__monotonic_time() + 1000;
    uint64_t now;
    while ((now = sentry__monotonic_time()) < deadline) {
        sentry__cond_wait_timeout(&heartbeat, &mutex, deadline - now);
    }
    sentry__mutex_unlock(&mutex);
    if (crash) {
        printf("Watchdog %" PRIu64 " is crashing while main thread %" PRIu64
               " continues sending heartbeats.\n",
            sentry__app_hang_current_tid(), main_id);
    } else {
        printf("Main thread %" PRIu64 " missed its heartbeat; watchdog %" PRIu64
               " is crashing.\n",
            main_id, sentry__app_hang_current_tid());
    }
    fflush(stdout);
    sentry_crash();
    return 0;
}

static NOINLINE void
wait_for_condition(void)
{
    sentry__mutex_lock(&mutex);
    started = true;
    sentry__cond_wake(&heartbeat);
    for (;;) {
        sentry__cond_wait(&condition, &mutex);
    }
}

static NOINLINE void
send_heartbeats(void)
{
    sentry__mutex_lock(&mutex);
    started = true;
    for (;;) {
        sentry__cond_wake(&heartbeat);
        sentry__cond_wait_timeout(&condition, &mutex, 100);
    }
}

int
main(int argc, char **argv)
{
    if (argc != 2
        || (strcmp(argv[1], "crash") != 0
            && strcmp(argv[1], "wait-condition") != 0)) {
        puts("Usage:\n"
             "  make app-hang SENTRY_DSN=\"<dsn>\"\n\n"
             "Each run deliberately crashes the watchdog and sends one crash event.\n"
             "Compare whether an ordinary crash and a hang group together.");
        return argc == 1 ? 0 : 1;
    }
    const char *dsn = getenv("SENTRY_DSN");
    if (!dsn || !*dsn) {
        fputs("Set SENTRY_DSN for the crash report.\n", stderr);
        return 1;
    }

    bool crash = strcmp(argv[1], "crash") == 0;
    main_id = sentry__app_hang_current_tid();
    sentry_options_t *options = sentry_options_new();
    sentry_options_set_crash_reporting_mode(
        options, SENTRY_CRASH_REPORTING_MODE_MINIDUMP);
    if (!crash) {
        sentry_options_set_on_crash(options, on_crash, NULL);
    }
    if (sentry_init(options) != 0) {
        return 1;
    }
    sentry_set_tag("test.case", argv[1]);

    sentry__mutex_init(&mutex);
    sentry__cond_init(&heartbeat);
    sentry__cond_init(&condition);
    sentry__thread_init(&watchdog_thread);
    if (sentry__thread_spawn(&watchdog_thread, watchdog, &crash) == 0) {
        if (crash) {
            send_heartbeats();
        } else {
            wait_for_condition();
        }
        sentry__thread_free(&watchdog_thread);
    } else {
        fputs("Cannot start watchdog.\n", stderr);
    }
    sentry__cond_free(&heartbeat);
    sentry__cond_free(&condition);
    sentry__mutex_free(&mutex);
    sentry_close();
    return 1;
}
