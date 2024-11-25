#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "arm_math.h" 
#include "arm_const_structs.h"

// FFT 관련 상수
#define FFT_SIZE        256  // FFT 포인트 수
#define SAMPLE_FREQ     44100
#define BUFFER_SIZE     512
#define NUM_BANDS       16   // 주파수 대역 수

// 그래픽 상수
#define BAR_WIDTH       15
#define BAR_GAP        4
#define START_Y        220
#define MAX_HEIGHT     150

// 전역 변수
float32_t inputBuffer[FFT_SIZE*2];  // 실수+허수
float32_t outputBuffer[FFT_SIZE];   // FFT 결과
float32_t bandEnergy[NUM_BANDS];    // 주파수 대역별 에너지
static uint16_t prevHeight[NUM_BANDS] = {0};

// 주파수 대역 경계 정의 (Hz)
const uint16_t bandLimits[NUM_BANDS+1] = {
    20, 40, 80, 160, 320, 640, 1280, 2560, 
    5120, 10240, 12000, 14000, 16000, 17000, 18000, 19000, 20000
};

// FFT 초기화
void InitFFT(void) {
    // ADC 초기화
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC1->CR2 = 0;  // ADC 비활성화
    ADC1->CR1 = 0;  // 12비트 해상도
    ADC1->SMPR2 = 0x00000007;  // 샘플링 시간 설정
    ADC1->SQR1 = 0;
    ADC1->SQR3 = 0;  // 채널 0 선택
    ADC1->CR2 |= ADC_CR2_ADON;  // ADC 활성화

    // 타이머 초기화 (샘플링 주기)
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->PSC = (216000000/1000000) - 1;  // 216MHz를 1MHz로 분주
    TIM2->ARR = (1000000/SAMPLE_FREQ) - 1;      // 샘플링 주기
    TIM2->CR1 = TIM_CR1_CEN;
}

// ADC 샘플 수집
void CollectSamples(void) {
    for(int i = 0; i < FFT_SIZE; i++) {
        while(!(ADC1->SR & ADC_SR_EOC));  // 변환 완료 대기
        inputBuffer[i*2] = (float32_t)ADC1->DR;     // 실수부
        inputBuffer[i*2+1] = 0.0f;                   // 허수부
        while(!(TIM2->SR & TIM_SR_UIF));            // 타이머 주기 대기
        TIM2->SR = 0;                               // 플래그 클리어
    }
}

// FFT 계산 및 주파수 대역 분석
void ProcessFFT(void) {
    arm_cfft_f32(&arm_cfft_sR_f32_len256, inputBuffer, 0, 1);
    arm_cmplx_mag_f32(inputBuffer, outputBuffer, FFT_SIZE);

    // 주파수 대역별 에너지 계산
    for(int band = 0; band < NUM_BANDS; band++) {
        float32_t sum = 0.0f;
        uint16_t startBin = (bandLimits[band] * FFT_SIZE) / SAMPLE_FREQ;
        uint16_t endBin = (bandLimits[band+1] * FFT_SIZE) / SAMPLE_FREQ;
        
        for(uint16_t bin = startBin; bin < endBin; bin++) {
            sum += outputBuffer[bin];
        }
        bandEnergy[band] = sum / (endBin - startBin);
    }
}

// 스펙트럼 표시
void DrawSpectrum(void) {
    for(int i = 0; i < NUM_BANDS; i++) {
        uint16_t x = 20 + i * (BAR_WIDTH + BAR_GAP);
        uint16_t height = (uint16_t)(bandEnergy[i] * MAX_HEIGHT / 100.0f);
        
        if(height > MAX_HEIGHT) height = MAX_HEIGHT;
        
        // 이전 막대 지우기
        Rectangle(x, START_Y - prevHeight[i],
                 x + BAR_WIDTH, START_Y, Black);
                 
        // 새 막대 그리기
        uint16_t color = (i < 5) ? Blue :
                        (i < 11) ? Magenta : Cyan;
        Rectangle(x, START_Y - height,
                 x + BAR_WIDTH, START_Y, color);
                 
        prevHeight[i] = height;
    }
}

int main(void) {
    Initialize_MCU();
    Initialize_TFT_LCD();
    InitFFT();
    
    TFT_clear_screen();
    TFT_string(0,0,White,Magenta,"Real-time FFT Spectrum Analyzer");
    
    while(1) {
        CollectSamples();    // 샘플 수집
        ProcessFFT();        // FFT 처리
        DrawSpectrum();      // 결과 표시
        Delay_ms(20);        // 업데이트 주기
    }
}