#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h> // 可変長引数に必要なヘッダー
#include "kana_ime.h"
#include "draw_font.h" 
#include "mplus_font_10x10.h"

// デバッグログ用関数 (iprintfのラッパー)
void debug_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    viprintf(format, args);
    va_end(args);
}

// キーボードの表示状態
static bool keyboardVisible = false;
// 描画バッファへのポインタ (メインスクリーン用)
static u16* mainScreenBuffer = NULL; 

// 入力されたローマ字を保持するバッファ
static char input_romaji_buffer[256] = {0};
static int input_romaji_len = 0;

// 変換済みひらがなを保持するバッファ
static u16 converted_kana_buffer[256] = {0}; // SJISコードを保持
static int converted_kana_len = 0;

// ローマ字変換テーブル
typedef struct {
    const char* romaji;
    u16 sjis_code;
} RomajiKanaMap;

// Corrected Romaji-Kana mapping table
const RomajiKanaMap romaji_kana_map[] = {
    // 3-letter combinations first
    {"kya", 0x82AB}, {"kyu", 0x82AD}, {"kyo", 0x82AF},
    {"sya", 0x82B5}, {"syu", 0x82B7}, {"syo", 0x82B9},
    {"sha", 0x82B5}, {"shu", 0x82B7}, {"sho", 0x82B9},
    {"tya", 0x82BF}, {"tyu", 0x82C1}, {"tyo", 0x82C3},
    {"cha", 0x82BF}, {"chu", 0x82C1}, {"cho", 0x82C3},
    {"nya", 0x82C9}, {"nyu", 0x82CB}, {"nyo", 0x82CD},
    {"hya", 0x82D1}, {"hyu", 0x82D3}, {"hyo", 0x82D5},
    {"mya", 0x82D9}, {"myu", 0x82DB}, {"myo", 0x82DD},
    {"rya", 0x82E1}, {"ryu", 0x82E3}, {"ryo", 0x82E5},
    {"gya", 0x82AC}, {"gyu", 0x82AE}, {"gyo", 0x82B0},
    {"zya", 0x82B6}, {"zyu", 0x82B8}, {"zyo", 0x82BA},
    {"ja",  0x82B6}, {"ji",  0x82B6}, {"ju",  0x82B8}, {"jo",  0x82BA},
    {"dya", 0x82C0}, {"dyu", 0x82C2}, {"dyo", 0x82C4},
    {"bya", 0x82D2}, {"byu", 0x82D4}, {"byo", 0x82D6},
    {"pya", 0x82D3}, {"pyu", 0x82D5}, {"pyo", 0x82D7},
    {"tsa", 0x82C1}, {"tsi", 0x82C1}, {"tse", 0x82C1}, {"tso", 0x82C1},
    {"fa",  0x834A}, {"fi",  0x834C}, {"fe",  0x834F}, {"fo",  0x8350},

    // 2 letters
    {"ka", 0x82A9}, {"ki", 0x82AA}, {"ku", 0x82AD}, {"ke", 0x82AF}, {"ko", 0x82B1},
    {"sa", 0x82B3}, {"si", 0x82B5}, {"su", 0x82B7}, {"se", 0x82B9}, {"so", 0x82BB},
    {"shi",0x82B5},
    {"ta", 0x82BD}, {"ti", 0x82BF}, {"tu", 0x82C1}, {"te", 0x82C3}, {"to", 0x82C4},
    {"chi",0x82BF}, {"tsu",0x82C1},
    {"na", 0x82C5}, {"ni", 0x82C6}, {"nu", 0x82C7}, {"ne", 0x82C8}, {"no", 0x82C9},
    {"ha", 0x82CA}, {"hi", 0x82CB}, {"hu", 0x82CD}, {"he", 0x82CE}, {"ho", 0x82CF},
    {"fu", 0x82CD},
    {"ma", 0x82D0}, {"mi", 0x82D1}, {"mu", 0x82D2}, {"me", 0x82D3}, {"mo", 0x82D4},
    {"ya", 0x82D5}, {"yu", 0x82D6}, {"yo", 0x82D7},
    {"ra", 0x82D8}, {"ri", 0x82D9}, {"ru", 0x82DA}, {"re", 0x82DB}, {"ro", 0x82DC},
    {"wa", 0x82DD}, {"wo", 0x82DF},
    {"ga", 0x82AA}, {"gi", 0x82AC}, {"gu", 0x82AE}, {"ge", 0x82B0}, {"go", 0x82B2},
    {"za", 0x82B4}, {"zi", 0x82B6}, {"zu", 0x82B8}, {"ze", 0x82BA}, {"zo", 0x82BC},
    {"da", 0x82BE}, {"di", 0x82C0}, {"dzu", 0x82C2}, {"de", 0x82C4}, {"do", 0x82C5},
    {"ba", 0x82CB}, {"bi", 0x82CD}, {"bu", 0x82CF}, {"be", 0x82D1}, {"bo", 0x82D3},
    {"pa", 0x82CC}, {"pi", 0x82CE}, {"pu", 0x82D0}, {"pe", 0x82D2}, {"po", 0x82D4},
    {"nn", 0x82F1},
    {"n'", 0x82F1},
    {"xtu",0x82C0}, // っ
    {"xtsu",0x82C0},

    // 1 letter
    {"a", 0x829F}, {"i", 0x82A1}, {"u", 0x82A3}, {"e", 0x82A5}, {"o", 0x82A7},
    {"n", 0x82F1},
    {"-", 0x815B}, // ー

    {NULL, 0}
};

void kanaIME_init(void) {
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);
    mainScreenBuffer = (u16*)VRAM_A;

    consoleDemoInit();
    keyboardDemoInit();
    keyboardShow();

    #ifdef ENABLE_DEBUG_LOG
    iprintf("\x1b[2J"); // Clear console
    debug_log("Kana IME Initialized.\n");
    #endif
}

void kanaIME_update(void) {
    int key = keyboardUpdate();

    if (key > 0) {
        #ifdef ENABLE_DEBUG_LOG
        iprintf("\x1b[2J");
        debug_log("Key: %c (0x%X)\n", (key > 31 && key < 127) ? key : '?', key);
        debug_log("Before: Romaji='%s' (%d), Kana=%d\n", input_romaji_buffer, input_romaji_len, converted_kana_len);
        #endif

        if (key == '\b') { // Backspace
            if (input_romaji_len > 0) {
                input_romaji_len--;
                input_romaji_buffer[input_romaji_len] = '\0';
            } else if (converted_kana_len > 0) {
                converted_kana_len--;
                converted_kana_buffer[converted_kana_len] = 0;
            }
        } else if (key == '\n') { // Enter
            // Flush remaining romaji as is
            for(int i = 0; i < input_romaji_len; i++) {
                if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = input_romaji_buffer[i];
                }
            }
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';
        } else { // Character
            if (input_romaji_len < 30) {
                input_romaji_buffer[input_romaji_len++] = (char)key;
                input_romaji_buffer[input_romaji_len] = '\0';
            }
        }

        // Conversion Logic
        bool converted;
        do {
            converted = false;
            
            // Sokuon (促音) "っ" handling (e.g., kk, tt)
            if (input_romaji_len >= 2 && input_romaji_buffer[0] == input_romaji_buffer[1]) {
                char c = input_romaji_buffer[0];
                if (strchr("kstnhmyrwgzdbpv", c) != NULL) {
                     if(converted_kana_len < 255) {
                        converted_kana_buffer[converted_kana_len++] = 0x82C0; // っ
                    }
                    memmove(input_romaji_buffer, input_romaji_buffer + 1, --input_romaji_len);
                    input_romaji_buffer[input_romaji_len] = '\0';
                    converted = true;
                    continue; // Restart conversion loop
                }
            }

            for (int i = 0; romaji_kana_map[i].romaji != NULL; i++) {
                const char* romaji = romaji_kana_map[i].romaji;
                int len = strlen(romaji);

                if (strncmp(input_romaji_buffer, romaji, len) == 0) {
                    // Special 'n' handling
                    if (len == 1 && *romaji == 'n') {
                        char next_char = input_romaji_buffer[1];
                        if (next_char != '\0' && strchr("aiueoyn'", next_char) == NULL) {
                            // Convert 'n' to 'ん' if it's followed by a consonant (or end of buffer)
                        } else {
                            // Part of a larger syllable (na, ni, nn, n'), so don't convert yet.
                            continue;
                        }
                    }

                    if(converted_kana_len < 255) {
                        converted_kana_buffer[converted_kana_len++] = romaji_kana_map[i].sjis_code;
                    }
                    memmove(input_romaji_buffer, input_romaji_buffer + len, input_romaji_len - len + 1);
                    input_romaji_len -= len;
                    converted = true;
                    break; // Restart the scan from the beginning of the map
                }
            }
        } while (converted);

        #ifdef ENABLE_DEBUG_LOG
        debug_log("After: Romaji='%s' (%d), Kana=%d\n", input_romaji_buffer, input_romaji_len, converted_kana_len);
        #endif

        // Drawing
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
}

void kanaIME_showKeyboard(void) { keyboardShow(); }
void kanaIME_hideKeyboard(void) { keyboardHide(); }
char kanaIME_getChar(void) { return 0; }