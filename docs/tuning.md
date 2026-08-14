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

## Serial low-latency mode

USB serial adapters buffer received bytes behind a driver timer before handing
them up. An FTDI ships with that timer at **16 ms**, so a 1 ms packet at 115200
baud can still reach your callback 16 ms late no matter what the rest of the
stack does. `low_latency` clears it (`ASYNC_LOW_LATENCY` on Linux) and is **on
by default**.

```cpp
auto port = wirestead::serial("/dev/ttyUSB0", 115200)
                .low_latency(false)  // leave the driver's timer alone
                .on_data([](const wirestead::MessageContext& ctx) { /* ... */ })
                .build();
```

Best effort, and deliberately so: drivers with no such timer — native UARTs,
CDC-ACM — refuse the request, the port opens normally, and the refusal is
logged at debug level. Linux only; elsewhere the setting is a no-op.

Turn it off to trade latency back for fewer wakeups on a high-rate stream where
arrival time does not matter. Unlike everything else on this page, this one is
worth setting without a measurement first: nothing else in the library recovers
16 ms.

## Detecting a sensor that went quiet

`RuntimeStats::last_receive_age_ms` is milliseconds since bytes last arrived,
on every transport, and `nullopt` until something has. It exists because a
device that stops streaming without erroring leaves every other counter looking
healthy and the link reporting `Connected` — nothing failed, the data just
stopped.

```cpp
const auto s = channel->stats();
const bool quiet = s.last_receive_age_ms && *s.last_receive_age_ms > 500;
```

Compare it against the slowest interval that device is expected to hit, with
margin. It reports rather than acts, because what to do about silence differs
per device — and `reset_stats()` clears it, so a restarted channel does not
report an age from its previous life.

Serial additionally offers an active version below, because there a concrete
recovery exists.

## Serial receive watchdog

A device that stops streaming without erroring leaves a healthy open port and a
read that never completes, so nothing else in the transport notices — the
classic wedged USB adapter or sensor that just goes quiet.
`rx_idle_timeout` closes and reopens the port after that long without received
data. Off by default.

```cpp
auto port = wirestead::serial("/dev/ttyUSB0", 115200)
                .rx_idle_timeout(std::chrono::milliseconds(500))
                .build();
```

Set it above the device's slowest expected interval, with margin — expiry tears
the link down, so a value below the real gap between messages produces a reopen
loop rather than a recovery.

Receives only, deliberately unlike the TCP idle timeout, which any traffic in
either direction resets: a driver polling a mute device writes on schedule and
would hold a bidirectional timer open forever. Expiry runs the same path as a
read error, so `reopen_on_error` decides whether the port is reopened or the
link goes to `Error`.

## Io thread policy

The library starts its own threads, so nothing else can reach them: a
deployment that wants `SCHED_FIFO`, a CPU affinity mask, or just a readable
name in `top` had no handle to apply it to. `set_io_thread_init()` installs a
callback that runs on each io thread the library starts, before that thread
runs any work — which is where `pthread_self()`-based APIs need to be called
from.

```cpp
wirestead::concurrency::set_io_thread_init([] {
  pthread_setname_np(pthread_self(), "wirestead-io");
  sched_param p{.sched_priority = 20};
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &p);
});
```

Process-wide on purpose. Thread policy belongs to the deployment, not to one
connection, and a per-channel field would have to be threaded through six
transports to say the same thing.

Set it before starting any channel — it does not reach threads already
running. The library never undoes what the hook did, so a raised priority
outlives the channel that triggered it when the thread is a shared one.
Blocking in the hook blocks that channel's io; an exception escaping it is
caught, because the alternative at a thread entry point is process
termination.

Read the table under [What tuning will not fix](#what-tuning-will-not-fix)
first if the goal is throughput rather than scheduling determinism. Raising
priority does not make a channel faster; it makes it preempt other work, which
is only what you want on a robot with a deadline.

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

**Client objects cost threads. Server sessions do not.** The two are easy to
conflate and behave nothing alike:

| shape | threads |
|---|---|
| one server, N accepted sessions | **constant** — measured 3 at N=0 and 3 at N=64 |
| N client objects in one process | **~2 each** — measured 131 threads at N=64 |

A server multiplexes every session onto one `io_context`, so accepting more
connections costs memory and nothing else. Each `TcpClient`, `UdpChannel` or
`UdsClient` instead owns an `io_context` and a thread, so a process holding
many client objects holds many threads.

That only bites a process that fans out — a gateway dialling many peers, or a
load generator. Most embedded deployments hold a handful of client objects and
never notice. If you are the fan-out case: on a 20-core x86_64 host p99 stayed
flat from 8 to 1024 client objects, while on a 6-core Jetson Orin Nano the same
sweep rose about 2.6x from 8 to 512. Roughly 0.4–0.6 MiB per connection either
way. No buffer setting changes it.

`use_shared_context` puts a channel on the shared `IoContextManager` singleton
instead of its own, but today it is only wired up for `TcpServer` and `Serial`,
which are the two that did not need it. Extending it to the client transports
is additive and non-breaking whenever a real fan-out use case turns up.

`benchmarks/tcp/tcp_load_latency.cpp` in
[wirestead-benchmarks](https://github.com/wirestead/wirestead-benchmarks) is
the harness those numbers come from, if you want them for your own hardware.
