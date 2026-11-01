# Snips Controller V2 — ESP32-S3-MINI-1U pin table

> The wiring source is `PCB/v2.x/stackup/board.kdl`, and the module is modeled in
> `PCB/v2.x/stackup/parts.kdl`. Pin data is from Espressif's ESP32-S3-MINI-1 & MINI-1U datasheet
> v1.7, in `datasheets/`. The module is on the top side.

The ESP32-S3-MINI-1U-N8 runs the firmware. The XBee 3 is optional: it sits on a castellated daughter board on the underside, and the ESP32's own Bluetooth and ESP-NOW work without it.

Pins are chosen by position, not carried over from V1, so V2 needs its own firmware pin map. The module is turned 270 degrees so its pins face what they connect to:

| Module side on the board | Pins | Faces |
|---|---|---|
| Top edge, left to right | IO11 ... IO0, 3V3, GND | Macro buttons, stick, trigger |
| Left edge, top to bottom | IO12 ... IO18, USB, IO21 ... IO48 | OLED connector, USB route, charger, lower-left parts |
| Bottom edge, left to right | IO35 ... IO42, TXD0, RXD0, IO45, GND, IO46, EN | XBee pad row toward USB |
| Right edge | GND only | Nothing needs it, so the side strip stays free |

## Module rules
- **Variant:** use the -N8.
- **Straps:** IO0 is pulled up by R_BOOT_PU1. IO3, IO45 and IO46 are unconnected, so nothing can disturb the boot mode or flash voltage.
- **ADC:** analog inputs stay on ADC1 (IO1-IO10), since ADC2 is not usable while the Wi-Fi radio runs, which ESP-NOW needs.
- **PWR_HOLD is on RXD0,** whose pull-up at and after reset (ESP32-S3 datasheet Table 2-1) holds the latch through any reset or USB upload with no firmware help. The same pull-up returns as the rail collapses at power-off, so C_LATCH_FB1 gives the latch hysteresis and R_PWR_EN_PD1 is 22 kΩ. `scripts/spice/latch_v2_sim.py` covers this.
- **Firmware drives RXD0 high early in boot and low to power off,** and must never start UART0 receive on it. Do not use `gpio_hold` for PWR_HOLD: on V1 hardware IO42's hold did not survive the USB reset that ends every upload.
- **Power button sense stays on an RTC-domain pin.** With the board off, VSYS lifts this node through R_INV_BASE1 and D_INV_ISO1. V1 verified that on a VDD3P3_RTC pin; IO33-IO37 are in the VDD_SPI domain (Table 2-1).
- **TXD0 (IO43)** prints the boot log, so it stays free for debugging. RGB data has moved off it, so the LED no longer sees that traffic.
- **Boot glitches:** the low-level power-up glitches on IO1-IO17 (Table 2-2, where IO15 and IO16 appear as XTAL_32K) are 60 µs. Those pins carry only inputs, except IO1 (SLEEP_RQ, where low means awake). XBee RESET is on IO42, which has no listed glitch.
- **IO26 stays unconnected.** It is the PSRAM chip-select, and Arduino runs a PSRAM probe at every boot, so it is driven before `setup()`. The probe fails on the -N8, which has no PSRAM, and boot continues.
- **Do not flash V1 firmware on V2.** It would treat V2 pins as SPI and read the wrong buttons. V2 needs its own pin map.
- **EN:** 10 kΩ and 1 µF, as Espressif advises (Peripheral Schematics), so EN crosses 0.75 × VDD about 14 ms after the rail is up. V1 has no capacitor.
- **Pad 61:** the thermal pad is 9 segments, soldered to GND.

## Pin assignments

| Function | GPIO | Module pin | Notes |
|---|---|---|---|
| Boot button | IO0 | 4 | R_BOOT_PU1 pull-up |
| XBee SLEEP_RQ (DTR) | IO1 | 5 | Directly over carrier pad 7. Drive it low as one of the first instructions in `setup()` |
| Macro 6 | IO2 | 6 | Direct, active low, internal pull-up |
| Macro 3 | IO4 | 8 | Direct, active low, internal pull-up |
| Macro 2 | IO5 | 9 | Direct, active low, internal pull-up |
| Stick Y | IO6 (ADC1_CH5) | 10 | Analog |
| Stick X | IO7 (ADC1_CH6) | 11 | Analog |
| Macro 5 | IO8 | 12 | Direct, active low, internal pull-up |
| Analog trigger | IO9 (ADC1_CH8) | 13 | DRV5055 through R_TRIG1 and C_TRIG1, about 1.6 kHz low-pass. C_TRIG1 sits beside the pin |
| VSYS sense | IO10 (ADC1_CH9) | 14 | Divide-by-3 from VSYS. The divider sits beside the pin |
| Macro 4 | IO11 | 15 | Direct, active low, internal pull-up |
| I2C SDA | IO12 | 16 | OLED, IMU. R_I2C_SDA1 4.7k fitted. Top of the left edge, so SDA and SCL do not cross at the connector |
| I2C SCL | IO13 | 17 | R_I2C_SCL1 4.7k fitted |
| Charger STAT2 | IO14 | 18 | Open drain, pulled up |
| Charger STAT1 | IO15 | 19 | Open drain, pulled up |
| Macro 1 | IO16 | 20 | Direct, active low, internal pull-up |
| Thumbstick click | IO17 | 21 | Direct, active low, internal pull-up |
| Trigger down | IO18 | 22 | Direct, active low, internal pull-up |
| USB D- / D+ | IO19 / IO20 | 23 / 24 | Native USB, through R_USB_DM1 and R_USB_DP1 27 Ω |
| Power button sense | IO21 | 25 | RTC-domain pin, like V1's IO2 |
| Digital trigger (bumper) | IO47 | 27 | Direct, active low, internal pull-up |
| Trigger up | IO33 | 28 | Direct, active low, internal pull-up |
| XBee CTS | IO34 | 29 | Carrier pad 1 |
| RGB LED data | IO48 | 30 | SK6812 through R_RGB1 |
| XBee DIN | IO35 | 31 | Carrier pad 2. ESP32 transmits here |
| XBee DOUT | IO36 | 32 | Carrier pad 3. ESP32 receives here |
| IMU interrupt | IO37 | 33 | ICM-42670-P INT1. No longer on a strap pin |
| Vol up | IO38 | 34 | Direct, active low, internal pull-up |
| XBee RTS | IO39 | 35 | Carrier pad 4 |
| Vol down | IO40 | 36 | Direct, active low, internal pull-up |
| XBee ON/SLEEP | IO41 | 37 | Carrier pad 5 |
| Power hold (PWR_HOLD) | RXD0 (IO44) | 40 | RXD0's pull-up holds it through any reset. Firmware drives it high in boot and low to power off. Never use it as UART0 |
| XBee RESET | IO42 | 38 | Carrier pad 6. Drive open drain only: low or released. The pull-up is on the carrier |
| Reset | EN | 45 | R_EN_PU1 and C_EN1 only, no button |

Antenna: the module's connector is the W.FL/MHF III size, not standard U.FL. The XBee on the carrier has its own U.FL, so each radio has a separate antenna.

## XBee carrier pads

U_XBEE1 is 12 pads on the bottom side, 2.54 mm pitch, for a 22 x 17 mm castellated board. The chamfered corner sits by MH3. Pads 1-6 are the row toward USB, left to right seen from the top side; pads 7-12 are the row toward the stick, right to left.

| Pad | Signal | Pad | Signal |
|---|---|---|---|
| 1 | CTS | 7 | SLEEP_RQ |
| 2 | DIN | 8 | GND |
| 3 | DOUT | 9 | 3V3 |
| 4 | RTS | 10 | 3V3 |
| 5 | ON/SLEEP | 11 | GND |
| 6 | RESET | 12 | GND |

With no carrier fitted these lines float, so firmware should enable pulls on IO34, IO36 and IO41 and treat a failed XBee AT query as "no XBee".

## Buttons
Every button wires straight to a module GPIO and to GND, using the internal pull-ups, so simultaneous presses need no diodes.

## Addresses
OLED 0x3C, ICM-42670-P 0x68. R_I2C_SDA1 and R_I2C_SCL1 stay on the board so the IMU works with no OLED attached.

## Free pins
TXD0 (IO43) is free. Leave IO26 unconnected (see Module rules). IO3, IO45 and IO46 are straps, so leave them unconnected.
