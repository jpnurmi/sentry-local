from sentry.models.organization import Organization

for org in Organization.objects.all():
    if org.get_option("sentry:store_crash_reports") is None:
        org.update_option("sentry:store_crash_reports", -1)
