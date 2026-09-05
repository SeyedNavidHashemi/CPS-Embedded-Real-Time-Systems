# CPS — Assignment 1

## Communication Protocol Simulation

This assignment focuses on the simulation and implementation of communication protocols in an embedded real-time system.

The project extends the provided hardware simulator by adding support for three communication components:

* **C2I** — Address-based communication protocol
* **Full-Duplex USART** — Serial communication protocol
* **C2I Multiplexer** — Multiplexed communication between multiple sensors and a microcontroller

The implementations are developed in **C++** and integrated with the provided simulator framework.

---

## Project Overview

The overall communication architecture developed throughout the assignment is:

```text
                    ┌─────────────────┐
                    │ Microcontroller │
                    └────────┬────────┘
                             │
                         USART
                             │
                    ┌────────▼────────┐
                    │    C2I MUX      │
                    └───┬────┬────┬───┘
                        │    │    │
                      C2I  C2I  C2I
                        │    │    │
                       ▼     ▼    ▼
                    Sensor Sensor Sensor
```

The project demonstrates how different communication protocols can be combined to build a more flexible embedded communication system.

---

## Assignment Components

### 1. C2I Protocol

A simplified address-based C2I protocol is implemented to establish communication between a microcontroller and sensor.

The protocol uses **SDA** for data and **SCL** for synchronization. The sensor responds to its assigned address and transfers sensor data to the microcontroller.

### 2. Full-Duplex USART

A simplified **Full-Duplex USART** protocol is implemented for bidirectional serial communication between a microcontroller and a simulated storage device.

The communication uses **TX/RX** lines and follows a frame structure containing:

```text
Start Bit → Data Bits → Parity Bit → Stop Bit
```

Parity handling is not required as part of the implementation.

### 3. C2I Multiplexer

A C2I Multiplexer is implemented to allow multiple C2I sensors, including sensors with identical addresses, to communicate with a single microcontroller.

The resulting communication path is:

```text
Sensors
   │
   ▼
C2I Channels
   │
   ▼
C2I Multiplexer
   │
  USART
   │
   ▼
Microcontroller
```

The microcontroller selects the required C2I channel through USART and receives data from the corresponding sensor.

---

## Technologies

* **C++20**
* CMake
* Boost
* MinGW
* Provided Embedded Systems Simulator

The assignment requires a compiler with C++20 support. The simulator uses header-only parts of Boost, with the required include paths configured through CMake.

---

## Repository Structure

```text
Assignment-1/
├── Comm/
│   └── Hardwares/
├── Sketchs/
├── Utils/
├── include/
├── src/
├── CMakeLists.txt
├── README.md
└── report.md
```

The exact simulator structure may depend on the provided project template.

---

## Documentation

The final report describes the work distribution among team members and the implementation of each part.

---

## Team

This assignment was completed as a group project for the **Embedded Real-Time Systems** course.
group members = [Seyed Navid Hashemi, Seyed Mohammad Mazhari, Seyed Mohammad Jaazayeri, Amirarsalan Shahbazi]

**Course:** Embedded Real-Time Systems
**Assignment:** Computer Exercise 1
**Language:** C++20
