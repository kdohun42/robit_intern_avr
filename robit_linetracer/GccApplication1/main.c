#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>


// ======================================================
// 기본 설정
// ======================================================

#define SENSOR_COUNT 6

/*
 * 검은 절연테이프에서 ADC 값이 낮게 나온다면 1
 * 검은 절연테이프에서 ADC 값이 높게 나온다면 0
 */
#define BLACK_IS_LOW_ADC 1


// 정규화 최댓값
#define NORMALIZED_MAX 1000


// 라인 검출 기준값
#define LINE_DETECT_THRESHOLD 300


// 기본 모터 속도
// 처음 테스트할 때는 80~100 정도 권장
#define BASE_SPEED 100


// PWM 최대값
#define MAX_SPEED 255


/*
 * P 제어 강도
 *
 * correction = position / KP_DIV
 *
 * KP_DIV이 작을수록 강하게 회전
 * KP_DIV이 클수록 부드럽게 회전
 */
#define KP_DIV 20


// 캘리브레이션 횟수
#define CALIBRATION_SAMPLES 800


// 캘리브레이션 최소 범위
#define MIN_CAL_RANGE 30



// ======================================================
// 센서 데이터
// ======================================================

// ADC 원본값
uint16_t sensor_raw[SENSOR_COUNT];


// 캘리브레이션 최소값
uint16_t sensor_min[SENSOR_COUNT];


// 캘리브레이션 최대값
uint16_t sensor_max[SENSOR_COUNT];


// 캘리브레이션 적용값
uint16_t sensor_calibrated[SENSOR_COUNT];


// 0 ~ 1000 정규화 값
uint16_t sensor_normalized[SENSOR_COUNT];



// ======================================================
// 센서 위치 가중치
// ======================================================
//
// 차량 진행 방향 ↑
//
// 왼쪽                              오른쪽
//
// IR1     IR2     IR3     IR4     IR5     IR6
//
// -2500  -1500   -500    +500   +1500   +2500
//
// ======================================================

const int16_t sensor_weight[SENSOR_COUNT] =
{
    -2500,
    -1500,
    -500,
     500,
     1500,
     2500
};



// ======================================================
// ADC 초기화
// ======================================================

void ADC_Init(void)
{
    /*
     * ATmega128의 PF4~PF7은
     * JTAG 기능과 겹침.
     *
     * PF4, PF5, PF6을 ADC로 사용하기 위해
     * JTAG 비활성화.
     */

    uint8_t sreg = SREG;

    cli();

    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);

    SREG = sreg;


    /*
     * PF1 ~ PF6 입력 설정
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
     * 내부 Pull-Up 비활성화
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
     * ADMUX
     *
     * REFS1 = 0
     * REFS0 = 1
     *
     * 기준전압 = AVCC
     */
    ADMUX = (1 << REFS0);


    /*
     * ADC 활성화
     *
     * ADEN = 1
     *
     * ADC 분주비 = 128
     *
     * 16MHz / 128
     * = 125kHz
     */
    ADCSRA =
        (1 << ADEN)  |
        (1 << ADPS2) |
        (1 << ADPS1) |
        (1 << ADPS0);
}



// ======================================================
// ADC 한 채널 읽기
// ======================================================

uint16_t ADC_Read(uint8_t channel)
{
    /*
     * 기존 기준전압 설정은 유지하고
     * ADC 채널만 변경
     */
    ADMUX =
        (ADMUX & 0xE0)
        |
        (channel & 0x1F);


    /*
     * ADC 변환 시작
     */
    ADCSRA |= (1 << ADSC);


    /*
     * 변환 완료까지 기다림
     */
    while (ADCSRA & (1 << ADSC));


    /*
     * 10bit ADC 결과 반환
     *
     * 0 ~ 1023
     */
    return ADC;
}



// ======================================================
// 1단계
// ADC 센서 6개 읽기
// ======================================================

void Sensor_ReadADC(void)
{
    /*
     * 실제 연결
     *
     * sensor_raw[0] → ADC1 → PF1 → IR1
     * sensor_raw[1] → ADC2 → PF2 → IR2
     * sensor_raw[2] → ADC3 → PF3 → IR3
     * sensor_raw[3] → ADC4 → PF4 → IR4
     * sensor_raw[4] → ADC5 → PF5 → IR5
     * sensor_raw[5] → ADC6 → PF6 → IR6
     */

    for (uint8_t i = 0; i < SENSOR_COUNT; i++)
    {
        sensor_raw[i] = ADC_Read(i + 1);
    }
}



// ======================================================
// 캘리브레이션
// ======================================================
//
// 전원을 켠 후 약 4초 동안
//
// 센서판을 좌우로 움직여서
// 6개의 센서 모두
//
// 검은 절연테이프
// +
// 흰 바닥
//
// 을 보도록 해야 함.
//
// 센서별 최소/최대 ADC 값을 저장.
// ======================================================

void Sensor_Calibration(void)
{
    /*
     * 초기값 설정
     */
    for (uint8_t i = 0; i < SENSOR_COUNT; i++)
    {
        sensor_min[i] = 1023;
        sensor_max[i] = 0;
    }


    /*
     * 800번 × 약 5ms
     *
     * 약 4초
     */
    for (uint16_t sample = 0;
         sample < CALIBRATION_SAMPLES;
         sample++)
    {
        Sensor_ReadADC();


        for (uint8_t i = 0; i < SENSOR_COUNT; i++)
        {
            /*
             * 최소값 갱신
             */
            if (sensor_raw[i] < sensor_min[i])
            {
                sensor_min[i] = sensor_raw[i];
            }


            /*
             * 최대값 갱신
             */
            if (sensor_raw[i] > sensor_max[i])
            {
                sensor_max[i] = sensor_raw[i];
            }
        }


        _delay_ms(5);
    }
}



// ======================================================
// 2단계
// 캘리브레이션 적용
// ======================================================

void Sensor_ApplyCalibration(void)
{
    for (uint8_t i = 0; i < SENSOR_COUNT; i++)
    {
        uint16_t raw = sensor_raw[i];


        /*
         * 캘리브레이션 범위 밖으로
         * 값이 나가지 않도록 제한
         */

        if (raw < sensor_min[i])
        {
            raw = sensor_min[i];
        }


        if (raw > sensor_max[i])
        {
            raw = sensor_max[i];
        }


        /*
         * 최소 ADC 값을 0 기준으로 변환
         */
        sensor_calibrated[i] =
            raw - sensor_min[i];
    }
}



// ======================================================
// 3단계
// 정규화
// ======================================================
//
// 센서마다 ADC 범위가 달라도
// 최종적으로:
//
// 흰색 ≈ 0
// 검은색 ≈ 1000
//
// 이 되도록 변환.
// ======================================================

void Sensor_Normalize(void)
{
    for (uint8_t i = 0; i < SENSOR_COUNT; i++)
    {
        /*
         * 센서 ADC 전체 범위
         */
        uint16_t range =
            sensor_max[i] - sensor_min[i];


        /*
         * 캘리브레이션 범위가 너무 작으면
         * 정상적으로 캘리브레이션되지 않은 것으로 판단
         */
        if (range < MIN_CAL_RANGE)
        {
            sensor_normalized[i] = 0;
            continue;
        }


        /*
         * 0 ~ 1000으로 변환
         */
        uint32_t value =
            ((uint32_t)sensor_calibrated[i]
             * NORMALIZED_MAX)
            / range;


#if BLACK_IS_LOW_ADC

        /*
         * 검은색 ADC가 낮은 센서
         *
         * raw = 최소값
         * → 검은색
         * → 1000
         *
         * raw = 최대값
         * → 흰색
         * → 0
         */
        sensor_normalized[i] =
            NORMALIZED_MAX - value;

#else

        /*
         * 검은색 ADC가 높은 센서
         *
         * raw = 최소값
         * → 흰색
         * → 0
         *
         * raw = 최대값
         * → 검은색
         * → 1000
         */
        sensor_normalized[i] = value;

#endif
    }
}



// ======================================================
// 4단계
// 가중치를 이용한 라인 위치 계산
// ======================================================
//
// position =
//
// Σ(센서 정규화 값 × 센서 가중치)
// ────────────────────────────
//       Σ(센서 정규화 값)
//
// 결과:
//
// 음수 → 라인이 왼쪽
//
// 0    → 라인이 중앙
//
// 양수 → 라인이 오른쪽
//
// ======================================================

int16_t Line_CalculatePosition(uint32_t *strength)
{
    int32_t weighted_sum = 0;

    uint32_t sensor_sum = 0;


    for (uint8_t i = 0; i < SENSOR_COUNT; i++)
    {
        /*
         * 센서값 × 위치 가중치
         */
        weighted_sum +=
            (int32_t)sensor_normalized[i]
            * sensor_weight[i];


        /*
         * 전체 센서 감지 강도
         */
        sensor_sum += sensor_normalized[i];
    }


    /*
     * 라인 검출 판단에 사용할 값
     */
    *strength = sensor_sum;


    /*
     * 0으로 나누는 것 방지
     */
    if (sensor_sum == 0)
    {
        return 0;
    }


    /*
     * 라인 위치 계산
     */
    return (int16_t)
        (weighted_sum / (int32_t)sensor_sum);
}



// ======================================================
// 5단계
// 라인 존재 여부 판단
// ======================================================

uint8_t Line_IsDetected(uint32_t strength)
{
    /*
     * 센서 전체 감지 강도가
     * 기준값보다 크면
     * 검은 라인이 있다고 판단
     */
    if (strength >= LINE_DETECT_THRESHOLD)
    {
        return 1;
    }


    return 0;
}



// ======================================================
// PWM 초기화
// ======================================================

void PWM_Init(void)
{
    /*
     * L298N
     *
     * PB5 / OC1A → ENA
     * PB6 / OC1B → ENB
     */

    DDRB |=
        (1 << PB5) |
        (1 << PB6);


    /*
     * Timer1
     *
     * 8bit Fast PWM
     * 비반전 출력
     *
     * OC1A
     * OC1B
     */
    TCCR1A =
        (1 << COM1A1) |
        (1 << COM1B1) |
        (1 << WGM10);


    /*
     * WGM12 = 1
     *
     * WGM12:10 = 101
     * → Fast PWM 8bit
     *
     * 분주비 = 8
     */
    TCCR1B =
        (1 << WGM12) |
        (1 << CS11);


    /*
     * 처음에는 모터 정지
     */
    OCR1A = 0;
    OCR1B = 0;
}



// ======================================================
// L298N 방향 제어 초기화
// ======================================================

void Motor_Init(void)
{
    /*
     * PB0 → IN1
     * PB1 → IN2
     * PB2 → IN3
     * PB3 → IN4
     */

    DDRB |=
        (1 << PB0) |
        (1 << PB1) |
        (1 << PB2) |
        (1 << PB3);


    /*
     * 왼쪽 모터 전진
     *
     * IN1 = 1
     * IN2 = 0
     */
    PORTB |=  (1 << PB0);
    PORTB &= ~(1 << PB1);


    /*
     * 오른쪽 모터 전진
     *
     * IN3 = 1
     * IN4 = 0
     */
    PORTB |=  (1 << PB2);
    PORTB &= ~(1 << PB3);
}



// ======================================================
// 모터 정지
// ======================================================

void Motor_Stop(void)
{
    OCR1A = 0;
    OCR1B = 0;
}



// ======================================================
// 모터 속도 설정
// ======================================================

void Motor_SetSpeed(int16_t left,
                    int16_t right)
{
    /*
     * 0보다 작아지면 0으로 제한
     */
    if (left < 0)
    {
        left = 0;
    }


    if (right < 0)
    {
        right = 0;
    }


    /*
     * 255보다 커지면
     * 255로 제한
     */
    if (left > MAX_SPEED)
    {
        left = MAX_SPEED;
    }


    if (right > MAX_SPEED)
    {
        right = MAX_SPEED;
    }


    /*
     * PWM 출력
     */
    OCR1A = (uint8_t)left;
    OCR1B = (uint8_t)right;
}



// ======================================================
// 6단계
// 라인에 따른 이동
// ======================================================

void Line_Move(int16_t position,
               uint8_t line_detected)
{
    /*
     * 라인이 없는 경우
     *
     * 현재 기본 코드는 정지
     */
    if (line_detected == 0)
    {
        Motor_Stop();

        return;
    }


    /*
     * 목표 위치 = 0
     *
     * position < 0
     * → 라인이 왼쪽
     *
     * position > 0
     * → 라인이 오른쪽
     */
    int16_t error = position;


    /*
     * P 제어
     *
     * position이 클수록
     * 모터 속도 차이가 크게 발생
     */
    int16_t correction =
        error / KP_DIV;


    /*
     * 왼쪽 모터 속도
     */
    int16_t left_speed =
        BASE_SPEED + correction;


    /*
     * 오른쪽 모터 속도
     */
    int16_t right_speed =
        BASE_SPEED - correction;


    /*
     * 실제 PWM 출력
     */
    Motor_SetSpeed(
        left_speed,
        right_speed
    );
}



// ======================================================
// MAIN
// ======================================================

int main(void)
{
    // ==============================
    // 초기화
    // ==============================

    ADC_Init();

    PWM_Init();

    Motor_Init();


    /*
     * 캘리브레이션 중에는
     * 모터 정지
     */
    Motor_Stop();



    // ==============================
    // 센서 캘리브레이션
    // ==============================
    //
    // 전원을 켠 뒤 약 4초 동안
    //
    // 센서판을 좌우로 움직여
    //
    // IR1 ~ IR6 모두
    //
    // 흰 바닥
    // +
    // 검은 절연테이프
    //
    // 를 보도록 한다.
    // ==============================

    Sensor_Calibration();



    /*
     * 캘리브레이션 완료 후
     *
     * 자동차를 라인 위에
     * 올려놓을 시간
     *
     * 3초
     */

    Motor_Stop();

    _delay_ms(3000);



    // ==================================================
    // 라인트레이서 시작
    // ==================================================

    while (1)
    {
        uint32_t line_strength;

        int16_t line_position;

        uint8_t line_detected;



        // ==============================================
        // 1. ADC 읽기
        // ==============================================

        Sensor_ReadADC();



        // ==============================================
        // 2. 캘리브레이션 적용
        // ==============================================

        Sensor_ApplyCalibration();



        // ==============================================
        // 3. 정규화
        // ==============================================

        Sensor_Normalize();



        // ==============================================
        // 4. 가중치를 이용해 라인 위치 계산
        // ==============================================

        line_position =
            Line_CalculatePosition(
                &line_strength
            );



        // ==============================================
        // 5. 라인 존재 여부 판단
        // ==============================================

        line_detected =
            Line_IsDetected(
                line_strength
            );



        // ==============================================
        // 6. 이동
        // ==============================================

        Line_Move(
            line_position,
            line_detected
        );



        /*
         * 다음 제어까지 5ms 대기
         */
        _delay_ms(5);
    }
}