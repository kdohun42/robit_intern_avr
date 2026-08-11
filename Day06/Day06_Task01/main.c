#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "i2c_lcd.h"

#define N 6
#define MAF 5

uint16_t raw[N], filter[N], min[N], max[N];
uint16_t maf_buf[N][MAF];
uint8_t norm[N], maf_index = 0;

void UART0_Init(void)
{
	UBRR0H = 0;
	UBRR0L = 103;                              // 9600bps
	UCSR0B = (1 << TXEN0);                     
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);   
}

void UART0_Tx(char *str)
{
	while (*str)
	{
		while (!(UCSR0A & (1 << UDRE0)));
		UDR0 = *str++;
	}
}

void ADC_Init(void)
{
	DDRF = 0x00;                               // PF 입력
	PORTF = 0x00;                              // Pull-up OFF
	ADMUX = (1 << REFS0);                     // 기준전압 AVCC
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);     // ADC 분주비 128
}

uint16_t ADC_Read(uint8_t ch)
{
	ADMUX = (ADMUX & 0xE0) | ch;              // ADC 채널 선택
	ADCSRA |= (1 << ADSC);                    // 변환 시작
	while (ADCSRA & (1 << ADSC));
	return ADC;
}

void JTAG_Disable(void)
{
	MCUCSR |= (1 << JTD);
	MCUCSR |= (1 << JTD);                     // PF4~PF6 ADC 사용
}

void IR_Init(void)
{
	uint8_t i, j;

	for (i = 0; i < N; i++)
	{
		raw[i] = ADC_Read(i + 1);              // PF1~PF6

		for (j = 0; j < MAF; j++)
		maf_buf[i][j] = raw[i];

		filter[i] = raw[i];
		min[i] = raw[i];
		max[i] = raw[i];
	}
}

void IR_Read(void)
{
	uint8_t i, j;
	uint32_t sum;

	for (i = 0; i < N; i++)
	{
		raw[i] = ADC_Read(i + 1);              // 원본값
		maf_buf[i][maf_index] = raw[i];

		sum = 0;

		for (j = 0; j < MAF; j++)
		sum += maf_buf[i][j];

		filter[i] = sum / MAF;                 // 이동평균 필터

		if (filter[i] < min[i]) min[i] = filter[i];
		if (filter[i] > max[i]) max[i] = filter[i];

		if (max[i] != min[i])
		norm[i] = (uint32_t)(filter[i] - min[i]) * 100 /
		(max[i] - min[i]);        // 0~100 정규화
		else
		norm[i] = 0;
	}

	maf_index++;
	if (maf_index >= MAF) maf_index = 0;
}

void LED_Control(void)
{
	uint8_t i;

	for (i = 0; i < N; i++)
	{
		if (raw[i] >= 0)
		PORTA |= (1 << i);                 // 0.8 이상 ON
		else
		PORTA &= ~(1 << i);                // 0.8 미만 OFF
	}
}

void UART_Print(void)
{
	uint8_t i;
	char buf[80];

	UART0_Tx("\r\n      original / filter / min / max / norm\r\n");

	for (i = 0; i < N; i++)
	{
		sprintf(buf, "%4u\r\n", raw[i]);
		UART0_Tx(buf);
	}
}

void LCD_Print(void)
{
	char buf[21];
	for(int i = 0; i < 6; i++){
	sprintf(buf, "%4u\r\n", raw[i]);
	}
	i2c_lcd_goto_xy(0, 0);
	i2c_lcd_string(buf);                       // 위쪽 IR1~IR3
/*
	sprintf(buf, "%u.%02u %u.%02u %u.%02u",
	norm[3] / 100, norm[3] % 100,
	norm[4] / 100, norm[4] % 100,
	norm[5] / 100, norm[5] % 100);
*/
	i2c_lcd_goto_xy(1, 0);
	i2c_lcd_string(buf);                       // 아래쪽 IR4~IR6
}

int main(void)
{
	JTAG_Disable();

	ADC_Init();
	UART0_Init();

	DDRA |= 0x3F;                              // PA0~PA5 LED 출력
	PORTA &= ~0x3F;

	i2c_lcd_init();                            // LCD 초기화
	i2c_lcd_clear();

	IR_Init();

	while (1)
	{
		IR_Read();                             // ADC + MAF + min/max + 정규화
		LED_Control();                         // norm >= 0.8 LED ON
		UART_Print();                          // 터미널 출력
		LCD_Print();                           // LCD 출력

		_delay_ms(200);
	}
}