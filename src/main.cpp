//
// Dump Info from a Credit Card over RFID
//
// Copyright (c) 2025 James Wanderer
//
// Uses a PN352 to read tap-to-pay apps and credit cards
//

#include <Arduino.h>
#include <SPI.h>
#define NFC_INTERFACE_SPI
#include <PN532_SPI.h>
// Include a CPP - seems like that is how this lib works.
#include <PN532_SPI.cpp>
#include "PN532.h"
#include "tlv.h"
#include "emv_tag_names.h"

// Drivers for the PN532
PN532_SPI pn532_spi(SPI, 3);
PN532 nfc(pn532_spi);

// Global buffers for sending and recieving
uint8_t rx_buffer[255];     // Buffer for received messages
uint8_t tx_buffer[255];     // Buffer for transmitted messages
uint8_t data_buffer[255];   // Buffer to save or assemble data

// Utility to decode BER TLV messages
TLVS rx_tlvs;               // Decode received TLVs

// Pre-declarations of later functions
TLVNode* getPreferredAID();
bool selectApplicationID(TLVNode* aid_node, TLVNode*& pdol_node);
TLVNode* getProcessingOptions(TLVNode* pdol_node);
void readAppRecords(uint8_t sfi, uint8_t start, uint8_t end);

// Elements of the data table for PDO default values
struct DataOption {
  DataOption(uint16_t tag, uint8_t* val, uint8_t val_length);
  uint16_t tag;
  uint8_t* value;
  uint8_t value_length;
};

DataOption::DataOption(uint16_t tag, uint8_t* val, uint8_t val_length) {
  this->tag = tag;
  this->value = val;
  this->value_length = val_length;
  
}
//
// Setup function
// Mostly borrowed from PN352 example code.
void setup()
{
  Serial.begin(115200);
  while (!Serial) {}
  delay(2000);
  Serial.println("-------Read EMV via PN53x--------");

  // Setup Tag value to Name lookup table.
  init_tag_names();

  SPI.begin(SCK, MISO, MOSI, 3);
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata)
  {
    Serial.println("Didn't find PN53x board");
    while (1)
      ; // halt
  }

  // Got ok data, print it out!
  Serial.print("Found chip PN5");
  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.');
  Serial.println((versiondata >> 8) & 0xFF, DEC);

  // Set the max number of retry attempts to read from a card
  // This prevents us from waiting forever for a card, which is
  // the default behaviour of the PN532.
  nfc.setPassiveActivationRetries(0xFF);

  // configure board to read RFID tags
  nfc.SAMConfig();
}

void loop()
{
  // Loop to detect and process a card touch
  // Note: this may take awhile. It may be better to break the steps up, one per loop,
  // but this is just experimental code, and is easier to read this way. 

  bool success;
  Serial.println("Waiting for an ISO14443A card");

  // Look for a new card
  success = nfc.inListPassiveTarget();

  if (success)
  {
    Serial.println("Found something!");
    Serial.println("");

    // Query to find the preferred application ID
    TLVNode* aid_node = getPreferredAID();
    if (aid_node == NULL) {
      return;
    }
    Serial.println();

    // Select the application ID 
    TLVNode* pdol_node;
    if (!selectApplicationID(aid_node, pdol_node)) {
      Serial.println("Failed to select AID");
      return;
    }

    // Run Get Processing Options - returns Application File Locator
    TLVNode* app_files_node = getProcessingOptions(pdol_node);
    if (app_files_node == NULL) {
      Serial.println("No app files found");
      return;
    }
    Serial.println();

    // Save the short file identifier list
    // The TX/RX of further will overwrite the app_files_node
    memcpy(data_buffer, app_files_node->getValue(), app_files_node->getValueLength());

    // Parse the short file idenfiier list, and read the app records for each entry
    ReadBuffer app_files(data_buffer, app_files_node->getValueLength());
    while (!app_files.atEnd()) {
      uint8_t sfi, start, end, num_auth_rec;
      if (!app_files.getByte(sfi) ||
          !app_files.getByte(start) ||
          !app_files.getByte(end) ||
          !app_files.getByte(num_auth_rec)) {
            continue;
      }
      // extract SFI value
      sfi = sfi >> 3;

      // Read the App record
      readAppRecords(sfi, start, end);
      Serial.println();
    }
  }
}

//
// Dump a binary message to the serial port.
//
void printMessage(uint8_t *buffer, uint8_t length)
{
  String msgBuf;

  for (int i = 0; i < length; i++)
  {

    if (buffer[i] < 0x10)
      msgBuf = msgBuf + "0"; //Adds leading zeros if hex value is smaller than 0x10

    msgBuf = msgBuf + String(buffer[i], HEX) + " ";
  }

  Serial.print("TX message (");
  Serial.print(length);
  Serial.print(" bytes): ");
  Serial.println(msgBuf);
}


//
// Dump an APDU response to the serial port.
//
void printResponse(uint8_t *buffer, uint8_t length)
{
  Serial.print("RX message (");
  Serial.print(length);
  Serial.println(" bytes): ");
  nfc.PrintHexChar(buffer, length);
}

//
// Print the value of a TLV and all sub-TLVs to the serial port.
//
void printTLV(TLVNode* node, int indent=0)
{
    for (int i = 0; i < indent; i++)
        Serial.print("  ");

    if (node == NULL) {
        Serial.println("NULL pointer for TLVNode....");
        return;
    }

    Serial.print("Tag: ");
    Serial.print(node->getTag(), HEX);
    const char* tag_name = get_tag_name(node->getTag());
    if (strlen(tag_name) > 0) {
      Serial.print(" - ");
      Serial.print(tag_name);
    }
    Serial.print(" (");
    Serial.print(node->getValueLength());
    Serial.println(" bytes)");
    TLVNode *child = node->firstChild();
    if (child == NULL) {
        for (int i = 0; i <= indent; i++)
            Serial.print("    ");
        TLVS::printValue(node->getValue(), node->getValueLength());
        Serial.println("");
    }

    while (child != NULL) {
        printTLV(child, indent + 1);
        child = node->nextChild(child);
    }
}


//
// Check the status bytes in a Response APDU.
// Return True if OK
//
bool checkApduResponse(const uint8_t *rx_buffer, uint8_t length)
{
  if (length < 2) {
    Serial.print("Short APDU response - ");
    Serial.print(length);
    Serial.println(" bytes.");
    return false;
  }

  // Check SW1 and SW2
  if (rx_buffer[length-2] != 0x90 || rx_buffer[length-1] != 0x00) {
    // Note, real usage needs checks for other values. e.g. 'more data'
    Serial.println("Error response to APDU");
    return false;
  }
  return true;
}
 
/*** Step 1: read 2pay.sys.ddf01 and return an Application ID ***/

//
// Get the preferred App Identifier from the card
//
TLVNode* getPreferredAID()
{
  Serial.println("*** GetPreferredAID");

  // Build the Request APDU
  uint8_t apdu[] ={ 0x00,   /* CLA */
                    0xA4,   /* INS */   // SELECT
                    0x04,   /* P1 */
                    0x00,   /* P2 */
                    0x0e,   /* Length of filename */
                    /* 2pay.sys.ddf01 */
                    0x32, 0x50, 0x41, 0x59, 0x2e, 0x53, 0x59, 
                    0x53, 0x2e, 0x44, 0x44, 0x46, 0x30, 0x31, 
                    0x00 /* LE */ };

  // Send the request
  uint8_t length = sizeof(rx_buffer);
  printMessage(apdu, (uint8_t) sizeof(apdu));
  bool success = nfc.inDataExchange(apdu, sizeof(apdu), rx_buffer, &length);

  // Check the response
  if (!success) {
    Serial.print("No AID found");
    return NULL;
  }
  printResponse(rx_buffer, length);

  if (!checkApduResponse(rx_buffer, length)) {
    return NULL;
  }
  length -= 2;

  // Parse the result message
  rx_tlvs.decodeTLVs(rx_buffer, length);
  printTLV(rx_tlvs.firstTLV());

  TLVNode *node = rx_tlvs.findTLV(0x61);
  TLVNode *sel_aid_node = NULL;
  TLVNode *sel_label_node = NULL;
  uint8_t sel_app_pref = 99;

  while (node != NULL) {
    TLVNode *pref_node = node->findChild(0x87);
    TLVNode *label_node = node->findChild(0x50);
    TLVNode *aid_node = node->findChild(0x4f);

    if (aid_node != NULL) {
      // Use the App Pref value if present
      uint8_t app_pref = 98;
      if (pref_node != NULL) {
        app_pref = pref_node->getValue()[0];
      }

      if (app_pref < sel_app_pref) {
        sel_app_pref = app_pref;
        sel_aid_node = aid_node;
        sel_label_node = label_node;
      }
    }
    node = rx_tlvs.findNextTLV(node);
  }
  Serial.print("Returning app pref: ");
  Serial.print(sel_app_pref);
  if (sel_label_node != NULL) {
    Serial.print(": ");
    TLVS::printValue(sel_label_node->getValue(), sel_label_node->getValueLength());
  }
  Serial.println();
  return sel_aid_node;
}


/*** Step 2: Select the Application ID, and return the PD options list ***/

//
// Select the given AID, return the list of processing data options, if any
//
bool selectApplicationID(TLVNode* aid_node, TLVNode*& pdol_node)
{
  Serial.println("*** Select Application ID");

  uint8_t selectApdu[] = {0x00,  /* CLA */
                          0xA4,  /* INS */  // SELECT
                          0x04,  /* P1  */
                          0x00,  /* P2  */ };

  WriteBuffer tx(tx_buffer, sizeof(tx_buffer));
  tx.putBytes(selectApdu, sizeof(selectApdu));
  // Add command data
  tx.putByte(aid_node->getValueLength());   // AID Length
  tx.putBytes(aid_node->getValue(), aid_node->getValueLength()); // AID Value
  tx.putByte(0);  // Le
  printMessage(tx_buffer, (uint8_t) tx.pos);

  uint8_t rxlength = sizeof(rx_buffer);
  bool success = nfc.inDataExchange(tx.buffer, tx.pos, rx_buffer, &rxlength);

  if (!success) {
    Serial.println("Failed");
    return false;
  }
  printResponse(rx_buffer, rxlength);

  if (!checkApduResponse(rx_buffer, rxlength)) {
    return false;
  }
  rxlength -= 2; // Remove status bytes

  rx_tlvs.decodeTLVs(rx_buffer, rxlength);
  printTLV(rx_tlvs.firstTLV());

  // Return the Processing Data Options List
  pdol_node = rx_tlvs.findTLV(0x9f38);
  return  true;
}

/***  Default Data Options: Build Data Options List  ***/

// Pre-defined data for our emulated 'terminal' reading the card
// The choice of data here is mostly arbitrary

// Terminal transaction qualifiers
uint16_t dolTagTTQ = 0x9f66;
uint8_t dolValTTQ[] = { 0x36, 0x80, 0x40, 0x00 };

// Transaction currency code
uint16_t dolTagTCC = 0x5f2a;
uint8_t dolValTCC[] = { 0x08, 0x40 };  // USD numeric code

// Terminal Risk Management Data
uint16_t dolTagTRMD = 0x9f1d;
uint8_t dolValTRMD[] = { 0x40, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Terminal Country Code
uint16_t dolTagTCnC = 0x9f1a;
uint8_t dolValTCnC[] = { 0x08, 0x40 }; // US

// Terminal Type
uint16_t dolTagTT = 0x9f35;
uint8_t dolValTT[] = { 0x14 };

// Acquirer Identifier
uint16_t dolTagAI = 0x9f01;
uint8_t dolValAI[] = { 0x01 };

// Application lifecycle data
uint16_t dolTagALCD = 0x9f7e;
uint8_t dolValALCD[] = { 0x01 };

// Merchant name and location
uint16_t dolTagMNL = 0x9f4e;
uint8_t dolValMNL[] = { 
    0x41, 0x42, 0x43, 0x32, 0x30, 0x32, 0x34, 0x30, 0x38, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };



  
// Data Option Lookup Table
// Use these values to fill out a PDOL list for the Get Processing Options request
DataOption data_options_list[] = {
  DataOption(dolTagTTQ, &dolValTTQ[0], (uint8_t) sizeof(dolValTTQ)),
  DataOption(dolTagTCC, &dolValTCC[0], (uint8_t) sizeof(dolValTCC)),
  DataOption(dolTagTRMD, &dolValTRMD[0], (uint8_t) sizeof(dolValTRMD)),
  DataOption(dolTagTCnC, &dolValTCnC[0], (uint8_t) sizeof(dolValTCnC)),
  DataOption(dolTagTT, &dolValTT[0], (uint8_t) sizeof(dolValTT)),
  DataOption(dolTagAI, &dolValAI[0], (uint8_t) sizeof(dolValAI)),
  DataOption(dolTagALCD, &dolValALCD[0], (uint8_t) sizeof(dolValALCD)),
  DataOption(dolTagMNL, &dolValMNL[0], (uint8_t) sizeof(dolValMNL))
};

// Seach the table for a matching tag
// Return NULL if not found
DataOption* getDataOption(uint16_t tag) 
{
  for (int i = 0; i < sizeof(data_options_list); i++) {
    if (data_options_list[i].tag == tag) {
      return &data_options_list[i];
    }
  }
  return NULL;
}

// Build Processing Data Options
// In: A TLV node with tag 9f38. The value lists required Data Options. NULL is OK.
// Out: data_operations filled with expected response
//
bool buildDataOptionsList(TLVNode* pdol_node, WriteBuffer& data_options) 
{
  ReadBuffer dol_list;

  if (pdol_node != NULL) {
      dol_list.buffer = pdol_node->getValue();
      dol_list.buffer_size = pdol_node->getValueLength();
  }

  while (!dol_list.atEnd()) {
    uint16_t tag;
    uint8_t len;
    int error_flag;

    tag = TLVNode::parseTag(dol_list, &error_flag);
    if (error_flag ||
        !dol_list.getByte(len)) {
          Serial.println("Failed reading dol_list");
          return false;
    }

    DataOption* option = getDataOption(tag);
    if (option == NULL) {
      Serial.print("Don't have a requested option tag: ");
      Serial.print(tag, HEX);
      // Add with 0 values
      while (len-- > 0) data_options.putByte(0);
      continue;
    }

    // Copy value into PDOL buffer
    // Truncate if our value is too long
    int copy_len = min(option->value_length, len);
    data_options.putBytes(option->value, copy_len);

    // Report any mismatch length, handle need to pad value.
    if (option->value_length != len) {
      Serial.println("mismatched expectation on value length");
      Serial.print(tag, HEX);
      Serial.print(" requested len: ");
      Serial.print(len);
      Serial.print(" actual len: ");
      Serial.println(option->value_length);

      // Pad with zeros if it was too short
      if (copy_len < len) {
        len -= option->value_length;
        while (len-- > 0) data_options.putByte(0);
      }
    }
  }
  return true;
}


/***  Step 3: Get Processing Options - get AFL  ***/

//
// Returns the Application Files Locator (ALF) for files used in the transaction
//
TLVNode* getProcessingOptions(TLVNode* pdol_node)
{
  Serial.println("*** GetProcessingOptions");
  uint8_t selectApdu[] = {0x80,    /* CLA */
                          0xA8,    /* INS */   // GET PROCESSING OPTIONS
                          0x00,    /* P1  */
                          0x00,    /* P2  */ };

  WriteBuffer tx(tx_buffer, sizeof(tx_buffer));
  tx.putBytes(selectApdu, sizeof(selectApdu));

  WriteBuffer data_options(data_buffer, sizeof(data_buffer));
  if (!buildDataOptionsList(pdol_node, data_options)) {
    return NULL;
  }

  // Add PDOL
  tx.putByte(data_options.pos + 2);
  tx.putByte(0x83);
  tx.putByte(data_options.pos);
  tx.putBytes(data_options.buffer, data_options.pos);
  tx.putByte(0);  // Le
  printMessage(tx_buffer, (uint8_t) tx.pos);

  uint8_t rxlength = sizeof(rx_buffer);
  bool success = nfc.inDataExchange(tx.buffer, tx.pos, rx_buffer, &rxlength);

  if (!success) {
    Serial.println("Failed");
    return NULL;
  }

  printResponse(rx_buffer, rxlength);

  if (!checkApduResponse(rx_buffer, rxlength)) {
    return NULL;
  }
  rxlength -= 2;

  rx_tlvs.decodeTLVs(rx_buffer, rxlength);
  printTLV(rx_tlvs.firstTLV());
  TLVNode *node = rx_tlvs.findTLV(0x94);
  return node;
}


/***  Step 4: Read Application Records ***/

void readAppRecords(uint8_t sfi, uint8_t start, uint8_t end)
{
  Serial.println("*** Read app records");
  Serial.print("SFI: ");
  Serial.print(sfi);
  Serial.print(", start: ");
  Serial.print(start);
  Serial.print(", end: ");
  Serial.print(end);
  Serial.println();

  uint8_t readApdu[] = { 0x00,                                     /* CLA */
                         0xB2,                                     /* INS */ };

  for (uint8_t record = start; record <= end; record++) {
    WriteBuffer tx(tx_buffer, sizeof(tx_buffer));
    tx.putBytes(readApdu, sizeof(readApdu));
    tx.putByte(record);
    uint8_t p2 = sfi <<3 | 0b00000100;
    tx.putByte(p2);
    tx.putByte(0);  // Le
    printMessage(tx_buffer, (uint8_t) tx.pos);
    uint8_t rxlength = sizeof(rx_buffer);
    bool success = nfc.inDataExchange(tx.buffer, tx.pos, rx_buffer, &rxlength);
    if (!success) {
      Serial.println("Read Application Record: Failed");
      continue;
    }

    printResponse(rx_buffer, rxlength);

    if (!checkApduResponse(rx_buffer, rxlength)) {
      continue;
    }
    rxlength -= 2;
    Serial.println();

    rx_tlvs.decodeTLVs(rx_buffer, rxlength);
    printTLV(rx_tlvs.firstTLV());
    Serial.println();
  }
}

