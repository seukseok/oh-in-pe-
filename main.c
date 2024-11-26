// main.c
#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"
#include "arm_math.h"
#include "arm_const_structs.h"

/*******************************************************************************
 * ��� ����
 ******************************************************************************/
#define THEME_BG        Black       // ����
#define THEME_HEADER    0x861D      // ���� �����
#define THEME_TEXT      0xBDF7      // ���� ȸ��
#define THEME_HIGHLIGHT 0x05FF      // û�ϻ�
#define THEME_ACCENT    0xFD20      // ��ȣ��
#define THEME_BUTTON    0x2DB7      // û����
#define THEME_WARNING   0xFBE0      // amber��

#define BAR_WIDTH       15 
#define BAR_GAP         4
#define START_Y         220
#define MAX_HEIGHT      140
#define INITIAL_VOLUME  200
#define BUFFER_SIZE     512    // WAV ������ ���� ũ�� (���� �� * 2����Ʈ)
#define BUFFER_COUNT    2
#define UPDATE_PERIOD   20            // ����Ʈ�� ������Ʈ �ֱ�(ms)

#define FFT_SIZE        1024          // FFT ũ�� (2�� �ŵ�����)

#define WAV_HEADER_SIZE 44            // WAV ��� ũ��

// WAV ���� ����� ���� �߰� ���
#define WAV_HEADER_SIZE 44
#define SAMPLE_RATE     48000
#define CHANNELS        2
#define BITS_PER_SAMPLE 16

/*******************************************************************************
 * �Լ� ����
 ******************************************************************************/
void TFT_filename(void);                          // ���� �̸�, ��ȣ, ũ�� ǥ��
void TFT_volume(void);                            // ���� ǥ��
void TFT_bass(void);                              // ���̽� ǥ��
void TFT_treble(void);                            // Ʈ���� ǥ��
void Check_valid_increment_file(void);            // ��ȿ�� ���� ���� Ȯ��
void Check_valid_decrement_file(void);            // ��ȿ�� ���� ���� Ȯ��
void TFT_MP3_bitrate(U16 highbyte, U16 lowbyte);
unsigned char Icon_input(void);                   // ��ġ ��ũ�� ������ �Է�

void SetupMainScreen(void);                       // ���� ȭ�� ���� �Լ�

// WAV ���� ���� �Լ�
uint8_t ParseWAVHeader(uint32_t sector);
void ConfigureVS1053ForWAV(void);
void SendWAVData(uint8_t* buffer, uint16_t* index);

// FFT ���� �Լ�
void FFT_Init(void);
void PrepareFFTInput(uint8_t* buffer);
void PerformFFT(void);
void DrawSpectrum(void);
void DrawSpectrumBar(uint8_t index, uint16_t height);
void Initialize_VS1053b(void);                    // VS1053B �ʱ�ȭ �Լ�

/*******************************************************************************
 * ���� ����
 ******************************************************************************/
unsigned char total_file;                         // �� ���� ��
unsigned char file_number = 0;                    // ���� ���� ��ȣ

volatile uint32_t SysTick_Count = 0;
volatile uint8_t currentBuffer = 0;
uint32_t WAV_start_sector[MAX_FILE];
uint32_t WAV_start_backup[MAX_FILE];
uint32_t WAV_end_sector;
uint8_t play_flag = 0;                            // ��� �Ǵ� ���� �÷���
uint8_t WAVbuffer[BUFFER_COUNT][BUFFER_SIZE];
uint32_t file_start_cluster[MAX_FILE];
uint32_t file_size[MAX_FILE];
uint16_t volume = INITIAL_VOLUME;
uint8_t FFT_mode = 0; // FFT ��� ���� �߰�

// WAV ���� ��� ������ ������ ����ü
typedef struct {
    uint32_t sampleRate;
    uint16_t numChannels;
    uint16_t bitsPerSample;
    uint32_t dataSize;
} WAV_Header;

WAV_Header wavHeader;

// FFT �Է� �� ��� ����
float32_t fftInput[FFT_SIZE * 2];    // ���Ҽ� �Է� (�Ǽ��ο� �����)
float32_t fftOutput[FFT_SIZE];       // ũ�� ��� ���

// FFT �ν��Ͻ�
const arm_cfft_instance_f32 *S;     // FFT ����ü

/*******************************************************************************
 * SysTick ���� �Լ�
 ******************************************************************************/
void SysTick_Handler(void) {
    SysTick_Count++;
}

void SysTick_Initialize(void) {
    SysTick->LOAD = 216000 - 1;    /* 216MHz/1000 = 216000 (1ms �ֱ�) */
    SysTick->VAL = 0;              /* ī���� �ʱ�ȭ */
    SysTick->CTRL = 0x07;          /* Ŭ�� �ҽ�, ���ͷ�Ʈ, Ÿ�̸� Ȱ��ȭ */
}

/*******************************************************************************
 * ���� �Լ�
 ******************************************************************************/
int main(void) {
    unsigned char i, key;
    unsigned char func_mode = 0;                  // Function key mode
    unsigned short index = 512;                   // Byte index in a sector
    unsigned short loop = 0;                      // MP3 play loop counter
    unsigned int playPercentage;                  // Play percentage

    unsigned short time, stereo;                  // Decode time and mono/stereo
    unsigned short HDAT1, HDAT0;                  // MP3 file frame header information data

    Initialize_MCU();                             // MCU �� ŰƮ �ʱ�ȭ
    Delay_ms(50);                                 // �ý��� ����ȭ ���
    Initialize_TFT_LCD();                         // TFT-LCD ��� �ʱ�ȭ
    Initialize_touch_screen();                    // ��ġ ��ũ�� �ʱ�ȭ

    TFT_string(0, 4, Green, Black, "****************************************");
    TFT_string(0, 6, White, Black, "                OH-IN-PE-               ");
    TFT_string(0, 8, Green, Black, "****************************************");
    TFT_string(0, 23, Cyan, Black, "           SD ī�� �ʱ�ȭ...            ");
    Beep();
    Delay_ms(1000);
    TFT_clear_screen();

    Initialize_SD();                              // SD ī�� �ʱ�ȭ
    Initialize_FAT32();                           // FAT32 ���� �ý��� �ʱ�ȭ
    Initialize_VS1053b();                         // VS1053b �ʱ�ȭ
    SysTick_Initialize();                         // SysTick �ʱ�ȭ
    Delay_ms(1000);

    volume = 175;                       // ���� ����
    bass = 10;                          // ���̽� ����
    treble = 5;                         // Ʈ���� ����

    VS1053b_SetVolume(volume);
    Delay_ms(1);
    VS1053b_SetBassTreble(bass, treble);

    total_file = fatGetDirEntry(FirstDirCluster); // �� ���� �� ��������

    for (i = 0; i < total_file; i++) {            // ��� ������ ���� ���� �ּ� ��������
        WAV_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        WAV_start_backup[i] = WAV_start_sector[i];
    }

    file_number = 0;                              // ���� ��ȣ �ʱ�ȭ

    // ���� ȭ�� ���� �Լ� ȣ��
    SetupMainScreen();

    while (1) {
        if (((GPIOC->IDR & 0x0080) == 0x0080) && (play_flag == 1)) {
            if (index == 512) {
                if (WAV_start_sector[file_number] >= WAV_end_sector) {
                    if (file_number != (total_file - 1))
                        file_number++;
                    else
                        file_number = 0;

                    TFT_filename();
                    Check_valid_increment_file();

                    WAV_start_sector[file_number] = WAV_start_backup[file_number];
                    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
                    VS1053b_software_reset();  // VS1053b software reset to change music file
                }
                index = 0;
                SD_read_sector(WAV_start_sector[file_number]++, WAVbuffer[currentBuffer]);
            }

            for (i = 0; i < 32; i++) {           // Send 32 data bytes
                GPIOC->BSRR = 0x00400000;        // -MP3_DCS = 0
                SPI3_write(WAVbuffer[currentBuffer][index++]);  // Write a byte of MP3 data to VS1053b
                GPIOC->BSRR = 0x00000040;        // -MP3_DCS = 1
            }
        }

        loop++;                                  // Display MP3 file bitrate or play percentage
        if ((extension == 0x004D5033) && (loop == 250) && (play_flag == 1)) {
            HDAT1 = VS1053b_SCI_Read(0x09);
            HDAT0 = VS1053b_SCI_Read(0x08);

            if ((HDAT1 & 0xFFE0) == 0xFFE0)
                if (((HDAT1 & 0x0006) != 0x0000) && ((HDAT0 & 0x0C00) != 0x0C00))
                    TFT_MP3_bitrate(HDAT1, HDAT0);
        } else if ((loop == 500) && (play_flag == 1)) {
            loop = 0;

            time = VS1053b_SCI_Read(0x04);       // Decode time
            TFT_xy(18, 13);
            TFT_color(Magenta, Black);
            TFT_unsigned_decimal(time / 60, 1, 2);
            TFT_xy(21, 13);
            TFT_unsigned_decimal(time % 60, 1, 2);

            playPercentage = WAV_end_sector - WAV_start_sector[file_number];
            playPercentage = (unsigned int)((float)playPercentage / (float)(file_size[file_number] >> 9) * 100.);
            playPercentage = 100 - playPercentage;

            TFT_xy(24, 13);                      // Display play percentage
            TFT_color(Yellow, Black);
            if (playPercentage >= 100)
                TFT_unsigned_decimal(playPercentage, 0, 3);
            else if (playPercentage >= 10)
                TFT_unsigned_decimal(playPercentage, 0, 2);
            else
                TFT_unsigned_decimal(playPercentage, 0, 1);
            TFT_color(Cyan, Black);
            TFT_English('%');
            TFT_English(')');
            TFT_English(' ');
            TFT_English(' ');

            stereo = VS1053b_SCI_Read(0x05);
            TFT_xy(27, 11);                      // Sampling rate
            TFT_color(Yellow, Black);
            TFT_unsigned_decimal(stereo >> 1, 0, 5);
            if ((stereo & 0x0001) == 0x0001) {   // Channel mode = stereo
                TFT_string(32, 17, Cyan, Black, "(");
                TFT_string(33, 17, Yellow, Black, "stereo");
                TFT_string(39, 17, Cyan, Black, ")");
            } else {                             // Channel mode = mono
                TFT_string(32, 17, Cyan, Black, "(");
                TFT_string(33, 17, Yellow, Black, " mono ");
                TFT_string(39, 17, Cyan, Black, ")");
            }
        }


        key = Key_input();                       // Ű �Է�
        if (key == no_key)                       // Ű �Է��� ������ ��ġ ��ũ�� ������ �Է� �б�
            key = Icon_input();

        switch (key) {
            case KEY1:
                play_flag ^= 0x01;               // ��� �Ǵ� ���� ���
                if (play_flag == 1) 
                    TFT_string(33, 13, THEME_HIGHLIGHT, THEME_BG, "�����");
                else
                    TFT_string(33, 13, THEME_TEXT, THEME_BG, " ���� ");
                break;

            case KEY2:
                if (func_mode == 0) {            // Select function
                    func_mode = 1;
                    TFT_string(0, 5, Magenta, Black, "  ");
                    TFT_string(0,17, Magenta, Black, ">>");
                } else if (func_mode == 1) {
                    func_mode = 2;
                    TFT_string(0,17, Magenta, Black, "  ");
                    TFT_string(0,19, Magenta, Black, ">>");
                } else if (func_mode == 2) {
                    func_mode = 3;
                    TFT_string(0,19, Magenta, Black, "  ");
                    TFT_string(0,21, Magenta, Black, ">>");
                } else {
                    func_mode = 0;
                    TFT_string(0,21, Magenta, Black, "  ");
                    TFT_string(0, 5, Magenta, Black, ">>");
                }
                break;

            case KEY3:
                // ����, ���̽�, Ʈ���� ���� �Ǵ� ���� ���� ����
                if (func_mode == 0) {            // ���� ���� ����
                    if (file_number != (total_file - 1))
                        file_number++;
                    else
                        file_number = 0;

                    VS1053b_software_reset();
                    TFT_filename();
                    Check_valid_increment_file();

                    WAV_start_sector[file_number] = WAV_start_backup[file_number];
                    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
                    index = 512;
                    VS1053b_software_reset();
                } else if (func_mode == 1) {     // ���� ����
                    if (volume < 250)
                        volume++;
                    else
                        volume = 5;
                    VS1053b_SetVolume(volume);
                    TFT_volume();
                } else if (func_mode == 2) {     // ���̽� ����
                    if (bass < 15)
                        bass++;
                    else
                        bass = 0;
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_bass();
                } else {                         // Ʈ���� ����
                    if (treble < 7)
                        treble++;
                    else
                        treble = -8;
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_treble();
                }
                break;

            case KEY4:
                // FFT ��� ���
                FFT_mode ^= 1;
                if (FFT_mode == 1) {
                    TFT_string(0, 23, THEME_HIGHLIGHT, THEME_BG, "FFT ����Ʈ�� Ȱ��ȭ");
                } else {
                    TFT_string(0, 23, THEME_TEXT, THEME_BG, "FFT ����Ʈ�� ��Ȱ��ȭ");
                    // ����Ʈ�� ���� ����� (�ʿ�� ����)
                }
                break;

            default:
                break;
        }
    }
}

/* ���� ȭ�� ���� �Լ� */
void SetupMainScreen(void) {
    // ���� ȭ�� ����
    TFT_clear_screen();                           
    TFT_string(0, 0, Black, THEME_HEADER, "  OH-IN-PE-  ");
    TFT_string(0, 3, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 5, THEME_HIGHLIGHT, THEME_BG, ">>");
    TFT_string(0, 7, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 9, THEME_TEXT, THEME_BG, "      ���� ��ȣ : 000/000 (   kbps)     ");
    TFT_string(0,11, THEME_TEXT, THEME_BG, "      ���� �뷮 : 0000KB  (     Hz)     ");
    TFT_string(0,13, THEME_TEXT, THEME_BG, "     Music Play : 00:00(000%)   (      )");
    TFT_string(0,15, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0,17, THEME_TEXT, THEME_BG, "   ����(Volume) : 000%(000/250)         ");
    TFT_string(0,19, THEME_TEXT, THEME_BG, "   ����(Bass)   :  00 (00 ~ 15)         ");
    TFT_string(0,21, THEME_TEXT, THEME_BG, "   ����(Treble) :  00 (-8 ~ +7)         ");
    TFT_string(0,23, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0,25, THEME_TEXT, THEME_BG, "   KEY1      KEY2      KEY3      KEY4   ");
    TFT_string(0,27, THEME_ACCENT, THEME_BG, "  (PLAY)   (select)     (up)     (mode) ");
    
    // ��ư ����
    Rectangle(12, 196, 67, 235, THEME_BUTTON);    
    Rectangle(92, 196, 147, 235, THEME_BUTTON);
    Rectangle(176, 196, 231, 235, THEME_BUTTON);
    Rectangle(256, 196, 311, 235, THEME_BUTTON);

    TFT_volume();                                 // �ʱ� ���� ǥ��
    TFT_bass();
    TFT_treble();

    TFT_xy(22, 9);                                // �� ���� �� ǥ��
    TFT_color(Yellow, Black);
    TFT_unsigned_decimal(total_file, 1, 3);
    TFT_filename();
    Check_valid_increment_file();

    // WAV ���� ��� �Ľ� �� ���� ���� ����
    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
}

/*******************************************************************************
 * WAV ���� ���� �Լ�
 ******************************************************************************/

// WAV ���� ��� �Ľ�
uint8_t ParseWAVHeader(uint32_t sector) {
    uint8_t header[WAV_HEADER_SIZE];
    SD_read_sector(sector, header);

    // WAV ���� �ñ״�ó Ȯ�� ("RIFF" & "WAVE")
    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
        header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
        return 0;
    }

    // ��� ���� ����
    wavHeader.sampleRate = *(uint32_t*)&header[24];
    wavHeader.numChannels = *(uint16_t*)&header[22];
    wavHeader.bitsPerSample = *(uint16_t*)&header[34];
    wavHeader.dataSize = *(uint32_t*)&header[40];

    return 1;
}

// VS1053B WAV ��� ����
void ConfigureVS1053ForWAV(void) {
    VS1053b_software_reset();
    Delay_ms(10);
    
    // Native SPI ��� Ȱ��ȭ
    VS1053b_SCI_Write(0x00, 0x0820);
    Delay_ms(1);
    
    // ���ø� ����Ʈ ����
    VS1053b_SCI_Write(0x05, 0xAC45);  // 48kHz, ���׷���
    
    // ���� �� ���� ����
    VS1053b_SetVolume(volume);
    VS1053b_SetBassTreble(8, 3);
}

// WAV ������ ����
void SendWAVData(uint8_t* buffer, uint16_t* index) {
    GPIOC->BSRR = 0x00400000;      // -MP3_DCS = 0
    
    for(uint8_t i = 0; i < 32; i++) {
        while((GPIOC->IDR & 0x0080) == 0); // DREQ ���
        SPI3_write(buffer[(*index)++]);
    }
    
    GPIOC->BSRR = 0x00000040;      // -MP3_DCS = 1
}

/*******************************************************************************
 * FFT ���� �Լ�
 ******************************************************************************/

// FFT �ʱ�ȭ �Լ�
void FFT_Init(void) {
    switch (FFT_SIZE) {
        case 16:
            S = &arm_cfft_sR_f32_len16;
            break;
        case 32:
            S = &arm_cfft_sR_f32_len32;
            break;
        case 64:
            S = &arm_cfft_sR_f32_len64;
            break;
        case 128:
            S = &arm_cfft_sR_f32_len128;
            break;
        case 256:
            S = &arm_cfft_sR_f32_len256;
            break;
        case 512:
            S = &arm_cfft_sR_f32_len512;
            break;
        case 1024:
            S = &arm_cfft_sR_f32_len1024;
            break;
        case 2048:
            S = &arm_cfft_sR_f32_len2048;
            break;
        case 4096:
            S = &arm_cfft_sR_f32_len4096;
            break;
        default:
            // �������� �ʴ� FFT ũ��
            S = NULL;
            break;
    }
}

// FFT �Է� ������ �غ�
void PrepareFFTInput(uint8_t* buffer) {
    for (int i = 0; i < FFT_SIZE; i++) {
        // 16��Ʈ PCM �����͸� �ε��Ҽ��� �Ǽ��η� ��ȯ, ����δ� 0???�� ����
        int16_t sample = (buffer[i * 2 + 1] << 8) | buffer[i * 2];
        fftInput[2 * i] = (float32_t)sample;       // �Ǽ���
        fftInput[2 * i + 1] = 0.0f;                // �����
    }
}

// FFT ���� �Լ�
void PerformFFT(void) {
    // FFT ����
    arm_cfft_f32(S, fftInput, 0, 1);
    // ���Ҽ� ������� ũ�� ���
    arm_cmplx_mag_f32(fftInput, fftOutput, FFT_SIZE);
}

// ����Ʈ�� �׸��� �Լ�
void DrawSpectrum(void) {
    uint16_t height;
    uint8_t index = 0;
    uint16_t bin_size = FFT_SIZE / 16;    // 16���� ���ļ� �뿪���� ����

    for (int i = 0; i < FFT_SIZE / 2; i += bin_size) {
        float32_t sum = 0.0f;
        for (int j = 0; j < bin_size; j++) {
            sum += fftOutput[i + j];
        }
        // ����Ʈ�� ũ�� ���� �̿��Ͽ� ���� ���� ���
        height = (uint16_t)(sum / bin_size / 10000);  // ������ ����
        if (height > MAX_HEIGHT)
            height = MAX_HEIGHT;

        // ȭ�鿡 ���� �׷��� �׸���
        DrawSpectrumBar(index, height);
        index++;
    }
}

// ����Ʈ�� �ٸ� �׸��� �Լ�
void DrawSpectrumBar(uint8_t index, uint16_t height) {
    uint16_t x = 20 + index * (BAR_WIDTH + BAR_GAP);
    uint16_t color;

    // ���ļ� �뿪�� ���� ���� ���
    if (index < 5) {
        color = Blue;         // ������ - �Ķ���
    }
    else if (index < 11) {
        color = Magenta;      // ������ - ����Ÿ
    }
    else {
        color = Cyan;         // ������ - �þ�
    }

    // ���� ���� �����
    Rectangle(x, START_Y - MAX_HEIGHT, x + BAR_WIDTH, START_Y, THEME_BG);

    // ���ο� ���� �׸���
    Rectangle(x, START_Y - height, x + BAR_WIDTH, START_Y, color);
}

/*******************************************************************************
 * ��Ÿ �Լ�
 ******************************************************************************/

void TFT_filename(void) {
    unsigned char file_flag;
    unsigned short file_KB;

    TFT_string(0, 7, Cyan, Black, "----------------------------------------");
    TFT_string(3, 5, Green, Black, "                                     ");

    file_flag = Get_long_filename(file_number);   // ���� �̸� Ȯ��

    if (file_flag == 0)                           // ª�� ���� �̸� (8.3 ����)
        TFT_short_filename(3, 5, White, Black);
    else if (file_flag == 1)                      // �� ���� �̸�
        TFT_long_filename(3, 5, White, Black);
    else if (file_flag == 2)                      // ���� �̸��� 195�� �̻��� ���
        TFT_string(3, 5, Red, Black, "* ���ϸ� ���� �ʰ� *");
    else                                          // ���� �̸� ����
        TFT_string(3, 5, Red, Black, "*** ���ϸ� ���� ***");

    file_KB = file_size[file_number] / 1024;      // ���� ũ��(KB) ���
    if ((file_size[file_number] % 1024) != 0)
        file_KB++;

    if (file_flag != 3) {
        TFT_xy(18, 9);                            // ���� ��ȣ
        TFT_color(Magenta, Black);
        TFT_unsigned_decimal(file_number + 1, 1, 3);
        TFT_xy(17,11);                            // ���� ũ��
        TFT_color(Magenta, Black);
        TFT_unsigned_decimal(file_KB, 0, 5);
    }

    TFT_string(27, 9, Yellow, Black, "000");      // ��Ʈ����Ʈ �ʱ�ȭ

    TFT_xy(24, 13);                               // ��� �ۼ�Ʈ �ʱ�ȭ
    TFT_color(Yellow, Black);
    TFT_English('0');
    TFT_color(Cyan, Black);
    TFT_English('%');
    TFT_English(')');
    TFT_English(' ');
    TFT_English(' ');
}

void TFT_volume(void) {
    TFT_xy(18, 17);
    TFT_color(Magenta, Black);
    TFT_unsigned_decimal((unsigned int)(volume * 100.0 / 250.0 + 0.5), 0, 3);
    TFT_xy(23, 17);
    TFT_color(Yellow, Black);
    TFT_unsigned_decimal(volume, 1, 3);
}

void TFT_bass(void) {
    TFT_xy(19, 19);
    TFT_color(Magenta, Black);
    TFT_unsigned_decimal(bass, 1, 2);
}

void TFT_treble(void) {
    TFT_xy(19, 21);
    TFT_color(Magenta, Black);
    TFT_signed_decimal(treble, 0, 1);
}

// ���� Ȯ���� üũ ����
void Check_valid_increment_file(void) {
    unsigned char file_OK_flag = 0;
    do {
        // WAV ���� Ȯ���� �߰� (0x00574156)
        if ((extension != 0x004D5033) &&    // MP3
            (extension != 0x00564157) &&    // WAV (ASCII 'WAV' ����)
            (extension != 0x00414143) &&    // AAC
            (extension != 0x00574D41) &&    // WMA
            (extension != 0x004D4944)) {    // MIDI
            if (file_number < total_file - 1)
                file_number++;
            else
                file_number = 0;
            TFT_filename();
        } else {
            file_OK_flag = 1;
        }
    } while (file_OK_flag == 0);
}

void Check_valid_decrement_file(void) {
    unsigned char file_OK_flag = 0;
    do {
        if ((extension != 0x004D5033) && (extension != 0x00564157) &&
            (extension != 0x00414143) && (extension != 0x00574D41) &&
            (extension != 0x004D4944)) {
            if (file_number > 0)
                file_number--;
            else
                file_number = total_file - 1;
            TFT_filename();
        } else {
            file_OK_flag = 1;
        }
    } while (file_OK_flag == 0);
}

void TFT_MP3_bitrate(U16 highbyte, U16 lowbyte) {
  unsigned short MPEG10_Layer1[16] = {
    0, 32, 64, 96, 128, 160, 192, 224,
    256, 288, 320, 352, 384, 416, 448, 0
  };
  unsigned short MPEG10_Layer2[16] = {
    0, 32, 48, 56, 64, 80, 96, 112,
    128, 160, 192, 224, 256, 320, 384, 0
  };
  unsigned short MPEG10_Layer3[16] = {
    0, 32, 40, 48, 56, 64, 80, 96,
    112, 128, 160, 192, 224, 256, 320, 0
  };
  unsigned short MPEG20_Layer1[16] = {
    0, 32, 48, 56, 64, 80, 96, 112,
    128, 144, 160, 176, 192, 224, 256, 0
  };
  unsigned short MPEG20_Layer2[16] = {
    0, 8, 16, 24, 32, 40, 48, 56,
    64, 80, 96, 112, 128, 144, 160, 0
  };

  if ((highbyte & 0x0018) == 0x0018) {              // MPEG-1.0
    switch (highbyte & 0x0006) {
      case 0x0002:                              // Layer-3
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG10_Layer3[lowbyte >> 12], 1, 3);
		        break;
      case 0x0004:                              // Layer-2
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG10_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0006:                              // Layer-1
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG10_Layer1[lowbyte >> 12], 1, 3);
		        break;
	}
  } else if ((highbyte & 0x0018) == 0x0010) {       // MPEG-2.0
    switch (highbyte & 0x0006) {
      case 0x0002:                              // Layer-3
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0004:                              // Layer-2
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0006:                              // Layer-1
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer1[lowbyte >> 12], 1, 3);
		        break;
	}
  } else {                                          // MPEG-2.5
    switch (highbyte & 0x0006) {
      case 0x0002:                              // Layer-3
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0004:                              // Layer-2
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0006:                              // Layer-1
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer1[lowbyte >> 12], 1, 3);
		        break;
	}
    }
}

unsigned char icon_flag = 0;

unsigned char Icon_input(void) {
  unsigned char keyPressed;

  Touch_screen_input();                          // Input touch screen

  if ((icon_flag == 0) && (x_touch >= 12) && (x_touch <= 67) &&
    (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY1;
      icon_flag = 1;
    Rectangle(12, 196, 67, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 92) && (x_touch <= 147) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY2;
      icon_flag = 1;
    Rectangle(92, 196, 147, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 176) && (x_touch <= 231) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY3;
      icon_flag = 1;
    Rectangle(176, 196, 231, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 256) && (x_touch <= 311) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY4;
      icon_flag = 1;
    Rectangle(256, 196, 311, 235, Magenta);

  } else if ((icon_flag == 1) && (x_touch == 0) && (y_touch == 0)) {
    keyPressed = no_key;
      icon_flag = 0;
    Rectangle(12, 196, 67, 235, Yellow);
    Rectangle(92, 196, 147, 235, Yellow);
    Rectangle(176, 196, 231, 235, Yellow);
    Rectangle(256, 196, 311, 235, Yellow);
      Delay_ms(50);
  } else {
    keyPressed = no_key;
  }

  return keyPressed;
}