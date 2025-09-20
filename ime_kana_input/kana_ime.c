#include <nds.h>
#include <stdio.h>
#include <string.h> // memset, strlen, strcmp, memmoveのために追加
#include "kana_ime.h"
#include "draw_font.h" // drawFont関数を使うために追加
#include "mplus_font_10x10.h" // FONT_MPLUS_10x10を使うために追加

// キーボードの表示状態
static bool keyboardVisible = false;
// 描画バッファへのポインタ (メインスクリーン用)
static u16* mainScreenBuffer = NULL; // メインスクリーン用バッファを追加

// 入力されたローマ字を保持するバッファ
static char input_romaji_buffer[256] = {0};
static int input_romaji_len = 0;

// 変換済みひらがなを保持するバッファ
static u16 converted_kana_buffer[256] = {0}; // SJISコードを保持
static int converted_kana_len = 0;

// ローマ字変換テーブル (rk.cの考え方を参考に、長いものから順に並べる)
typedef struct {
    const char* romaji;
    u16 sjis_code;
} RomajiKanaMap;

const RomajiKanaMap romaji_kana_map[] = {
    // 拗音
    {"kya", 0x82B5}, {"kyu", 0x82B6}, {"kyo", 0x82B7},
    {"sha", 0x82B9}, {"shu", 0x82BA}, {"sho", 0x82BB},
    {"cha", 0x82BD}, {"chu", 0x82BE}, {"cho", 0x82BF},
    {"nya", 0x82C5}, {"nyu", 0x82C6}, {"nyo", 0x82C7},
    {"hya", 0x82C9}, {"hyu", 0x82CA}, {"hyo", 0x82CB},
    {"mya", 0x82CD}, {"myu", 0x82CE}, {"myo", 0x82CF},
    {"rya", 0x82D9}, {"ryu", 0x82DA}, {"ryo", 0x82DB},
    {"gya", 0x82B5 + 0x01}, {"gyu", 0x82B6 + 0x01}, {"gyo", 0x82B7 + 0x01},
    {"ja", 0x82DC}, {"ju", 0x82DD}, {"jo", 0x82DE},
    {"bya", 0x82B5 + 0x02}, {"byu", 0x82B6 + 0x02}, {"byo", 0x82B7 + 0x02},
    {"pya", 0x82B5 + 0x03}, {"pyu", 0x82B6 + 0x03}, {"pyo", 0x82B7 + 0x03},

    // 促音 (小さい「つ」)
    {"tta", 0x82C1}, {"tti", 0x82C1}, {"ttu", 0x82C1}, {"tte", 0x82C1}, {"tto", 0x82C1},
    {"kku", 0x82C1}, {"ssu", 0x82C1}, {"cchi", 0x82C1}, {"tchi", 0x82C1},
    {"pp", 0x82C1}, {"kk", 0x82C1}, {"ss", 0x82C1}, {"tt", 0x82C1}, {"ch", 0x82C1},

    // 撥音 (ん)
    {"nn", 0x82F3},
    {"n'", 0x82F3},

    // 五十音
    {"a", 0x82A0}, {"i", 0x82A2}, {"u", 0x82A4}, {"e", 0x82A6}, {"o", 0x82A8},
    {"ka", 0x82AB}, {"ki", 0x82AD}, {"ku", 0x82AF}, {"ke", 0x82B1}, {"ko", 0x82B3},
    {"sa", 0x82B4}, {"shi", 0x82B6}, {"su", 0x82B8}, {"se", 0x82BA}, {"so", 0x82BB},
    {"ta", 0x82BC}, {"chi", 0x82BE}, {"tsu", 0x82C0}, {"te", 0x82C2}, {"to", 0x82C3},
    {"na", 0x82C4}, {"ni", 0x82C6}, {"nu", 0x82C8}, {"ne", 0x82CA}, {"no", 0x82CB},
    {"ha", 0x82CC}, {"hi", 0x82CE}, {"fu", 0x82D0}, {"he", 0x82D2}, {"ho", 0x82D4},
    {"ma", 0x82D5}, {"mi", 0x82D7}, {"mu", 0x82D9}, {"me", 0x82DB}, {"mo", 0x82DD},
    {"ya", 0x82DE}, {"yu", 0x82DF}, {"yo", 0x82E0},
    {"ra", 0x82E1}, {"ri", 0x82E3}, {"ru", 0x82E5}, {"re", 0x82E7}, {"ro", 0x82E9},
    {"wa", 0x82EA}, {"wo", 0x82EB},
    {"n", 0x82F3},

    {"ga", 0x82AB + 0x01}, {"gi", 0x82AD + 0x01}, {"gu", 0x82AF + 0x01}, {"ge", 0x82B1 + 0x01}, {"go", 0x82B3 + 0x01},
    {"za", 0x82B4 + 0x01}, {"ji", 0x82B6 + 0x01}, {"zu", 0x82B8 + 0x01}, {"ze", 0x82BA + 0x01}, {"zo", 0x82BB + 0x01},
    {"da", 0x82BC + 0x01}, {"dji", 0x82BE + 0x01}, {"dzu", 0x82C0 + 0x01}, {"de", 0x82C2 + 0x01}, {"do", 0x82C3 + 0x01},
    {"ba", 0x82CC + 0x01}, {"bi", 0x82CE + 0x01}, {"bu", 0x82D0 + 0x01}, {"be", 0x82D2 + 0x01}, {"bo", 0x82D4 + 0x01},
    {"pa", 0x82CC + 0x02}, {"pi", 0x82CE + 0x02}, {"pu", 0x82D0 + 0x02}, {"pe", 0x82D2 + 0x02}, {"po", 0x82D4 + 0x02},

    {"xa", 0x82A1}, {"xi", 0x82A3}, {"xu", 0x82A5}, {"xe", 0x82A7}, {"xo", 0x82A9},
    {"xtsu", 0x82C1},
    {"xya", 0x82DE}, {"xyu", 0x82DF}, {"xyo", 0x82E0},

    {NULL, 0}
};

void kanaIME_init(void) {
    powerOn(POWER_ALL);

    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);
    mainScreenBuffer = (u16*)VRAM_A;

    consoleDemoInit();

    keyboardDemoInit();
    keyboardShow();

    dmaFillWords(0, mainScreenBuffer, 256 * 192 * 2);
    drawFont(10, 10, mainScreenBuffer, 'A', RGB15(31,31,31));
}

void kanaIME_update(void) {
    int key = keyboardUpdate();

    if (key > 0) {
        if (key == '\b') {
            if (input_romaji_len > 0) {
                input_romaji_len--;
                input_romaji_buffer[input_romaji_len] = 0;
            } else if (converted_kana_len > 0) {
                converted_kana_len--;
                converted_kana_buffer[converted_kana_len] = 0;
            }
        } else if (key == '\n') {
            // Enterが押されたら、入力バッファに残っているローマ字を確定
            // 未変換のローマ字をそのままconverted_kana_bufferに追加
            for (int i = 0; i < input_romaji_len; i++) {
                if (converted_kana_len < sizeof(converted_kana_buffer) / sizeof(u16) - 1) {
                    converted_kana_buffer[converted_kana_len] = (u16)input_romaji_buffer[i]; // ASCII文字をそのまま追加
                    converted_kana_len++;
                }
            }
            // 入力バッファをクリア
            memset(input_romaji_buffer, 0, sizeof(input_romaji_buffer));
            input_romaji_len = 0;
        } else {
            if (input_romaji_len < sizeof(input_romaji_buffer) - 1) {
                input_romaji_buffer[input_romaji_len] = (char)key;
                input_romaji_len++;
                input_romaji_buffer[input_romaji_len] = 0;
            }
        }

        u16 converted_sjis = 0;
        int matched_len = 0;

        for (int i = 0; romaji_kana_map[i].romaji != NULL; i++) {
            int len = strlen(romaji_kana_map[i].romaji);
            if (input_romaji_len >= len && strncmp(input_romaji_buffer, romaji_kana_map[i].romaji, len) == 0) {
                converted_sjis = romaji_kana_map[i].sjis_code;
                matched_len = len;
                break;
            }
        }

        dmaFillWords(0, mainScreenBuffer, 256 * 192 * 2);

        if (converted_sjis != 0) {
            if (converted_kana_len < sizeof(converted_kana_buffer) / sizeof(u16) - 1) {
                converted_kana_buffer[converted_kana_len] = converted_sjis;
                converted_kana_len++;
                converted_kana_buffer[converted_kana_len] = 0;
            }
            memmove(input_romaji_buffer, &input_romaji_buffer[matched_len], input_romaji_len - matched_len + 1);
            input_romaji_len -= matched_len;
        }

        for (int i = 0; i < converted_kana_len; i++) {
            drawFont(10 + i * 10, 10, mainScreenBuffer, converted_kana_buffer[i], RGB15(31,31,31));
        }
        for (int i = 0; i < input_romaji_len; i++) {
            drawFont(10 + (converted_kana_len * 10) + i * 10, 10, mainScreenBuffer, (u16)input_romaji_buffer[i], RGB15(31,31,31));
        }
    }
}

void kanaIME_showKeyboard(void) {
    keyboardVisible = true;
}

void kanaIME_hideKeyboard(void) {
    keyboardVisible = false;
    keyboardHide();
    dmaFillWords(0, mainScreenBuffer, 256 * 192 * 2);
}

char kanaIME_getChar(void) {
    return 0;
}