#ifndef GAME_STATE_ATTRIBUTES_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ENC_INT,
    ENC_BCD,
    ENC_CH,
    ENC_BOOL,
    ENC_WPC_RTC,
    ENC_UNKNOWN
} wpc_encoding_t;

typedef struct {
    char label[48];      // Sufficient for nested labels like "Grand Champion Initials"
    uint16_t start;
    wpc_encoding_t encoding;
    uint8_t length;
} wpc_ram_attribute_t;

void GameStateAttributes_Parse(void);
int GameStateAttributes_GetAttribute(uint16_t index, wpc_ram_attribute_t *out_attr);
uint16_t GameStateAttributes_GetAttributeCount(void);
bool GameStateAttributes_FormatAttributeForDisplay(wpc_ram_attribute_t *attr, uint8_t *ramAtOffset, char *outBuf);

#define GAME_STATE_ATTRIBUTES_H
#endif