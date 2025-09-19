/*---------------------------------------------------------------------------------

  main.c - Simple Calculator App (Console on Sub Screen)

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For strtod
#include <ctype.h>  // For isdigit

// Character dimensions of the console (approximate)
#define CONSOLE_WIDTH_CHARS 32
#define CONSOLE_HEIGHT_CHARS 24

// Pixel dimensions of the sub screen
#define SCREEN_WIDTH_PX 256
#define SCREEN_HEIGHT_PX 192

// Approximate pixels per character (used for layout, not direct touch conversion anymore)
#define CHAR_WIDTH_PX (SCREEN_WIDTH_PX / CONSOLE_WIDTH_CHARS) // ~8 pixels
#define CHAR_HEIGHT_PX (SCREEN_HEIGHT_PX / CONSOLE_HEIGHT_CHARS) // ~8 pixels

// Display buffers
char display_buffer[17]; // Max 16 digits + null terminator (current input/result)
char expression_buffer[32]; // Max 31 chars + null terminator (expression log)

// Calculator state variables
double current_value = 0.0;
char pending_operation = ' ';
bool new_number_flag = true;

// Button layout (row, col, label) - using character coordinates for drawing
const char* button_labels[5][4] = {
    {"C", "/", "*", "-"},
    {"7", "8", "9", "+"},
    {"4", "5", "6", "="},
    {"1", "2", "3", ""}, // = spans two rows, so last one is empty
    {"0", ".", "", ""} // 0 spans two columns, so last two are empty
};

// New button drawing dimensions (character units)
#define BUTTON_DRAW_WIDTH_CHAR 7  // e.g., "+-----+"
#define BUTTON_DRAW_HEIGHT_CHAR 3 // e.g., top border, label, bottom border

// Button grid properties (character coordinates for drawing and pixel calculation)
#define BUTTON_START_ROW_CHAR 5 // Starting character row for the first button row
#define BUTTON_ROW_SPACING_CHAR (BUTTON_DRAW_HEIGHT_CHAR + 1) // Spacing between button rows (e.g., 3+1=4 chars)
#define BUTTON_START_COL_CHAR 1 // Starting character column for the first button column
#define BUTTON_COL_SPACING_CHAR (BUTTON_DRAW_WIDTH_CHAR + 1) // Spacing between button columns (e.g., 7+1=8 chars)

// Pixel dimensions for a button's touch area (based on character spacing)
#define BUTTON_CELL_WIDTH_PX (BUTTON_COL_SPACING_CHAR * CHAR_WIDTH_PX)
#define BUTTON_CELL_HEIGHT_PX (BUTTON_ROW_SPACING_CHAR * CHAR_HEIGHT_PX)

// Pixel start coordinates for the first button (top-left of the grid)
#define BUTTON_START_ROW_PX (BUTTON_START_ROW_CHAR * CHAR_HEIGHT_PX)
#define BUTTON_START_COL_PX (BUTTON_START_COL_CHAR * CHAR_WIDTH_PX)

// Helper function to draw a button with borders and centered label
void drawButton(int row_char, int col_char, const char* label) {
    int label_len = strlen(label);
    int padding_left = (BUTTON_DRAW_WIDTH_CHAR - 2 - label_len) / 2;
    int padding_right = BUTTON_DRAW_WIDTH_CHAR - 2 - label_len - padding_left;

    // Top border
    iprintf("\x1b[%d;%dH+", row_char, col_char);
    for (int i = 0; i < BUTTON_DRAW_WIDTH_CHAR - 2; ++i) iprintf("-");
    iprintf("+");

    // Label row
    iprintf("\x1b[%d;%dH|", row_char + 1, col_char);
    for (int i = 0; i < padding_left; ++i) iprintf(" ");
    iprintf("%s", label);
    for (int i = 0; i < padding_right; ++i) iprintf(" ");
    iprintf("|");

    // Bottom border
    iprintf("\x1b[%d;%dH+", row_char + 2, col_char);
    for (int i = 0; i < BUTTON_DRAW_WIDTH_CHAR - 2; ++i) iprintf("-");
    iprintf("+");
}

// Function to perform pending operation
void performOperation() {
    double second_operand = strtod(display_buffer, NULL);
    if (pending_operation == '+') {
        current_value += second_operand;
    } else if (pending_operation == '-') {
        current_value -= second_operand;
    } else if (pending_operation == '*') {
        current_value *= second_operand;
    } else if (pending_operation == '/') {
        if (second_operand != 0.0) {
            current_value /= second_operand;
        } else {
            strcpy(display_buffer, "Error"); // Handle division by zero
            current_value = 0.0;
            strcpy(expression_buffer, "");
            pending_operation = ' ';
            new_number_flag = true;
            return;
        }
    }
    // Format current_value back to display_buffer
    snprintf(display_buffer, sizeof(display_buffer), "%.6g", current_value); // Use %g for general formatting
    // Clear expression after operation is complete and result is shown
    strcpy(expression_buffer, "");
    pending_operation = ' ';
    new_number_flag = true;
}

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
    // 上画面をグラフィックモードに設定し、赤い四角形を描画
    videoSetMode(MODE_5_2D); // 2Dモード、BG3をビットマップとして使用
    vramSetBankA(VRAM_A_MAIN_BG); // VRAM_Aをメイン画面のBGとして割り当て
    bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0); // BG3を使用

    u16* main_vram = (u16*)BG_BMP_RAM(3); // BG3のVRAMアドレスを取得 (BG_BMP_RAM(3) for BG3)
    u16 rect_color = RGB15(31, 0, 0); // 赤色 (Red)

    // 画面中央に四角形を描画
    int rect_x = 50;
    int rect_y = 50;
    int rect_width = 150;
    int rect_height = 100;

    for (int y = 0; y < rect_height; y++) {
        for (int x = 0; x < rect_width; x++) {
            if ((rect_x + x) < 256 && (rect_y + y) < 192) { // 画面範囲内かチェック
                main_vram[(rect_y + y) * 256 + (rect_x + x)] = rect_color;
            }
        }
    }

    // 下画面のコンソールを初期化 (consoleDemoInit() は下画面をデフォルトにする)
    consoleDemoInit();

    // 初期状態を設定
    strcpy(display_buffer, "0");
    strcpy(expression_buffer, "");

    // メインループ
    while(1) {
        swiWaitForVBlank();
        scanKeys();
        touchPosition touch;
        touchRead(&touch);

        // 下画面の描画
        iprintf("\x1b[2J"); // Clear sub screen

        // Draw Expression Area (on sub screen, Line 1)
        iprintf("\x1b[1;1H%s", expression_buffer); 
        // Draw Display Area (on sub screen, Line 2)
        iprintf("\x1b[2;1H%s", display_buffer); 

        // Draw Buttons
        for (int r = 0; r < 5; ++r) {
            for (int c = 0; c < 4; ++c) {
                if (button_labels[r][c][0] != '\0') { // If label exists
                    drawButton(BUTTON_START_ROW_CHAR + r * BUTTON_ROW_SPACING_CHAR,
                               BUTTON_START_COL_CHAR + c * BUTTON_COL_SPACING_CHAR,
                               button_labels[r][c]);
                }
            }
        }

        // Touch handling (on sub screen)
        if (keysDown() & KEY_TOUCH) { // Changed from keysHeld() to keysDown() for debouncing
            // Use raw pixel coordinates for more accurate hit detection
            int px = touch.px;
            int py = touch.py;

            bool button_found = false;
            for (int r = 0; r < 5; ++r) {
                for (int c = 0; c < 4; ++c) {
                    if (button_labels[r][c][0] != '\0') {
                        // Calculate button's pixel boundaries
                        int btn_px_start_x = BUTTON_START_COL_PX + c * BUTTON_CELL_WIDTH_PX;
                        int btn_px_end_x = btn_px_start_x + BUTTON_CELL_WIDTH_PX;
                        int btn_px_start_y = BUTTON_START_ROW_PX + r * BUTTON_CELL_HEIGHT_PX;
                        int btn_px_end_y = btn_px_start_y + BUTTON_CELL_HEIGHT_PX;

                        // Check if touch pixel is within this button's pixel area
                        if (px >= btn_px_start_x && px < btn_px_end_x &&
                            py >= btn_px_start_y && py < btn_px_end_y) {
                            
                            const char* pressed_label = button_labels[r][c];

                            // Handle digits
                            if (isdigit((unsigned char)pressed_label[0])) { // Fixed warning here
                                if (new_number_flag || strcmp(display_buffer, "0") == 0 || strcmp(display_buffer, "Error") == 0) {
                                    strcpy(display_buffer, pressed_label);
                                    // strcpy(expression_buffer, ""); // Clear expression when starting new number - removed for log display
                                    new_number_flag = false;
                                } else if (strlen(display_buffer) < 16) { // Max 16 digits
                                    strcat(display_buffer, pressed_label);
                                }
                            }
                            // Handle decimal point
                            else if (strcmp(pressed_label, ".") == 0) {
                                if (new_number_flag || strcmp(display_buffer, "Error") == 0) {
                                    strcpy(display_buffer, "0.");
                                    // strcpy(expression_buffer, ""); // removed
                                    new_number_flag = false;
                                } else if (!strchr(display_buffer, '.')) { // Only add if not already present
                                    strcat(display_buffer, pressed_label);
                                }
                            }
                            // Handle operators
                            else if (strchr("+-*/", pressed_label[0])) {
                                if (pending_operation != ' ') { // If there's a pending operation, perform it first
                                    performOperation();
                                } else { // First operand is the current display value
                                    current_value = strtod(display_buffer, NULL);
                                }
                                pending_operation = pressed_label[0];
                                // Update expression buffer with current value and operator
                                snprintf(expression_buffer, sizeof(expression_buffer), "%.6g %c", current_value, pending_operation);
                                new_number_flag = true;
                            }
                            // Handle equals
                            else if (strcmp(pressed_label, "=") == 0) {
                                if (pending_operation != ' ') {
                                    // Before performing, capture the full expression for display
                                    double second_operand_for_display = strtod(display_buffer, NULL);
                                    snprintf(expression_buffer, sizeof(expression_buffer), "%.6g %c %.6g =", current_value, pending_operation, second_operand_for_display);
                                    performOperation();
                                }
                                // performOperation already clears pending_operation and sets new_number_flag
                            }
                            // Handle clear
                            else if (strcmp(pressed_label, "C") == 0) {
                                strcpy(display_buffer, "0");
                                strcpy(expression_buffer, "");
                                current_value = 0.0;
                                pending_operation = ' ';
                                new_number_flag = true;
                            }

                            // For now, just print the pressed button's label to a debug area on the sub screen
                            // This debug line will be overwritten by the main display, so it's fine.
                            // iprintf("\x1b[23;1HPressed: %s", pressed_label);
                            button_found = true;
                            goto end_touch_check; // Exit loops once button found
                        }
                    }
                }
            }
            end_touch_check:;
            // Clear debug area if no button found or not touching
            // iprintf("\x1b[23;1H                                  ");
        } else {
            // iprintf("\x1b[23;1H                                  ");
        }

        if(keysDown() & KEY_START) break;
    }

    return 0;
}