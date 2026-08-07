# Callback Data Lifetime

`MessageContext::data()` returns a callback-scoped view. The view is valid only
while the current callback is running.

Use an ownership-copy helper before sending data to another thread, queue,
coroutine, or longer-lived object.

```cpp
client->on_data([](const wirestead::MessageContext& ctx) {
    auto owned = ctx.data_as_string();
    post_to_worker([owned = std::move(owned)] {
        process(owned);
    });
});
```

Do not store `std::string_view` or spans returned from callback context objects.

```cpp
client->on_data([](const wirestead::MessageContext& ctx) {
    auto view = ctx.data();
    post_to_worker([view] {
        process(view);  // dangling risk
    });
});
```

For binary payloads, use `data_as_vector()` before the callback returns.

Copying the context object itself is also safe. A `MessageContext` delivered to
`on_data`/`on_message` borrows the transport's receive buffer so the callback
costs no allocation, but a copy always takes ownership of the payload, so a
copy stays valid after the callback returns:

```cpp
client->on_data([&](const wirestead::MessageContext& ctx) {
    queue.push_back(ctx);  // copy — owns its payload, safe to keep
});
```

Only the view returned by `data()` is callback-scoped. Contexts delivered to
the batch handlers (`on_data_batch`/`on_message_batch`) always own their data.

## Do not call a blocking send from within a callback

`on_data`/`on_message` (and every other) callback runs on the channel's own
io thread. `send()`/`send_blocking()`/`send_move()`/`send_shared()` in the
default Reliable backpressure strategy block the calling thread until
backpressure clears - but clearing backpressure requires that same io
thread to keep making progress. Calling a blocking send from within a
callback while backpressure is active would deadlock the entire channel,
since the thread that would clear it is the one now waiting.

To prevent this, a blocking send called from inside a callback while
backpressure is active returns `false` immediately instead of blocking:

```cpp
client->on_data([&](const wirestead::MessageContext& ctx) {
    // If backpressure happens to be active when this fires, send() returns
    // false right away rather than deadlocking - it does not block here.
    client->send("reply");
});
```

Always use the non-blocking `try_send()`/`try_send_move()`/`try_send_shared()`
API from within a callback - check the return value and handle a `false`
result (e.g. drop, retry later, or switch to `BackpressureStrategy::BestEffort`)
rather than relying on a blocking send inside callback context.
