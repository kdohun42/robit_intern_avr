#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>


/* =========================
   UART0 초기화
   9600bps
   ========================= */
void UART0_Init(void)
{
    // 16MHz, 9600bps
    UBRR0H = 0;
    UBRR0L = 103;

    // 송신 기능 사용
    UCSR0B = (1 << TXEN0);

    // 8bit 데이터, Stop bit 1개, Parity 없음
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}


/* =========================
   UART 문자 1개 전송
   ========================= */
void UART0_TxChar(char data)
{
    while (!(UCSR0A & (1 << UDRE0)));

    UDR0 = data;
}


/* =========================
   UART 문자열 전송
   ========================= */
void UART0_TxString(char *str)
{
    while (*str)
    {
        UART0_TxChar(*str++);
    }
}


/* =========================
   ADC 초기화
   ========================= */
void ADC_Init(void)
{
    /*
       기준 전압 : AVCC = 5V
       오른쪽 정렬
    */
    ADMUX = (1 << REFS0);

    /*
       ADC Enable
       분주비 128

       16MHz / 128 = 125kHz
    */
    ADCSRA =
        (1 << ADEN)  |
        (1 << ADPS2) |
        (1 << ADPS1) |
        (1 << ADPS0);
}


/* =========================
   ADC 값 읽기
   channel : 0~7
   ========================= */
uint16_t ADC_Read(uint8_t channel)
{
    // 기존 채널 비트 제거 후 새로운 채널 선택
    ADMUX = (ADMUX & 0xE0) | (channel & 0x07);

    // ADC 변환 시작
    ADCSRA |= (1 << ADSC);

    // 변환 완료까지 대기
    while (ADCSRA & (1 << ADSC));

    return ADC;
}


/* =========================
   JTAG Disable
   PF4~PF7 ADC 사용을 위해 필요
   ========================= */
void JTAG_Disable(void)
{
    /*
       ATmega128에서는 JTD 비트를
       짧은 시간 안에 2번 연속 1로 써야 함
    */

    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);
}


/* =========================
   MAIN
   ========================= */
int main(void)
{
    uint16_t sensor[7];

    char buffer[100];

    // JTAG 해제
    JTAG_Disable();

    // ADC 초기화
    ADC_Init();

    // UART 초기화
    UART0_Init();


    UART0_TxString("\r\n");
    UART0_TxString("IR SENSOR TEST START\r\n");
    UART0_TxString("-----------------------------\r\n");


    while (1)
    {
        /*
           센서 연결

           Sensor 1 → PF1 → ADC1
           Sensor 2 → PF2 → ADC2
           Sensor 3 → PF3 → ADC3
           Sensor 4 → PF4 → ADC4
           Sensor 5 → PF5 → ADC5
           Sensor 6 → PF6 → ADC6
           Sensor 7 → PF7 → ADC7
        */

        sensor[0] = ADC_Read(1);
        sensor[1] = ADC_Read(2);
        sensor[2] = ADC_Read(3);
        sensor[3] = ADC_Read(4);
        sensor[4] = ADC_Read(5);
        sensor[5] = ADC_Read(6);
        sensor[6] = ADC_Read(7);


        sprintf(buffer,
                "IR1:%4u  IR2:%4u  IR3:%4u  IR4:%4u  "
                "IR5:%4u  IR6:%4u\r\n",
                sensor[0],
                sensor[1],
                sensor[2],
                sensor[3],
                sensor[4],
                sensor[5]);

        UART0_TxString(buffer);


        // 너무 빠르게 출력되지 않도록
        _delay_ms(200);
    }
}