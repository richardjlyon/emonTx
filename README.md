emonTx
======

Energy monitor wireless transmitter firmware.
Requires
	- JeeLib		https://github.com/jcw/jeelib
	- EmonLib		https://github.com/openenergymonitor/EmonLib.git
	- emontx_lib.ino

Connects up to three current transformers as follows:
1 - Total power
2 - Heating power
3 - Utility power

Set an LED code as 'RED' if (2) > 1kW - this indicates that heating is active.

Transmit (1), (2), (3), battery voltage, and LED code.
