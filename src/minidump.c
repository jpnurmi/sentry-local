/*
 * fixed minidump without an Exception stream
 *
 * The synthetic fixture contains thread 1 in wait_for_condition and thread 2
 * in wait_for_completion. Each invocation attaches the same dump, alternating
 * the JSON exception.thread_id between those threads. No client stack is sent.
 * Ignoring JSON thread selection groups both reports together; honoring it
 * groups by the selected stack. The event uses MinidumpTest/error/handled
 * metadata and marks both threads as non-crashed.
 */

#include "sentry.h"

#include <stdio.h>
#include <stdlib.h>

static int
next_case(void)
{
    FILE *file = fopen(".minidump-case", "r+");
    if (!file) {
        file = fopen(".minidump-case", "w+");
    }
    if (!file) {
        return -1;
    }
    unsigned int i = 0;
    (void)fscanf(file, "%u", &i);
    i %= 2;
    rewind(file);
    int written = fprintf(file, "%u\n", (i + 1) % 2);
    int closed = fclose(file);
    return written < 0 || closed != 0 ? -1 : (int)i;
}

static sentry_uuid_t
capture(unsigned int id)
{
    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "level", sentry_value_new_string("error"));
    sentry_value_t exception = sentry_value_new_exception("MinidumpTest", NULL);
    sentry_value_set_by_key(
        exception, "thread_id", sentry_value_new_uint64(id));
    sentry_value_t mechanism = sentry_value_new_object();
    sentry_value_set_by_key(
        mechanism, "type", sentry_value_new_string("generic"));
    sentry_value_set_by_key(mechanism, "handled", sentry_value_new_bool(1));
    sentry_value_set_by_key(exception, "mechanism", mechanism);
    sentry_event_add_exception(event, exception);

    for (unsigned int i = 1; i <= 2; i++) {
        sentry_value_t thread = sentry_value_new_thread(i, NULL);
        sentry_value_set_by_key(thread, "crashed", sentry_value_new_bool(0));
        sentry_event_add_thread(event, thread);
    }

    sentry_scope_t *scope = sentry_local_scope_new();
    sentry_value_t attachment = sentry_attachment_from_file(FIXTURE_PATH);
    sentry_attachment_set_type(attachment, SENTRY_ATTACHMENT_TYPE_MINIDUMP);
    sentry_uuid_t attachment_id = sentry_scope_add_attachment(scope, attachment);
    if (sentry_uuid_is_nil(&attachment_id)) {
        sentry_scope_free(scope);
        sentry_value_decref(event);
        return attachment_id;
    }
    return sentry_scope_capture_event(scope, event, NULL);
}

int
main(void)
{
    const char *dsn = getenv("SENTRY_DSN");
    if (!dsn || !*dsn) {
        fputs("Set SENTRY_DSN for the minidump report.\n", stderr);
        return 1;
    }
    int selected = next_case();
    if (selected < 0) {
        fputs("Failed to update .minidump-case.\n", stderr);
        return 1;
    }
    sentry_options_t *options = sentry_options_new();
    if (sentry_init(options) != 0) {
        return 1;
    }
    unsigned int id = (unsigned int)selected + 1;
    printf("Attaching the fixed minidump; JSON selects thread %u (%s).\n", id,
        selected == 0 ? "wait_for_condition" : "wait_for_completion");
    sentry_uuid_t event_id = capture(id);
    sentry_close();
    if (sentry_uuid_is_nil(&event_id)) {
        fputs("Failed to capture the minidump report.\n", stderr);
        return 1;
    }
    return 0;
}
