"""Generate Protocol Specification .docx document."""

from docx import Document
from docx.shared import Inches, Pt, RGBColor
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn

doc = Document()

style = doc.styles['Normal']
font = style.font
font.name = 'Calibri'
font.size = Pt(10)

# Helper functions
def add_table(doc, headers, rows, col_widths=None):
    table = doc.add_table(rows=1 + len(rows), cols=len(headers))
    table.style = 'Table Grid'
    table.alignment = WD_TABLE_ALIGNMENT.LEFT

    # Header row
    for i, h in enumerate(headers):
        cell = table.rows[0].cells[i]
        cell.text = h
        for p in cell.paragraphs:
            for run in p.runs:
                run.bold = True
                run.font.size = Pt(9)
        # Shade header
        shading = cell._element.get_or_add_tcPr()
        shading_elm = shading.makeelement(qn('w:shd'), {
            qn('w:fill'): '2F5496',
            qn('w:val'): 'clear'
        })
        shading.append(shading_elm)
        for p in cell.paragraphs:
            for run in p.runs:
                run.font.color.rgb = RGBColor(255, 255, 255)

    # Data rows
    for r_idx, row in enumerate(rows):
        for c_idx, val in enumerate(row):
            cell = table.rows[r_idx + 1].cells[c_idx]
            cell.text = str(val)
            for p in cell.paragraphs:
                for run in p.runs:
                    run.font.size = Pt(9)

    if col_widths:
        for i, w in enumerate(col_widths):
            for row in table.rows:
                row.cells[i].width = Inches(w)

    doc.add_paragraph()
    return table


def h1(text):
    doc.add_heading(text, level=1)

def h2(text):
    doc.add_heading(text, level=2)

def h3(text):
    doc.add_heading(text, level=3)

def para(text):
    doc.add_paragraph(text)

def bold_para(label, text):
    p = doc.add_paragraph()
    run = p.add_run(label)
    run.bold = True
    run.font.size = Pt(10)
    run2 = p.add_run(text)
    run2.font.size = Pt(10)


# ===== DOCUMENT CONTENT =====

# Title
title = doc.add_heading('LoRaWAN Temperature/Humidity Sensor', level=0)
doc.add_heading('Protocol Specification', level=1)

para('')

# Document Information
add_table(doc,
    ['Field', 'Value'],
    [
        ['Document Title',  'LoRaWAN Temperature/Humidity Sensor \u2013 Protocol Specification'],
        ['Author',          'Amogh MP'],
        ['Organisation',    'ANTZ Systems'],
        ['Status',          'Released'],
        ['Current Version', '1.1'],
    ],
    col_widths=[2.0, 4.5]
)

# Revision History
h2('Revision History')

add_table(doc,
    ['Version', 'Date', 'Author', 'Description'],
    [
        ['1.0', '2026-03-15', 'Amogh MP', 'Initial release \u2013 periodic data (0xA1), config report (0xA2), battery calibration (0xA3/0xB3), update interval (0xB1), fetch config (0xB2)'],
        ['1.1', '2026-03-20', 'Amogh MP', 'Added erase pending samples downlink (0xB4) and erase samples ACK uplink (0xA4)'],
    ],
    col_widths=[0.8, 1.0, 1.2, 3.5]
)


# LoRaWAN Parameters
h1('1. LoRaWAN Parameters')

add_table(doc,
    ['Parameter', 'Value'],
    [
        ['Activation', 'OTAA'],
        ['Class', 'A'],
        ['Spreading Factor', 'SF12 (ADR enabled)'],
        ['Application Port', '2'],
        ['Confirmation', 'Confirmed uplinks (ACK required)'],
    ],
    col_widths=[2.5, 4.0]
)


# Uplink Messages
h1('2. Uplink Messages')
para('All uplinks are sent on port 2 as confirmed messages.')

# --- 0xA1 ---
h2('2.1 Periodic Sensor Data (0xA1)')
bold_para('Size: ', '13 bytes')
bold_para('Trigger: ', 'TxTimer fires every upload_interval seconds (default 10s) when pending samples exist in flash.')

add_table(doc,
    ['Byte', 'Field', 'Type', 'Encoding'],
    [
        ['0',    'Message ID',    'uint8',     '0xA1'],
        ['1-4',  'Epoch Time',    'uint32 BE', 'Unix timestamp (seconds)'],
        ['5-8',  'Sample Number', 'uint32 BE', 'Monotonically incrementing sequence number'],
        ['9-10', 'Temperature',   'int16 BE',  'Value in 0.1\u00b0C units (e.g. 205 = 20.5\u00b0C)'],
        ['11',   'Humidity',      'uint8',     '0\u2013100 (%)'],
        ['12',   'Battery',       'uint8',     '0\u2013100 (%)'],
    ],
    col_widths=[0.6, 1.3, 1.0, 3.6]
)

h3('Encoding Details')
bold_para('Temperature: ', 'Sensor reading multiplied by 10. Error sentinel: 1000 (represents 100.0\u00b0C, indicates I2C sensor failure).')
bold_para('Humidity: ', 'Direct percentage (0\u2013100). Error value: 0.')
bold_para('Battery formula: ', '100 - ((battery_counter \u00d7 100) / 50000), clamped to 1\u2013100%.')
bold_para('Data source: ', 'Read from flash via read_next_sample() at the read_counter position. This is the oldest un-ACKed stored sample, not a live sensor reading.')

h3('Example')
para('Hex payload: A1 67B44340 00000010 00CD 64 4E')
add_table(doc,
    ['Field', 'Hex', 'Decoded'],
    [
        ['Message ID',    'A1',       '0xA1 (Periodic)'],
        ['Epoch Time',    '67B44340', '1739932480'],
        ['Sample Number', '00000010', '16'],
        ['Temperature',   '00CD',     '205 \u2192 20.5\u00b0C'],
        ['Humidity',      '64',       '100%'],
        ['Battery',       '4E',       '78%'],
    ],
    col_widths=[1.5, 1.5, 3.5]
)


# --- 0xA2 ---
h2('2.2 Device Configuration Report (0xA2)')
bold_para('Size: ', '8 bytes')
bold_para('Trigger: ', 'In response to downlink 0xB1 (config update) or 0xB2 (config fetch request).')

add_table(doc,
    ['Byte', 'Field', 'Type', 'Encoding'],
    [
        ['0',   'Message ID',      'uint8',     '0xA2'],
        ['1-4', 'Battery Counter',  'uint32 BE', 'Total TX count since last calibration'],
        ['5-6', 'Sample Interval',  'uint16 BE', 'Current sampling period in seconds'],
        ['7',   'Battery',          'uint8',     '0\u2013100 (%)'],
    ],
    col_widths=[0.6, 1.3, 1.0, 3.6]
)


# --- 0xA3 ---
h2('2.3 Battery Calibration ACK (0xA3)')
bold_para('Size: ', '2 bytes')
bold_para('Trigger: ', 'In response to downlink 0xB3 (battery calibration request).')

add_table(doc,
    ['Byte', 'Field', 'Type', 'Encoding'],
    [
        ['0', 'Message ID', 'uint8', '0xA3'],
        ['1', 'Battery',    'uint8', '0\u2013100 (%)'],
    ],
    col_widths=[0.6, 1.3, 1.0, 3.6]
)

bold_para('Side effect: ', 'On successful ACK reception from the network server, battery_counter is reset to 0 in flash.')


# --- 0xA4 ---
h2('2.4 Erase Samples ACK (0xA4)')
bold_para('Size: ', '2 bytes')
bold_para('Trigger: ', 'In response to downlink 0xB4 (erase pending samples request).')

add_table(doc,
    ['Byte', 'Field', 'Type', 'Encoding'],
    [
        ['0', 'Message ID', 'uint8', '0xA4'],
        ['1', 'Battery',    'uint8', '0\u2013100 (%)'],
    ],
    col_widths=[0.6, 1.3, 1.0, 3.6]
)

bold_para('Side effect: ', 'All pending samples are cleared (write_counter and read_counter reset to 0). battery_counter is preserved.')


# Downlink Commands
h1('3. Downlink Commands')
para('All downlinks are received on the application port.')

# --- 0xB1 ---
h2('3.1 Update Sampling Interval (0xB1)')
bold_para('Size: ', '3 bytes')

add_table(doc,
    ['Byte', 'Field', 'Type', 'Encoding'],
    [
        ['0',   'Command ID',   'uint8',     '0xB1'],
        ['1-2', 'New Interval', 'uint16 BE', 'Sampling period in seconds'],
    ],
    col_widths=[0.6, 1.3, 1.0, 3.6]
)

bold_para('Validation: ', 'Accepted only if the interval is in range [30, 65534]. Values outside this range are silently rejected.')
bold_para('On acceptance: ', 'New interval is saved to flash, SampleTimer is restarted with the new period, and device responds with 0xA2 configuration report.')

h3('Example')
para('Downlink B1 003C \u2192 Set sampling interval to 60 seconds.')
para('Downlink B1 000E \u2192 Rejected (14 < 30 minimum).')


# --- 0xB2 ---
h2('3.2 Fetch Device Configuration (0xB2)')
bold_para('Size: ', '1 byte')

add_table(doc,
    ['Byte', 'Field', 'Type', 'Encoding'],
    [
        ['0', 'Command ID', 'uint8', '0xB2'],
    ],
    col_widths=[0.6, 1.3, 1.0, 3.6]
)

bold_para('Response: ', 'Device sends 0xA2 configuration report on the next TxTimer event.')


# --- 0xB3 ---
h2('3.3 Battery Calibration (0xB3)')
bold_para('Size: ', '2 bytes')

add_table(doc,
    ['Byte', 'Field', 'Type', 'Encoding'],
    [
        ['0', 'Command ID', 'uint8', '0xB3'],
        ['1', 'Reserved',   'uint8', '(ignored)'],
    ],
    col_widths=[0.6, 1.3, 1.0, 3.6]
)

bold_para('Response: ', 'Device sends 0xA3 battery calibration ACK. On successful network ACK, battery_counter resets to 0.')


# --- 0xB4 ---
h2('3.4 Erase Pending Samples (0xB4)')
bold_para('Size: ', '1 byte')

add_table(doc,
    ['Byte', 'Field', 'Type', 'Encoding'],
    [
        ['0', 'Command ID', 'uint8', '0xB4'],
    ],
    col_widths=[0.6, 1.3, 1.0, 3.6]
)

bold_para('Action: ', 'Clears all pending samples by resetting write_counter and read_counter to 0. battery_counter is preserved. sample_number resets to 0 for fresh sequencing.')
bold_para('Response: ', 'Device sends 0xA4 erase samples ACK.')


# Flash Storage
h1('4. Flash Storage Layout')

h2('4.1 Device Configuration (0x0802D000)')
bold_para('Size: ', '32 bytes')

add_table(doc,
    ['Offset', 'Field', 'Size', 'Default'],
    [
        ['0\u20133',   'battery_counter',          'uint32', '0'],
        ['4\u20137',   'write_counter',             'uint32', '0'],
        ['8\u201311',  'read_counter',              'uint32', '0'],
        ['12\u201313', 'new_program',               'uint16', '0xAA after first init'],
        ['14\u201315', 'sample_interval',           'uint16', '30 (seconds)'],
        ['16\u201317', 'upload_interval',           'uint16', '10 (seconds)'],
        ['18',         'is_reset_battery_counter',  'uint8',  '0'],
        ['19\u201331', 'padding',                   '13 bytes', '\u2014'],
    ],
    col_widths=[0.8, 2.2, 1.0, 2.5]
)


h2('4.2 Sample Ring Buffer (0x08030000)')

add_table(doc,
    ['Parameter', 'Value'],
    [
        ['Base address',        '0x08030000'],
        ['Page size',           '2048 bytes'],
        ['Number of pages',     '10'],
        ['Sample size',         '32 bytes'],
        ['Samples per page',    '64'],
        ['Total capacity',      '640 samples'],
        ['Flash full threshold', '576 (MAX_SAMPLES \u2212 SAMPLES_PER_PAGE)'],
    ],
    col_widths=[2.5, 4.0]
)

h3('Sample Data Structure (32 bytes)')

add_table(doc,
    ['Offset', 'Field', 'Size', 'Description'],
    [
        ['0\u20133',   'epoch_time',     'uint32', 'Unix timestamp at sampling time'],
        ['4\u20137',   'sample_number',  'uint32', 'Monotonic sequence number'],
        ['8\u20139',   'temperature',    'uint16', 'Temperature \u00d7 10'],
        ['10',         'humidity',       'uint8',  'Humidity percentage (0\u2013100)'],
        ['11\u201331', 'reserved',       '21 bytes', 'Padding to 32-byte boundary'],
    ],
    col_widths=[0.8, 1.5, 1.0, 3.2]
)

para('Samples are written sequentially using write_counter % 640 as the ring index. Pages are erased just before being overwritten.')


# Timing
h1('5. Operational Timing')

add_table(doc,
    ['Timer', 'Interval', 'Purpose'],
    [
        ['SampleTimer',       '30s (configurable via 0xB1)', 'Trigger sensor read + flash store'],
        ['TxTimer',           '10s',                          'Transmit oldest pending sample'],
        ['LinkCheckTimer',    '120s (2 minutes)',             'Retry link verification after TX failure'],
        ['ClockSyncTimer',    '10s initially, then 8 hours',  'LoRaWAN AppTimeReq time synchronization'],
        ['JoinBackoffTimer',  '2s \u2192 doubles \u2192 max 1h', 'Exponential backoff on join failure'],
        ['IWDGRefreshTimer',  '25s',                          'Independent watchdog refresh'],
    ],
    col_widths=[1.8, 2.0, 2.7]
)


# State Machine
h1('6. State Machine & Recovery')

h2('6.1 Normal Operation')

add_table(doc,
    ['Condition', 'Action'],
    [
        ['No ACK on data TX',        'can_transmit_data = 0, TxTimer stops, LinkCheckTimer starts at 2-min intervals'],
        ['Link check succeeds',      'can_transmit_data = 1, TxTimer restarts'],
        ['15 link check failures',   'Reboot if power_up == 1 or pending samples >= 576'],
        ['Flash full + link down',   'Sampling paused until link recovers'],
        ['Join fails 14 times',      'Device reboots (~3 hours with exponential backoff)'],
    ],
    col_widths=[2.5, 4.0]
)


h2('6.2 Range Testing Mode (RANGE_TESTING_MODE = true)')
para('Compile-time mode for LoRaWAN range/penetration testing. Overrides normal recovery behavior:')

add_table(doc,
    ['Behavior', 'Description'],
    [
        ['No ACK received',         'Log only ("No ACK, range test mode"). TxTimer keeps running. Same packet retried on next TX.'],
        ['can_transmit_data check', 'Bypassed \u2014 TX fires whenever pending data exists regardless of link status.'],
        ['Sampling pause',          'Disabled \u2014 sampling never pauses regardless of flash state.'],
        ['LinkCheckTimer',          'Never started \u2014 link check and associated reboot paths are never reached.'],
        ['Time sync',               'Unchanged \u2014 operates normally.'],
    ],
    col_widths=[2.0, 4.5]
)

h3('Range Testing End-to-End Flow')
para('1. Device boots \u2192 joins \u2192 time syncs (unchanged)')
para('2. SampleTimer fires every 30s \u2192 stores sample \u2192 write_counter++')
para('3. TxTimer fires every 10s \u2192 reads oldest un-ACKed sample \u2192 transmits')
para('4. If ACK received: read_counter++ \u2192 next TxTimer fire sends next sample')
para('5. If no ACK received: read_counter stays \u2192 next TxTimer fire retries same sample')
para('6. No link check, no can_transmit_data=0, TxTimer keeps running continuously')


# Sensor encoding
h1('7. Sensor Encoding Reference')

h2('7.1 SHTC3 Onboard Sensor (Default)')
add_table(doc,
    ['Measurement', 'Formula', 'Output'],
    [
        ['Temperature', '((175 \u00d7 raw) / 65536) \u2212 45, then \u00d7 10', 'int16, 0.1\u00b0C units'],
        ['Humidity',    '(100 \u00d7 raw) / 65535',                                'uint8, 0\u2013100%'],
    ],
    col_widths=[1.5, 3.0, 2.0]
)
bold_para('I2C Address: ', '0xE0')
bold_para('Error fallback: ', 'temperature = 1000 (100.0\u00b0C sentinel), humidity = 0')

h2('7.2 SHT40 External Probe (Alternative)')
add_table(doc,
    ['Measurement', 'Formula', 'Output'],
    [
        ['Temperature', '\u221245.0 + 175.0 \u00d7 (raw / 65535.0), then \u00d7 10', 'int16, 0.1\u00b0C units'],
        ['Humidity',    '\u22126.0 + 125.0 \u00d7 (raw / 65535.0)',                     'uint8, 0\u2013100%'],
    ],
    col_widths=[1.5, 3.0, 2.0]
)
bold_para('I2C Address: ', '0x44')
bold_para('Measurement command: ', '0xFD (high precision)')

h2('7.3 DS18B20 External Probe (Alternative)')
add_table(doc,
    ['Measurement', 'Formula', 'Output'],
    [
        ['Temperature', 'raw / 16.0, then \u00d7 10', 'int16, 0.1\u00b0C units'],
        ['Humidity',    'N/A (temperature-only sensor)', 'Always 0'],
    ],
    col_widths=[1.5, 3.0, 2.0]
)
bold_para('Interface: ', '1-Wire (GPIO bit-banging)')


# Protocol Constants
h1('8. Protocol Constants Summary')

add_table(doc,
    ['Constant', 'Value', 'Source'],
    [
        ['PERIODIC_MESSAGE_SIZE',                    '13',    'protocol.h'],
        ['DEVICE_CONFIGURATION_ACK_MESSAGE_SIZE',    '8',     'protocol.h'],
        ['BATTERY_CALIBRATION_ACK_MESSAGE_SIZE',     '2',     'protocol.h'],
        ['UPDATE_DEVICE_CONFIGURATION_DOWNLINK_SIZE','3',     'protocol.h'],
        ['FETCH_DEVICE_CONFIGURATION_DOWNLINK_SIZE', '1',     'protocol.h'],
        ['BATTERY_CALIBERATION_DOWNLINK_SIZE',       '2',     'protocol.h'],
        ['ERASE_SAMPLES_ACK_MESSAGE_SIZE',           '2',     'protocol.h'],
        ['ERASE_SAMPLES_DOWNLINK_SIZE',              '1',     'protocol.h'],
        ['NUMBER_OF_MESSAGES_PER_BATTERY_LIFE',      '50000', 'protocol.h'],
        ['DEFAULT_SAMPLING_INTERVAL_IN_SECONDS',     '30',    'application_logic.h'],
        ['DEFAULT_UPLOAD_INTERVAL_IN_SECONDS',       '10',    'application_logic.h'],
        ['DEFAULT_MINIMUM_SAMPLING_INTERVAL_IN_SEC', '30',    'application_logic.h'],
        ['DEFAULT_MAXIMUM_SAMPLING_INTERVAL_IN_SEC', '65535', 'application_logic.h'],
        ['LORAWAN_MAX_RETRY_JOIN_ATTEMPTS',          '14',    'application_logic.h'],
        ['LINK_CHECK_RETRY_INTERVAL_MS',             '120000','application_logic.h'],
        ['LINK_CHECK_MAX_RETRY_ATTEMPTS',            '15',    'application_logic.h'],
        ['FLASH_FULL_THRESHOLD',                     '576',   'application_logic.h'],
        ['MAX_SAMPLES',                              '640',   'eeprom.h'],
        ['SAMPLES_PER_PAGE',                         '64',    'eeprom.h'],
        ['JANUARY_1_2025_EPOCH',                     '1735689600', 'application_logic.h'],
        ['JANUARY_1_2050_EPOCH',                     '2524608001', 'application_logic.h'],
    ],
    col_widths=[3.5, 1.2, 1.8]
)


# Save
output_path = r'E:\ANTZ_Systems\lorawan_temp_humidity_sensor_lightweight_stack\STM32WL-standalone\Protocol_Specification.docx'
doc.save(output_path)
print(f'Document saved to: {output_path}')
