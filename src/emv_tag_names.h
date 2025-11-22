#ifndef __EMV_TAG_NAMES_H__
#define __EMV_TAG_NAMES_H_
#include <stdint.h>


// 
// Support for looking up tag name for tag value
//

// Call once to setup the table
void init_tag_names();

// Return a null terminated string that is the name for the tag
const char* get_tag_name(uint16_t tag);
 

#endif /* __EMV_TAG_NAMES_H__*/