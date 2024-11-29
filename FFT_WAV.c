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

/* ----- ���� �Լ� --------------------------------------------------------------- */

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
/* ----- FFT ���� �Լ� ---------------------------------------------------------- */

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

  // x�� ���̺� ���� (200Hz ~ 2000Hz)
  TFT_color(White,Black);
  TFT_English_pixel( 26,222, '0');        // 200Hz
  TFT_English_pixel( 34,222, '2');
  TFT_English_pixel( 42,222, '0');
  TFT_English_pixel( 50,222, '0');

  TFT_English_pixel( 66,222, '4');        // 400Hz
  TFT_English_pixel( 74,222, '0');
  TFT_English_pixel( 82,222, '0');

  TFT_English_pixel( 98,222, '6');        // 600Hz
  TFT_English_pixel(106,222, '0');
  TFT_English_pixel(114,222, '0');

  TFT_English_pixel(130,222, '8');        // 800Hz
  TFT_English_pixel(138,222, '0');
  TFT_English_pixel(146,222, '0');

  TFT_English_pixel(158,222, '1');        // 1000Hz
  TFT_English_pixel(166,222, '0');
  TFT_English_pixel(174,222, '0');
  TFT_English_pixel(182,222, '0');

  TFT_English_pixel(194,222, '1');        // 1200Hz
  TFT_English_pixel(202,222, '2');
  TFT_English_pixel(210,222, '0');
  TFT_English_pixel(218,222, '0');

  TFT_English_pixel(230,222, '1');        // 1400Hz
  TFT_English_pixel(238,222, '4');
  TFT_English_pixel(246,222, '0');
  TFT_English_pixel(254,222, '0');

  TFT_English_pixel(266,222, '1');        // 1600Hz
  TFT_English_pixel(274,222, '6');
  TFT_English_pixel(282,222, '0');
  TFT_English_pixel(290,222, '0');

  TFT_English_pixel(302,222, '1');        // 1800Hz
  TFT_English_pixel(310,222, '8');
  TFT_English_pixel(318,222, '0');
  TFT_English_pixel(326,222, '0');

  TFT_English_pixel(338,222, '2');        // 2000Hz
  TFT_English_pixel(346,222, '0');
  TFT_English_pixel(354,222, '0');
  TFT_English_pixel(362,222, '0');

  TFT_color(Magenta,Black);
  TFT_English_pixel(370,223, '[');        // x�� ���� ǥ�� ([Hz])
  TFT_English_pixel(378,223, 'H');
  TFT_English_pixel(386,223, 'z');
  TFT_English_pixel(394,223, ']');

  // �� �׸���
  Line(30, 220, 310, 220, White);         // x�� �� �׸���
  Line(305,215, 310, 220, White);         // x�� ȭ��ǥ �׸���
  Line(305,225, 310, 220, White);
  
  // x�� ���� ���� ���� (200Hz ������)
  for(x = 50; x <= 309; x += 28)          // ������ 28�ȼ��� ����
    Line(x,218, x,222, White);

  // y���� �����ϰ� ����
  Line(30,  28,  30, 220, White);
  Line(35,  33,  30,  28, White);
  Line(25,  33,  30,  28, White);
  for(y = 40; y <= 202; y += 18)
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