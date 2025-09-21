#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "kana_ime.h"
#include "draw_font.h"
#include "romakana_map.h"

#define ENABLE_DEBUG_LOG

#ifdef ENABLE_DEBUG_LOG
void debug_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    viprintf(format, args);
    va_end(args);
}
#else
void debug_log(const char* format, ...) { (void)format; }
#endif

static u16* mainScreenBuffer = NULL;
static char input_romaji_buffer[32] = {0};
static int input_romaji_len = 0;
static u16 converted_kana_buffer[256] = {0};
static int converted_kana_len = 0;

void kanaIME_init(void) {
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);
    mainScreenBuffer = (u16*)VRAM_A;

    consoleDemoInit();
    keyboardDemoInit();
    keyboardShow();

    #ifdef ENABLE_DEBUG_LOG
    iprintf("\x1b[2J");
    debug_log("Kana IME Initialized.\n");
    #endif
}

void kanaIME_update(void) {
    int key = keyboardUpdate();

    if (key <= 0) {
        return;
    }

    #ifdef ENABLE_DEBUG_LOG
    iprintf("\x1b[2J");
    debug_log("Key: %c (0x%X)\n", (key > 31 && key < 127) ? key : '?', key);
    debug_log("Before: Romaji='%s' (%d)\n", input_romaji_buffer, input_romaji_len);
    #endif

    if (key == '\b') { 
        if (input_romaji_len > 0) {
            input_romaji_len--;
            input_romaji_buffer[input_romaji_len] = '\0';
        } else if (converted_kana_len > 0) {
            converted_kana_len--;
            converted_kana_buffer[converted_kana_len] = 0;
        }
    } else if (key == '\n') { 
        if (input_romaji_len == 1 && input_romaji_buffer[0] == 'n') {
            if(converted_kana_len < 255) {
                converted_kana_buffer[converted_kana_len++] = 0x82f1; // ん
            }
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';
        }
    } else if (key == ' ') {
        if (input_romaji_len == 1 && input_romaji_buffer[0] == 'n') {
            if(converted_kana_len < 255) {
                converted_kana_buffer[converted_kana_len++] = 0x82f1; // ん
            }
        }
        input_romaji_len = 0;
        input_romaji_buffer[0] = '\0';

        if(converted_kana_len < 255) {
            converted_kana_buffer[converted_kana_len++] = 0x8140;
        }
    } else { 
        if (input_romaji_len < 30) {
            input_romaji_buffer[input_romaji_len++] = (char)key;
            input_romaji_buffer[input_romaji_len] = '\0';
        }
    }

    bool converted_in_pass;
    do {
        converted_in_pass = false;

        if (input_romaji_len >= 2 && input_romaji_buffer[0] == input_romaji_buffer[1] && input_romaji_buffer[0] != 'n') {
            char c = input_romaji_buffer[0];
            if (strchr("kstnhmyrwgzdbpv", c) != NULL) {
                 if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = 0x82c1; // っ
                }
                memmove(input_romaji_buffer, input_romaji_buffer + 1, input_romaji_len);
                input_romaji_len--;
                converted_in_pass = true;
                continue;
            }
        }

        for (int i = 0; romakana_map[i].romaji != NULL; i++) {
            const char* romaji = romakana_map[i].romaji;
            int len = strlen(romaji);

            if (strncmp(input_romaji_buffer, romaji, len) == 0) {
                if (len == 1 && *romaji == 'n') {
                    char next_char = input_romaji_buffer[1];
                    if (next_char != '\0' && strchr("aiueoy',", next_char) == NULL) {
                    } else {
                        continue;
                    }
                }

                if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = romakana_map[i].sjis_code;
                }
                
                memmove(input_romaji_buffer, input_romaji_buffer + len, input_romaji_len - len + 1);
                input_romaji_len -= len;
                converted_in_pass = true;
                break;
            }
        }
    } while (converted_in_pass);

    #ifdef ENABLE_DEBUG_LOG
    debug_log("After: Romaji='%s' (%d)\n", input_romaji_buffer, input_romaji_len);
    #endif

    dmaFillWords(0, mainScreenBuffer, 256 * 192 * 2);
    int x = 10;
    for (int i = 0; i < converted_kana_len; i++) {
        drawFont(x, 10, mainScreenBuffer, converted_kana_buffer[i], RGB15(31,31,31));
        x += 10;
    }
    for (int i = 0; i < input_romaji_len; i++) {
        drawFont(x, 10, mainScreenBuffer, (u16)input_romaji_buffer[i], RGB15(31,31,31));
        x += 10;
    }
}

void kanaIME_showKeyboard(void) { keyboardShow(); }
void kanaIME_hideKeyboard(void) { keyboardHide(); }
char kanaIME_getChar(void) { return 0; }