/**
 * ChirpStack v4 / v3 JavaScript Payload Codec for PZEM-004T Energy Meter & Theft Alarm Node
 *
 * Supported Uplink Messages:
 *  - 0xA1: Periodic Temp/Humidity Message (13 bytes)
 *  - 0xA2: Configuration Retrieval Uplink (8 bytes)
 *  - 0xA3: Battery Calibration Response (2 bytes)
 *  - 0xA4: Erase Samples ACK (2 bytes)
 *  - 0xA5: Theft / Vibration Alarm Alert (7 bytes)
 *  - 0xA6: PZEM-004T-100A(V4.0) Energy Meter Reading (21 bytes)
 */

// Decode uplink function for ChirpStack v4
function decodeUplink(input) {
    var bytes = input.bytes;
    var fPort = input.fPort;
    var data = {};

    if (!bytes || bytes.length === 0) {
        return { data: data };
    }

    var msgType = bytes[0];

    switch (msgType) {
        // PZEM-004T Energy Meter Message (21 bytes)
        case 0xA6:
            data.message_type = "ENERGY_METER_MEASUREMENT";
            data.msg_id_hex = "0xA6";

            // Epoch time (4 bytes, big endian, unsigned 32-bit)
            var epoch = ((bytes[1] << 24) | (bytes[2] << 16) | (bytes[3] << 8) | bytes[4]) >>> 0;
            data.epoch_time = epoch;
            data.timestamp = (epoch > 0) ? new Date(epoch * 1000).toISOString() : null;

            // Voltage (2 bytes, 0.1 V resolution)
            var raw_v = (bytes[5] << 8) | bytes[6];
            data.voltage_v = Number((raw_v / 10.0).toFixed(1));

            // Current (3 bytes, 1 mA resolution)
            var raw_i = (bytes[7] << 16) | (bytes[8] << 8) | bytes[9];
            data.current_a = Number((raw_i / 1000.0).toFixed(3));

            // Power (3 bytes, 0.1 W resolution)
            var raw_p = (bytes[10] << 16) | (bytes[11] << 8) | bytes[12];
            data.power_w = Number((raw_p / 10.0).toFixed(1));

            // Energy (4 bytes, 1 Wh resolution, unsigned 32-bit)
            var raw_e = ((bytes[13] << 24) | (bytes[14] << 16) | (bytes[15] << 8) | bytes[16]) >>> 0;
            data.energy_wh = raw_e;
            data.energy_kwh = Number((raw_e / 1000.0).toFixed(3));

            // Frequency (2 bytes, 0.1 Hz resolution)
            var raw_f = (bytes[17] << 8) | bytes[18];
            data.frequency_hz = Number((raw_f / 10.0).toFixed(1));

            // Power Factor (1 byte, 0.01 resolution)
            var raw_pf = bytes[19];
            data.power_factor = Number((raw_pf / 100.0).toFixed(2));

            // Battery % (1 byte)
            data.battery_percent = bytes[20];
            break;

        // Vibration / Theft Alarm Message (7 bytes)
        case 0xA5:
            data.message_type = "VIBRATION_THEFT_ALERT";
            data.msg_id_hex = "0xA5";
            var v_epoch = ((bytes[1] << 24) | (bytes[2] << 16) | (bytes[3] << 8) | bytes[4]) >>> 0;
            data.epoch_time = v_epoch;
            data.timestamp = (v_epoch > 0) ? new Date(v_epoch * 1000).toISOString() : null;
            data.vibration_detected = (bytes[5] === 0x01);
            data.battery_percent = bytes[6];
            break;

        // Legacy Periodic Temperature & Humidity Message (13 bytes)
        case 0xA1:
            data.message_type = "TEMP_HUMIDITY_PERIODIC";
            data.msg_id_hex = "0xA1";
            var t_epoch = ((bytes[1] << 24) | (bytes[2] << 16) | (bytes[3] << 8) | bytes[4]) >>> 0;
            data.epoch_time = t_epoch;
            data.timestamp = (t_epoch > 0) ? new Date(t_epoch * 1000).toISOString() : null;
            data.sample_number = ((bytes[5] << 24) | (bytes[6] << 16) | (bytes[7] << 8) | bytes[8]) >>> 0;
            var raw_temp = (bytes[9] << 8) | bytes[10];
            data.temperature_c = Number((raw_temp / 10.0).toFixed(1));
            data.humidity_percent = bytes[11];
            data.battery_percent = bytes[12];
            break;

        // Configuration Retrieval Message (8 bytes)
        case 0xA2:
            data.message_type = "CONFIG_RETRIEVAL_RESPONSE";
            data.msg_id_hex = "0xA2";
            data.battery_counter = ((bytes[1] << 24) | (bytes[2] << 16) | (bytes[3] << 8) | bytes[4]) >>> 0;
            data.sample_interval_sec = (bytes[5] << 8) | bytes[6];
            data.battery_percent = bytes[7];
            break;

        // Battery Calibration Response (2 bytes)
        case 0xA3:
            data.message_type = "BATTERY_CALIBRATION_ACK";
            data.msg_id_hex = "0xA3";
            data.battery_percent = bytes[1];
            break;

        // Erase Samples Response (2 bytes)
        case 0xA4:
            data.message_type = "ERASE_SAMPLES_ACK";
            data.msg_id_hex = "0xA4";
            data.battery_percent = bytes[1];
            break;

        default:
            data.message_type = "UNKNOWN";
            data.raw_bytes = bytes;
            break;
    }

    return {
        data: data
    };
}

// ChirpStack v3 compatibility Decode function
function Decode(fPort, bytes, variables) {
    return decodeUplink({ bytes: bytes, fPort: fPort }).data;
}
