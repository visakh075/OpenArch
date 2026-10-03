# OpenArch Example Architecture Databases

This directory contains curated example architecture databases designed to demonstrate OpenArch's edge routing algorithms, connection topologies, multi-tier containers, bridge crossings, parallel edge separations, and governance workflows.

Each architecture is provided in both **relational SQLite (`.db`)** and **human-readable JSON (`.json`)** formats, ready to load in the OpenArch GUI or CLI.

---

## Example Databases

### 1. `edge_routing_showcase` (`.db` / `.json`)
**The Complete Visual Routing Reference Gallery**

A visual gallery showcasing **all 7 edge routing algorithms** side-by-side in distinct, purpose-built testing zones:

| Zone | Algorithm | Style Identifier | Key Visual Characteristics |
| :--- | :--- | :--- | :--- |
| **Zone 1** | **Smart Orthogonal** | `smart` | Dynamic A* obstacle-free pathfinding with rounded 90° corners routing cleanly around intermediate nodes. |
| **Zone 2** | **Direct Orthogonal** | `direct` | Classic Manhattan stepped orthogonal routing (L-step and Z-step) with rounded corner transitions. |
| **Zone 3** | **Circuit Board** | `circuit` | Sharp 90° rectilinear PCB trace routing with zero corner radius, matching electronic board designs. |
| **Zone 4** | **Smooth Curved** | `bezier` | Cubic Bezier spline with continuous curvature and organic S-curve transitions. |
| **Zone 5** | **Straight Line** | `straight` | Direct point-to-point shortest vector between ports, ideal for high-frequency or direct interconnects. |
| **Zone 6** | **Octilinear** | `octilinear` | 45-degree chamfered transit / metro-map angles for modern, high-density diagrams. |
| **Zone 7** | **Bus Highway** | `bus` | Central backbone highway trunk routing where multiple nodes tap into a shared spine channel. |
| **Zone 8** | **Bridge Hops** | `direct` | Crossing orthogonal lines automatically render semi-circular line jump arcs on intersections. |
| **Zone 9** | **Parallel Separation** | `direct` | Multiple parallel edges between the same node pair are distributed across offset channels without overlap. |

---

### 2. `enterprise_cloud_architecture` (`.db` / `.json`)
**Full-Scale Multi-Tier Production Microservices Platform**

A production-grade e-commerce cloud deployment demonstrating nested container hierarchies, cross-tier communication, governance statuses, and diverse network protocols:

- **Ingress & DMZ Tier** (Container):
  - Cloudflare Edge CDN & Anycast
  - WAF & DDoS Shield
  - Traefik Ingress Gateway
- **Service Mesh & Kubernetes Cluster** (Container):
  - Auth & Identity Service (`OAuth 2.1 / OIDC`)
  - Product Catalog Service
  - Order Orchestration (`Saga Transaction Pattern`)
  - Payment Vault Service (`PCI-DSS Level 1`)
- **Event Mesh & Worker Cluster** (Container):
  - Kafka Event Broker (`32 Partitions`)
  - Notification Dispatcher Worker
  - Inventory Sync Worker
- **Persistence & Cache Tier** (Container):
  - PostgreSQL Primary (Read/Write) & Standby (Read-Only)
  - Redis Distributed Cache (`RESP Protocol`)
  - Elasticsearch Cluster (`Vector Search`)
- **Edge Types & Protocols Used**:
  - `HTTPS / TLS 1.3` (Smooth Bezier)
  - `HTTP/3 QUIC (UDP)` (Smooth Bezier)
  - `gRPC / HTTP2` (Smart Orthogonal avoiding DMZ boundaries)
  - `Kafka Topic Streams` (Bus Highway Trunk)
  - `WAL Sync Replication` (Straight Line Vector)
  - `RESP / Redis Pool` (Direct Orthogonal)
  - `Elasticsearch Query` (45° Octilinear)
  - `Stripe API & Datadog OTLP` (Smart Orthogonal & Octilinear)

---

### 3. `embedded_iot_system` (`.db` / `.json`)
**Robotics, IoT Hardware & Vehicle Telemetry Architecture**

An embedded automotive and drone robotics system demonstrating board-level hardware traces, sensor buses, power rails, and wireless comms:

- **Core MCU & Memory Board** (Container):
  - STM32H753 Dual-Core MCU (Arm Cortex-M7 + M4 @ 480MHz)
  - 64MB QSPI NOR Flash (133MHz Quad-SPI)
  - ATECC608 Crypto Hardware Root of Trust
- **Precision Sensor Hub** (Container):
  - 6-Axis IMU (BMI088, 1.6kHz)
  - High-Precision Barometer (BMP390)
  - Optical Flow Camera (PMW3901, 120fps)
- **Actuator & Motor Control** (Container):
  - Dual BLDC Motor Driver (DRV8305, 40kHz PWM)
  - High-Speed Current Shunt ADC (1 MSPS)
  - SIL-3 Emergency Stop Hardware Circuit
- **Peripherals & Radio**:
  - Solid-State 16-Beam LiDAR (3M bps UART/DMA)
  - RTK GNSS GPS Module (20Hz Precision Navigation)
  - Wi-Fi 6 & BLE 5.3 Gateway (ESP32-S3)
  - AWS IoT Core Cloud Telemetry
  - CAN-FD 2.0B Isolated Vehicle Bus (1 Mbps)
- **Hardware Edge Routing Styles**:
  - `Circuit Board (Sharp 90°)`: QSPI memory bus, complementary PWM motor lines, I2C traces.
  - `Bus Highway`: Shared I2C multi-sensor bus backbone.
  - `Straight Vector`: High-speed SPI clock lines, analog current feedback loops.
  - `Octilinear (45°)`: High-bandwidth LiDAR DMA feed.
  - `Smooth Bezier`: 2.4GHz / 5GHz wireless RF radio emissions to Cloud telemetry.

---

## How to Open in OpenArch

### Graphical User Interface (GUI)
1. Launch OpenArch:
   ```bash
   ./build/openarch-gui
   ```
2. Navigate to **File** &rarr; **Open Architecture Database...** (or press `Ctrl+O`).
3. Select any `.db` or `.json` file from the `examples/` directory:
   - `examples/edge_routing_showcase.db`
   - `examples/enterprise_cloud_architecture.db`
   - `examples/embedded_iot_system.db`

### Command-Line Interface (CLI)
Inspect or convert the architectures directly from the terminal:
```bash
# Print summary of nodes, layers, and edges
./build/openarch examples/edge_routing_showcase.db

# Convert SQLite to JSON
./build/openarch convert examples/edge_routing_showcase.db examples/edge_routing_showcase.json

# Convert JSON to SQLite
./build/openarch convert examples/enterprise_cloud_architecture.json examples/enterprise_cloud_architecture.db
```

### Interactive HTML Export
Once opened in the GUI, select **File** &rarr; **Export** &rarr; **Interactive HTML...** (or shortcut) to generate a standalone interactive SVG viewer preserving all 7 routing algorithms, cubic bezier curves, and bridge hop crossings.
