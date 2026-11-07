#pragma once
// ZX Trinity card = its RP firmware on the hardware model (Trinity firmware/rp-ay/test/emu.bat -> trinity_card.dll
// next to Unreal.exe). [AY] AYFX=1 and the DLL is there - the card sits on chip #FF (in CHRV - ay[1]): every #FFFD /
// #BFFD write goes to the card, reads of #FFFD with chip #FF selected come from it, and ay[1] plays the card's chip
// registers. No DLL - the old built-in AY FX model (sndchip.cpp trin_*). Card console - trinity_card.log
extern bool tc_on;
void tc_init();                      // after the ini is read
void tc_write(int a0, unsigned char v);
unsigned char tc_read();
void tc_frame();                     // end of frame: run the card up to now, its chip registers -> ay[1]
