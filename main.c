#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"
#include "arm_math.h"
#include "stdio.h"
#include "stdint.h"

/*******************************************************************************
 * ��� ����
 ******************************************************************************/
#define THEME_BG        Black       // ����
#define THEME_HEADER    0x861D      // ���� ȸ��
#define THEME_TEXT      0xBDF7      // ���� ȸ��
#define THEME_HIGHLIGHT 0x05FF      // ���̶���Ʈ ����
#define THEME_ACCENT    0xFD20      // ���� ����
#define THEME_BUTTON    0x2DB7      // ��ư ����
#define THEME_WARNING   0xFBE0      // ��� ����
#define GRAY            0x7BEF      // ȸ��

#define WHITE_TILES         7       // ��� ����
#define BLACK_TILES         5       // ��� ����

#define WHITE_KEY_WIDTH     36      // ��� �ǹ� ���� ����
#define WHITE_KEY_HEIGHT    81      // ��� �ǹ� ���� ����
#define BLACK_KEY_WIDTH     22      // ��� �ǹ� ���� ����
#define BLACK_KEY_HEIGHT    54      // ��� �ǹ� ���� ����

#define C_NOTE  382  // �� (261.63 Hz)
#define D_NOTE  340  // �� (293.66 Hz)
#define E_NOTE  303  // �� (329.63 Hz)
#define F_NOTE  286  // �� (349.23 Hz)
#define G_NOTE  255  // �� (392.00 Hz)
#define A_NOTE  227  // �� (440.00 Hz)
#define B_NOTE  202  // �� (493.88 Hz)
#define CH_NOTE 191  // ���� �� (523.25 Hz)

#define SLIDER_NUM      3
#define SLIDER_Y_MAX    180
#define SLIDER_Y_MIN    60
#define SLIDER_MOVE     6

// WAV ��� ���� ��� ���� �߰�
#define BAR_WIDTH       15 
#define BAR_GAP         4
#define START_Y         220
#define MAX_HEIGHT      140
#define INITIAL_VOLUME  200
#define BUFFER_SIZE     512
#define BUFFER_COUNT    2
#define WAV_HEADER_SIZE 44

/*******************************************************************************
 * Ÿ�� ����
 ******************************************************************************/
typedef struct {
    uint16_t xstart;
    uint16_t xend;
    uint16_t ystart;
    uint16_t yend;
} KeyInfo;

typedef struct {
    int value;
    int y;
} Slider;

// WAV ��� ����ü �߰�
typedef struct {
    uint32_t sampleRate;
    uint16_t numChannels;
    uint16_t bitsPerSample;
    uint32_t dataSize;
} WAV_Header;

/*******************************************************************************
 * �Լ� ����
 ******************************************************************************/
/* �ʱ�ȭ �Լ� */
void System_Init(void);
void TIM1_Init(void);
void GPIO_Init(void);

/* ���� ȭ�� �� �޴� */
void MainScreen(void);
void Menu_Equalizer(void);
void Menu_Piano(void);
void Menu_Piano_WAV(void); // �ǾƳ�� WAV �÷��̾ ���ÿ� ǥ���ϴ� �Լ�

/* �Է� ó�� */
unsigned char Get_Key_Input(void);
unsigned char Get_Touch_Input(void);

/* UI �׸��� */
void Draw_MainScreen(void);
void Draw_Equalizer_UI(void);
void Update_Equalizer_UI(void);
void Draw_Piano_WAV_UI(void);
void Draw_Keys(void);

/* �ǾƳ� ��� */
void White_Key_Init(void);
void Black_Key_Init(void);
void Play_Note(unsigned int note);
void Reset_Key_State(KeyInfo *key, unsigned char *key_touch, int key_count, uint16_t color);
void Key_Touch_Handler(uint16_t xpos, uint16_t ypos);
void Piano_Input_Handler(void);

/* ���������� ��� */
void Process_Equalizer_Key1(void);
void Process_Equalizer_Key2(void);
void Process_Equalizer_Key3(void);

/* WAV �÷��̾� ��� �߰� */
void Initialize_Peripherals(void);
void Play_Audio(void);
void Update_WAV_Display(void);
void TFT_Filename(void);
void Check_valid_increment_file(void);
void Check_valid_decrement_file(void);
uint8_t ParseWAVHeader(uint32_t sector);
void ConfigureVS1053ForWAV(void);

/*******************************************************************************
 * ���� ����
 ******************************************************************************/
volatile int menu_selected = 0;
volatile int graph_piano_mode = 0;

KeyInfo white_keys[WHITE_TILES];
KeyInfo black_keys[BLACK_TILES];
unsigned char is_white_key_touching[WHITE_TILES] = {0};
unsigned char is_black_key_touching[BLACK_TILES] = {0};

unsigned int notes[] = {C_NOTE, D_NOTE, E_NOTE, F_NOTE, G_NOTE, A_NOTE, B_NOTE, CH_NOTE};

Slider sliders[SLIDER_NUM] = {{10, 120}, {10, 120}, {10, 120}};
volatile uint8_t selected_slider = 0;

/* WAV �÷��̾� ���� ���� �߰� */
unsigned char total_file;
unsigned char file_number = 0;
unsigned short index = 512; 
volatile uint8_t currentBuffer = 0;
uint32_t WAV_start_sector[MAX_FILE];
uint32_t WAV_start_backup[MAX_FILE];
uint32_t WAV_end_sector;
uint8_t play_flag = 0;
uint8_t WAVbuffer[BUFFER_COUNT][BUFFER_SIZE];
uint32_t file_start[MAX_FILE];
uint32_t file_size[MAX_FILE];
uint16_t volume = INITIAL_VOLUME;
WAV_Header wavHeader;

/*******************************************************************************
 * ���� �Լ�
 ******************************************************************************/
int main(void) {
    System_Init();

    while (1) {
        unsigned char key = Get_Key_Input();

        if (menu_selected == 2) {
            // �ǾƳ� �Է� ó��
            Piano_Input_Handler();
            // WAV �÷��̾� ����
            Play_Audio();
            Update_WAV_Display();
        }

        switch (key) {
            case KEY1:
                if (menu_selected == 1) {
                    Process_Equalizer_Key1();
                    Update_Equalizer_UI();
                } else if (menu_selected == 0) {
                    menu_selected = 1;
                    Menu_Equalizer();
                } else if (menu_selected == 2) {
                    play_flag ^= 0x01; // ���/�Ͻ����� ���
                }
                break;
            case KEY2:
                if (menu_selected == 0) {
                    menu_selected = 2;
                    Menu_Piano_WAV(); // �ǾƳ�� WAV �÷��̾� ���� ����
                } else if (menu_selected == 2) {
                    if (file_number != (total_file - 1))
                        file_number++;
                    else
                        file_number = 0;
                    TFT_Filename();
                    Check_valid_increment_file();
                    WAV_start_sector[file_number] = WAV_start_backup[file_number];
                    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
                    index = 512;
                    VS1053b_software_reset();
                }
                break;
            case KEY3:
                if (menu_selected == 1) {
                    Process_Equalizer_Key3();
                    Update_Equalizer_UI();
                } else if (menu_selected == 2) {
                    if (file_number != 0)
                        file_number--;
                    else
                        file_number = total_file - 1;
                    TFT_Filename();
                    Check_valid_decrement_file();
                    WAV_start_sector[file_number] = WAV_start_backup[file_number];
                    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
                    index = 512;
                    VS1053b_software_reset();
                }
                break;
            case KEY4:
                if (menu_selected != 0) {
                    menu_selected = 0;
                    graph_piano_mode = 0;
                    MainScreen();
                }
                break;
            default:
                break;
        }
    }
}

/*******************************************************************************
 * �ʱ�ȭ �Լ�
 ******************************************************************************/
void System_Init(void) {
    Initialize_MCU();
    Delay_ms(50);
    Initialize_TFT_LCD();
    Initialize_touch_screen();
    TIM1_Init();
    White_Key_Init();
    Black_Key_Init();
    Initialize_Peripherals(); // WAV �÷��̾� �ʱ�ȭ �߰�
    MainScreen();
}

void TIM1_Init(void) {
    GPIOE->MODER &= 0xCFFFFFFF;     // PE14 = alternate function mode
    GPIOE->MODER |= 0x20000000;
    GPIOE->AFR[1] &= 0xF0FFFFFF;    // PE14 = TIM1_CH4
    GPIOE->AFR[1] |= 0x01000000;

    RCC->APB2ENR |= 0x00000001;     // Enable TIM1 clock

    TIM1->PSC = 107;                // 108MHz/(107+1) = 1MHz
    TIM1->CNT = 0;                  // Clear counter
    TIM1->CCMR2 = 0x00006C00;       // OC4M = 0110(PWM mode 1), CC4S = 00(output)
    TIM1->CCER = 0x00001000;        // CC4E = 1(enable OC4 output)
    TIM1->BDTR = 0x00008000;        // MOE = 1
    TIM1->CR1 = 0x0005;             // Edge-aligned, up-counter, enable TIM1
}

/*******************************************************************************
 * ���� ȭ�� �� �޴�
 ******************************************************************************/
void MainScreen(void) {
    TFT_clear_screen();
    Draw_MainScreen();
}

void Menu_Equalizer(void) {
    TFT_clear_screen();
    Draw_Equalizer_UI();
}

void Menu_Piano_WAV(void) {
    TFT_clear_screen();
    Draw_Piano_WAV_UI();
    graph_piano_mode = 1;

    // WAV �÷��̾� �ʱ�ȭ
    TFT_Filename();
    Check_valid_increment_file();
    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
}

/*******************************************************************************
 * �Է� ó�� �Լ�
 ******************************************************************************/
unsigned char Get_Key_Input(void) {
    unsigned char key = Key_input();
    if (key == no_key) {
        key = Get_Touch_Input();
    }
    return key;
}

unsigned char Get_Touch_Input(void) {
    unsigned char keyPressed = no_key;
    Touch_screen_input();

    if (menu_selected == 0) {
        if (x_touch >= 32 && x_touch <= 152 && y_touch >= 96 && y_touch <= 206) {
            keyPressed = KEY1;
        } else if (x_touch >= 168 && x_touch <= 288 && y_touch >= 96 && y_touch <= 206) {
            keyPressed = KEY2;
        }
    } else if (menu_selected == 1) {
        // ���������� �޴� �Է� ó��
        if (x_touch >= 30 && x_touch <= 95 && y_touch >= 206 && y_touch <= 225) {
            keyPressed = KEY1;
        } else if (x_touch >= 95 && x_touch <= 160 && y_touch >= 206 && y_touch <= 225) {
            keyPressed = KEY2;
        } else if (x_touch >= 160 && x_touch <= 225 && y_touch >= 206 && y_touch <= 225) {
            keyPressed = KEY3;
        } else if (x_touch >= 225 && x_touch <= 290 && y_touch >= 206 && y_touch <= 225) {
            keyPressed = KEY4;
        }
    } else if (menu_selected == 2) {
        // WAV �÷��̾� ���� ��ư �Է� ó��
        if (x_touch >= 10 && x_touch <= 70 && y_touch >= 70 && y_touch <= 100) {
            keyPressed = KEY1; // ���/�Ͻ�����
        } else if (x_touch >= 80 && x_touch <= 140 && y_touch >= 70 && y_touch <= 100) {
            keyPressed = KEY2; // ���� ��
        } else if (x_touch >= 150 && x_touch <= 210 && y_touch >= 70 && y_touch <= 100) {
            keyPressed = KEY3; // ���� ��
        } else if (x_touch >= 220 && x_touch <= 280 && y_touch >= 70 && y_touch <= 100) {
            keyPressed = KEY4; // ���� ȭ������
        }
    }
    return keyPressed;
}

/*******************************************************************************
 * UI �׸��� �Լ�
 ******************************************************************************/
void Draw_MainScreen(void) {
    Block(7, 32, 312, 224, White, White);
    Block(15, 40, 304, 216, Black, Black);
    TFT_string(0, 0, Black, THEME_HEADER, "              [ Oh-In-Pe- ]             ");
    TFT_string(7, 7, White, THEME_BG, " Sound Modulation Project ");

    Block(32, 96, 152, 206, White, GRAY);
    Block(168, 96, 288, 206, White, GRAY);

    TFT_string(7, 17, THEME_ACCENT, GRAY, "Equalizer");
    TFT_string(7, 19, THEME_ACCENT, GRAY, "  [KEY1]");

    TFT_string(23, 17, THEME_ACCENT, GRAY, "Piano & WAV");
    TFT_string(23, 19, THEME_ACCENT, GRAY, "   [KEY2]");
}

void Draw_Equalizer_UI(void) {
    TFT_string(0, 1, Black, THEME_HEADER, "              [ Oh-In-Pe- ]             ");
    TFT_string(6, 3, White, Black, "�����ļ�");
    TFT_string(16, 3, White, Black, "�����ļ�");
    TFT_string(26, 3, White, Black, "�����ļ�");

    for (int i = 0; i <= 9; i++) {
        Line(30, 72 + 12 * i, 290, 72 + 12 * i, GRAY);
    }
    Line(30, 60, 290, 60, White);
    Line(30, 180, 290, 180, White);
    Line(30, 120, 290, 120, White);

    TFT_string(0, 26, White, THEME_BG, "     Select    UP     DOWN     Home   ");

    Rectangle(30, 206, 95, 225, White);
    Rectangle(95, 206, 160, 225, White);
    Rectangle(160, 206, 225, 225, White);
    Rectangle(225, 206, 290, 225, White);

    Update_Equalizer_UI();
}

void Update_Equalizer_UI(void) {
    for (int i = 0; i < SLIDER_NUM; i++) {
        char value_str[12];
        memset(value_str, 0, sizeof(value_str));
        sprintf(value_str, "%2d", sliders[i].value);
        if (i == selected_slider) {
            TFT_string(6 + i * 10, 23, White, THEME_BG, "SELECTED");
            TFT_string(9 + i * 10, 5, THEME_ACCENT, THEME_BG, (U08 *)value_str);
        } else {
            TFT_string(6 + i * 10, 23, THEME_BG, THEME_BG, "SELECTED");
            TFT_string(9 + i * 10, 5, THEME_TEXT, THEME_BG, (U08 *)value_str);
        }
        Block(65 + i * 80, 57, 95 + i * 80, 183, THEME_BG, GRAY);
        Block(65 + i * 80, sliders[i].y, 95 + i * 80, 183, Blue, Blue);
        Block(65 + i * 80, sliders[i].y - 3, 95 + i * 80, sliders[i].y + 3, White, Blue);
    }
}

void Draw_Piano_WAV_UI(void) {
    TFT_string(0, 0, Black, THEME_HEADER, "              [ Oh-In-Pe- ]             ");
    Draw_Keys();

    // WAV �÷��̾� ���� ǥ��
    TFT_string(0, 2, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 4, THEME_HIGHLIGHT, THEME_BG, "���� ��� ���� WAV ����:");
    TFT_Filename();
    TFT_string(0, 8, White, THEME_BG, "PLAY[KEY1]Next[KEY2]Prev[KEY3]home[KEY4]");
}

void Draw_Keys(void) {
    for (int i = 0; i < WHITE_TILES; i++) {
        Block(white_keys[i].xstart, white_keys[i].ystart, white_keys[i].xend, white_keys[i].yend, Black, White);
    }
    for (int i = 0; i < BLACK_TILES; i++) {
        Block(black_keys[i].xstart, black_keys[i].ystart, black_keys[i].xend, black_keys[i].yend, White, Black);
    }
    Rectangle(32, 127, 296, 208, THEME_HIGHLIGHT);
}

/*******************************************************************************
 * �ǾƳ� ��� �Լ�
 ******************************************************************************/
void White_Key_Init(void) {
    int x = 32;
    for (int i = 0; i < WHITE_TILES; i++) {
        white_keys[i].xstart = x;
        white_keys[i].xend = x + WHITE_KEY_WIDTH;
        white_keys[i].ystart = 127;
        white_keys[i].yend = 127 + WHITE_KEY_HEIGHT;
        x += (WHITE_KEY_WIDTH + 2);
    }
}

void Black_Key_Init(void) {
    int x = 32 + WHITE_KEY_WIDTH - (BLACK_KEY_WIDTH / 2);
    int black_key_pos[5] = {0, 1, 3, 4, 5};

    for (int i = 0; i < BLACK_TILES; i++) {
        black_keys[i].xstart = x + (black_key_pos[i] * (WHITE_KEY_WIDTH + 2));
        black_keys[i].xend = black_keys[i].xstart + BLACK_KEY_WIDTH;
        black_keys[i].ystart = 127;
        black_keys[i].yend = 127 + BLACK_KEY_HEIGHT;
    }
}

void Play_Note(unsigned int note) {
    TIM1->ARR = note;
    TIM1->CCR4 = note / 2;
}

void Reset_Key_State(KeyInfo *key, unsigned char *key_touch, int key_count, uint16_t color) {
    for (int i = 0; i < key_count; i++) {
        if (key_touch[i]) {
            key_touch[i] = 0;
            Block(key[i].xstart, key[i].ystart, key[i].xend, key[i].yend, Black, color);
        }
    }
}

void Key_Touch_Handler(uint16_t xpos, uint16_t ypos) {
    for (int i = 0; i < WHITE_TILES; i++) {
        if (xpos >= white_keys[i].xstart && xpos <= white_keys[i].xend &&
            ypos >= white_keys[i].ystart && ypos <= white_keys[i].yend) {
            if (!is_white_key_touching[i]) {
                is_white_key_touching[i] = 1;
                Block(white_keys[i].xstart, white_keys[i].ystart, white_keys[i].xend, white_keys[i].yend, Black, GRAY);
                Play_Note(notes[i]);
            }
        } else {
            is_white_key_touching[i] = 0;
            Block(white_keys[i].xstart, white_keys[i].ystart, white_keys[i].xend, white_keys[i].yend, Black, White);
        }
    }

    for (int i = 0; i < BLACK_TILES; i++) {
        if (xpos >= black_keys[i].xstart && xpos <= black_keys[i].xend &&
            ypos >= black_keys[i].ystart && ypos <= black_keys[i].yend) {
            if (!is_black_key_touching[i]) {
                is_black_key_touching[i] = 1;
                Block(black_keys[i].xstart, black_keys[i].ystart, black_keys[i].xend, black_keys[i].yend, Black, GRAY);
            }
        } else {
            is_black_key_touching[i] = 0;
            Block(black_keys[i].xstart, black_keys[i].ystart, black_keys[i].xend, black_keys[i].yend, Black, Black);
        }
    }
}

void Piano_Input_Handler(void) {
    if (graph_piano_mode == 1) {
        Touch_screen_input();  // ��ġ ��ǥ ������Ʈ
        if (x_touch >= 32 && x_touch <= 296 && y_touch >= 127 && y_touch <= 208) {
            Key_Touch_Handler(x_touch, y_touch);
        } else {
            Reset_Key_State(white_keys, is_white_key_touching, WHITE_TILES, White);
            Reset_Key_State(black_keys, is_black_key_touching, BLACK_TILES, Black);
            TIM1->CCR4 = 0;
        }
    }
}

/*******************************************************************************
 * ���������� ��� �Լ�
 ******************************************************************************/
void Process_Equalizer_Key1(void) {
    selected_slider++;
    if (selected_slider >= SLIDER_NUM)
        selected_slider = 0;
}

void Process_Equalizer_Key2(void) {
    if (sliders[selected_slider].y > SLIDER_Y_MIN) {
        sliders[selected_slider].y -= SLIDER_MOVE;
        sliders[selected_slider].value++;
        if (sliders[selected_slider].value > 20) {
            sliders[selected_slider].value = 20;
        }
    }
}

void Process_Equalizer_Key3(void) {
    if (sliders[selected_slider].y < SLIDER_Y_MAX) {
        sliders[selected_slider].y += SLIDER_MOVE;
        sliders[selected_slider].value--;
        if (sliders[selected_slider].value < 0) {
            sliders[selected_slider].value = 0;
        }
    }
}

/*******************************************************************************
 * WAV �÷��̾� ��� ����
 ******************************************************************************/
void Initialize_Peripherals(void) {
    Initialize_SD();
    Initialize_FAT32();
    Initialize_VS1053b();
    Delay_ms(1000);

    volume = 175;
    uint8_t bass = 10;
    uint8_t treble = 5;

    VS1053b_SetVolume(volume);
    Delay_ms(1);
    VS1053b_SetBassTreble(bass, treble);

    total_file = fatGetDirEntry(FirstDirCluster);

    for (unsigned char i = 0; i < total_file; i++) {
        WAV_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        WAV_start_backup[i] = WAV_start_sector[i];
    }

    file_number = 0;
}

void Play_Audio(void) {
    static unsigned short index = 512;
    static unsigned char i;

    if (((GPIOC->IDR & 0x0080) == 0x0080) && (play_flag == 1)) {
        if (index == 512) {
            if (WAV_end_sector == WAV_start_sector[file_number]) {
                if (file_number != (total_file - 1))
                    file_number++;
                else
                    file_number = 0;

                TFT_Filename();
                Check_valid_increment_file();

                WAV_start_sector[file_number] = WAV_start_backup[file_number];
                WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
                VS1053b_software_reset();
            }
            index = 0;
            SD_read_sector(WAV_start_sector[file_number]++, WAVbuffer[currentBuffer]);
        }

        for (i = 0; i < 32; i++) {
            GPIOC->BSRR = 0x00400000;
            SPI3_write(WAVbuffer[currentBuffer][index++]);
            GPIOC->BSRR = 0x00000040;
        }
    }
}

void Update_WAV_Display(void) {
    static unsigned short loop = 0;
    unsigned char key = Key_input();

    loop++;
    if ((loop == 500) && (play_flag == 1)) {
        loop = 0;
    }
}

void TFT_Filename(void) {
    unsigned char file_flag;

    TFT_string(0, 6, THEME_TEXT, THEME_BG, "                                    ");

    file_flag = Get_long_filename(file_number);

    if (file_flag == 0)
        TFT_short_filename(0, 6, White, THEME_BG);
    else if (file_flag == 1)
        TFT_long_filename(0, 6, White, THEME_BG);
    else if (file_flag == 2)
        TFT_string(0, 6, Red, THEME_BG, "* ���� �̸��� �ʹ� ��ϴ� *");
    else
        TFT_string(0, 6, Red, THEME_BG, "*** ���� �̸� ���� ***");
}

void Check_valid_increment_file(void) {
    unsigned char file_OK_flag = 0;
    do {
        if (extension != 0x00574156) { // "WAV" �������� Ȯ��
            if (file_number != (total_file - 1))
                file_number++;
            else
                file_number = 0;
            TFT_Filename();
        } else {
            file_OK_flag = 1;
        }
    } while (file_OK_flag == 0);
}

void Check_valid_decrement_file(void) {
    unsigned char file_OK_flag = 0;
    do {
        if (extension != 0x00574156) { // "WAV" �������� Ȯ��
            if (file_number != 0)
                file_number--;
            else
                file_number = total_file - 1;
            TFT_Filename();
        } else {
            file_OK_flag = 1;
        }
    } while (file_OK_flag == 0);
}