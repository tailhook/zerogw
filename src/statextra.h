DEFINE_EXTRA(connections, "Conns", "%5lu", "gauge",
    newstat->connects - newstat->disconnects)
DEFINE_EXTRA(unanswered_replies, "Reqs", "%3lu", "gauge",
    newstat->http_requests - newstat->http_replies)
DEFINE_EXTRA(websockets, "Wsock", "%5lu", "gauge",
    newstat->websock_connects - newstat->websock_disconnects)
DEFINE_EXTRA(comet, "Comet", "%5lu", "gauge",
    newstat->comet_connects - newstat->comet_disconnects)
DEFINE_EXTRA(topics, "Topic", "%5lu", "gauge",
    newstat->topics_created - newstat->topics_removed)
DEFINE_EXTRA(subscriptions, "Subs ", "%5lu", "gauge",
    newstat->topics_created - newstat->topics_removed)
DEFINE_EXTRA(backend_queue, "BeQ", "%3lu", "gauge",
    newstat->websock_backend_queued - newstat->websock_backend_unqueued)

