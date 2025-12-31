# EMV Arduino
Example code for reading credit cards

C++ code to drive a PN532 RFID reader connected
on the SPI bus to dump information from
tap to pay credit cards and payment apps.

## Caveats

Be aware that some of the information read from credit cards is
sensitive and should be kept private. It may not be obvious when looking at 
it, so use caution in sharing any information read from cards or apps.

Also be aware that this code isn't tested for safety. It only dumps information
and does not try to initiate a transaction. However it has only been run 
against a few cards, and there is not garuntee that it couldn't leave a 
card in a bad state without possible recovery.

## Required Libraries

This app needs two libraries:
- https://github.com/Seeed-Studio/PN532  - driver for the PN532
- https://github.com/jmwanderer/tlv.arduino - decode response messages 

## Platform IO

The code is developed with Platform IO, and you will likely only
need to select the correct board type to get it to work.

## Arduino IDE

Even though the repo is in a format for Platform IO, it is simple to get this
working under the Arduino IDE:

- Copy the three files in src to a directory emv
- Rename main.cpp to emv.ino
- Install the two libraries listed above. (The library manager should work)


## Limitations

Large records are unable to be read from credit cards. he PN532 driver
library doesn't appear to support any data payloads larger than 246 
bytes (and I have not verified that size works). 

The failure is caught as a length check error in the inDataExchange 
function.

## The Adafruit Library

Adafruit has a [PN532 library](https://github.com/adafruit/Adafruit-PN532)
that has more degbug information and may have more fixes that the 
Seed Studio library. The adafruit branch of this repository uses the
adafruit library.

One caveat, the default buffer size in Adafruit is only 64. This needs to be
increased to 255 in the following line found in Adafruit_PN532.cpp

#define PN532_PACKBUFFSIZ 64                ///< Packet buffer size in bytes


## Notes
