#include "tlv.h"
#include "emv_tag_names.h"

//
// Tag to name mapping
// Note: to be completely accurate, it may be necessary to consider the
// value length in addition to the tag number to determine the correct
// tag name.
//
// A source for this data might be: https://www.eftlab.com/knowledge-base/complete-list-of-emv-nfc-tags
//

const char* tag_names[] = {
    "4F", "Application Identifier (AID) - card",
    "50", "Application Label",
    "56", "Track 1 Data",
    "57", "Track 2 Equivalent Data",
    "5A", "Application Primary Account Number (PAN)",
    "5F24", "Application Expiration Date",
    "5F25", "Application Effective Date",
    "5F28", "Issuer Country Code",
    "5F2A", "Transaction Currency Code",
    "5F2D", "Language Preference",
    "5F30", "Service Code",
    "5F34", "Application Primary Account Number (PAN) Sequence Number",
    "61", "Application Template",
    "6F", "File Control Information (FCI) Template",
    "70", "READ RECORD Response Message Template",
    "77", "Response Message Template Format 2",
    "82", "Application Interchange Profile",
    "84", "Dedicated file (DF) Name",
    "87", "Application Priority Indicator",
    "8C", "Card Risk Management Data object List 1 (CDOL1)",
    "8D", "Card Risk Management Data object List 2 (CDOL2)",
    "8E", "Cardholder Verification Method (CVM) List",
    "8F", "Certification Authority Public Key Index (PKI)",
    "90", "Issuer Public Key Certificate",
    "92", "Issuer Public Key Remainder",
    "94", "Application File Locator (AFL)",
    "9F01", "Acquirer Identifier",
    "9F07", "Application Usage Control",
    "9F08", "Application Version Number",
    "9F0D", "Issuer Action Code - Default",
    "9F0E", "Issuer Action Code - Denial",
    "9F0F", "Issuer Action Code - Online",
    "9F11", "Issuer Code Table Index",
    "9F12", "Application Preferred Name",
    "9F1A", "Terminal Country Code",
    "9F1D", "Terminal Risk Management Data",
    "9F24", "Payment Account Reference (PAR)",
    "9F32", "Issuer Public Key Exponent",
    "9F35", "Terminal type",
    "9F38", "Processing Options Data Option List (PDOL)",
    "9F42", "Currency Code, Application",
    "9F44", "Currency Exponent, Application",
    "9F46", "Integrated Circuit Card (ICC) Public Key Certificate",
    "9F47", "Integrated Circuit Card (ICC) Public Key Exponent",
    "9F48", "Integrated Circuit Card (ICC) Public Key Remainder",
    "9F49", "Dynamic Data Authentication Data Object List (DDOL)",
    "9F4A", "Static Data Authentication Tag List",
    "9F4D", "Log Entry",
    "9F4E", "Merchant Name and Location",
    "9F5D", "Available Offline Spending Amount (AOSA)",
    "9F62", "PCVC3 (Track1)",
    "9F63", "PUNATC (Track1)",
    "9F64", "NATC (Track1)",
    "9F65", "PCVC3 (Track2)",
    "9F66", "Terminal Transaction Qualifiers",
    "9F67", "NATC (Track2)",
    "9F69", "UDOL",
    "9F6B", "Card CVM Limit",
    "9F6C", "Card Transaction Qualifiers (CTQ)",
    "9F6E", "Third Party Data",
    "A5",   "File Control Information (FCI) Proprietary Template",
    "BF0C", "File Control Information (FCI) Issuer Discretionary Data",
};

#define NUM_TAGS sizeof(tag_names) / sizeof(tag_names[0]) / 2

struct TagName {
    uint16_t tag;
    const char* name;
};

int num_tag_entries = 0;
TagName tag_name_table[NUM_TAGS];


// 
// Set up the tag lookup table.
// If this is not called, the get_tag_name lookup 
// will always return a zero length string.
//
void init_tag_names() {
    uint8_t buf[2];

    for (int i = 0; i < NUM_TAGS; i++) {
        int tag_size = TLVS::hexToBin(tag_names[i*2], &buf[0], 2);    
        tag_name_table[i].tag = buf[0];
        if (tag_size == 2) {
            tag_name_table[i].tag <<= 8;
            tag_name_table[i].tag |= buf[1];
        }
        tag_name_table[i].name = tag_names[i*2+1];
    }
    num_tag_entries = NUM_TAGS;
}

// Return a tag name for a 1 or 2 byte tag value
// Return a zero length string for an unknown tag
const char* get_tag_name(uint16_t tag) {
    for (int i = 0; i < num_tag_entries; i++) {
        if (tag == tag_name_table[i].tag) {
            return tag_name_table[i].name;
        }
    }
    return "";
}


