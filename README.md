cd ~/Projects/AeroHedge

cat << 'EOF' > README.md
# AeroHedge 🚀

> **High-Frequency Quantitative Risk Engine, Real-Time Delta Hedging, and Low-Latency Financial Architecture.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![Python Version](https://img.shields.io/badge/python-3.10%2B-blue.svg)](https://www.python.org/)
[![FastAPI](https://img.shields.io/badge/FastAPI-0.100%2B-009688.svg)](https://fastapi.tiangolo.com/)
[![Docker](https://img.shields.io/badge/Docker-Enabled-2496ED.svg)](https://www.docker.com/)

---

## 📖 Table of Contents
1. [Executive Summary](#-executive-summary)
2. [Mathematical & Quantitative Foundations](#-mathematical--quantitative-foundations)
   * [Black-Scholes-Merton Option Pricing & Greeks](#black-scholes-merton-option-pricing--greeks)
   * [Dynamic Delta Hedging Mechanics](#dynamic-delta-hedging-mechanics)
3. [System Architecture & Low-Latency Pipeline](#-system-architecture--low-latency-pipeline)
4. [Data Structures: Lock-Free Circular Buffers](#-data-structures-lock-free-circular-buffers)
5. [Project Structure and Module Functionality](#-project-structure-and-module-functionality)
   * [1. The Memory Skeleton: market_data.hpp](#1-the-memory-skeleton-market_datahpp)
   * [2. Bypassing the Kernel Clock: time_utils.hpp](#2-bypassing-the-kernel-clock-time_utilshpp)
   * [3. The Zero-Copy Catch: udp_listener.hpp](#3-the-zero-copy-catch-udp_listenerhpp)
   * [4. The Lock-Free Highway: spsc_queue.hpp](#4-the-lock-free-highway-spsc_queuehpp)
   * [5. The Math Engine: risk_engine.hpp](#5-the-math-engine-risk_enginehpp)
   * [6. The Execution Gateway: order_gateway.hpp](#6-the-execution-gateway-order_gatewayhpp)
   * [7. Overcoming the "Observer Effect": The Telemetry Pipeline](#7-overcoming-the-observer-effect-the-telemetry-pipeline)
6. [License](#-license)

---

## 🎯 Executive Summary

**AeroHedge** is a high-performance, low-latency financial engineering platform built to solve real-time risk exposure and portfolio hedging challenges. Designed with a dual-core paradigm—a **C++ Core Engine** for sub-microsecond market tick processing and order routing, paired with a **Python/FastAPI asynchronous microservice layer**.

---

## 📐 Mathematical & Quantitative Foundations

### Black-Scholes-Merton Option Pricing & Greeks
In continuous time, the price of an underlying asset $S(t)$ follows Geometric Brownian Motion:
$$dS(t) = \mu S(t) dt + \sigma S(t) dW(t)$$

* **Delta ($\Delta$):** The rate of change of option value $V$ with respect to the underlying asset price $S$:
  $$\Delta = \frac{\partial V}{\partial S} = \Phi(d_1)$$
  where $d_1 = \frac{\ln(S / K) + \left(r + \frac{\sigma^2}{2}\right)T}{\sigma \sqrt{T}}$

* **Gamma ($\Gamma$):** The rate of change of Delta with respect to the underlying price (measures convexity risk):
  $$\Gamma = \frac{\partial^2 V}{\partial S^2} = \frac{\phi(d_1)}{S \sigma \sqrt{T}}$$

### Dynamic Delta Hedging Mechanics
To achieve a **Delta-Neutral Portfolio**, the net delta must be minimized. The engine evaluates:
$$\left\vert{} \Delta_{\text{net}} \right\vert{} > \tau$$
If the condition breaches threshold $\tau$, the engine computes the hedge quantity:
$$\text{Shares}_{\text{hedge}} = - \left( \Delta_{\text{net}} \times 100 \right)$$

---

## 🏗 System Architecture & Low-Latency Pipeline

```mermaid
flowchart TD
    Market(["📈 Market Event"])
    subgraph Portfolio ["Portfolio Risk State"]
        Opt["Options Position"]
        Stock["Stock Inventory"]
        NetDelta{"Net Delta Evaluation"}
    end
    subgraph Engine ["AeroHedge C++ Risk Engine"]
        Check{{"Is |Net Δ| > Threshold?"}}
        Calc["Calculate Hedge Quantity"]
        Route["Construct OrderRequest"]
    end
    Exchange[("Exchange Matching Engine")]

    Market --> Opt
    Opt --> NetDelta
    Stock -.-> NetDelta
    NetDelta --> Check
    Check -- "YES" --> Calc
    Calc --> Route
    Route == "TCP send" ==> Exchange
    Exchange -. "Filled" .-> Stock







    ⚡ Data Structures: Lock-Free Circular Buffers

AeroHedge utilizes Single-Producer Single-Consumer (SPSC) Circular Buffers for inter-thread market data streaming to avoid kernel context switches.
Plaintext

Memory Layout:
+-------+-------+-------+
| Tick1 | Tick2 |  ...  |
+-------+-------+-------+
    ^               ^
   Head            Tail

📂 Project Structure and Module Functionality
1. The Memory Skeleton: market_data.hpp

In high-frequency trading, if your data structures are improperly aligned, your entire pipeline is bottle-necked by memory access latency. This file defines the MarketTick struct, the atomic unit of data traveling through the system.

What makes this file critical is Struct Packing. Variables are ordered strictly from largest byte size to smallest:

    uint64_t timestamp (8 bytes)

    double price (8 bytes)

    uint64_t ingress_cycles (8 bytes)

    uint32_t instrument_id (4 bytes)

    uint32_t volume (4 bytes)

If a 4-byte integer is placed before an 8-byte double, the C++ compiler silently injects 4 bytes of empty "padding" to maintain memory alignment. By optimizing the order, this struct packs perfectly into 32 bytes with zero wasted memory. Modern CPUs pull memory into L1 cache lines in 64-byte chunks. A 32-byte struct ensures that exactly two market ticks fit perfectly into a single cache line, drastically reducing cache miss ratios during sequential reads.
2. Bypassing the Kernel Clock: time_utils.hpp

Measuring nanosecond latency using standard libraries like std::chrono is counterproductive, as calling the OS clock introduces microsecond-level system calls and context switches. This file solves that latency trap by talking directly to the silicon.

The TSCClock class implements hardware-level timekeeping. Depending on the architecture, it issues the __rdtsc() assembly instruction (for x86/Linux) or mach_absolute_time() (for macOS). These instructions read the Time-Stamp Counter register on the CPU, returning the exact number of clock cycles executed since the machine booted. Crucially, this read takes only a single CPU cycle. Because clock cycles are not absolute time, the class runs a calibrate() function on startup, measuring the cycles_per_ns_ ratio to mathematically convert CPU spins into human-readable latency without interrupting the critical path.
3. The Zero-Copy Catch: udp_listener.hpp

This module represents the boundary where the engine touches external market data. The UdpListener is designed to bind to a UDP multicast group and pull binary data out of the Linux kernel network stack.

The latency reduction occurs inside the listen_and_publish loop. The network socket read operation executes as:
recv(socket_fd_, &tick, sizeof(MarketTick) - sizeof(uint64_t), 0);

    The system does not read bytes into a temporary buffer to be parsed later.

    It passes the memory address (&tick) of a pre-allocated stack variable directly to the kernel.

    The OS drops the incoming bytes directly into the C++ struct memory footprint.

    The system reads exactly 24 bytes (accounting for the 8-byte internal ingress_cycles tracker) because the exchange standard packet is exactly 24 bytes.

    At the exact microsecond the recv() completes, the thread calls global_clock.rdtsc() to tag the ingress_cycles, stamping the data before instantaneously pushing it to the lock-free queue.

4. The Lock-Free Highway: spsc_queue.hpp

If the ingestion layer used a standard std::mutex to hand data to the math engine, the thread would have to ask the OS kernel for permission to lock the memory, destroying determinism. The SPSCQueue (Single-Producer Single-Consumer) entirely bypasses the OS scheduler.

This is the most intricate concurrent C++ implementation in the project, designed to manipulate CPU cache mechanics:

    The Bitwise Trick: The array Capacity must be a power of 2. Instead of finding the next ring index using slow modulo division (head % Capacity), it utilizes a bitwise AND ((current_head + 1) & Mask), executing in a single clock cycle.

    Memory Fencing: It enforces strict C++ atomics (memory_order_relaxed, memory_order_acquire, memory_order_release) to prevent the compiler or CPU from reordering instructions out of sequence. The release flag guarantees that the actual data is written to RAM before the consumer thread is permitted to see the updated index.

    False Sharing Prevention: The alignas(64) tags on the head_ and tail_ atomic indices are vital. If these two variables sat adjacent in memory, Core 1 (writing to head) and Core 2 (reading from tail) would continuously invalidate each other's L1 cache line, causing catastrophic bus traffic. Padding them to 64 bytes forces them onto completely isolated physical silicon pathways.

5. The Math Engine: risk_engine.hpp

This module operates as the quantitative brain of the trading system. It pops ticks off the lock-free queue, computes the Black-Scholes pricing model, manages current inventory, and determines if a hedging trade is required.

    Functionality: Real-time Delta calculation and Order Generation.

    Intricate Detail: The Black-Scholes formula requires the calculation of the Cumulative Distribution Function (CDF) of the Standard Normal Distribution. Using the standard C++ library std::erf (error function) consumes roughly 150–200 CPU clock cycles due to extreme scientific precision requirements. In options market making, precision past a certain decimal is irrelevant if it causes a missed execution.

    To solve this, the engine implements fast_cdf(): a branchless polynomial approximation based on the Abramowitz and Stegun formula. Utilizing hardcoded constants and single-cycle arithmetic operations (+, *), it computes the option's Delta in under 10 nanoseconds without branching logic (which would otherwise risk pipeline flushes on branch mispredictions).

6. The Execution Gateway: order_gateway.hpp

Once the Risk Engine determines a hedge is necessary, it constructs a 24-byte OrderRequest struct and pushes it to the outbound queue. The OrderGateway thread pops it and routes the binary payload to the exchange.

    Functionality: Deterministic, non-blocking TCP IPv4 transmission.

    Intricate Detail: TCP guarantees delivery, making it inherently dangerous for latency. If the exchange matching engine is slow to acknowledge packets, the OS kernel will eventually fill up the NIC's transmission buffer and block (suspend) the executing thread.

    This module immunizes the engine by configuring the socket to O_NONBLOCK via fcntl, and enforcing MSG_DONTWAIT on the send() call. If the kernel buffer is full, send() immediately returns an EWOULDBLOCK error. Instead of crashing or yielding the thread to the OS, the C++ thread enters a "hot spin," continuously retrying the send operation in user-space until a microsecond window opens, ensuring the system never surrenders its CPU core.

7. Overcoming the "Observer Effect": The Telemetry Pipeline

A classic systems engineering challenge is observing a low-latency application without impacting its performance. String formatting, JSON serialization, and UI rendering are catastrophically slow. If the C++ Execution Thread paused to update a dashboard, the p99 latency would spike from nanoseconds to milliseconds.

AeroHedge completely decouples execution from observation using a three-stage, out-of-band telemetry architecture:

    Fire and Forget (telemetry.hpp): On every processed tick, the C++ Risk Engine executes a lightning-fast memory copy of its state (Delta, P&L, Inventory) to an asynchronous MetricsQueue. A 4th C++ thread pops this struct and blasts it over a local UDP socket. There are no strings or JSON payloads—just raw binary. If the UDP packet drops under high load, the core engine is entirely unaffected.

    The Bridge (websocket_bridge.py): An asynchronous Python process listens on the local UDP port. It absorbs the heavy computational penalty of unpacking the binary struct and serializing it into a JSON string, subsequently hosting a WebSocket server to broadcast the telemetry stream.

    The Terminal (dashboard.html): A custom HTML5/JS dashboard connects to the WebSocket. It renders the live order book spread, charts underlying asset movement at 60 FPS, and plots a dynamic, color-coded histogram of the C++ engine's hardware-timed execution latency.

📄 License

Distributed under the MIT License.
