# Snips Controller PCB (V2)

A wireless handheld controller for R2-D2 droid operation, designed for use at public events and conventions. Communicates with the droid over Zigbee, in a packetized state-report architecture.

The electrical design is in [Stackup KDL](stackup/board.kdl), using the pinned
Stackup library plus a [local thumbstick model](stackup/parts.kdl). The existing [KiCad PCB](snips_controller.kicad_pcb) is linked
through `snips_controller.stackup_sch`; sync it with the Stackup KiCad plugin. Run
`stackup check PCB/v2.x/stackup/board.kdl` from the repository root before syncing.

## V2 board

70 x 30 mm, 4 layers: F.Cu signals, In1 solid 3V3, In2 solid GND, B.Cu signals. 1.6 mm thick. Routed, not yet fabricated.

- **Module:** an ESP32-S3-MINI-1U-N8 on the top, between the macro buttons and USB, turned 270 degrees. The 19.2 mm WROOM-1U does not fit the 17.5 mm there. The turn puts the pinless GND edge on the side and the signal pins toward the stick, the OLED and the carrier; pins are assigned by position, as `GPIO_table.md` explains. USB runs about 15 mm as a result, which is well within full-speed limits. Bluetooth and ESP-NOW work with no other radio.
- **XBee:** optional. U_XBEE1 is a row of 12 hand-solder pads on the bottom for a 22 x 17 mm castellated daughter board, which carries the XBee 3 Micro, its decoupling, the RESET pull-up and RC, and the ATTN/BOOTMODE pull-up. The link is UART with flow control, not SPI. RTS and DTR also allow XBee serial firmware recovery through the ESP32. Leave the area inside the pads free of parts; vias there stay tented. See `GPIO_table.md`.
- **Reflow:** the bottom side goes first and the top side last, because Espressif requires a single reflow for the module (datasheet section 12.3). The XBee reflows on its own carrier and never goes upside down, which is what stopped the two modules from sharing this board before.
- **Radio link:** Amidala's droids use XBee Zigbee 3.0, so a Zigbee controller needs the carrier. The firmware still has to move from the XBee's SPI interface to UART, and needs a V2 pin map.
- **Programming:** USB, through the module's native USB. To force download mode, hold SW_BOOT1 while pressing the power button. A blank board has no firmware to latch PWR_HOLD, so hold the power button through its first flash. After that, uploads need no button.
- **No reset button:** the power button can only turn the board on. PWR_HOLD is on IO3, an RTC pad with no reset pull, and C_LATCH_G1 (1 µF) carries the latch through a reset of about 300 ms at worst (derated capacitor, flat cell). Firmware latches the pin with `gpio_hold_en`, so a longer reset or a USB upload holds power with no button, and releases it with `gpio_hold_dis` to power off. Keep a watchdog running to recover from a hang.
- **Buttons:** all 13 connect directly to module GPIOs. See `GPIO_table.md`.
- **Power blocks:** the buck, charger and power latch follow V1's relative placement, all on the bottom. Two charger GND vias were moved 0.1-0.75 mm in V1's layout to clear earlier button positions. C_BUCK_IN1 turned 90 degrees with its VSYS pad straight above the VIN pin. BUCK_FB runs around the input cap and hops to F.Cu past the inductor, so both planes shield it; keep it out from under L_BUCK1. The latch is too long to fit whole, so it is split at `PWR_BTN_TRIGGER`: the switch and `PWR_LATCH_G` node sit under the stick, turned 270 degrees, and the inverter sits beside the stick's lower pins. See `diagrams/` for why they are arranged this way.
- **OLED cable:** U_OLED1's opening faces USB, and a Qwiic plug is 7.0 mm wide with its protrusions, so keep top-side parts out of x 0.9-7.9 mm from the mouth at y 52.7 mm down to the MH3 post. Check that a real cable goes in before committing the shell.
- **BOM:** the Value column carries each part's full details, including a capacitor's rated voltage, because fabs read it and the BOM has no voltage column. Keep rail names out of designators: PCBWay queried the V1.2 caps named C_3V3_* as 3.3 V parts, so V2 uses C_MCU_*.
- **Hall sensor:** U_HALL1 is on the bottom at (6.5, 4.75) mm, rotated 180 degrees, forward and left of the stick, 7.8 mm from its nearest pot pin in plan view against about 15.4 mm on V1. A 2 x 2 mm N52 magnet swings up to sit directly under the package centre. Keep at least 1.8 mm from the top of the package to the magnet at full press, or the output clips at the sensor's ±44 mT range. The stick reading against trigger travel still needs a bench test.
- **Mounting:** four Ø2.2 mm unplated holes, MH1-4, at (2.5, 2.6), (27.4, 2.8), (3, 62.5) and (27, 67) from the top-left corner, for M2 self-tapping screws into shell posts. MH3 sits above the power button, which holds the true corner, so a Ø4 post clears `C_DBNC1` by only about 0.15 mm. The carrier has a long shallow chamfer at that corner, which also clears a Ø4 post on the bottom.
- **Battery:** BT1 is a 2-pin JST PH, the common connector on pouch cells this size: pin 1 VBAT, pin 2 GND. R_TS1 ties the charger's TS pin to a fixed 10 kΩ, so there is no pack temperature monitoring. Check the polarity of the cell's lead before plugging it in.
- **USB and status LED:** J_USB1 sits 2 mm right of centre, with its CC resistors and TVS moved with it. D_RGB1 sits between SW_PWR1 and J_USB1, turned 90 degrees.
- **Through-board parts:** the Alps pegs, stick pins, USB stakes and BT1 pins all reach the other side.

## GPIO Assignment

See [`GPIO_table.md`](GPIO_table.md) for every module pin.

## Hardware Overview

| Component | Part | Notes | Layout diagram |
|---|---|---|---|
| MCU and radio | Espressif ESP32-S3-MINI-1U-N8 | Wi-Fi, Bluetooth LE and ESP-NOW, 8 MB flash, 15.4 x 15.4 x 2.4 mm, external antenna | — |
| XBee (optional) | Digi XB3-24Z8UM on a castellated carrier | Zigbee 3.0, +19 dBm, U.FL. Hand-soldered to the underside | — |
| Display | SSD1306 1.3" OLED | 128x64, monochrome, I2C | — |
| Hall trigger | DRV5055A2QDBZR (SOT-23) | Ratiometric linear hall effect, 3.3V. On the bottom of the board under the trigger magnet; senses the field perpendicular to the board. Trigger calibration accepts either magnet polarity | — |
| Thumbstick | GuliKit hall effect module | Native 3.3V, analog X/Y + click | — |
| Macro and boot buttons | Alps SKRPABE010 | 4.2 x 3.2 x 2.5 mm SMD tact switch, 1.57 N, 0.2 mm travel, 100,000 cycles. Footprint checked against the Alps land drawing | — |
| RGB LED | SK6812MINI-HS | Status indicator; specified supply begins at 3.7 V, but board supplies 3.3 V | — |
| Charger | bq25185 | USB/DC input, power path, 4.2V/500mA | [diagram](diagrams/power_charger_layout.md) |
| Buck | TLV62569 | 3.3V output, 2A | [diagram](diagrams/power_buck_layout.md) |
| Battery | Single-cell pouch, 2-pin JST PH | Cell not chosen yet. Set R_ISET1 for it; 604 Ω charges at about 500 mA | — |
| Power switch | AO3401A + AO3400A | Soft latch, hybrid hardware+firmware | [diagram](diagrams/power_control_layout.md) |

## Layout Diagrams

Suggested top-copper floorplans for the three sheets with real layout
stakes — a switching regulator, a linear charger with a thermal pad, and
the soft latch that switches the buck's enable. Each is checked
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
| DRV5055 VCC | 100nF ceramic | |
| GuliKit VCC | 100nF ceramic | |
| SK6812 VCC | 100nF ceramic | Close to supply pin |
| ICM-42670-P | 100nF + 2.2µF on VDD, 10nF on VDDIO | Close to their pins |
| OLED connector | 1µF + 100nF ceramic | |
| bq25185 VIN | 10µF + 100nF ceramic | |
| bq25185 SYS | 10µF + 100nF ceramic | |
| bq25185 BAT | 10µF ceramic | |
| TLV62569 VIN | 10µF ceramic | |
| TLV62569 VOUT | 22µF ceramic | As on V1. TI specifies 10-47 µF |
| ESP32-S3-MINI-1U 3V3 | 10µF + 100nF + 39pF ceramic | As on V1 |
| ESP32-S3-MINI-1U EN | 1µF ceramic | With R_EN_PU1, per Espressif |
| XBee VCC | 10µF + 1µF + 100nF + 47pF ceramic | On the carrier |

### Resistors
| Signal | Value | Notes |
|---|---|---|
| bq25185 STAT1/STAT2 | 10kΩ | Open drain, pulled up to 3V3 |
| bq25185 TS/MR | 10kΩ to GND | Required with no thermistor: floating means no charging |
| I2C SDA/SCL | 4.7kΩ | So the IMU works with no OLED attached |
| Boot button | 10kΩ to 3V3 | R_BOOT_PU1 |
| ESP32 EN | 10kΩ to 3V3 | R_EN_PU1 |
| USB D+/D- | 27Ω series | R_USB_DP1 and R_USB_DM1, as on V1 |
| Trigger output | 1kΩ + 100nF | R_TRIG1 and C_TRIG1, about 1.6 kHz. The DRV5055 must not see a capacitor without a resistor |
| SK6812 DIN | 470Ω | R_RGB1 |


### Buttons
No external parts. Every button wires to GND and a module GPIO, using the internal pull-ups; see [`GPIO_table.md`](GPIO_table.md).

---

## Communication Architecture

The droids use XBee Zigbee 3.0, so the controller joins that network through the XBee on the carrier, over UART in API mode. Each droid has its own coordinator and PAN ID, and all coordinators share one AES key. Without the carrier the ESP32 can use Bluetooth or ESP-NOW.

---

## Bringup Sequence

Recommended bringup order:
1. Current-limited supply on VSYS, with no carrier fitted: check 3V3 and idle current with the power button held.
2. Flash over USB, holding the power button, with SW_BOOT1 held if the chip does not enumerate. Confirm firmware drives `PWR_HOLD` high.
3. Time the shortest press that stays on, at 3.3 V and 4.2 V. The simulation gives 0.1-0.2 s on a normal cell and up to about 0.4 s on a flat one, set by when firmware drives `PWR_HOLD`, so plan on a half-second press and keep that drive early in boot. Run a USB upload with no button held, which is the first test of IO3's hold under D_OR2's load, then confirm a watchdog reset reboots the board and a graceful shutdown stays off.
4. OLED and ICM-42670-P on I2C.
5. SK6812 RGB LED.
6. Buttons, using the pin table.
7. Charge status on STAT1 and STAT2, and VSYS sense.
8. DRV5055 and thumbstick on the ADC. Sweep the stick and the trigger together to measure how much each moves the other.
9. Solder the carrier on. Check UART traffic, CTS, SLEEP_RQ and ON/SLEEP, then XBee reset, then join a droid's network.

---

## Power Architecture

- Battery → bq25185 (charger and power path, 4.2V, 500mA) → TLV62569 (3.3V buck) → 3V3 for everything, including the carrier.
- Power switch: soft latch (AO3401A PFET + AO3400A NFET) controls the TLV62569 EN pin. USB does not hold it on.
- Boot: press the power button → latch enables 3V3 → module boots → IO3 goes high through D_OR2 to hold the latch, then firmware latches it with `gpio_hold_en`.
- Shutdown: firmware sees a hold on IO21 → graceful shutdown → `gpio_hold_dis`, IO3 low → the gate decays through R_LATCH_G1 in about 0.6-1.7 s → power cut.
- bq25185 STAT1 and STAT2 go to IO15 and IO14 (open drain, 10kΩ pull-ups to 3V3).
- Battery voltage: VSYS through R_VSYS1/R_VSYS2 to IO7, reading VSYS/3.
- Below about 3.3 V on the battery the buck passes it through, and the DRV5055's minimum is 3.0 V, so the firmware cutoff should sit above that.

## Power Management

Firmware implements tiered power saving:
- **Active**: full brightness OLED, normal polling rate
- **Idle (short timeout)**: OLED dimmed via SSD1306 contrast command
- **Idle (long timeout)**: OLED off
- **Sleep**: MCU sleep, wake on any button press via GPIO interrupt
