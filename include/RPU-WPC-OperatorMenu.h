#ifndef RPU_WPC_OPERATOR_MENU_H


void RPU_WPC_SetupPorts();
bool RPU_WPC_CheckForMenuRequest(bool readFromPort);
bool RPU_WPC_Menu(uint32_t curTicks);
bool RPU_WPC_MenuRequiresReset();

#define RPU_WPC_OPERATOR_MENU_H
#endif
