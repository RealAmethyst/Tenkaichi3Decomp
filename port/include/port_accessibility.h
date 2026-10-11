#ifndef PORT_ACCESSIBILITY_H
#define PORT_ACCESSIBILITY_H

/* Game pointers cross this boundary as their actual 32-bit addresses, as in plat_songs.h. */
void Port_A11yStop(void);
void Port_A11yScope(int active);
void Port_A11yLogo(int section);
void Port_A11yLegal(void);
void Port_A11yTitle(void);
void Port_A11yTitlePrompt(void);
void Port_A11yTitleItem(int cursor, int count);
void Port_A11yDialogOpen(void);
void Port_A11yDialogClose(void);
void Port_A11yDialogFrame(void);
void Port_A11yDialogVisibility(int visible);
void Port_A11yDialogText(int part, unsigned int address);
void Port_A11yDialogDraw(int visible, int choices, int cursor);

#endif
