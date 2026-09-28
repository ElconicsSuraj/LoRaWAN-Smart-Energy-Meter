# LoRaWAN-Smart-Energy-Meter
A compact LoRaWAN-based energy monitoring system using the PZEM-004T V4 100A and Seeed Studio Wio-E5 Mini to remotely monitor voltage, current, power, energy consumption, power factor, and frequency.

The system uses the PZEM-004T V4 100A Energy Monitoring Module for electrical measurements and a Seeed Studio Wio-E5 Mini LoRaWAN board for long-range wireless communication.

It is suitable for applications such as industrial energy monitoring, smart buildings, agricultural equipment, commercial power monitoring, machine monitoring, and IoT-based energy management.

🚀 Features
⚡ Real-time voltage measurement
🔌 AC current measurement up to 100A
📊 Active power monitoring
🔋 Energy consumption measurement
📈 Power factor monitoring
📡 LoRaWAN long-range communication
🌐 Remote energy monitoring
🔧 Compact IoT node architecture
🔋 Suitable for low-power remote deployments
🏭 Suitable for industrial and commercial applications
🧩 Hardware Used
Component	Description
Seeed Studio Wio-E5 Mini	LoRaWAN communication controller
PZEM-004T V4 100A	AC voltage, current, power and energy measurement
100A CT	Non-invasive current measurement
Power Supply	Provides power to the monitoring node
Antenna	LoRa/LoRaWAN communication
Main Hardware

PZEM-004T V4 – 100A

Used to measure:

Voltage
Current
Active Power
Energy
Power Factor
Frequency

Wio-E5 Mini

Used as the wireless communication controller to transmit energy data over a LoRaWAN network.

🏗️ System Architecture
                 AC LOAD
                    │
                    │
             ┌──────▼──────┐
             │    100A CT  │
             └──────┬──────┘
                    │
                    ▼
          ┌───────────────────┐
          │   PZEM-004T V4    │
          │    Energy Meter   │
          │                   │
          │ V / A / W / kWh   │
          │ PF / Frequency    │
          └─────────┬─────────┘
                    │
                  UART
                    │
                    ▼
          ┌───────────────────┐
          │   Wio-E5 Mini     │
          │    LoRaWAN Node   │
          └─────────┬─────────┘
                    │
                 LoRaWAN
                    │
                    ▼
             ┌──────────────┐
             │ LoRaWAN      │
             │ Gateway      │
             └──────┬───────┘
                    │
                    ▼
             ┌──────────────┐
             │ LoRaWAN      │
             │ Network      │
             │ Server       │
             └──────┬───────┘
                    │
                    ▼
             ┌──────────────┐
             │ Dashboard /  │
             │ Cloud        │
             └──────────────┘
📡 LoRaWAN Communication

The Wio-E5 Mini communicates with a LoRaWAN gateway and transmits measured electrical parameters to the LoRaWAN network server.

Typical communication flow:

Energy Meter
     ↓
Wio-E5 Mini
     ↓
LoRaWAN
     ↓
Gateway
     ↓
LoRaWAN Network Server
     ↓
MQTT / API
     ↓
Dashboard

The device can be integrated with LoRaWAN network servers such as:

ChirpStack
The Things Stack
Other compatible LoRaWAN platforms
📊 Parameters

The device can transmit the following parameters:

Parameter	Unit
Voltage	V
Current	A
Active Power	W
Energy	kWh
Power Factor	PF
Frequency	Hz

Example payload:

{
  "voltage": 230.5,
  "current": 4.82,
  "power": 1087.4,
  "energy": 12.64,
  "power_factor": 0.98,
  "frequency": 50.0
}
🔄 Operating Flow
The PZEM-004T measures electrical parameters.
The Wio-E5 Mini communicates with the PZEM module.
Measurement data is processed by the firmware.
The data is formatted into a compact LoRaWAN payload.
The Wio-E5 Mini transmits the payload through LoRaWAN.
The LoRaWAN gateway receives the packet.
The network server processes the data.
Data can be forwarded to MQTT, database or dashboard applications.
🔌 PZEM-004T Interface

The PZEM-004T communicates with the controller using a serial interface.

Typical interface:

PZEM-004T          Wio-E5 Mini
-----------        -----------
TX      ----------> RX2
RX      <---------- TX3
GND     ----------- GND
VCC     ----------- Supply

Important: Verify the voltage levels and exact pin configuration of your PZEM-004T and Wio-E5 Mini hardware before connecting them.