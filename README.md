# DPCS-SimultaneousTx-Rx-BLE-Demo
A minimal three-node Zephyr reference implementation of Dynamic Primary Channel Scanning (DPCS) for ultra-low-power relaying, showcasing simultaneous RX/TX operation at the intermediate relay node.

# BLE DPCS Simultaneous TX/RX Demo

A minimal three-node Zephyr implementation demonstrating:

- Dynamic Primary Channel Scanning (DPCS)
- Periodic Advertising Synchronization
- Simultaneous reception and transmission at a relay node
- Manufacturer Specific AD structure handling

This demo is intentionally simple and designed to clearly illustrate the core principle of DPCS.

---

## 🧠 What This Demo Shows

The second node (relay) demonstrates the basic principle of **Dynamic Primary Channel Scanning (DPCS)**:

- Primary channel scanning is disabled immediately after periodic synchronization is established.
- Primary channel scanning is automatically re-enabled when synchronization is terminated.
- The relay performs simultaneous periodic reception and periodic transmission.

Transmission at the relay node is not activated until synchronization with Node-1 is established.

---

## 📂 Repository Structure
DPCS_Node1_Tx/
DPCS_Node2_Relay/
DPCS_Node3_Rx/


Each folder is a standalone Zephyr application.

---

## 🛠 Supported Hardware

Tested and guaranteed for:

- nRF52832
- nRF52840
- nRF5340  

(Nordic DKs or custom boards using these SoCs)

---

## 📡 Static Addresses Used

Random Static advertising addresses:

- **Node-1:** `DE:AD:BE:AF:BA:11`
- **Node-2:** `D2:F0:F4:22:53:28`
- **Node-3:** `D2:F4:F4:F4:53:28`

Nodes use Filter Accept List (whitelisting).

---

## ⚙️ Configuration Parameters

### Advertising (Node-1 and Node-2)

- Periodic Advertising Interval: **1 second**
- Extended Advertising Interval: **500 ms**

### Scanning (Node-2 and Node-3)

- Scan Interval: **100 ms**
- Scan Window: **100 ms**
- Sync Timeout: **5 seconds**

The sync timeout is deliberately kept small to clearly demonstrate DPCS behavior.

---

## 📦 Manufacturer Specific AD Structure

Data is transmitted using a Manufacturer Specific AD structure inside the `AdvData` field.

Structure:

- Length (1 byte, added by controller)
- AD Type (1 byte = `0xFF`)
- Company Identifier (2 bytes = `0x0059`)
- Application Data (4 bytes)

Example log output:
7 ff 59 0 XX 1 2 3


Where:

- `AA 01 02 03` → Application data
- The first byte (`AA`) is incremented at every transmission from Node-1

Node-1 transmits 4 bytes of application data.  
Node-2 extracts only the application data and reconstructs the Manufacturer Specific AD structure before forwarding.

---

## 🚀 How to Run the Demo

### Step 1: Flash the Boards

Flash the three folders onto three separate boards:

- `DPCS_Node1_Tx` → mark as Node-1
- `DPCS_Node2_Relay` → mark as Node-2
- `DPCS_Node3_Rx` → mark as Node-3

---

### Step 2: Power-On Sequence

Power on **Node-2 and Node-3 first**.  
Power on **Node-1 last**.

Node-1 triggers Node-2 advertising.

---

### Step 3: Observe Initial Synchronization

At Node-2 and Node-3:

- A sync established log appears.
- Primary channel scanning is disabled (`bt_le_scan_stop()`).
- Periodic reception continues in known slots.
- Node-2 starts transmitting its own periodic advertising.

Node-3 receives exactly the same values seen at Node-2, every 1 second.

---

### Step 4: Demonstrate Sync Termination

Turn OFF Node-1.

After 5 seconds:

- Node-2 logs a sync terminated message.
- Primary channel scanning is automatically re-enabled (`bt_le_scan_start()`).

At this stage:

- Node-3 continues receiving packets from Node-2.
- The data does not change (because Node-2 is not receiving new data).

This clearly demonstrates DPCS behavior.

---

### Step 5: Re-establish Sync

Turn Node-1 ON again.

You will observe:

- Node-2 re-enables scanning
- Sync gets re-established
- Fresh data propagation resumes
- Sync loss counter increments

This process can be repeated multiple times.

---

## 🔁 Sync Loss Counter

Node-2 maintains a counter tracking how many times synchronization has been lost.

Each time Node-1 is turned off and on again, this counter increments.

A similar sync-loss scenario can also be demonstrated between Node-2 and Node-3.

---

## 🎯 Summary

This demo shows:

- Practical realization of Dynamic Primary Channel Scanning
- Autonomous scanning enable/disable based on sync state
- Simultaneous periodic RX/TX at a relay node
- Deterministic relaying with minimal implementation

The middle node performs simple forwarding without concatenation.

---

## 📜 License

Apache-2.0
