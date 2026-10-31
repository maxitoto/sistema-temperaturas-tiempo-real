Summary

Detect Power‑On Reset and Insufficient Power for DS18B20
Contributed by @Laxilef, reviewed by @RobTillaart

Changes:

Added detection of two special error conditions specific to DS18B20 sensors:
Power‑On Reset (85°C) – Returns DEVICE_POWER_ON_RESET_RAW when the sensor reports 85.00°C and scratchpad[6] == 0x0C. This identifies the well‑known “85°C glitch” on power‑up (common with counterfeit or mis‑wired sensors).
Insufficient Power / Conversion Failure (127.94°C) – Returns DEVICE_INSUFFICIENT_POWER_RAW when the sensor reports 0x07FF (127.94°C), indicating a failed conversion (e.g., parasitic power without Vdd grounded, voltage drops).
Implementation details

The logic is borrowed from the Linux kernel driver and verified by community testing.
Only applies to DS18B20 family devices.
Existing getTempC() still returns the raw value; advanced users can check the specific error codes via the new constants.
Credits
Thanks to @Laxilef for the contribution and @RobTillaart for in‑depth technical review.