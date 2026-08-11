#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>

#include "i2c_lcd.h"


#define SENSOR_NUM      6
#define MAF_SIZE        5

// 정규화 값 0.80 이상이면 LED ON
// 정규화 값은 코드 내부에서 0~100으로 저장
#define THRESHOLD       80



// IR 센서 원본 ADC 값
uint16_t ir_raw[SENSOR_NUM];

// 이동평균 필터 적용 값
uint16_t ir_filter[SENSOR_NUM];

// 각 센서의 최소값
uint16_t ir_min[SENSOR_NUM];

// 각 센서의 최대값
uint16_t ir_max[SENSOR_NUM];

uint8_t ir_norm[SENSOR_NUM];


// 이동평균필터용 버퍼
uint16_t maf_buffer[SENSOR_NUM][MAF_SIZE];

uint8_t maf_index = 0;


void UART0_Init(void)
{
    // 16MHz, 9600bps
    UBRR0H = 0;
    UBRR0L = 103;

    // 송신 기능 사용
    UCSR0B = (1 << TXEN0);

    // 8bit 데이터
    // Stop bit 1
    // Parity 없음
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}


void UART0_TxChar(char data)
{
    while (!(UCSR0A & (1 << UDRE0)));

    UDR0 = data;
}


void UART0_TxString(const char *str)
{
    while (*str)
    {
        UART0_TxChar(*str++);
    }
}

void ADC_Init(void)
{


    // PORTF 입력
    DDRF = 0x00;

    // 내부 Pull-up OFF
    PORTF = 0x00;
    ADMUX = (1 << REFS0);
    ADCSRA =
          (1 << ADEN)
        | (1 << ADPS2)
        | (1 << ADPS1)
        | (1 << ADPS0);
}

uint16_t ADC_Read(uint8_t channel)
{

    ADMUX = (ADMUX & 0xE0) | (channel & 0x07);

    _delay_us(10);


    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC));
    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC));


    return ADC;
}
void JTAG_Disable(void)
{
    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);
}

void LED_Init(void)
{
    // PA0 ~ PA5 출력 설정
    DDRA |= 0x3F;

    // 모든 LED OFF
    PORTA &= ~0x3F;
}
void IR_Read(void)
{
    uint8_t i;

    for (i = 0; i < SENSOR_NUM; i++)
    {
        ir_raw[i] = ADC_Read(i + 1);
    }
}
void MAF_Init(void)
{
    uint8_t i;
    uint8_t j;

    uint16_t value;


    for (i = 0; i < SENSOR_NUM; i++)
    {
        value = ADC_Read(i + 1);


        for (j = 0; j < MAF_SIZE; j++)
        {
            maf_buffer[i][j] = value;
        }


        ir_raw[i] = value;

        ir_filter[i] = value;
    }
}

void IR_MAF(void)
{
    uint8_t i;
    uint8_t j;

    uint32_t sum;


    for (i = 0; i < SENSOR_NUM; i++)
    {
        /*
           현재 ADC 값 저장
        */
        maf_buffer[i][maf_index] = ir_raw[i];


        sum = 0;


        /*
           최근 5개 값 합산
        */
        for (j = 0; j < MAF_SIZE; j++)
        {
            sum += maf_buffer[i][j];
        }


        /*
           평균
        */
        ir_filter[i] = sum / MAF_SIZE;
    }


    /*
       Circular Buffer
    */
    maf_index++;


    if (maf_index >= MAF_SIZE)
    {
        maf_index = 0;
    }
}


void IR_MinMax_Init(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_NUM; i++)
    {


        ir_min[i] = ir_filter[i];

        ir_max[i] = ir_filter[i];
    }
}


/* ==================================================
   MIN / MAX 갱신
   ================================================== */

void IR_MinMax_Update(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_NUM; i++)
    {
        /*
           최소값 갱신
        */
        if (ir_filter[i] < ir_min[i])
        {
            ir_min[i] = ir_filter[i];
        }


        /*
           최대값 갱신
        */
        if (ir_filter[i] > ir_max[i])
        {
            ir_max[i] = ir_filter[i];
        }
    }
}


/* ==================================================
   정규화

               filter - min
   norm = -----------------------
                 max - min


   float을 사용하지 않고
   0 ~ 100으로 변환

   0   -> 0.00
   25  -> 0.25
   80  -> 0.80
   100 -> 1.00
   ================================================== */

void IR_Normalize(void)
{
    uint8_t i;

    uint32_t numerator;
    uint16_t denominator;


    for (i = 0; i < SENSOR_NUM; i++)
    {
        /*
           max와 min이 같으면
           0으로 나누게 되므로 예외 처리
        */
        if (ir_max[i] == ir_min[i])
        {
            ir_norm[i] = 0;
        }

        else
        {
            /*
                    filter - min
               -------------------- x 100
                     max - min
            */

            numerator =
                (uint32_t)(ir_filter[i] - ir_min[i]) * 100;


            denominator =
                ir_max[i] - ir_min[i];


            ir_norm[i] =
                numerator / denominator;


            /*
               최대값 제한
            */
            if (ir_norm[i] > 100)
            {
                ir_norm[i] = 100;
            }
        }
    }
}


/* ==================================================
   LED 제어

   정규화 값 >= 0.80
   LED ON

   정규화 값 < 0.80
   LED OFF
   ================================================== */

void LED_Control(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_NUM; i++)
    {
        if (ir_norm[i] >= THRESHOLD)
        {
            /*
               해당 LED ON
            */
            PORTA |= (1 << i);
        }

        else
        {
            /*
               해당 LED OFF
            */
            PORTA &= ~(1 << i);
        }
    }
}


/* ==================================================
   USART 출력

   original
   filter(MAF)
   min
   max
   norm
   ================================================== */

void IR_UART_Print(void)
{
    uint8_t i;

    char buffer[100];

    uint8_t norm_integer;
    uint8_t norm_decimal;


    UART0_TxString(
        "\r\n"
        "       original / filter(MAF) / min / max / norm\r\n"
    );


    for (i = 0; i < SENSOR_NUM; i++)
    {
        /*
           예)

           ir_norm = 95

           95 / 100 = 0
           95 % 100 = 95

           -> 0.95
        */

        norm_integer = ir_norm[i] / 100;

        norm_decimal = ir_norm[i] % 100;


        sprintf(
            buffer,

            "IR %u : %4u    %4u    %4u   %4u   %u.%02u\r\n",

            (unsigned int)(i + 1),

            (unsigned int)ir_raw[i],

            (unsigned int)ir_filter[i],

            (unsigned int)ir_min[i],

            (unsigned int)ir_max[i],

            (unsigned int)norm_integer,

            (unsigned int)norm_decimal
        );


        UART0_TxString(buffer);
    }
}


/* ==================================================
   LCD 출력

   1번째 줄
   IR1 IR2 IR3

   2번째 줄
   IR4 IR5 IR6


   출력 예시

   1:0.02 2:0.97 3:0.87
   4:0.64 5:0.00 6:0.91
   ================================================== */

void IR_LCD_Print(void)
{
    char buffer[21];


    /* ==================================================
       LCD 첫 번째 줄
       IR1, IR2, IR3
       ================================================== */

    i2c_lcd_goto_xy(0, 0);


    sprintf(
        buffer,

        "%u.%02u %u.%02u %u.%02u",

        (unsigned int)(ir_norm[0] / 100),
        (unsigned int)(ir_norm[0] % 100),

        (unsigned int)(ir_norm[1] / 100),
        (unsigned int)(ir_norm[1] % 100),

        (unsigned int)(ir_norm[2] / 100),
        (unsigned int)(ir_norm[2] % 100)
    );


    i2c_lcd_string(buffer);



    /* ==================================================
       LCD 두 번째 줄
       IR4, IR5, IR6
       ================================================== */

    i2c_lcd_goto_xy(1, 0);


    sprintf(
        buffer,

        "%u.%02u %u.%02u %u.%02u",

        (unsigned int)(ir_norm[3] / 100),
        (unsigned int)(ir_norm[3] % 100),

        (unsigned int)(ir_norm[4] / 100),
        (unsigned int)(ir_norm[4] % 100),

        (unsigned int)(ir_norm[5] / 100),
        (unsigned int)(ir_norm[5] % 100)
    );


    i2c_lcd_string(buffer);
}


/* ==================================================
   MAIN
   ================================================== */

int main(void)
{
    /*
       PF4 ~ PF6을 ADC로 사용하기 때문에
       JTAG Disable
    */
    JTAG_Disable();


    /*
       ADC 초기화
    */
    ADC_Init();


    /*
       UART0 초기화
    */
    UART0_Init();


    /*
       LED 초기화
    */
    LED_Init();


    /*
       I2C LCD 초기화
    */
    i2c_lcd_init();

    i2c_lcd_clear();


    /*
       이동평균필터 초기화
    */
    MAF_Init();


    /*
       MIN / MAX 초기화
    */
    IR_MinMax_Init();


    /*
       USART 시작 메시지
    */
    UART0_TxString("\r\n");

    UART0_TxString(
        "============================================\r\n"
    );

    UART0_TxString(
        "        IR SENSOR 6CH TEST START\r\n"
    );

    UART0_TxString(
        "============================================\r\n"
    );


    while (1)
    {
        /*
           1. IR 센서 6개 ADC 값 측정
        */
        IR_Read();


        /*
           2. 이동평균 필터 적용
        */
        IR_MAF();


        /*
           3. MIN / MAX 갱신
        */
        IR_MinMax_Update();


        /*
           4. 정규화
        */
        IR_Normalize();


        /*
           5. 정규화 값이
              0.80 이상이면 LED ON
        */
        LED_Control();


        /*
           6. USART 출력
        */
        IR_UART_Print();


        /*
           7. LCD 출력
        */
        IR_LCD_Print();


        /*
           출력 간격
        */
        _delay_ms(200);
    }
}