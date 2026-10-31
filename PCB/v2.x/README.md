# Snips Controller PCB (V2)

A wireless handheld controller for R2-D2 droid operation, designed for use at public events and conventions. Communicates with the droid over Zigbee, in a packetized state-report architecture.

The electrical design is in [Stackup KDL](stackup/board.kdl), using the pinned
Stackup library plus a [local thumbstick model](stackup/parts.kdl). The existing [KiCad PCB](snips_controller.kicad_pcb) is linked
through `snips_controller.stackup_sch`; sync it with the Stackup KiCad plugin. Run
`stackup check PCB/v2.x/stackup/board.kdl` from the repository root before syncing.

## V2 board

70 x 30 mm, 4 layers: F.Cu signals, In1 signals with a partial 3V3 fill, In2 solid GND, B.Cu signals. Routed, not yet fabricated.

- **One module:** the BT840XE replaces the ESP32 and XBee, which could not sit back to back. Espressif requires a single reflow, and Digi forbids reflowing the XBee upside down.
- **Reflow:** Fanstel allows at most 2 cycles and no opposite-side reflow, so the bottom side, which carries the module, must be the last pass. It is MSL 3 per JLC's listing.
- **Radio link:** Amidala's droids use XBee Zigbee 3.0, so the controller must join that network. That Zigbee firmware is still to be written.
- **First programming:** SWD, through J_SWD1, a TC2030-NL Tag-Connect footprint on the bottom with no part fitted; use a TC2030-CTX-NL cable. New modules hold Fanstel's AT firmware, so USB only works once a bootloader is loaded. The latch only stays on through the power button or `PWR_HOLD`, not USB, so hold the power button while flashing a new board.
- **Bootloader entry:** hold SW_BOOT1 (P1.00) while pressing the power button. The bootloader must drive `PWR_HOLD` high straight away. SW_RST1, on the bottom, resets the module. That releases `PWR_HOLD`, so it forces the board off when firmware hangs, which the power button cannot do; a quick tap may only restart it. The bootloader must enable the reset pin. Use the BT840XE, not the -p part, which has access port protection on.
- **Power:** VDD and VDDH are tied together after FB_RF1 and C_RF_BULK1, per Fanstel's Note 1 and supply design note.
- **Buttons:** all 13 connect directly to module GPIOs. See `GPIO_table.md`.
- **Power blocks:** the buck, charger and power latch keep V1's exact relative placement and routing, moved as rigid blocks. The buck and charger are mirrored onto the top side, near USB; the latch stays on the bottom. Two charger GND vias were later moved 0.1-0.75 mm in V1's layout to clear earlier button positions. The latch is too long to fit whole, so it is split at `PWR_BTN_TRIGGER`: the switch and `PWR_LATCH_G` node sit under the stick, turned 270 degrees, and the inverter sits beside the stick's lower pins. Change them as blocks, and see `diagrams/` for why they are arranged this way.
- **Hall sensor:** U_HALL1 sits off-center at (20.3, 4.0) mm, 7.8 mm from the stick's top sensing block rather than 3.3 mm, to reduce magnetic coupling. Confirm on a real stick.
- **Mounting:** four Ø2.2 mm unplated holes, MH1-4, at (2.5, 2.6), (27.4, 2.8), (3, 62.5) and (27, 67) from the top-left corner, for M2 self-tapping screws into shell posts. MH3 sits above the power button, which holds the true corner, so a Ø4 post clears `C_VIN_1` and `C_DBNC1` by only about 0.15 mm.
- **Battery:** BT1 is a 2-pin JST PH, the common connector on pouch cells this size: pin 1 VBAT, pin 2 GND. R_TS1 ties the charger's TS pin to a fixed 10 kΩ, so there is no pack temperature monitoring. Check the polarity of the cell's lead before plugging it in.
- **USB and status LED:** J_USB1 sits 2 mm right of centre, with its CC resistors and TVS moved with it. D_RGB1 sits between SW_PWR1 and J_USB1, turned 90 degrees.
- **Through-board parts:** the Alps pegs, stick pins, USB stakes and BT1 pins all reach the other side.

## GPIO Assignment

See [`GPIO_table.md`](GPIO_table.md) for every module pin.

## Hardware Overview

| Component | Part | Notes | Layout diagram |
|---|---|---|---|
| MCU and radio | Fanstel BT840XE | nRF52840 with SKY66112 PA, +21 dBm, u.FL, 15 x 28 x 1.9 mm. Runs the firmware and the Zigbee link | — |
| Display | SSD1306 1.3" OLED | 128x64, monochrome, I2C | — |
| Hall trigger | DRV5055A2QDBZR (SOT-23) | Ratiometric linear hall effect, 3.3V. On the bottom of the board under the trigger magnet; senses the field perpendicular to the board. Trigger calibration accepts either magnet polarity | — |
| Thumbstick | GuliKit hall effect module | Native 3.3V, analog X/Y + click | — |
| Macro and boot buttons | Alps SKRPABE010 | 4.2 x 3.2 x 2.5 mm SMD tact switch, 1.57 N, 0.2 mm travel, 100,000 cycles. Footprint checked against the Alps land drawing | — |
| RGB LED | SK6812MINI-HS | Status indicator; specified supply begins at 3.7 V, but board supplies 3.3 V | — |
| Charger | bq25185 | USB/DC input, power path, 4.2V/500mA | [diagram](diagrams/power_charger_layout.md) |
| Buck | TLV62569 | 3.3V output, 1A | [diagram](diagrams/power_buck_layout.md) |
| Battery | Single-cell pouch, 2-pin JST PH | Cell not chosen yet. Set R_ISET1 for it; 604 Ω charges at about 500 mA | — |
| Power switch | AO3401A + AO3400A | Soft latch, hybrid hardware+firmware | [diagram](diagrams/power_control_layout.md) |

## Layout Diagrams

Suggested top-copper floorplans for the three sheets with real layout
stakes — a switching regulator, a linear charger with a thermal pad, and
the transistor carrying the whole board's current. Each is checked
against the actual component datasheet, not just the schematic. See
[`diagrams/README.md`](diagrams/README.md) for the full list and how the
files are organized.

- [Buck converter](diagrams/power_buck_layout.md) — `/Power_Buck/*` nets in Stackup
- [Charger](diagrams/power_charger_layout.md) — `/Power_Charger/*` nets in Stackup
- [Power latch](diagrams/power_control_layout.md) — `/Power_Control/*` nets in Stackup

## Passives & Support Components

### Decoupling Caps
| Component | Value | Notes |
|---|---|---|
| DRV5055 VCC | 100nF ceramic | On 3V3_RF |
| GuliKit VCC | 100nF ceramic | On 3V3_RF |
| SK6812 VCC | 100nF ceramic | Close to supply pin |
| ICM-42670-P | 100nF + 2.2µF on VDD, 10nF on VDDIO | Close to their pins |
| OLED connector | 1µF + 100nF ceramic | |
| bq25185 VIN | 10µF + 100nF ceramic | |
| bq25185 SYS | 10µF + 100nF ceramic | |
| bq25185 BAT | 10µF ceramic | |
| TLV62569 VIN | 10µF ceramic | |
| TLV62569 VOUT | 10µF ceramic | TI specifies 10-47 µF and the rail totals about 70 µF nominal, less under DC bias. Check with a load step |
| BT840XE VDD | 47µF + 10µF + 100nF ceramic | After FB_RF1, per Fanstel's noisy-supply note |
| BT840XE VBUS | 1µF ceramic | Kept small so VBUS stays near USB's 10 µF limit. Nordic's value is unconfirmed |

### Resistors
| Signal | Value | Notes |
|---|---|---|
| bq25185 STAT1/STAT2 | 10kΩ | Open drain, pulled up to 3V3 |
| bq25185 TS/MR | 10kΩ to GND | Required with no thermistor: floating means no charging |
| I2C SDA/SCL | 4.7kΩ | So the IMU works with no OLED attached |
| Boot button | 10kΩ to 3V3 | R_BOOT_PU1 |
| Trigger output | 1kΩ + 100nF | R_TRIG1 and C_TRIG1, about 1.6 kHz. The DRV5055 must not see a capacitor without a resistor |
| SK6812 DIN | 470Ω | R_RGB1 |

USB D+/D- connect directly, with no series resistors. That is unconfirmed against Nordic's datasheet.

### Buttons
No external parts. Every button wires to GND and a module GPIO, using the internal pull-ups; see [`GPIO_table.md`](GPIO_table.md).

---

## Communication Architecture

The droids use XBee Zigbee 3.0, so the controller has to join that network. The nRF52840 runs the Zigbee stack, and the firmware for it is not written yet. Each droid has its own coordinator and PAN ID, and all coordinators share one AES key.

---

## Bringup Sequence

Recommended bringup order:
1. Current-limited supply on VSYS: check 3V3 and idle current with the power button held.
2. Load the bootloader over SWD through J_SWD1, holding the power button. Confirm it drives `PWR_HOLD` high, enables the reset pin, and enumerates over USB.
3. Boot entry and reset: hold BOOT while pressing power, and confirm RESET forces the board off.
4. OLED and ICM-42670-P on I2C.
5. SK6812 RGB LED.
6. Buttons, using the pin table.
7. Charge status on STAT1 and STAT2, and VSYS sense.
8. DRV5055 and thumbstick on the ADC, with the SAADC's VDD/4 reference. Sweep the stick and the trigger together to measure how much each moves the other.
9. Radio: limit nRF TX to +2 dBm with ANT000, per Fanstel, and join a droid's network.

---

## Power Architecture

- Battery → bq25185 (charger and power path, 4.2V, 500mA) → TLV62569 (3.3V buck) → 3V3 logic, and through FB_RF1 to 3V3_RF for the module, DRV5055 and thumbstick.
- Power switch: soft latch (AO3401A PFET + AO3400A NFET) controls the TLV62569 EN pin. USB does not hold it on.
- Boot: press the power button → latch enables 3V3 → module boots → P1.13 goes high through D_OR2 to hold the latch.
- Shutdown: firmware sees a hold on P0.11 → graceful shutdown → P1.13 low → power cut. A reset also releases the latch.
- bq25185 STAT1 and STAT2 go to P1.14 and P1.15 (open drain, 10kΩ pull-ups to 3V3).
- Battery voltage: VSYS through R_VSYS1/R_VSYS2 to P0.05, reading VSYS/3.
- Below about 3.3 V on the battery the buck passes it through, and the DRV5055's minimum is 3.0 V, so the firmware cutoff should sit above that.

## Power Management

Firmware implements tiered power saving:
- **Active**: full brightness OLED, normal polling rate
- **Idle (short timeout)**: OLED dimmed via SSD1306 contrast command
- **Idle (long timeout)**: OLED off
- **Sleep**: MCU sleep, wake on any button press via GPIO interrupt
