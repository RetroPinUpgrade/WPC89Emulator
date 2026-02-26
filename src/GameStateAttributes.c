#include "GameStateAttributes.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define JSMN_PARENT_LINKS
#include "jsmn.h" // Standard jsmn header
#include "ram_map_data.h"

// The external flash array
//extern const uint8_t RAMMapJSON[32768];

// Maximum expected attributes (adjust based on your RAM needs)
#define MAX_ATTRIBUTES 150
static wpc_ram_attribute_t attr_list[MAX_ATTRIBUTES];
static uint16_t total_attributes = 0;

// Helper to convert JSON string to enum
wpc_encoding_t get_encoding_type(const char* s, int len) {
    if (strncmp(s, "int", len) == 0) return ENC_INT;
    if (strncmp(s, "bcd", len) == 0) return ENC_BCD;
    if (strncmp(s, "ch", len) == 0)  return ENC_CH;
    if (strncmp(s, "bool", len) == 0) return ENC_BOOL;
    if (strncmp(s, "wpc_rtc", len) == 0) return ENC_WPC_RTC;
    return ENC_UNKNOWN;
}



void GameStateAttributes_Parse(void) {
    jsmn_parser p;
    static jsmntok_t tokens[2048]; 
    
    jsmn_init(&p);
    int r = jsmn_parse(&p, (const char*)RAMMapJSON, 32768, tokens, 2048);
    if (r < 0) return;

    total_attributes = 0;

    for (int i = 0; i < r; i++) {
        /* Identify "start" keys to locate RAM attributes */
        if (tokens[i].type == JSMN_STRING && 
            (tokens[i].end - tokens[i].start) == 5 &&
            strncmp((const char*)RAMMapJSON + tokens[i].start, "start", 5) == 0) {
            
            if (total_attributes >= MAX_ATTRIBUTES) break;

            wpc_ram_attribute_t *attr = &attr_list[total_attributes];
            attr->length = 1;
            attr->encoding = ENC_INT;
            attr->label[0] = '\0';
            attr->start = 0;

            int current_obj = tokens[i].parent;
            if (current_obj < 0) continue;

            char parent_prio_label[48] = {0};
            char sub_label[32] = {0};

            /* 1. SUB-LABEL: The key name for this object (e.g., 'initials' or 'last_played') */
            int key_idx = current_obj - 1;
            if (key_idx >= 0 && tokens[key_idx].type == JSMN_STRING) {
                int k_len = tokens[key_idx].end - tokens[key_idx].start;
                int copy_len = (k_len > 31) ? 31 : k_len;
                memcpy(sub_label, (const char*)RAMMapJSON + tokens[key_idx].start, copy_len);
                sub_label[copy_len] = '\0';
            }

            /* 2. SIBS-ONLY RECURSIVE SEARCH: Climb up but only look at IMMEDIATE siblings */
            int search_ptr = tokens[current_obj].parent;
            while (search_ptr >= 0 && parent_prio_label[0] == '\0') {
                /* Stop if we hit an array - sections like 'high_scores' are arrays */
                if (tokens[search_ptr].type == JSMN_ARRAY) break;

                if (tokens[search_ptr].type == JSMN_OBJECT) {
                    for (int m = search_ptr + 1; m < r && tokens[m].start < tokens[search_ptr].end; m++) {
                        /* CRITICAL: Only check tokens that are DIRECT children of this parent */
                        if (tokens[m].parent == search_ptr && tokens[m].type == JSMN_STRING) {
                            const char* g_key = (const char*)RAMMapJSON + tokens[m].start;
                            int g_key_len = tokens[m].end - tokens[m].start;
                            int v_idx = m + 1;

                            /* Prioritize short_label */
                            if (strncmp(g_key, "short_label", g_key_len) == 0) {
                                int v_len = tokens[v_idx].end - tokens[v_idx].start;
                                int copy_len = (v_len > 47) ? 47 : v_len;
                                memcpy(parent_prio_label, (const char*)RAMMapJSON + tokens[v_idx].start, copy_len);
                                parent_prio_label[copy_len] = '\0';
                                break; 
                            } 
                            /* Fallback to long label */
                            else if (strncmp(g_key, "label", g_key_len) == 0 && parent_prio_label[0] == '\0') {
                                int v_len = tokens[v_idx].end - tokens[v_idx].start;
                                int copy_len = (v_len > 47) ? 47 : v_len;
                                memcpy(parent_prio_label, (const char*)RAMMapJSON + tokens[v_idx].start, copy_len);
                                parent_prio_label[copy_len] = '\0';
                            }
                        }
                    }
                }
                search_ptr = tokens[search_ptr].parent;
            }

            /* 3. LOCAL SCAN: Metadata inside the attribute's own object */
            for (int k = current_obj + 1; k < r && tokens[k].start < tokens[current_obj].end; k++) {
                if (tokens[k].parent == current_obj && tokens[k].type == JSMN_STRING) {
                    const char* key = (const char*)RAMMapJSON + tokens[k].start;
                    int key_len = tokens[k].end - tokens[k].start;
                    jsmntok_t *val_tok = &tokens[k+1];
                    const char* val = (const char*)RAMMapJSON + val_tok->start;
                    int val_len = val_tok->end - val_tok->start;

                    if (strncmp(key, "start", key_len) == 0) {
                        attr->start = (uint16_t)atoi(val);
                    } else if (strncmp(key, "label", key_len) == 0) {
                        int copy_len = (val_len > 47) ? 47 : val_len;
                        memcpy(attr->label, val, copy_len);
                        attr->label[copy_len] = '\0';
                    } else if (strncmp(key, "encoding", key_len) == 0) {
                        attr->encoding = get_encoding_type(val, val_len);
                    } else if (strncmp(key, "length", key_len) == 0) {
                        attr->length = (uint8_t)atoi(val);
                    }
                }
            }
            
            /* 4. FINAL CONCAT: Merge parent with sub-label */
            if (attr->label[0] == '\0') {
                if (parent_prio_label[0] != '\0') {
                    snprintf(attr->label, sizeof(attr->label), "%.22s - %.22s", parent_prio_label, sub_label);
                } else {
                    strncpy(attr->label, sub_label, sizeof(attr->label) - 1);
                    attr->label[sizeof(attr->label) - 1] = '\0';
                }
            }

            if (attr->start > 0) total_attributes++;
        }
    }
}


/**
 * @brief Get attribute by index
 * @param index 0 to (total_attributes - 1)
 * @param out_attr Pointer to a struct to be filled
 * @return 0 on success, -1 on error
 */
int GameStateAttributes_GetAttribute(uint16_t index, wpc_ram_attribute_t *out_attr) {
    if (index >= total_attributes || out_attr == NULL) {
        return -1;
    }
    
    *out_attr = attr_list[index];
    return 0;
}

/**
 * @brief Get the total number of attributes found
 */
uint16_t GameStateAttributes_GetAttributeCount(void) {
    return total_attributes;
}

const char DOWTranslation[8][3] = {"na\0","Su\0", "Mo\0", "Tu\0", "We\0", "Th\0", "Fr\0", "Sa\0"};

/**
 * @brief Formats a RAM attribute into a 40x2 display string.
 * @param attr Pointer to the attribute metadata.
 * @param ramAtOffset Pointer to the start of the RAM data for this attribute.
 * @param outBuf Buffer of at least 82 bytes (40x2 lines + \n + \0).
 * @return true if formatting succeeded.
 */
bool GameStateAttributes_FormatAttributeForDisplay(wpc_ram_attribute_t *attr, uint8_t *ramAtOffset, char *outBuf) {
    if (!attr || !ramAtOffset || !outBuf) return false;

    char valueStr[41] = {0}; 
    uint32_t tempVal = 0;

    switch (attr->encoding) {
        case ENC_INT:
            // WPC/6809 is Big-Endian: high byte at lowest address
            for (int i = 0; i < attr->length; i++) {
                tempVal = (tempVal << 8) | ramAtOffset[i];
            }
            snprintf(valueStr, sizeof(valueStr), "%lu", (unsigned long)tempVal);
            break;

        case ENC_BCD:
            // Scores: 5 bytes = 10 digits. 0x12 0x34 0x56 0x78 0x90 -> "1,234,567,890"
            {
                char rawBcd[21] = {0};
                int pos = 0;
                for (int i = 0; i < attr->length && pos < 19; i++) {
                    pos += snprintf(&rawBcd[pos], 3, "%02X", ramAtOffset[i]);
                }
                
                // Remove leading zeros
                char *start = rawBcd;
                while (*start == '0' && *(start + 1) != '\0') start++;
                
                // Copy to final value string
                strncpy(valueStr, start, sizeof(valueStr) - 1);
            }
            break;

        case ENC_CH:
            // Strings/Initials
            for (int i = 0; i < attr->length && i < 40; i++) {
                // Ensure character is printable; replace with space if not
                char c = (char)ramAtOffset[i];
                valueStr[i] = (c >= 32 && c <= 126) ? c : ' ';
            }
            valueStr[attr->length > 40 ? 40 : attr->length] = '\0';
            break;

        case ENC_BOOL:
            // 0x00 = NO, 0x01 (or non-zero) = YES
            snprintf(valueStr, sizeof(valueStr), "%s", ramAtOffset[0] ? "YES" : "NO");
            break;

/*
ASICDateTimeBase[0] = ASICYearHigh;
ASICDateTimeBase[1] = ASICYearLow;
ASICDateTimeBase[2] = ASICMonth;
ASICDateTimeBase[3] = ASICDay;
ASICDateTimeBase[4] = ASICDOW;
ASICDateTimeBase[5] = 0; // Hour is 0
ASICDateTimeBase[6] = 1; // valid = 1
ASICDateTimeBase[7] = ASICDateChecksum1; // checksum high byte
ASICDateTimeBase[8] = ASICDateChecksum2; // checksum low byte
*/

        case ENC_WPC_RTC:
            // RTC is usually 7 bytes: YY MM DD HH MM SS DayOfWeek
            // Simple format: YYYY-MM-DD
            snprintf(valueStr, sizeof(valueStr), "%d-%d-%d %.2s %d:%02d", 
                     ramAtOffset[0]*256+ramAtOffset[1], ramAtOffset[2], ramAtOffset[3], 
                     DOWTranslation[ramAtOffset[4]], ramAtOffset[5], ramAtOffset[6]);
            break;

        default:
            snprintf(valueStr, sizeof(valueStr), "--");
            break;
    }

    // Format for 40x2 DMD: Line 1 (Label), Line 2 (Value)
    // Using \n as the delimiter for your display driver
    /* %.40s ensures each line is truncated to 40 characters to fit the 82-byte buffer (40+1+40+1) */
    snprintf(outBuf, 82, "%.21s\n%.21s", attr->label, valueStr);

    return true;
}