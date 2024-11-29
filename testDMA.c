#include "stm32f767xx.h"            // STM32F767 ����ũ����Ʈ�ѷ� ��� ���� ����
#include "OK-STM767.h"              // OK-STM767 ���� ���� ��� ���� ����
#include <stdlib.h>                 // ǥ�� ���̺귯�� �Լ� ����� ���� ��� ���� ����
#include <arm_math.h>               // ARM�� ���� ���̺귯�� �Լ� ����� ���� ��� ���� ����
#include "fft.h"                    // FFT �Լ� ��� ���� ����
#include "complex.h"                // ���Ҽ� ������ ���� ��� ���� ����

#define SAMPLE_SIZE 2048            // ���� �������� ũ�⸦ 2048�� ����
#define FFT_SIZE (SAMPLE_SIZE/2)    // FFT ũ�⸦ ���� ũ���� �������� ����

complex_f fft_buffer_pool[SAMPLE_SIZE];   // FFT�� ����� ���Ҽ� ���� Ǯ ����

void Display_FFT_screen(void);            // FFT ȭ���� ǥ���ϴ� �Լ� ����
void do_fft(void);                        // FFT ������ �����ϴ� �Լ� ����
void Draw_FFT(U16 index, float value);    // FFT ����� �� ���·� �׸��� �Լ� ����

unsigned char FFT_mode, FFT_flag;         // FFT ���� �÷��׸� ���� ���� ����
unsigned short FFT_count;                 // FFT ���� ī��Ʈ�� ���� ���� ����

uint16_t ADC_buffer[SAMPLE_SIZE];            // ADC�κ��� ������ ���� �����͸� �����ϴ� ����
float FFT_buffer[SAMPLE_SIZE];            // FFT �Է� �����͸� �����ϴ� ����
float FFT_output[SAMPLE_SIZE/2];          // FFT ����� �����ϴ� ����
float max_value;                          // FFT ��� �� �ִ밪�� �����ϴ� ����
unsigned int max_index;                   // �ִ밪�� �ε����� �����ϴ� ����

/* ----- DMA �Ϸ� ���ͷ�Ʈ �ڵ鷯 -------------------------------------------------- */

void DMA2_Stream0_IRQHandler(void) {
    if(DMA2->LISR & DMA_LISR_TCIF0) {
        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;
        
        // ADC �����͸� float�� ��ȯ�ϰ� ����ȭ
        for(int i = 0; i < SAMPLE_SIZE; i++) {
            FFT_buffer[i] = ((float)(ADC_buffer[i] & 0x0FFF) - 2048.0f)/2048.0f;
        }
        FFT_flag = 1;
    }
}

/* ----- ���� �Լ� --------------------------------------------------------------- */

int main(void) {
    Initialize_MCU();
    Delay_ms(50);
    Initialize_LCD();
    Initialize_TFT_LCD();

    // GPIO ����
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  // GPIOA Ŭ�� Ȱ��ȭ
    GPIOA->MODER |= 0x00003000;            // PA6�� �Ƴ��α� ���� ����

    // ADC ����
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;    // ADC1 Ŭ�� Ȱ��ȭ
    ADC->CCR = 0x00000000;                  // ADCCLK = 54MHz/2 = 27MHz
    
    // ADC ���ø� �ð� ���� - �� ���� ���ø��� ���� ����
    ADC1->SMPR2 = 0x00000000;              // ä�� 6 ���ø� �ð� = 3 ����Ŭ
    
    // ADC ����
    ADC1->CR1 = 0x00000000;                // 12��Ʈ �ػ�
    ADC1->CR2 = ADC_CR2_ADON |             // ADC Ȱ��ȭ
                ADC_CR2_CONT |              // ���� ��ȯ ���
                ADC_CR2_DMA |               // DMA ���
                ADC_CR2_DDS;                // DMA ��û Ȱ��ȭ
    
    ADC1->SQR1 = 0x00000000;               // ��ȯ ä�� �� = 1
    ADC1->SQR3 = 0x00000006;               // ä�� 6 ����

    // DMA ����
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;    // DMA2 Ŭ�� Ȱ��ȭ
    
    // DMA ��Ʈ�� ��Ȱ��ȭ
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while(DMA2_Stream0->CR & DMA_SxCR_EN);
    
    // DMA ���� �ʱ�ȭ
    DMA2->LIFCR = 0x0F400000;              // �÷��� Ŭ����
    
    // DMA ��Ʈ�� ����
    DMA2_Stream0->PAR = (uint32_t)&ADC1->DR;
    DMA2_Stream0->M0AR = (uint32_t)ADC_buffer;
    DMA2_Stream0->NDTR = SAMPLE_SIZE;
    
    DMA2_Stream0->CR = 0;
    DMA2_Stream0->CR |= (0 << 25);          // ä�� 0
    DMA2_Stream0->CR |= DMA_SxCR_PL_1;      // �켱���� ����
    DMA2_Stream0->CR |= DMA_SxCR_MSIZE_0;   // �޸� 16��Ʈ
    DMA2_Stream0->CR |= DMA_SxCR_PSIZE_0;   // �ֺ���ġ 16��Ʈ
    DMA2_Stream0->CR |= DMA_SxCR_MINC;      // �޸� ����
    DMA2_Stream0->CR |= DMA_SxCR_CIRC;      // ��ȯ ���
    DMA2_Stream0->CR |= DMA_SxCR_TCIE;      // ���� �Ϸ� ���ͷ�Ʈ

    // DMA ���ͷ�Ʈ ����
    NVIC_SetPriority(DMA2_Stream0_IRQn, 0);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    // DMA Ȱ��ȭ
    DMA2_Stream0->CR |= DMA_SxCR_EN;

    Display_FFT_screen();
    TFT_string(5,0,White,Magenta," �ǽð� FFT ��ȯ �� ǥ�� ");

    // ADC ��ȯ ����
    ADC1->CR2 |= ADC_CR2_SWSTART;

    while(1) {
        if(FFT_flag) {
            FFT_flag = 0;
            do_fft();
        }
        Delay_us(10);  // ������ �� ����
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