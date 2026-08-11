#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>


/* =========================================================
   사용자 설정
   ========================================================= */

/*
    검은색에서 ADC 값이

    1 : 작아짐
    0 : 커짐

    검은 선에 올렸는데 LED가 반대로 동작하면
    1 ↔ 0 변경
*/
#define BLACK_IS_LOW           1


/*
    LED 동작 방식

    1 : PA 핀 HIGH → LED ON
    0 : PA 핀 LOW  → LED ON
*/
#define LED_ACTIVE_HIGH        1


/*
    모터 방향 반전

    전진해야 하는데 특정 바퀴가 반대로 돌면
    해당 값을 1로 변경
*/
#define LEFT_MOTOR_REVERSED    0
#define RIGHT_MOTOR_REVERSED   0


/*
    모터 속도
    범위 : 0 ~ 255

    처음에는 너무 빠르지 않게 설정
*/
#define FORWARD_SPEED          150
#define PIVOT_SPEED            140


/*
    라인 중앙 허용 범위

    position 범위
    약 -2500 ~ +2500

    -700 ~ +700
    → 직진
*/
#define CENTER_DEADBAND        700


/*
    정규화 센서값이 이 값 이상이면
    검은 선을 인식했다고 판단
*/
#define LINE_DETECT_THRESHOLD  300


/*
    작은 센서값 제거
*/
#define SENSOR_NOISE_FLOOR     40


/*
    캘리브레이션 최소 ADC 변화량
*/
#define MIN_CAL_RANGE          40


/*
    센서 개수
*/
#define SENSOR_COUNT           6



/* =========================================================
   L298N 모터 드라이버

   왼쪽 모터
   IN1 = PB0
   IN2 = PB1
   ENA = PB5 = OC1A

   오른쪽 모터
   IN3 = PB2
   IN4 = PB3
   ENB = PB6 = OC1B
   ========================================================= */

#define LEFT_IN1       PB0
#define LEFT_IN2       PB1

#define RIGHT_IN1      PB2
#define RIGHT_IN2      PB3

#define LEFT_PWM_PIN   PB5
#define RIGHT_PWM_PIN  PB6



/* =========================================================
   스위치

   SW1 = PE4
   → 캘리브레이션

   SW2 = PE5
   → 주행 시작

   외부 Pull-Up

   안 누름 = HIGH
   누름    = LOW
   ========================================================= */

#define SW1_PIN        PE4
#define SW2_PIN        PE5



/* =========================================================
   LED

   실제 센서 왼쪽 → 오른쪽 기준

   IR1 → PA0
   IR2 → PA1
   IR3 → PA2
   IR4 → PA3
   IR5 → PA4
   IR6 → PA5
   ========================================================= */

#define LED1           PA0
#define LED2           PA1
#define LED3           PA2
#define LED4           PA3
#define LED5           PA4
#define LED6           PA5



/* =========================================================
   실제 IR 센서 배열

   테스트 결과:

   실제 왼쪽 → 오른쪽 순서

   PF1 PF2 PF3 PF6 PF5 PF4

   즉 ADC 채널:

   1, 2, 3, 6, 5, 4
   ========================================================= */

const uint8_t sensor_channel[SENSOR_COUNT] =
{
    1, 2, 3, 6, 5, 4
};



/* =========================================================
   가중치

   실제 차량 왼쪽 → 오른쪽

   IR1    IR2    IR3   IR4   IR5    IR6

  -2500  -1500  -500   500   1500   2500
   ========================================================= */

const int16_t sensor_weight[SENSOR_COUNT] =
{
    -2500,
    -1500,
     -500,
      500,
     1500,
     2500
};



/* =========================================================
   센서 데이터
   ========================================================= */

uint16_t sensor_raw[SENSOR_COUNT];

uint16_t sensor_min[SENSOR_COUNT];
uint16_t sensor_max[SENSOR_COUNT];

uint16_t sensor_normalized[SENSOR_COUNT];



/* =========================================================
   상태 변수
   ========================================================= */

uint8_t calibration_done = 0;

uint8_t running = 0;


/*
    마지막 라인 방향

    -1 = 왼쪽
     0 = 중앙 / 모름
     1 = 오른쪽
*/
int8_t last_direction = 0;



/* =========================================================
   JTAG 비활성화

   PF4 ~ PF6를 ADC로 사용하기 위해 필요
   ========================================================= */

void JTAG_Disable(void)
{
    uint8_t old_sreg;
    uint8_t value;

    old_sreg = SREG;

    cli();

    value = MCUCSR | (1 << JTD);

    MCUCSR = value;
    MCUCSR = value;

    SREG = old_sreg;
}



/* =========================================================
   ADC 초기화
   ========================================================= */

void ADC_Init(void)
{
    /*
        PF1 ~ PF6 입력
    */
    DDRF &= ~(
        (1 << PF1) |
        (1 << PF2) |
        (1 << PF3) |
        (1 << PF4) |
        (1 << PF5) |
        (1 << PF6)
    );


    /*
        내부 Pull-Up OFF
    */
    PORTF &= ~(
        (1 << PF1) |
        (1 << PF2) |
        (1 << PF3) |
        (1 << PF4) |
        (1 << PF5) |
        (1 << PF6)
    );


    /*
        ADC 기준전압 = AVCC
        오른쪽 정렬
    */
    ADMUX = (1 << REFS0);


    /*
        ADC Enable

        분주비 = 128

        16MHz / 128
        = 125kHz
    */
    ADCSRA =
        (1 << ADEN) |
        (1 << ADPS2) |
        (1 << ADPS1) |
        (1 << ADPS0);
}



/* =========================================================
   ADC 1회 변환
   ========================================================= */

uint16_t ADC_Convert_Once(void)
{
    uint8_t low;
    uint8_t high;


    /*
        변환 시작
    */
    ADCSRA |= (1 << ADSC);


    /*
        변환 완료 대기
    */
    while (ADCSRA & (1 << ADSC))
    {
        ;
    }


    /*
        ADCL 먼저 읽음
    */
    low = ADCL;

    high = ADCH;


    return ((uint16_t)high << 8) | low;
}



/* =========================================================
   특정 ADC 채널 읽기
   ========================================================= */

uint16_t ADC_Read(uint8_t channel)
{
    /*
        AVCC 기준전압
        ADC 채널 선택
    */
    ADMUX =
        (1 << REFS0) |
        (channel & 0x07);


    /*
        채널 변경 직후 첫 값은 버림
    */
    ADC_Convert_Once();


    /*
        두 번째 값 사용
    */
    return ADC_Convert_Once();
}



/* =========================================================
   IR 센서 6개 읽기
   ========================================================= */

void Read_All_Sensors(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        sensor_raw[i] =
            ADC_Read(sensor_channel[i]);
    }
}



/* =========================================================
   LED 초기화
   ========================================================= */

void LED_Init(void)
{
    /*
        PA0 ~ PA5 출력
    */
    DDRA |=
        (1 << LED1) |
        (1 << LED2) |
        (1 << LED3) |
        (1 << LED4) |
        (1 << LED5) |
        (1 << LED6);


#if LED_ACTIVE_HIGH

    PORTA &= ~(
        (1 << LED1) |
        (1 << LED2) |
        (1 << LED3) |
        (1 << LED4) |
        (1 << LED5) |
        (1 << LED6)
    );

#else

    PORTA |=
        (1 << LED1) |
        (1 << LED2) |
        (1 << LED3) |
        (1 << LED4) |
        (1 << LED5) |
        (1 << LED6);

#endif
}



/* =========================================================
   LED ON
   ========================================================= */

void LED_On(uint8_t led)
{
#if LED_ACTIVE_HIGH

    PORTA |= (1 << led);

#else

    PORTA &= ~(1 << led);

#endif
}



/* =========================================================
   LED OFF
   ========================================================= */

void LED_Off(uint8_t led)
{
#if LED_ACTIVE_HIGH

    PORTA &= ~(1 << led);

#else

    PORTA |= (1 << led);

#endif
}



/* =========================================================
   LED 전체 OFF
   ========================================================= */

void LED_All_Off(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        LED_Off(i);
    }
}



/* =========================================================
   센서 상태 LED 표시

   normalized >= threshold

   → 검은선

   → LED ON
   ========================================================= */

void LED_Update(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        if (sensor_normalized[i] >= LINE_DETECT_THRESHOLD)
        {
            LED_On(i);
        }
        else
        {
            LED_Off(i);
        }
    }
}



/* =========================================================
   스위치 초기화
   ========================================================= */

void Switch_Init(void)
{
    /*
        PE4, PE5 입력
    */
    DDRE &= ~(
        (1 << SW1_PIN) |
        (1 << SW2_PIN)
    );


    /*
        외부 Pull-Up 사용
        내부 Pull-Up OFF
    */
    PORTE &= ~(
        (1 << SW1_PIN) |
        (1 << SW2_PIN)
    );
}



/* =========================================================
   버튼 눌림 확인
   ========================================================= */

uint8_t Switch_Pressed(uint8_t pin)
{
    /*
        LOW이면 눌림
    */
    if (!(PINE & (1 << pin)))
    {
        /*
            채터링 제거
        */
        _delay_ms(20);


        if (!(PINE & (1 << pin)))
        {
            /*
                버튼에서 손을 뗄 때까지 대기
            */
            while (!(PINE & (1 << pin)))
            {
                ;
            }


            _delay_ms(20);


            return 1;
        }
    }


    return 0;
}



/* =========================================================
   PWM 초기화

   ★ 이번에 실제 모터 동작 확인된 설정 ★

   Timer1

   PB5 = OC1A
   PB6 = OC1B

   8-bit Phase Correct PWM

   TOP = 255
   Prescaler = 1

   PWM Frequency

   16MHz / 510
   ≈ 31.37kHz

   삐 소리도 거의 들리지 않음
   ========================================================= */

void PWM_Init(void)
{
    /*
        PB5 / PB6 출력
    */
    DDRB |=
        (1 << LEFT_PWM_PIN) |
        (1 << RIGHT_PWM_PIN);


    /*
        Phase Correct PWM 8bit

        WGM13 = 0
        WGM12 = 0
        WGM11 = 0
        WGM10 = 1

        OC1A / OC1B
        Non-Inverting
    */
    TCCR1A =
        (1 << COM1A1) |
        (1 << COM1B1) |
        (1 << WGM10);


    /*
        Prescaler = 1
    */
    TCCR1B =
        (1 << CS10);


    /*
        초기 PWM = 0
    */
    OCR1A = 0;

    OCR1B = 0;
}



/* =========================================================
   모터 방향핀 초기화
   ========================================================= */

void Motor_Init(void)
{
    /*
        PB0 ~ PB3 출력
    */
    DDRB |=
        (1 << LEFT_IN1) |
        (1 << LEFT_IN2) |
        (1 << RIGHT_IN1) |
        (1 << RIGHT_IN2);


    /*
        초기 LOW
    */
    PORTB &= ~(
        (1 << LEFT_IN1) |
        (1 << LEFT_IN2) |
        (1 << RIGHT_IN1) |
        (1 << RIGHT_IN2)
    );
}



/* =========================================================
   왼쪽 모터

   +255 = 최대 전진
      0 = 정지
   -255 = 최대 후진
   ========================================================= */

void Motor_Left(int16_t speed)
{
#if LEFT_MOTOR_REVERSED

    speed = -speed;

#endif


    /*
        범위 제한
    */
    if (speed > 255)
    {
        speed = 255;
    }


    if (speed < -255)
    {
        speed = -255;
    }


    /* -----------------------------------------------------
       전진
       ----------------------------------------------------- */

    if (speed > 0)
    {
        /*
            IN1 = 1
            IN2 = 0
        */
        PORTB |=  (1 << LEFT_IN1);

        PORTB &= ~(1 << LEFT_IN2);


        OCR1A = (uint8_t)speed;
    }


    /* -----------------------------------------------------
       후진
       ----------------------------------------------------- */

    else if (speed < 0)
    {
        /*
            IN1 = 0
            IN2 = 1
        */
        PORTB &= ~(1 << LEFT_IN1);

        PORTB |=  (1 << LEFT_IN2);


        OCR1A = (uint8_t)(-speed);
    }


    /* -----------------------------------------------------
       정지
       ----------------------------------------------------- */

    else
    {
        OCR1A = 0;


        PORTB &= ~(1 << LEFT_IN1);

        PORTB &= ~(1 << LEFT_IN2);
    }
}



/* =========================================================
   오른쪽 모터
   ========================================================= */

void Motor_Right(int16_t speed)
{
#if RIGHT_MOTOR_REVERSED

    speed = -speed;

#endif


    /*
        범위 제한
    */
    if (speed > 255)
    {
        speed = 255;
    }


    if (speed < -255)
    {
        speed = -255;
    }


    /* -----------------------------------------------------
       전진
       ----------------------------------------------------- */

    if (speed > 0)
    {
        /*
            IN3 = 1
            IN4 = 0
        */
        PORTB |=  (1 << RIGHT_IN1);

        PORTB &= ~(1 << RIGHT_IN2);


        OCR1B = (uint8_t)speed;
    }


    /* -----------------------------------------------------
       후진
       ----------------------------------------------------- */

    else if (speed < 0)
    {
        /*
            IN3 = 0
            IN4 = 1
        */
        PORTB &= ~(1 << RIGHT_IN1);

        PORTB |=  (1 << RIGHT_IN2);


        OCR1B = (uint8_t)(-speed);
    }


    /* -----------------------------------------------------
       정지
       ----------------------------------------------------- */

    else
    {
        OCR1B = 0;


        PORTB &= ~(1 << RIGHT_IN1);

        PORTB &= ~(1 << RIGHT_IN2);
    }
}



/* =========================================================
   모터 정지
   ========================================================= */

void Motor_Stop(void)
{
    Motor_Left(0);

    Motor_Right(0);
}



/* =========================================================
   직진
   ========================================================= */

void Motor_Forward(void)
{
    Motor_Left(FORWARD_SPEED);

    Motor_Right(FORWARD_SPEED);
}



/* =========================================================
   왼쪽 제자리 회전

   왼쪽 바퀴  = 후진
   오른쪽 바퀴 = 전진
   ========================================================= */

void Motor_Pivot_Left(void)
{
    Motor_Left(-PIVOT_SPEED);

    Motor_Right(PIVOT_SPEED);
}



/* =========================================================
   오른쪽 제자리 회전

   왼쪽 바퀴  = 전진
   오른쪽 바퀴 = 후진
   ========================================================= */

void Motor_Pivot_Right(void)
{
    Motor_Left(PIVOT_SPEED);

    Motor_Right(-PIVOT_SPEED);
}



/* =========================================================
   센서 정규화

   결과

   0    = 흰색
   1000 = 검은색
   ========================================================= */

void Normalize_Sensors(void)
{
    uint8_t i;

    uint16_t raw;

    uint16_t min_value;
    uint16_t max_value;

    uint16_t range;

    int32_t value;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        raw = sensor_raw[i];

        min_value = sensor_min[i];

        max_value = sensor_max[i];


        /*
            캘리브레이션 값 이상
        */
        if (max_value <= min_value)
        {
            sensor_normalized[i] = 0;

            continue;
        }


        range =
            max_value - min_value;


        /*
            변화량이 너무 작음
        */
        if (range < MIN_CAL_RANGE)
        {
            sensor_normalized[i] = 0;

            continue;
        }


        /*
            범위 제한
        */
        if (raw < min_value)
        {
            raw = min_value;
        }


        if (raw > max_value)
        {
            raw = max_value;
        }



#if BLACK_IS_LOW

        /*
            검은색에서 ADC 낮아짐

            min → 1000
            max → 0
        */
        value =
            ((int32_t)(max_value - raw) * 1000)
            / range;

#else

        /*
            검은색에서 ADC 높아짐

            min → 0
            max → 1000
        */
        value =
            ((int32_t)(raw - min_value) * 1000)
            / range;

#endif


        /*
            0 ~ 1000 제한
        */
        if (value < 0)
        {
            value = 0;
        }


        if (value > 1000)
        {
            value = 1000;
        }


        /*
            작은 값 제거
        */
        if (value < SENSOR_NOISE_FLOOR)
        {
            value = 0;
        }


        sensor_normalized[i] =
            (uint16_t)value;
    }
}



/* =========================================================
   캘리브레이션

   SW1 누름

        ↓

   약 5초간

   각 센서 MIN / MAX 저장

   이때 차량을 손으로 좌우로 움직여
   모든 센서가

   흰색 + 검은색

   모두 보도록 함.
   ========================================================= */

void Calibration(void)
{
    uint8_t i;

    uint16_t count;


    /*
        모터 정지
    */
    Motor_Stop();


    /*
        LED OFF
    */
    LED_All_Off();


    /*
        캘리브레이션 초기화
    */
    for (i = 0; i < SENSOR_COUNT; i++)
    {
        sensor_min[i] = 1023;

        sensor_max[i] = 0;
    }


    /*
        약 5초
    */
    for (count = 0; count < 1000; count++)
    {
        /*
            ADC 읽기
        */
        Read_All_Sensors();


        /*
            MIN / MAX 업데이트
        */
        for (i = 0; i < SENSOR_COUNT; i++)
        {
            if (sensor_raw[i] < sensor_min[i])
            {
                sensor_min[i] =
                    sensor_raw[i];
            }


            if (sensor_raw[i] > sensor_max[i])
            {
                sensor_max[i] =
                    sensor_raw[i];
            }
        }


        /*
            현재까지의 값으로 정규화
        */
        Normalize_Sensors();


        /*
            센서 상태 LED 표시
        */
        LED_Update();


        _delay_ms(5);
    }


    /*
        최종 센서 상태 읽기
    */
    Read_All_Sensors();

    Normalize_Sensors();

    LED_Update();


    calibration_done = 1;
}



/* =========================================================
   라인 위치 계산

   return

   1 = 라인 발견
   0 = 라인 없음


   position

   -2500 정도 = 왼쪽 끝
       0       = 중앙
   +2500 정도 = 오른쪽 끝
   ========================================================= */

uint8_t Calculate_Line_Position(int16_t *position)
{
    uint8_t i;

    uint16_t max_sensor = 0;

    uint32_t total = 0;

    int32_t weighted_sum = 0;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        uint16_t value;


        value =
            sensor_normalized[i];


        /*
            가장 강한 센서값
        */
        if (value > max_sensor)
        {
            max_sensor = value;
        }


        /*
            센서값 총합
        */
        total += value;


        /*
            센서값 × 가중치
        */
        weighted_sum +=
            (int32_t)value *
            sensor_weight[i];
    }


    /*
        아무 센서도 검은선을
        충분히 감지하지 못함
    */
    if (max_sensor < LINE_DETECT_THRESHOLD)
    {
        return 0;
    }


    if (total == 0)
    {
        return 0;
    }


    /*
        가중 평균
    */
    *position =
        (int16_t)(
            weighted_sum /
            (int32_t)total
        );


    return 1;
}



/* =========================================================
   라인 추종
   ========================================================= */

void Line_Control(void)
{
    int16_t position;

    uint8_t line_found;


    /* =====================================================
       1. ADC 읽기
       ===================================================== */

    Read_All_Sensors();


    /* =====================================================
       2. 정규화
       ===================================================== */

    Normalize_Sensors();


    /* =====================================================
       3. LED 표시
       ===================================================== */

    LED_Update();


    /* =====================================================
       4. 가중치 계산
       ===================================================== */

    line_found =
        Calculate_Line_Position(&position);



    /* =====================================================
       라인 발견
       ===================================================== */

    if (line_found)
    {
        /* -------------------------------------------------
           라인 왼쪽
           ------------------------------------------------- */

        if (position < -CENTER_DEADBAND)
        {
            last_direction = -1;


            /*
                좌 제자리 회전
            */
            Motor_Pivot_Left();
        }


        /* -------------------------------------------------
           라인 오른쪽
           ------------------------------------------------- */

        else if (position > CENTER_DEADBAND)
        {
            last_direction = 1;


            /*
                우 제자리 회전
            */
            Motor_Pivot_Right();
        }


        /* -------------------------------------------------
           라인 중앙
           ------------------------------------------------- */

        else
        {
            /*
                미세 방향 기억
            */
            if (position < -100)
            {
                last_direction = -1;
            }


            else if (position > 100)
            {
                last_direction = 1;
            }


            /*
                직진
            */
            Motor_Forward();
        }
    }



    /* =====================================================
       라인 분실
       ===================================================== */

    else
    {
        /*
            마지막 라인이 왼쪽
        */
        if (last_direction < 0)
        {
            Motor_Pivot_Left();
        }


        /*
            마지막 라인이 오른쪽
        */
        else if (last_direction > 0)
        {
            Motor_Pivot_Right();
        }


        /*
            방향을 모르면 정지
        */
        else
        {
            Motor_Stop();
        }
    }
}

int main(void)
{
    /*
        PF4 ~ PF6 ADC 사용
    */
    JTAG_Disable();


    /*
        각 장치 초기화
    */
    ADC_Init();

    Motor_Init();

    PWM_Init();

    Switch_Init();

    LED_Init();


    /*
        초기 상태
    */
    Motor_Stop();

    LED_All_Off();


    while (1)
    {
        /* =================================================
           주행 시작 전
           ================================================= */

        if (!running)
        {
            /* ---------------------------------------------
               SW1

               캘리브레이션
               --------------------------------------------- */

            if (Switch_Pressed(SW1_PIN))
            {
                Calibration();
            }


            /*
                캘리브레이션 완료 후에는

                SW2를 누르지 않아도
                센서 상태를 LED로 표시
            */
            if (calibration_done)
            {
                Read_All_Sensors();

                Normalize_Sensors();

                LED_Update();
            }


            /* ---------------------------------------------
               SW2

               주행 시작
               --------------------------------------------- */

            if (calibration_done)
            {
                if (Switch_Pressed(SW2_PIN))
                {
                    running = 1;
                    /*
                        출발 시 방향 정보 초기화
                    */
                    last_direction = 0;
                }
            }


            /*
                주행 전 모터 정지
            */
            if (!running)
            {
                Motor_Stop();
            }
        }


        /* =================================================
           주행 중
           ================================================= */

        else
        {
            Line_Control();
        }
		
    }


    return 0;
}