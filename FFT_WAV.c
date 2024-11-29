#include "stm32f767xx.h"            // STM32F767 ����ũ����Ʈ�ѷ� ��� ���� ����
#include "OK-STM767.h"              // OK-STM767 ���� ���� ��� ���� ����
#include <stdlib.h>                 // ǥ�� ���̺귯�� �Լ� ����� ���� ��� ���� ����
#include <arm_math.h>               // ARM�� ���� ���̺귯�� �Լ� ����� ���� ��� ���� ����
#include "fft.h"                    // FFT �Լ� ��� ���� ����
#include "complex.h"                // ���Ҽ� ������ ���� ��� ���� ����

#define SAMPLE_SIZE 2048            // ���� �������� ũ�⸦ 2048�� ����
#define FFT_SIZE (SAMPLE_SIZE/2)    // FFT ũ�⸦ ���� ũ���� �������� ����

complex_f fft_buffer_pool[SAMPLE_SIZE];   // FFT�� ����� ���Ҽ� ���� Ǯ ����

void TIM7_IRQHandler(void);               // TIM7 ���ͷ�Ʈ �ڵ鷯 ���� (25.6kHz���� ADC �Է�)
void Display_FFT_screen(void);            // FFT ȭ���� ǥ���ϴ� �Լ� ����
void do_fft(void);                        // FFT ������ �����ϴ� �Լ� ����
void Draw_FFT(U16 index, float value);    // FFT ����� �� ���·� �׸��� �Լ� ����

unsigned char FFT_mode, FFT_flag;         // FFT ���� �÷��׸� ���� ���� ����
unsigned short FFT_count;                 // FFT ���� ī��Ʈ�� ���� ���� ����

float ADC_buffer[SAMPLE_SIZE];            // ADC�κ��� ������ ���� �����͸� �����ϴ� ����
float FFT_buffer[SAMPLE_SIZE];            // FFT �Է� �����͸� �����ϴ� ����
float FFT_input[SAMPLE_SIZE];             // FFT �Է� ���� (������ ����)
float FFT_output[SAMPLE_SIZE/2];          // FFT ����� �����ϴ� ����
float max_value;                          // FFT ��� �� �ִ밪�� �����ϴ� ����
unsigned int max_index;                   // �ִ밪�� �ε����� �����ϴ� ����

/* ----- ���ͷ�Ʈ ���� ��ƾ ---------------------------------------------------- */

void TIM7_IRQHandler(void)                /* TIM7 ���ͷ�Ʈ(25.6kHz)���� ADC �Է� ó�� */
{
  TIM7->SR = 0x0000;                      // TIM7 ���ͷ�Ʈ�� �ҵ� ��Ʈ Ŭ����

  ADC1->CR2 |= 0x40000000;                // ����Ʈ����� ADC ��ȯ ����
  while(!(ADC1->SR & 0x00000002));        // ��ȯ �Ϸ� ���
  FFT_buffer[FFT_count] = ((float)ADC1->DR - 2048.)/2048.; // ADC ����� FFT ���ۿ� ���� �� ����ȭ
  FFT_count++;
  
  if(FFT_count >= SAMPLE_SIZE)
  { 
    FFT_count = 0;                        // ���� ī��Ʈ �ʱ�ȭ
    FFT_flag = 1;                         // FFT ������ �����ϵ��� �÷��� ����
  }
}

/* ----- ???? ??????? -------------------------------------------------------- */

int main(void)
{
  Initialize_MCU();                       // MCU �ʱ�ȭ
  Delay_ms(50);                           // 50ms ����
  Initialize_LCD();                       // LCD �ʱ�ȭ
  Initialize_TFT_LCD();                   // TFT LCD �ʱ�ȭ

  GPIOA->MODER |= 0x00003000;             // PA6�� �Ƴ��α� ���(ADC12_IN6)�� ����
  RCC->APB2ENR |= 0x00000100;             // ADC1 Ŭ�� Ȱ��ȭ
  ADC->CCR = 0x00000000;                  // ADCCLK = 54MHz/2 = 27MHz ����
  ADC1->SMPR2 = 0x00040000;               // ä�� 6�� ���ø� �ð� = 15 ����Ŭ ����
  ADC1->CR1 = 0x00000000;                 // 12��Ʈ �ػ� ����
  ADC1->CR2 = 0x00000001;                 // ������ ����, ���� ��ȯ, ADON = 1 ����
  ADC1->SQR1 = 0x00000000;                // �� ���� ä�� �� = 1 ����
  ADC1->SQR3 = 0x00000006;                // ä�� 6 ����

  RCC->APB1ENR |= 0x00000020;             // TIM7 Ŭ�� Ȱ��ȭ
  TIM7->PSC = 9;                          // ���������Ϸ� ����: 108MHz/(9+1) = 10.8MHz
  TIM7->ARR = 421;                        // �ڵ� ������ �� ����: 10.8MHz/(421+1) = 25.6kHz
  TIM7->CNT = 0;                          // ī���� �ʱ�ȭ
  TIM7->DIER = 0x0001;                    // ������Ʈ ���ͷ�Ʈ Ȱ��ȭ
  TIM7->CR1 = 0x0085;                     // TIM7 Ȱ��ȭ, ������Ʈ �̺�Ʈ, �����ε� ����
  NVIC->ISER[1] |= 0x00800000;            // (55)TIM7 ���ͷ�Ʈ Ȱ��ȭ

  Display_FFT_screen();                   // FFT ȭ�� ǥ��
  TFT_string(5,0,White,Magenta," �ǽð� FFT ��ȯ �� ǥ�� "); // ȭ�鿡 ���ڿ� ǥ��

  while(1)
  {   
    if(FFT_flag == 1)
    {
      FFT_flag = 0;
      do_fft();                           // FFT ���� ����
    }
     Delay_us(1);                         // 1��s ����
  }
}
/* ----- ????? ???? ??? ----------------------------------------------------- */

void do_fft(void)
{
  complex_f *fft_data = fft_buffer_pool;  // FFT�� ����� ���� ������ ����

  unsigned r = log2(SAMPLE_SIZE);         // ���� ũ���� �α�2 �� ���
  unsigned N = 1 << r;                    // FFT ũ�� N = 2^r ���
  float max_data = 0.0;                   // �ִ밪 ���� �ʱ�ȭ

  for(int i = 0; i < SAMPLE_SIZE; i++)
  {
    fft_data[i].re = FFT_buffer[i];       // �Ǽ��ο� FFT �Է� ������ ����
    fft_data[i].im = 0.0;                 // ����θ� 0���� �ʱ�ȭ
  }

  ffti_f(fft_data, r, FFT_FORWARD);       // FFT ����
  for(int i = 1; i < 128; i++)            // Ư�� ������ ����Ʈ�� ���
  {
    FFT_output[i-1] = sqrt(pow(fft_data[i].re,2)+pow(fft_data[i].im,2)); // ����Ʈ���� ũ�� ���
    if(max_data < FFT_output[i-1])
      max_data = FFT_output[i-1];         // �ִ밪 ������Ʈ
  }
  
  for(int i = 0; i < 128; i++)
  {
    Draw_FFT(i, FFT_output[i] * 180 / max_data); // FFT ����� �� ���·� ǥ��
  }
}

/* ----- FFT ȭ�� ǥ�� �Լ� ----------------------------------------------------- */

void Display_FFT_screen(void)             /* FFT ȭ�� ǥ�� �Լ� */
{
  unsigned short x, y;

  TFT_clear_screen();                     // ȭ�� �ʱ�ȭ

  TFT_color(White,Black);
  TFT_English_pixel(18,213, '0');         // y�� ���̺� ǥ�� (0%)
  TFT_English_pixel(10,195, '1');         // y�� ���̺� ǥ�� (10%)
  TFT_English_pixel(18,195, '0');
  TFT_English_pixel(10,177, '2');         // y�� ���̺� ǥ�� (20%)
  TFT_English_pixel(18,177, '0');
  TFT_English_pixel(10,159, '3');         // y�� ���̺� ǥ�� (30%)
  TFT_English_pixel(18,159, '0');
  TFT_English_pixel(10,141, '4');         // y�� ���̺� ǥ�� (40%)
  TFT_English_pixel(18,141, '0');
  TFT_English_pixel(10,123, '5');         // y�� ���̺� ǥ�� (50%)
  TFT_English_pixel(18,123, '0');
  TFT_English_pixel(10,105, '6');         // y�� ���̺� ǥ�� (60%)
  TFT_English_pixel(18,105, '0');
  TFT_English_pixel(10, 87, '7');         // y�� ���̺� ǥ�� (70%)
  TFT_English_pixel(18, 87, '0');
  TFT_English_pixel(10, 69, '8');         // y�� ���̺� ǥ�� (80%)
  TFT_English_pixel(18, 69, '0');
  TFT_English_pixel(10, 51, '9');         // y�� ���̺� ǥ�� (90%)
  TFT_English_pixel(18, 51, '0');
  TFT_English_pixel(2,  33, '1');         // y�� ���̺� ǥ�� (100%)
  TFT_English_pixel(10, 33, '0');
  TFT_English_pixel(18, 33, '0');
  TFT_color(Magenta,Black);
  TFT_English_pixel(2,  16, '[');         // y�� ���� ǥ�� ([%])
  TFT_English_pixel(10, 16, '%');
  TFT_English_pixel(18, 16, ']');

  TFT_color(White,Black);
  TFT_English_pixel( 26,222, '0');        // x�� ���̺� ǥ�� (0kHz)
  TFT_English_pixel( 46,222, '2');        // x�� ���̺� ǥ�� (2kHz)
  TFT_English_pixel( 66,222, '4');        // x�� ���̺� ǥ�� (4kHz)
  TFT_English_pixel( 86,222, '6');        // x�� ���̺� ǥ�� (6kHz)
  TFT_English_pixel(106,222, '8');        // x�� ���̺� ǥ�� (8kHz)
  TFT_English_pixel(118,222, '1');        // x�� ���̺� ǥ�� (10kHz)
  TFT_English_pixel(126,222, '0');
  TFT_English_pixel(138,222, '1');        // x�� ���̺� ǥ�� (12kHz)
  TFT_English_pixel(146,222, '2');
  TFT_English_pixel(158,222, '1');        // x�� ���̺� ǥ�� (14kHz)
  TFT_English_pixel(166,222, '4');
  TFT_English_pixel(178,222, '1');        // x�� ���̺� ǥ�� (16kHz)
  TFT_English_pixel(186,222, '6');
  TFT_English_pixel(199,222, '1');        // x�� ���̺� ǥ�� (18kHz)
  TFT_English_pixel(207,222, '8');
  TFT_English_pixel(220,222, '2');        // x�� ���̺� ǥ�� (20kHz)
  TFT_English_pixel(228,222, '0');
  TFT_English_pixel(241,222, '2');        // x�� ���̺� ǥ�� (22kHz)
  TFT_English_pixel(249,222, '2');
  TFT_English_pixel(262,222, '2');        // x�� ���̺� ǥ�� (24kHz)
  TFT_English_pixel(270,222, '4');

  TFT_color(Magenta,Black);
  TFT_English_pixel(280,223, '[');        // x�� ���� ǥ�� ([kHz])
  TFT_English_pixel(288,223, 'k');
  TFT_English_pixel(296,223, 'H');
  TFT_English_pixel(304,223, 'z');
  TFT_English_pixel(312,223, ']');

  Line(30, 220, 310, 220, White);         // x�� �� �׸���
  Line(305,215, 310, 220, White);         // x�� ȭ��ǥ �׸���
  Line(305,225, 310, 220, White);
  for(x = 50; x <= 309; x += 20)          // x�� ���� �׸���
    Line(x,218, x,222, White);

  Line(30,  28,  30, 220, White);         // y�� �� �׸���
  Line(35,  33,  30,  28, White);         // y�� ȭ��ǥ �׸���
  Line(25,  33,  30,  28, White);
  for(y = 40; y <= 202; y += 18)          // y�� ���� �׸���
    Line(28,y, 32,y, White);
}

/* ----- FFT ��� �� �׷��� �׸��� �Լ� ------------------------------------------ */

void Draw_FFT(U16 index, float value)     /* FFT ����� �� ���·� �׸��� �Լ� */
{
  unsigned short height;

  height = (unsigned short)value;         // ���� ���� ���
  if(height >= 180.) height = 180.;       // �ִ� ���� ����

  Line(30+2*index, 219, 30+2*index, 219 - 180, Black);    // ���� �� ����
  Line(30+2*index, 219, 30+2*index, 219 - height, Red);   // ���ο� �� �׸���
}