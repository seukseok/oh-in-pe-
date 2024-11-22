#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"
#include "arm_math.h"
#include <stdint.h>
#include <math.h>


// FFT 설정
#define FFT_SIZE 1024
#define SAMPLE_RATE 44100
#define BUFFER_SIZE (FFT_SIZE * 2) // 스테레오 채널 고려

// 버퍼 선언
int16_t audio_buffer[BUFFER_SIZE];
float32_t fft_input[FFT_SIZE];
float32_t fft_output[FFT_SIZE / 2];
arm_rfft_fast_instance_f32 fft_instance;

// 함수 프로토타입
void System_Init(void);
void Read_Audio_Data(void);
void Process_FFT(void);
void Display_Spectrum(void);
void Handle_User_Input(void);

int main(void) {
    System_Init();

    while (1) {
        Read_Audio_Data();
        Process_FFT();
        Display_Spectrum();
        Handle_User_Input();
    }
}

void System_Init(void) {
    // 시스템 초기화
    SystemInit();
    Delay_Init();
    TFT_init();
    SD_Init();
    VS1053b_Init();

    // FFT 초기화
    arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);

    // 디스플레이 초기 메시지
    TFT_string(0, 0, White, Black, "FFT Spectrum Analyzer");
}

void Read_Audio_Data(void) {
    // SD 카드에서 WAV 파일 열기
    if (SD_Open_File("audio.wav") != 0) {
        TFT_string(0, 16, Red, Black, "Error: Cannot open audio.wav");
        while (1);
    }

    // WAV 파일 헤더 건너뛰기
    SD_Seek(44);

    // 오디오 데이터 읽기
    if (SD_Read_Data((uint8_t*)audio_buffer, BUFFER_SIZE * sizeof(int16_t)) != 0) {
        TFT_string(0, 16, Red, Black, "Error: Cannot read audio data");
        while (1);
    }

    // 스테레오 데이터를 모노로 변환하여 FFT 입력 준비
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_input[i] = (float32_t)(audio_buffer[2 * i] + audio_buffer[2 * i + 1]) / 65536.0f;
    }
}

void Process_FFT(void) {
    // FFT 수행
    arm_rfft_fast_f32(&fft_instance, fft_input, fft_output, 0);

    // 주파수 스펙트럼 크기 계산
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        fft_output[i] = sqrtf(fft_output[2 * i] * fft_output[2 * i] + fft_output[2 * i + 1] * fft_output[2 * i + 1]);
    }
}

void Display_Spectrum(void) {
    // 디스플레이 초기화
    TFT_clear_screen();

    // 주파수 스펙트럼 그래프 표시
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        uint16_t amplitude = (uint16_t)(fft_output[i] * 100);
        uint16_t x_pos = i * (TFT_WIDTH / (FFT_SIZE / 2));
        uint16_t y_pos = TFT_HEIGHT - amplitude;

        if (y_pos > TFT_HEIGHT) y_pos = TFT_HEIGHT;

        // 막대 그래프 그리기
        TFT_draw_line(x_pos, TFT_HEIGHT, x_pos, y_pos, Green);
    }
}

void Handle_User_Input(void) {
    // 사용자 입력 처리 (예: 버튼, 터치스크린)
    // 구현 필요
}
