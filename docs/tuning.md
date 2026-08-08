# Tuning

Every setting here has a default that is meant to be right for most callers.
Change one when a measurement says to, not on principle — and see
[Performance Validation](performance_validation.md) for what counts as a
measurement.

## Read buffer

The buffer each read fills, in userspace. Not the kernel's socket buffer:
`receive_buffer_size` sets `SO_RCVBUF`, this sets how much of a filled socket
buffer one read completion can take.

| transport | setting | default |
|---|---|---|
| TCP client / server | `read_buffer_size(bytes)` | 4 KiB |
| UDS client / server | `read_buffer_size(bytes)` | 4 KiB |
| Serial | `read_chunk(bytes)` | 4 KiB |

Serial's name is older; the setting is the same one. All three clamp to
`[MIN_READ_BUFFER_SIZE, MAX_READ_BUFFER_SIZE]` — 512 B to 1 MiB.

```cpp
auto client = wirestead::tcp_client("127.0.0.1", 9000)
                  .read_buffer_size(64 * 1024)
                  .on_data([](const wirestead::MessageContext& ctx) { /* ... */ })
                  .build();
```

Raising it cuts read completions, and so callback dispatches, on bulk
transfers. It buys nothing for small messages that already arrive one per read.

The ceiling is far below `MAX_SOCKET_BUFFER_SIZE` on purpose: this buffer is
**per connection**, so a server multiplies it by `max_connections`. A 1 MiB
read buffer against the default 1024 connection limit is a gigabyte of
userspace buffers before any payload.

## Memory pool prefill

`MemoryPool(initial_pool_size, max_pool_size)` pre-allocates
`initial_pool_size` buffers, split evenly across four size buckets (1 KiB,
4 KiB, 16 KiB, 64 KiB).

It defaults to **0**. Prefilling trades startup memory for a first-acquire that
does not allocate, and because the buckets reach 64 KiB the trade is steeper
than the number suggests: a nominal 400 reserves roughly 8.3 MiB.

Prefill only if a measurement shows first-acquire allocation on a latency-
sensitive path. Steady-state traffic recycles buffers through the pool anyway,
so the prefill stops mattering once the pool is warm.

## What tuning will not fix

Two things worth knowing before reaching for a knob, both measured rather than
assumed:

**Per-message costs do not move tail latency.** Removing three allocations per
received message and collapsing up to 16 send syscalls into one — both real,
both merged — produced no p99 change at 8, 32 or 64 connections under sustained
load, with the mechanism confirmed live in the same runs. Hundreds of
nanoseconds do not show inside a p99 of hundreds of microseconds. Those changes
are worth having for CPU and allocator pressure; they are not latency work.

**Connection count costs threads, and threads cost more on small machines.**
Each TCP/UDP/UDS client owns an `io_context` and a thread, so connections and
threads scale together — measured at 3.0 threads and roughly 0.4–0.6 MiB per
connection. On a 20-core x86_64 host, p99 is flat from 8 to 1024 connections.
On a 6-core Jetson Orin Nano the same sweep rises about 2.6x from 8 to 512.
If you are fanning out many connections from one embedded process, that is the
cost to plan around; no buffer setting changes it.

`benchmarks/tcp/tcp_load_latency.cpp` in
[wirestead-benchmarks](https://github.com/wirestead/wirestead-benchmarks) is
the harness those numbers come from, if you want them for your own hardware.
