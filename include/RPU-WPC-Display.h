#ifndef RPU_WPC_DISPLAY_H

bool RPUWPCDisplayInit();
bool RPUWPCShowLogoScreen();
void RPUWPCDisplayText(const char *message);
void RPUWPCDisplayShowLogo(int pageNum, uint32_t tickCount);


#define RPU_WPC_DISPLAY_H
#endif