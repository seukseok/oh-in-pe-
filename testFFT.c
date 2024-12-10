#include "stm32f767xx.h"            // STM32F767 마이크로컨트롤러 헤더 파일 포함
#include "OK-STM767.h"              // OK-STM767 보드 관련 헤더 파일 포함
#include <stdlib.h>                 // 표준 라이브러리 함수 사용을 위한 헤더 파일 포함
#include <arm_math.h>               // ARM의 수학 라이브러리 함수 사용을 위한 헤더 파일 포함
#include "fft.h"                    // FFT 함수 헤더 파일 포함
#include "complex.h"                // 복소수 연산을 위한 헤더 파일 포함

#define SAMPLE_SIZE 2048            // 샘플 데이터의 크기를 2048로 정의
#define FFT_SIZE (SAMPLE_SIZE/2)    // FFT 크기를 샘플 크기의 절반으로 정의

complex_f fft_buffer_pool[SAMPLE_SIZE];   // FFT에 사용할 복소수 버퍼 풀 선언

// 이동 평균 필터의 크기
#define MOVING_AVG_SIZE 10  // 이동 평균 필터의 크기 증가


// 필터 변수
float moving_avg_buffer[MOVING_AVG_SIZE];
unsigned int moving_avg_index = 0;
float moving_avg_sum = 0.0f;

void TIM7_IRQHandler(void);               // TIM7 인터럽트 핸들러 선언 (25.6kHz에서 ADC 입력)
void Display_FFT_screen(void);            // FFT 화면을 표시하는 함수 선언
void Clear_FFT_Display(void);             // FFT 화면을 지우는 함수 선언
void do_fft(void);                        // FFT 연산을 수행하는 함수 선언
void Draw_FFT(U16 index, float value);    // FFT 결과를 바 형태로 그리는 함수 선언
void display_info();                      // 화면에 정보를 표시하는 함수 선언

unsigned char FFT_mode, FFT_flag;         // FFT 모드와 플래그를 위한 변수 선언
unsigned short FFT_count;                 // FFT 샘플 카운트를 위한 변수 선언

float ADC_buffer[SAMPLE_SIZE];            // ADC로부터 수집한 샘플 데이터를 저장하는 버퍼
float FFT_buffer[SAMPLE_SIZE];            // FFT 입력 데이터를 저장하는 버퍼
float FFT_input[SAMPLE_SIZE];             // FFT 입력 버퍼 (사용되지 않음)
float FFT_output[SAMPLE_SIZE/2];          // FFT 결과를 저장하는 버퍼
float max_value;                          // FFT 결과 중 최대값을 저장하는 변수
unsigned int max_index;                   // 최대값의 인덱스를 저장하는 변수

// RMS 임계값 설정
#define RMS_THRESHOLD 0.01f  // RMS 값이 0.01 이하일 때는 스펙트럼을 표시하지 않음

// 이동 평균 필터 적용 함수
float apply_moving_avg(float new_sample) {
    moving_avg_sum += new_sample - moving_avg_buffer[moving_avg_index];
    moving_avg_buffer[moving_avg_index] = new_sample;
    moving_avg_index = (moving_avg_index + 1) % MOVING_AVG_SIZE;
    return moving_avg_sum / MOVING_AVG_SIZE;
}

/* ----- 인터럽트 서비스 루틴 ---------------------------------------------------- */

void TIM7_IRQHandler(void)                /* TIM7 인터럽트(25.6kHz)에서 ADC 입력 처리 */
{
  TIM7->SR = 0x0000;                      // TIM7 인터럽트의 팬딩 비트 클리어

  ADC1->CR2 |= 0x40000000;                // 소프트웨어로 ADC 변환 시작
  while(!(ADC1->SR & 0x00000002));        // 변환 완료 대기

  // ADC 값 읽고 이동 평균 필터 적용
  float adc_value = ((float)ADC1->DR - 2048.0f) / 2048.0f; // 정규화
  float filtered_value = apply_moving_avg(adc_value);  // 필터링된 값

  // 필터링된 값을 FFT 버퍼에 저장
  FFT_buffer[FFT_count] = filtered_value;
  FFT_count++;
  
  if(FFT_count >= SAMPLE_SIZE)
  { 
    FFT_count = 0;                        // 샘플 카운트 초기화
    FFT_flag = 1;                         // FFT 연산을 수행하도록 플래그 설정
  }
}

/* ----- 메인 함수 --------------------------------------------------------------- */

int main(void)
{
  Initialize_MCU();                       // MCU 초기화
  Delay_ms(50);                           // 50ms 지연
  Initialize_LCD();                       // LCD 초기화
  Initialize_TFT_LCD();                   // TFT LCD 초기화

  GPIOA->MODER |= 0x00003000;             // PA6를 아날로그 모드(ADC12_IN6)로 설정
  RCC->APB2ENR |= 0x00000100;             // ADC1 클럭 활성화
  ADC->CCR = 0x00000000;                  // ADCCLK = 54MHz/2 = 27MHz 설정
  ADC1->SMPR2 = 0x00040000;               // 채널 6의 샘플링 시간 = 15 사이클 설정
  ADC1->CR1 = 0x00000000;                 // 12비트 해상도 설정
  ADC1->CR2 = 0x00000001;                 // 오른쪽 정렬, 단일 변환, ADON = 1 설정
  ADC1->SQR1 = 0x00000000;                // 총 정규 채널 수 = 1 설정
  ADC1->SQR3 = 0x00000006;                // 채널 6 선택

  RCC->APB1ENR |= 0x00000020;             // TIM7 클럭 활성화
  TIM7->PSC = 1;                          // 프리스케일러 값: 108MHz / (1 + 1) = 54MHz
  TIM7->ARR = 224;                        // ARR 값: 54MHz / (224 + 1) = 48kHz
  TIM7->CNT = 0;                          // 카운터 초기화
  TIM7->DIER = 0x0001;                    // 업데이트 인터럽트 활성화
  TIM7->CR1 = 0x0085;                     // TIM7 활성화, 업데이트 이벤트, 프리로드 설정
  NVIC->ISER[1] |= 0x00800000;            // (55)TIM7 인터럽트 활성화

  // 이동 평균 버퍼 초기화
  for(int i = 0; i < MOVING_AVG_SIZE; i++)
  {
    moving_avg_buffer[i] = 0.0f;
  }
  moving_avg_sum = 0.0f;
  moving_avg_index = 0;

    Display_FFT_screen();                   // FFT 화면 표시
  TFT_string(5,0,White,Magenta," 실시간 FFT 변환 및 표시 "); // 화면에 문자열 표시

  while(1)
  {   
    if(FFT_flag == 1)
    {
      FFT_flag = 0;
      do_fft();                           // FFT 연산 수행
      display_info();                     // 정보 표시
    }
     Delay_us(1);                         // 1μs 지연
  }
}
/* ----- FFT 연산 함수 ---------------------------------------------------------- */

void do_fft(void)
{
  // RMS 계산 (주파수 영역에서 RMS 계산)
  float rms_value = 0.0;
  for (int i = 0; i < SAMPLE_SIZE; i++)
  {
      rms_value += FFT_buffer[i] * FFT_buffer[i];
  }
  rms_value = sqrtf(rms_value / SAMPLE_SIZE);

  // RMS 값이 임계값 이하일 때는 스펙트럼을 지우고 반환
  if (rms_value < RMS_THRESHOLD)
  {
      Clear_FFT_Display();  // 스펙트럼 지우기 함수 호출
      return;  // RMS 값이 낮으면 FFT 수행하지 않음
  }

  // FFT 수행을 위한 준비
  complex_f *fft_data = fft_buffer_pool;  // FFT에 사용할 버퍼 포인터 설정

  unsigned r = log2(SAMPLE_SIZE);         // 샘플 크기의 로그2 값 계산
  unsigned N = 1 << r;                    // FFT 크기 N = 2^r 계산
  float max_data = 0.0f;                   // 최대값 변수 초기화

  for(int i = 0; i < SAMPLE_SIZE; i++)
  {
    fft_data[i].re = FFT_buffer[i];       // 실수부에 FFT 입력 데이터 저장
    fft_data[i].im = 0.0;                 // 허수부를 0으로 초기화
  }

  ffti_f(fft_data, r, FFT_FORWARD);       // FFT 수행

  // FFT 결과 처리
  for(int i = 1; i < 128; i++)
  {
      FFT_output[i-1] = sqrtf(fft_data[i].re * fft_data[i].re + fft_data[i].im * fft_data[i].im);
      if(max_data < FFT_output[i-1])
          max_data = FFT_output[i-1];
  }
  
  for(int i = 0; i < 128; i++)
  {
    Draw_FFT(i, FFT_output[i] * 180 / max_data); // FFT 결과를 바 형태로 표시
  }
}

/* ----- FFT 화면 표시 함수 ----------------------------------------------------- */

void Display_FFT_screen(void)             /* FFT 화면 표시 함수 */
{
  unsigned short x, y;

  TFT_clear_screen();                     // 화면 초기화

  TFT_color(White, Black);

  // y축 레이블 (0% ~ 100%)
  TFT_English_pixel(18, 213, '0');        // 0%
  TFT_English_pixel(10, 195, '1');        // 10%
  TFT_English_pixel(18, 195, '0');
  TFT_English_pixel(10, 177, '2');        // 20%
  TFT_English_pixel(18, 177, '0');
  TFT_English_pixel(10, 159, '3');        // 30%
  TFT_English_pixel(18, 159, '0');
  TFT_English_pixel(10, 141, '4');        // 40%
  TFT_English_pixel(18, 141, '0');
  TFT_English_pixel(10, 123, '5');        // 50%
  TFT_English_pixel(18, 123, '0');
  TFT_English_pixel(10, 105, '6');        // 60%
  TFT_English_pixel(18, 105, '0');
  TFT_English_pixel(10, 87, '7');         // 70%
  TFT_English_pixel(18, 87, '0');
  TFT_English_pixel(10, 69, '8');         // 80%
  TFT_English_pixel(18, 69, '0');
  TFT_English_pixel(10, 51, '9');         // 90%
  TFT_English_pixel(18, 51, '0');
  TFT_English_pixel(2, 33, '1');          // 100%
  TFT_English_pixel(10, 33, '0');
  TFT_English_pixel(18, 33, '0');
  
  TFT_color(Magenta, Black);
  TFT_English_pixel(2, 16, '[');          // y축 단위 표시 ([%])
  TFT_English_pixel(10, 16, '%');
  TFT_English_pixel(18, 16, ']');

  TFT_color(Pink,Black);
  // x축 레이블 (0kHz ~ 24kHz) 간격 조정
  for (x = 0; x <= 24; x++) {
    if (x % 2 == 0) { // 2kHz 간격으로 레이블 표시
      int pos_x = 30 + (x * 13);  // 1kHz 간격으로 320px에 분배
      TFT_English_pixel(pos_x, 222, '0' + (x / 10));  // 10의 자릿수
      TFT_English_pixel(pos_x + 6, 222, '0' + (x % 10));  // 1의 자릿수
    }
  }
  
  TFT_color(Magenta, Black);
  TFT_English_pixel(280, 223, '[');        // x축 단위 표시 ([kHz])
  TFT_English_pixel(288, 223, 'k');
  TFT_English_pixel(296, 223, 'H');
  TFT_English_pixel(304, 223, 'z');
  TFT_English_pixel(312, 223, ']');

  // x축 선 그리기
  Line(30, 220, 310, 220, White);         // x축 선
  Line(305, 215, 310, 220, White);        // x축 화살표
  Line(305, 225, 310, 220, White);
  
  // x축 눈금 그리기
  for (x = 50; x <= 309; x += 20) {
    Line(x, 218, x, 222, White);
  }

  // y축 선 그리기
  Line(30, 28, 30, 220, White);          // y축 선
  Line(35, 33, 30, 28, White);           // y축 화살표
  Line(25, 33, 30, 28, White);

  // y축 눈금 그리기
  for (y = 40; y <= 202; y += 18) {
    Line(28, y, 32, y, White);
  }
}

void Clear_FFT_Display(void)
{
    for(int i = 0; i < 128; i++)
    {
        Line(30 + 2 * i, 219, 30 + 2 * i, 219 - 180, Black);  // 이전 바 삭제
    }
}


/* ----- FFT 결과 바 그래프 그리기 함수 ------------------------------------------ */

void Draw_FFT(U16 index, float value)     /* FFT 결과를 바 형태로 그리는 함수 */
{
  unsigned short height;

  height = (unsigned short)value;         // 바의 높이 계산
  if(height >= 180.) height = 180.;       // 최대 높이 제한

  Line(30+2*index, 219, 30+2*index, 219 - 180, Black);    // 이전 바 삭제
  Line(30+2*index, 219, 30+2*index, 219 - height, Red);   // 새로운 바 그리기
}

void display_info() {
    static uint8_t blink = 0;  // 깜박임 상태를 저장할 변수
    
    if(blink) {
        // 문구를 표시
        TFT_string(7, 3, Cyan, Black, "오디오를 분석하고 있습니다..."); 
    } else {
        // 문구를 지움 (검은색 배경으로 덮어씀)
        TFT_string(7, 3, Black, Black, "                             ");
    }
    
    Rectangle(0, 0, 319, 239, Blue);
    
    // 상태 토글 (0->1, 1->0)
    blink ^= 1;
}