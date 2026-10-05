# embedded-linux-client

Two TCP clients for the SAR payload simulation server, plus the auxiliary tools
I used to design and verify them.

- `client1` — reads the three outputs, prints one JSON line every 100 ms.
- `client2` — same at 20 ms, and drives output 1 over the UDP control channel
  based on output 3.
- `udptest` — auxiliary tool that probes the UDP control protocol; I used it to
  map which object and property IDs mean what.
- `analyze` — auxiliary tool that captures all three outputs and measures their
  amplitude, frequency and shape (the numbers in this document).

## Build and run

- Standard C library only, C (gnu99), x86_64 Linux.
- Build:
  - `make client1`
  - `make client2`
- Start the provided Docker server first, then run a client.
- `client1` prints only JSON to stdout, so it can be piped straight into tools:
  - `./client1 | jq .`
- Auxiliary tools:
  - `make udptest` — probe the UDP control protocol
  - `make analyze` — measure amplitude, frequency and shape of the outputs

## What the clients do

- Connect to ports 4001, 4002, 4003 (one output each).
- Emit one line per time window:
  - `client1` → 100 ms window
  - `client2` → 20 ms window
- Each field is the **most recent** value received on that output during the
  window, as a string.
- If no value arrived in the window, the field is `"--"`.
- `timestamp` is epoch milliseconds.
- Output format:

```json
{"timestamp": 1730000000000, "out1": "1.23", "out2": "-4.00", "out3": "--"}
```

## Design decisions

### poll() for the three sockets

- I wait on all three outputs at once with `poll()` and a millisecond timeout.
- The timeout maps directly to the window: I compute the time left in the
  current window and pass it to `poll()`, so the client wakes up to read when
  data arrives, and wakes up on time to emit when the window ends.
- Chose `poll()` over `select()` for a simpler API (no fd_set to rebuild each
  loop, no FD_SETSIZE limit). `epoll()` would be over-engineering for three
  descriptors.

### Two clocks

- Window pacing uses `CLOCK_MONOTONIC` — it never jumps due to NTP or a clock
  change, so the 100/20 ms cadence stays steady.
- The `timestamp` field uses `CLOCK_REALTIME` — that is the wall-clock epoch
  value the format asks for.
- Monotonic decides *when* to emit; realtime fills in the timestamp *value*.

### Partial line handling

- TCP is a byte stream, not messages.
- A value can arrive split across two reads (`3.1` then `4\n`), or several
  values can arrive in one read.
- Each socket has its own accumulation buffer: I append every read, split on
  `\n`, and treat only text up to the last `\n` as complete values.
- Any trailing partial stays in the buffer for the next read.
- Without this the parser would produce garbage at read boundaries.

### Most recent value and "--"

- In one window an output may send zero, one, or many values.
- I keep the last complete value seen in the window; at window end I print it
  and reset to `"--"`.
- A value belongs to the window in which its terminating `\n` is read — one
  consistent rule for values that straddle a boundary.

### long long for time

- Epoch milliseconds is about 1.8e12, which overflows a 32-bit int.
- I use `long long` for all time values.
- This also avoids counter wrap in a long-running process, which matters for
  software meant to run continuously.

### stdout discipline

- The task requires nothing but JSON on stdout.
- All diagnostics go to stderr, so a pipe of stdout stays clean JSON.
- I checked this with `jq` (see verification below).

### Control logic (client2) — edge-triggered

- Rule: out3 ≥ 3.0 → set output 1 to frequency 1 Hz, amplitude 8000;
  out3 < 3.0 → frequency 2 Hz, amplitude 4000.
- I send a UDP command only when out3 **crosses** the threshold (low→high or
  high→low), not every window — re-sending the same setting 50 times a second
  would be pointless traffic.
- A state variable (-1 = unknown, 0 = low, 1 = high) holds the last setting
  sent; a command goes out only on a change.

### Binary control protocol

- Each field is a 16-bit unsigned integer in network byte order (big-endian).
- I use `uint16_t` and `htons()` per field and send with `sendto()` to
  `127.0.0.1:4000`.
- A write is four fields: operation (2 = write), object, property, value.

## Server outputs: frequencies, amplitudes, shapes

I did not eyeball these — `analyze.c` connects to all three ports, captures
every sample over a fixed window, and computes each property.

| output | amplitude            | frequency | shape    |
|--------|----------------------|-----------|----------|
| out1   | ±5 (centered on 0)   | 0.50 Hz   | sine     |
| out2   | ±5 (centered on 0)   | 0.25 Hz   | triangle |
| out3   | 0 to 5 (offset)      | ~0.15 Hz  | square   |

How each is measured:

- **Amplitude** — min and max over all samples.
  - out1 and out2 swing ±5.
  - out3 is different: it runs 0 to 5, so it is not symmetric around zero; its
    peak is 5 with roughly a 2.5 offset.
- **Frequency** — count mid-line crossings at `(min+max)/2` (using the mid-line
  instead of zero handles out3's offset correctly), two crossings per cycle,
  divided by the capture duration.
- **Shape** — a histogram signature: the fraction of samples that land in the
  middle third of the range.
  - square sits at two levels → near 0 (measured 0.00)
  - sine slows near its peaks → about 0.22 (measured 0.21)
  - triangle is linear, so samples are uniform → about 0.33 (measured 0.31)

### Why the capture is 60 seconds

- The outputs have very different sample rates (out1 ~40/s, out3 ~2/s).
- A short capture sees only about one period of the slow square (out3), and its
  measured frequency moved between a 10 s and a 60 s run (0.10 → 0.15 Hz).
- 60 s gives the slow outputs ~9–15 full periods, so the frequency estimate
  settles.
- out3 is still the least precise output (lowest rate, fewest periods), so I
  report it as approximate (~0.15 Hz).
- For all three, the sample rate is well above twice the signal frequency, so
  there is no aliasing in these numbers.

## How I found the control protocol (udptest)

- The task gives the wire format (16-bit big-endian fields, read/write
  operations) but not which object/property IDs mean what.
- `udptest.c` sends one read or write from command-line arguments; I watched the
  server's stdout response to map them.
- I found the valid property IDs by scanning: a one-line shell loop ran `udptest`
  as a read over IDs 1 to 300 and I watched which ones the server recognized —
  recognized IDs returned a value, the rest returned "no such property".
  - `for p in $(seq 1 300); do ./udptest 1 1 $p; done`
- The resulting map:
  - object = output index (1, 2, 3)
  - property 14 = enabled
  - property 170 = amplitude, in milli-units (5000 = ±5)
  - property 255 = frequency, in milli-Hz (500 = 0.5 Hz)
  - property 300 = glitch chance
- The task states the control values as amplitude 8000 / 4000 and frequency
  1 Hz / 2 Hz:
  - frequency is sent in milli-Hz → 1 Hz = 1000, 2 Hz = 2000
  - amplitude is sent as the literal value the task gives (8000, 4000), which the
    server reports as ±8 / ±4
- So the high state writes object 1, property 255 = 1000 and property 170 = 8000;
  the low state is 2000 and 4000.
- These constants live in `clients.h`.

## How I know the solution is correct

- **Valid JSON** — I pipe `client1` stdout through `jq`. If any line were not
  valid JSON, jq would error; every line parses. This confirms the format and
  the stdout discipline at the same time.
- **Timing** — consecutive timestamps are about 100 ms apart for `client1` and
  about 20 ms for `client2`, read straight off the output.
- **"--" behaviour** — the slow output (out3, ~2/s) shows `"--"` in most 100 ms
  windows, because it genuinely has no new value most of the time — consistent
  with its measured rate.
- **Control, end to end** — I captured `client2` output while the server drove
  out3 across the threshold. Every window with out3 ≥ 3.0 lined up with the high
  setting having been sent, every window below with the low setting, and the
  command fires only on the transition. On the server side out1's amplitude
  actually changed (±8 vs ±4) in response.
  - While developing I added a single temporary debug line to stderr to watch the
    decision, confirmed it behaved correctly, then removed it:
    - `fprintf(stderr, "control: out3=%.2f -> %s freq=%d amp=%d\n", out3, new_state ? "HIGH" : "LOW", new_state ? FREQ_HIGH : FREQ_LOW, new_state ? AMP_HIGH : AMP_LOW);`
- **Cross-check** — the measured baseline frequency of out1 (0.5 Hz) matches the
  protocol's milli-Hz reading (500). Independent evidence the units are right.
- **Clean build** — everything compiles with `-Wall -Wextra` and no warnings.

### A note on automated testing

- The behaviour here is mostly I/O and timing against a live server, so the most
  meaningful checks are the functional ones above rather than isolated unit
  tests.
- The pure parts (packet encoding, the threshold decision) could be factored
  into functions and unit-tested; I kept the clients small and verified behaviour
  against the running server instead.

## Auxiliary tools

- `udptest.c` — probes the UDP control protocol (how I mapped the object and
  property IDs above).
- `analyze.c` — captures all three outputs and measures amplitude, frequency and
  shape (the numbers in this document).
- The task allows third-party dependencies in auxiliary tools; I kept both on the
  standard C library anyway.