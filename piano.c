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
#define GRAY 0x7BEF // ȸ�� �߰�

#define WHITE_TILES 7   // ��� ����
#define BLACK_TILES 5   // ��� ����

#define WHITE_KEY_WIDTH 36  // ��� �ǹ� ���� ����
#define WHITE_KEY_HEIGHT 81 // ��� �ǹ� ���� ����
#define BLACK_KEY_WIDTH 22  // ��� �ǹ� ���� ����
#define BLACK_KEY_HEIGHT 54 // ��� �ǹ� ���� ����

#define C_NOTE  382  // �� (261.63 Hz)
#define D_NOTE  340  // �� (293.66 Hz)
#define E_NOTE  303  // �� (329.63 Hz)
#define F_NOTE  286  // �� (349.23 Hz)
#define G_NOTE  255  // �� (392.00 Hz)
#define A_NOTE  227  // �� (440.00 Hz)
#define B_NOTE  202  // �� (493.88 Hz)
#define CH_NOTE 191  // ���� �� (523.25 Hz)


/*******************************************************************************
 * �Լ� ����
 ******************************************************************************/
void MainScreenSetup(void);
unsigned char Screen_Change_icon(void);
void Menu1(void);
void Menu2(void);
void select_keyboard(U08 key);
void GPIO_Init(void);

// Equalizer
void Draw_bar(int bar_index, int y);
void process_key1(void);
void process_key2(void);
void process_key3(void);
void update_Main1_UI(void);

void Draw_line(void);

// Piano
// unsigned char Last_touch_input(void); - ������.
void Piano_TILES(void);
void White_key_Init(void);
void Black_key_Init(void);
void Draw_Keys(void);
void Key_Touch(U16 touch_x, U16 touch_y);
void Key_input_handler(void);
void Play_Note(unsigned int note);

// Mode 2�� Music Player
void MusicPlayer(void);

/*******************************************************************************
 * ���� ����
 ******************************************************************************/
volatile int Menu_selected = 0;
unsigned char icon_flag1, icon_flag2;
volatile int graph_piano_mode;

// Piano PWM
unsigned char key;      // ���� Ű ����
unsigned char key_sum;  // Ű �Է��� �ջ� ���

typedef struct{ // ����ü KeyInfo ����
  U16 xstart;
  U16 xend;
  U16 ystart;
  U16 yend;
} KeyInfo;
KeyInfo White_key[WHITE_TILES]; // 7�� ��� ����ü ����
KeyInfo Black_key[BLACK_TILES]; // 5�� ��� ����ü ����
void Reset_Key_State(KeyInfo *key, unsigned char *key_touch, int key_count, U16 color);
unsigned char IsWhiteKeyTouching[WHITE_TILES] = {0};    // White key ���� �ν�
unsigned char IsBlackKeyTouching[BLACK_TILES] = {0};    // Black key ���� �ν�

// �Ҹ� ���
unsigned int notes[] = {C_NOTE, D_NOTE, E_NOTE, F_NOTE, G_NOTE, A_NOTE, B_NOTE, CH_NOTE};
const U08 *scale[] = {(U08 *)"C4", (U08 *)"D4", (U08 *)"E4", (U08 *)"F4", (U08 *)"G4", (U08 *)"A4", (U08 *)"B4", (U08 *)"C5"};

volatile int debug_mode = 0;    // ����� ����

/*******************************************************************************
 * Timer ���� �Լ�
 ******************************************************************************/
void Initialize_TIM1(void){
   // Configure PE14 as TIM1_CH4 (PWM output)
    GPIOE->MODER &= 0xCFFFFFFF;     // PE14 = alternate function mode
    GPIOE->MODER |= 0x20000000;
    GPIOE->AFR[1] &= 0xF0FFFFFF;   // PE14 = TIM1_CH4
    GPIOE->AFR[1] |= 0x01000000;

    RCC->APB2ENR |= 0x00000001;    // Enable TIM1 clock

    TIM1->PSC = 107;               // 108MHz/(107+1) = 1MHz
    TIM1->CNT = 0;                 // Clear counter
    TIM1->CCMR2 = 0x00006C00;      // OC4M = 0110(PWM mode 1), CC4S = 00(output)
    TIM1->CCER = 0x00001000;       // CC4E = 1(enable OC4 output)
    TIM1->BDTR = 0x00008000;       // MOE = 1
    TIM1->CR1 = 0x0005;            // Edge-aligned, up-counter, enable TIM1
}

/*******************************************************************************
 * ���� �Լ�
 ******************************************************************************/

int main(void){
    unsigned char key;

    Initialize_MCU();                             // Initialize MCU and kit
    Delay_ms(50);                                 // Wait for system stabilization
    Initialize_TFT_LCD();                         // Initialize TFT-LCD module
    Initialize_touch_screen();                    // Initialize touch screen

    TFT_string(0, 9, Green, Black, "****************************************");
    TFT_string(0, 11, White, Black, "                OH-IN-PE-               ");
    TFT_string(0, 13, Green, Black, "****************************************");

    Initialize_TIM1();
    Initialize_Debug_Screen();
    White_key_Init();
    Black_key_Init();
    Delay_ms(1000);
    TFT_clear_screen();

    MainScreenSetup();
    Menu_selected = 0;

    // debug_mode = 1;

    while(1)
    {
        key = Key_input();                       // Key input
        if (key == no_key)                       // If no key input, read touch screen icon
            key = Screen_Change_icon();

        if(Menu_selected == 2){
            Key_input_handler();
        }

        switch (key)
        {
        case KEY1:
            if(Menu_selected == 1){
                process_key1();
                Delay_ms(5);
                update_Main1_UI();
            } else if (Menu_selected == 0) {
                Menu_selected = 1;  
                Menu1();
            }
            break;
        
        case KEY2:
            if(Menu_selected == 1){
                process_key2();
                Delay_ms(5);
                update_Main1_UI();
            } else if (Menu_selected == 0) {
                Menu_selected = 2;
                graph_piano_mode = 1;
                Menu2();
            }
            break;
        
        case KEY3:
            if(Menu_selected == 1){
                process_key3();
                Delay_ms(5);
                update_Main1_UI();
            }else{
                
            }
            break;

        case KEY4:
            if(Menu_selected == 1 || Menu_selected == 2){
                Menu_selected = 0;
                graph_piano_mode = 0;
                LED_off();
                MainScreenSetup();
            } else {
                break;
            }            
        
        default:
            break;
        }
        
    }
}
void MainScreenSetup(void){
    // ���� ȭ�� ����
    TFT_clear_screen();
    Block(7, 32, 312, 224, White,White);
    Block(15, 40, 304, 216, Black,Black);
    TFT_string(0, 0, Black, THEME_HEADER, "              [ Oh-In-Pe- ]             ");
    TFT_string(7, 7, White, THEME_BG, " Sound Modulation Project ");

    // ��ư �׵θ�
    Block(32, 96, 152, 206, White, GRAY);  // button 1
    Block(168, 96, 288, 206, White, GRAY); // butteon 2

    TFT_string(7,17, THEME_ACCENT, GRAY, "Equalizer");
    TFT_string(7,19, THEME_ACCENT, GRAY, "  [KEY1]");

    TFT_string(23,17, THEME_ACCENT, GRAY, "Synthesizer");
    TFT_string(23,19, THEME_ACCENT, GRAY, "   [KEY2]");

}

unsigned char icon_flag1 = 0;
unsigned char icon_flag2 = 0;

unsigned char Screen_Change_icon(void){
    unsigned char keyPressed;

    Touch_screen_input();
    switch (Menu_selected)
    {
    case 0:
        // Main ȭ�鿡���� ȭ�� ��ġ
        if (icon_flag1 == 0 && x_touch >= 32 && x_touch <= 152 && y_touch >= 96 && y_touch <= 206){
            icon_flag1 = 1;
            keyPressed = KEY1;
            Rectangle(32, 96, 152, 206, THEME_ACCENT);  // button 1 Selected

        } else if (icon_flag1 == 0 && x_touch >= 168 && x_touch <= 288 && y_touch >= 96 && y_touch <= 206){
            icon_flag1 = 1;
            keyPressed = KEY2;
            Rectangle(168, 96, 288, 206, THEME_ACCENT); // button 2 selected

        } else if ((icon_flag1 == 1) && (x_touch == 0) && (y_touch == 0)) {
            keyPressed = no_key;
            icon_flag1 = 0;
            Rectangle(32,96,152,206,THEME_BUTTON);
            Rectangle(168,96,288,206,THEME_BUTTON);
            Delay_ms(50);
        } else {    // ��ư Ŭ�� ��ȯ ��
            keyPressed = no_key;
        }
        return keyPressed;
        break;

    case 1:
        // Main ȭ���� �ƴѰ����� ȭ�� ��ġ
        if ((icon_flag2 == 0) && (x_touch >= 30) && (x_touch <= 95) &&
            (y_touch >= 206) && (y_touch <= 225)) {
            keyPressed = KEY1;
            icon_flag2 = 1;
            Rectangle(30, 206, 95, 225, Blue);
    
        } else if ((icon_flag2 == 0) && (x_touch >= 95) && (x_touch <= 160) &&
            (y_touch >= 206) && (y_touch <= 225)) {
            keyPressed = KEY2;
            icon_flag2 = 1;
            Rectangle(95, 206, 160, 225, Blue);
    
        } else if ((icon_flag2 == 0) && (x_touch >= 160) && (x_touch <= 225) &&
            (y_touch >= 206) && (y_touch <= 225)) {
            keyPressed = KEY3;
            icon_flag2 = 1;
            Rectangle(160, 206, 225, 225, Blue);

        } else if ((icon_flag2 == 0) && (x_touch >= 225) && (x_touch <= 290) &&
            (y_touch >= 206) && (y_touch <= 225)) {
            keyPressed = KEY4;
            icon_flag2 = 1;
            Rectangle(225, 206, 290, 225, Blue);

        } else if ((icon_flag2 == 1) && (x_touch == 0) && (y_touch == 0)) {
            keyPressed = no_key;
            icon_flag2 = 0;
            Rectangle(30, 206, 95, 225, Yellow);
            Rectangle(95, 206, 160, 225, Yellow);
            Rectangle(160, 206, 225, 225, Yellow);
            Rectangle(225, 206, 290, 225, Yellow);
            Delay_ms(50);
        } else {
            keyPressed = no_key;
        }
        return keyPressed;
        break;

    default:
        break;
    }
    return keyPressed;
}

/*******************************************************************************
 * Menu1 UI
 ******************************************************************************/

void Menu1(void){   // ���������� ȭ��

    TFT_clear_screen();
    Delay_ms(10);
    // equalizer_init();

    TFT_string(0, 1, Black, THEME_HEADER, "              [ Oh-In-Pe- ]             "); // 0-8
    TFT_string(6, 3, White,Black, "�����ļ�"); // 16-24
    TFT_string(16, 3, White,Black, "�����ļ�"); // 16-24
    TFT_string(26, 3, White,Black, "�����ļ�"); // 16-24
    Draw_line();
    TFT_string(0,26, White, THEME_BG, "     Select    UP     DOWN     Home   ");
    
    Rectangle(30, 206, 95, 225, White);    
    Rectangle(95, 206, 160, 225, White);
    Rectangle(160, 206, 225, 225, White);
    Rectangle(225, 206, 290, 225, White);
    LED_toggle();
    update_Main1_UI();
}
void Draw_line(void){
    for(int i = 0; i <= 9; i++){
        Line(30, 72 + 12*i, 290, 72 + 12*i, GRAY);
    }
    Line(30, 60, 290, 60, White);
    Line(30, 180, 290, 180, White);
    Line(30, 120, 290, 120, White);
}
unsigned int current_flag;
volatile int up_flag, down_flag;
volatile int stage = 0;
#define SLIDER_NUM 3
#define SLIDER_Y_MAX 180
#define SLIDER_Y_MIN 60
#define SLIDER_MOVE 6

typedef struct{
    int value;
    int y;
} Sliders;

volatile uint8_t IsSelected = 10;
Sliders bar[SLIDER_NUM] = {{10, 120},{10, 120},{10, 120}};

void process_key1(void){    // KEY1�� ���� �� ���� ����
    IsSelected++;
    if(IsSelected >= SLIDER_NUM)
        IsSelected = 0;
}
void process_key2(void){    // KEY2�� ���� �� ��ġ ++
    if(bar[IsSelected].y > SLIDER_Y_MIN){
        bar[IsSelected].y -= SLIDER_MOVE;
        bar[IsSelected].value++; // ��ġ�� ����(0 ~ 20)
        if(bar[IsSelected].value > 20){
            bar[IsSelected].value = 20;
        }
    }
}
void process_key3(void){    // KEY3�� ���� �� ��ġ --
    if(bar[IsSelected].y < SLIDER_Y_MAX){
        bar[IsSelected].y += SLIDER_MOVE;
        bar[IsSelected].value--; // ��ġ�� ����(0 ~ 20)
        if(bar[IsSelected].value <= 0){
            bar[IsSelected].value = 0;
        }
    }
}

void update_Main1_UI(void){
    for(int i = 0; i < SLIDER_NUM; i++){
        char value_str[12];
        memset(value_str, 0, sizeof(value_str)); // ���ڿ� ���� �ʱ�ȭ
        sprintf(value_str, "%2d", bar[i].value); // �������� ���ڿ��� ��ȯ
        if(i == IsSelected){
            TFT_string(6 + i * 10, 23,White,THEME_BG,"SELECTED");
            TFT_string(9 + i * 10, 5, THEME_ACCENT, THEME_BG, (U08 *)value_str); // UI�� ��ġ �� ���û��� ǥ��(����)
        }else{
            TFT_string(6 + i * 10, 23,THEME_BG,THEME_BG,"SELECTED");
            TFT_string(9 + i * 10, 5, THEME_TEXT, THEME_BG, (U08 *)value_str);  // UI�� ��ġ �� ���û��� ǥ��(����)
        }
        Block(65 + i * 80, 57, 95 + i * 80, 183, THEME_BG, GRAY);   // ���������� �� ��� �� ��ŭ ��������� ��� ������Ʈ
        Block(65 + i * 80, bar[i].y, 95 + i * 80, 183, Blue, Blue);
        Block(65 + i * 80, bar[i].y-3, 95 + i * 80, bar[i].y + 3, White, Blue);
    }
}

/*******************************************************************************
 * Nenu2 UI
 ******************************************************************************/

void Menu2(void){   // �ǾƳ� FFT ȭ��

    TFT_clear_screen();
    Delay_ms(10);
    Piano_TILES();
    Line(7,120,312,120,White);
    Key_input_handler();

    TFT_string(3,27,White,THEME_BG,"Ű���带 �����ּ���.(������ - KEY4)");
}

// ===============================================================
// Piano Tiles(C to B)
// ===============================================================

void Piano_TILES(void){
  TFT_clear_screen();
  Delay_ms(20);
  MusicPlayer();
  Draw_Keys();
}

void White_key_Init(void){  // ��� �ʱ�ȭ
  int x = 32;

  for(int i = 0; i < WHITE_TILES; i++){ // ����ü�� �׸� ��ġ ����
    White_key[i].xstart = x;
    White_key[i].xend = x + WHITE_KEY_WIDTH;
    White_key[i].ystart = 127;
    White_key[i].yend = 127 + WHITE_KEY_HEIGHT;
    x += (WHITE_KEY_WIDTH + 2); 
  }
}
void Black_key_Init(void){  // ��� �ʱ�ȭ
  int x = 32 + WHITE_KEY_WIDTH - (BLACK_KEY_WIDTH / 2);

  //��� ��ġ
  int Black_key_pos[5] = {0,1,3,4,5};

  for(int i = 0; i < BLACK_TILES; i++){ // ����ü�� �׸� ��ġ ����
    Black_key[i].xstart = x + (Black_key_pos[i] * (WHITE_KEY_WIDTH + 2));
    Black_key[i].xend = Black_key[i].xstart + BLACK_KEY_WIDTH;
    Black_key[i].ystart = 127;
    Black_key[i].yend = 127 + BLACK_KEY_HEIGHT;
  }
}

void Play_Note(unsigned int note) { // ���� ���
    TIM1->ARR = note;            
    TIM1->CCR4 = note / 2;        // ��Ƽ����Ŭ 50% ����
}

void Draw_Keys(void) {
    TFT_string(0, 0, Black, THEME_HEADER, "              [ Oh-In-Pe- ]             ");
    // �� �ǹ� �׸���
    for (int i = 0; i < WHITE_TILES; i++) {
        Block(White_key[i].xstart, White_key[i].ystart, White_key[i].xend, White_key[i].yend, Black, White);
    }
    // ���� �ǹ� �׸���
    for (int i = 0; i < BLACK_TILES; i++) {
        Block(Black_key[i].xstart, Black_key[i].ystart, Black_key[i].xend, Black_key[i].yend, White, Black);
    }
    Rectangle(32, 127, 296, 208, THEME_HIGHLIGHT); // �׵θ� �簢��    
}

void Reset_Key_State(KeyInfo *key, unsigned char *key_touch, int key_count, U16 color) {    // State reset
    for (int i = 0; i < key_count; i++) {
        if (key_touch[i]) {
            key_touch[i] = 0;
            Block(key[i].xstart, key[i].ystart, key[i].xend, key[i].yend, Black, color);
        }
    }
}

void Key_Touch(U16 xpos, U16 ypos){
    for(int i = 0; i < WHITE_TILES; i++){
        if(xpos >= White_key[i].xstart && xpos <= White_key[i].xend && ypos >= White_key[i].ystart && ypos <= White_key[i].yend){
            if(!IsWhiteKeyTouching[i]){
                IsWhiteKeyTouching[i] = 1;
                Block(White_key[i].xstart, White_key[i].ystart,
                        White_key[i].xend, White_key[i].yend, Black, GRAY);    // Ŭ���� ȸ������ ��ȯ
                Play_Note(notes[i]); // ��� ����(Toggle)
            }
        }  
        else {
            // ���¸� �ʱ�ȭ�Ͽ� �ٸ� ��ġ �� �ٽ� ���� �����ϰ� ��
            IsWhiteKeyTouching[i] = 0;
            Block(White_key[i].xstart, White_key[i].ystart,
                        White_key[i].xend, White_key[i].yend, Black, White);
        }
    }


    for(int i = 0; i < BLACK_TILES; i++){
        if(xpos >= Black_key[i].xstart && xpos <= Black_key[i].xend && ypos >= Black_key[i].ystart && ypos <= Black_key[i].yend){
            if(!IsBlackKeyTouching[i]){
                IsBlackKeyTouching[i] = 1;
                Block(Black_key[i].xstart, Black_key[i].ystart,
                        Black_key[i].xend, Black_key[i].yend, Black, GRAY);    // ��ġ�� ȸ������ ��ȯ
                // Play_Note(notes[i]);  // ���� ��� - ��� ����.
            }
        } 
        else {
            // ���¸� �ʱ�ȭ�Ͽ� �ٸ� ��ġ �� �ٽ� ���� �����ϰ� ��
            IsBlackKeyTouching[i] = 0;
            Block(Black_key[i].xstart, Black_key[i].ystart,
                    Black_key[i].xend, Black_key[i].yend, Black, Black);
        }
    }  
}

void Key_input_handler(void){ // ��ġ �����ؼ� �ǾƳ� ���� ����
    if(graph_piano_mode == 1){
        if(x_touch >= 32 && x_touch <= 296 && y_touch >= 127 && y_touch <= 208){
            Key_Touch(x_touch, y_touch);    // �ǾƳ� ���� ���� Ŭ���� �ش� �Լ� ȣ��
        } else {
            Reset_Key_State(White_key, IsWhiteKeyTouching, WHITE_TILES, White); // ��� �ʱ�ȭ

            // ���� �ǹ� �ʱ�ȭ �� �� �ǹ� ���� �ٽ� ǥ��
            for (int i = 0; i < BLACK_TILES; i++) {
                Block(Black_key[i].xstart, Black_key[i].ystart,
                      Black_key[i].xend, Black_key[i].yend, White, Black);
                IsBlackKeyTouching[i] = 0;
            }
            TIM1->CCR4 = 0;  // PWM ����                
        }
  }
}

void MusicPlayer(void){
    // �簢�� �׵θ�
    Rectangle(31, 15, 297, 112, THEME_HEADER);
    // �Լ� ���� ��
}


/*******************************************************************************
 * Debug
 ******************************************************************************/