#include <nds.h>
#include "mplus_font_10x10.h"
#include "mplus_font_10x10alpha.h"

//FrameBuffer
void drawFont(int x, int y, u16* buffer, u16 code, u16 color) {
	int i, j;
	u16 bit;
	u16 block;
	
	buffer += y * SCREEN_WIDTH + x;
	
	// 1-byte char
	if ( code < 0x100 ) {
		u8 block8;
		u8 bit8;
		
		for ( i = 0; i < 13; i++ ) {
			u16* line = buffer + (SCREEN_WIDTH * i);
			bit8 = 0x80;
			block8 = FONT_MPLUS_10x10A[code][i];
		
			for ( j = 0; j < 8; j++ ) {
				if ( (block8 & bit8) > 0 ) {
					*(line + j) = color;
				}
				bit8 = bit8 >> 1;
			}
		}
	} else {
	// 2-byte char
		for ( i = 0; i < 11; i++ ) {
			uint16* line = buffer + (SCREEN_WIDTH * i);
			bit = 0x8000;
			block = FONT_MPLUS_10x10[code][i];
		
			for ( j = 0; j < 11; j++ ) {
				if ( (block & bit) > 0 ) {
					*(line + j) = color;
				}
				bit = bit >> 1;
			}
		}
	}
}