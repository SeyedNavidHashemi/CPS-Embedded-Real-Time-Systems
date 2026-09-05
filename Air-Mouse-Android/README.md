# CPS Assignment 2 — Air Mouse

Implementation of an **Air Mouse** using an Android smartphone's sensors. The project uses sensor data to control a computer mouse wirelessly, including cursor movement, left-click, and scrolling.

## Overview

The system consists of two main components:

* **Android Application** — reads and processes smartphone sensor data, detects movement and gestures, and sends control data over the network.
* **Computer Application** — receives the processed data and translates it into mouse movement, clicks, and scrolling.

The assignment also includes analyzing the application's runtime behavior and sensor processing using **Perfetto**.

## System Architecture

```text
┌──────────────────────┐
│   Android Phone      │
│                      │
│  Gyroscope           │
│  Accelerometer       │
│  Magnetometer        │
│         │            │
│         ▼            │
│  Calibration &       │
│  Sensor Processing   │
│         │            │
│         ▼            │
│  Movement / Gesture  │
│  Detection           │
└──────────┬───────────┘
           │
           │ UDP / JSON
           ▼
┌──────────────────────┐
│   Computer / Laptop  │
│                      │
│  UDP Receiver        │
│         │            │
│         ▼            │
│  Mouse Controller    │
│         │            │
│         ▼            │
│  Cursor / Click /    │
│  Scroll              │
└──────────────────────┘
```

## Features

### Cursor Movement

The phone's orientation and movement are used to control the mouse cursor.

* **Z-axis** → horizontal cursor movement
* **X-axis** → vertical cursor movement
* Movement is transmitted as changes (`DeltaX`, `DeltaY`) rather than absolute values.

### Left Click

A rapid rotation or movement around the **Y-axis** is detected as a left-click command.

### Scrolling

Rapid movement along the **Y-axis** is used for scrolling:

* Positive direction → scroll up
* Negative direction → scroll down

Thresholds and timing mechanisms are used to reduce false detections caused by hand movement and sensor noise.

## Sensor Processing

The project uses raw sensor data and performs its own calibration and processing rather than relying on Android's pre-calibrated sensor outputs.

The main sensors are:

* Gyroscope
* Accelerometer
* Magnetometer

Calibration is used to reduce **noise, bias, and drift**. Sensor fusion and filtering techniques may be used to improve the stability and accuracy of the final output.

The sensitivity of the mouse should be configurable so that the system can be adjusted during testing.

## Communication

The Android application communicates with the computer using:

* **UDP**
* **Programming Sockets**
* A predefined network port
* JSON or another structured message format

A typical message can contain values such as:

```text
DeltaX
DeltaY
Click
Scroll
```

Click and scroll packets require an **ACK** from the receiver. Motion-only packets may be dropped when necessary, while important gesture packets should be preserved and retransmitted if their delivery is not confirmed.

Both devices must be connected to the same local network.

## Android Application

The Android application provides:

* Sensor calibration
* Start/stop data transmission
* Real-time sensor values
* Visual indication of cursor movement
* Visual indication of click and scroll events
* Laptop IP address configuration

The application should remain usable across different screen sizes and target **Android 10 (API 29) and above**.

## Computer Application

The computer-side application:

1. Listens on the configured UDP port.
2. Receives sensor and gesture data.
3. Processes the received messages.
4. Moves the mouse cursor.
5. Performs left-click and scroll operations.
6. Sends acknowledgements for required gesture packets.

Python is suggested for this component, with libraries such as PyAutoGUI available for mouse control.

## Performance Analysis

The project also investigates the internal behavior of the Android application using **Perfetto**.

The analysis covers topics such as:

* Sensor data acquisition
* Sampling intervals
* Thread behavior
* System calls
* CPU execution time
* Sensor processing overhead
* Communication latency
* UI and rendering activity
* Differences between slow and sudden phone movements

Perfetto traces are used to support the answers to the assignment's performance and operating-system questions.

## Suggested Structure

```text
air-mouse-android/
├── android/
│   ├── app/
│   └── README.md
├── computer/
│   ├── receiver.py
│   └── README.md
├── perfetto/
│   ├── pbtx.config
│   └── traces/
├── docs/
│   └── report.pdf
├── media/
│   └── demo.mp4
└── README.md
```

## Technologies

* Kotlin / Java
* Android Studio
* Android Sensors API
* UDP / Programming Sockets
* JSON
* Python
* PyAutoGUI
* Perfetto
* Python Perfetto Trace Processor

## Documentation

The `docs/` directory contains the project report, analysis, and answers to the assignment questions.

The `perfetto/` directory contains the configuration and trace-related files used for performance analysis.

## Team

This project was developed as a group assignment. A project breakdown document is included to describe each member's responsibilities and contributions, as required by the assignment.
team members = [Seyed Navid Hashemi, Seyed Mohammad Hosein Mazhari, Seyed Mohammad Jazayeri, Amirarsalan Shahbazi]
