/*
 * 초기화·폴링·인터럽트 실습 템플릿입니다. CubeMX 자동 생성 파일이 아닙니다.
 * 학생은 아래 함수의 '실습 코드 작성' 부분을 채웁니다.
 * 아래쪽 HAL 콜백은 자동 생성된 인터럽트 처리 코드와 실습 함수를 연결합니다.
 * 인터럽트에서는 처리할 일을 기록하고 빠르게 돌아옵니다.
 * 인터럽트 함수 안에서는 HAL_Delay, scanf, getchar, printf를 사용하지 않습니다.
 */
#include "ece2_474.h"
#include "main.h"
/* 주변장치 변수의 extern 선언은 ece2_474.h에 있습니다. */

/* Debug 프리셋: 과제 3, PWM 프리셋: 과제 4. 두 과제는 따로 실행합니다. */
#ifndef STM32_1_TASK
#define STM32_1_TASK 3
#endif
#if STM32_1_TASK != 3 && STM32_1_TASK != 4
#error "STM32_1_TASK must be 3 or 4"
#endif

/* VS Code Watch에서 이름을 입력하여 확인할 수 있는 상태 변수입니다. */
volatile uint32_t blink_interval_ms = 1000U;
volatile uint32_t tim6_count = 0U;
volatile uint32_t tim6_irq_count = 0U;
volatile uint32_t led_toggle_count = 0U;
volatile uint32_t pwm_percent = 25U;
volatile uint32_t pwm_compare = 250U;
volatile uint32_t button_press_count = 0U;

static uint32_t last_press_ms = 0U;
static uint32_t press_seen = 0U;

/* UART 인터럽트 수신 실습 때 필요한 버퍼의 주석을 해제합니다.
 * 파일 범위에 두면 수신 완료 함수에서도 같은 버퍼를 사용할 수 있습니다.
 */
// static uint8_t uart2_rx_ece2;
// static uint8_t uart3_rx_ece2;

/* 주변장치 초기화가 끝난 뒤, 메인 루프에 들어가기 전에 한 번 호출됩니다. */
void ece2_474_init(void)
{
    blink_interval_ms = 1000U;
    tim6_count = 0U;
    tim6_irq_count = 0U;
    led_toggle_count = 0U;
    pwm_percent = 25U;
    pwm_compare = 250U;
    button_press_count = 0U;
    last_press_ms = 0U;
    press_seen = 0U;
    HAL_GPIO_WritePin(LED_ece2_GPIO_Port, LED_ece2_Pin, GPIO_PIN_RESET);

#if STM32_1_TASK == 3
    /* 초기화 과정에서 생긴 갱신 플래그를 지워 첫 주기도 100 ms가 되게 합니다. */
    __HAL_TIM_SET_COUNTER(&htim6, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK)
    {
        Error_Handler();
    }
#else
    /* ARR=999: 한 주기 1000칸. PWM을 시작하기 전에 25%로 설정합니다. */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_compare);
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
#endif
}

/* 메인 루프에서 반복해서 호출됩니다. */
void polling_routine(void)
{
    /* 과제 3과 4 모두 인터럽트로 처리하므로 폴링 코드는 비워 둡니다. */
}

/* 1. 사용자 버튼 PC13을 눌렀을 때 호출됩니다. 우선순위: 7.
 * 버튼의 접점 때문에 한 번 눌러도 여러 번 호출될 수 있습니다.
 */
static void button_isr_p7(void)
{
    uint32_t now_ms = HAL_GetTick();

    /* 첫 누름은 바로 인정하고, 이후 200 ms 이내의 반복 에지는 무시합니다.
     * PC13은 상승 에지 인터럽트이므로 계속 누르고 있어도 반복 처리하지 않습니다.
     * unsigned 뺄셈을 사용하여 HAL_GetTick의 오버플로도 처리합니다.
     */
    if (press_seen != 0U && (uint32_t)(now_ms - last_press_ms) < 200U)
    {
        return;
    }
    press_seen = 1U;
    last_press_ms = now_ms;
    ++button_press_count;

#if STM32_1_TASK == 3
    /* TIM6(우선순위 4)가 버튼 ISR(7)을 선점할 수 있습니다.
     * 간격 변경과 횟수 초기화 사이에 TIM6가 끼어들지 않게 짧게 보호합니다.
     * volatile만으로는 이 두 연산의 동시성을 보장할 수 없습니다.
     */
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    blink_interval_ms = (blink_interval_ms >= 8000U)
                        ? 1000U : blink_interval_ms * 2U;
    tim6_count = 0U;
    __set_PRIMASK(saved_primask);
#else
    /* 이 줄에 F9 중단점: F10으로 pwm_percent와 CCR1의 변화를 확인합니다. */
    pwm_percent = (pwm_percent >= 75U) ? 25U : pwm_percent + 25U;
    pwm_compare = pwm_percent * 10U;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_compare);
#endif
}

/* 2. TIM6를 인터럽트 방식으로 시작하면 100 ms마다 호출됩니다. 우선순위: 4.
 * 시작 호출: HAL_TIM_Base_Start_IT(&htim6);
 */
static void tim6_isr_p4(void)
{
#if STM32_1_TASK == 3
    ++tim6_irq_count;
    ++tim6_count;
    /* TIM6 1회=100 ms. 10/20/40/80회마다 LED 상태를 바꿉니다. */
    if (tim6_count >= blink_interval_ms / 100U)
    {
        tim6_count = 0U;
        HAL_GPIO_TogglePin(LED_ece2_GPIO_Port, LED_ece2_Pin);
        ++led_toggle_count;
    }
#endif
}

/* 3. PC 통신 UART2의 요청한 수신이 완료되면 호출됩니다. 우선순위: 6.
 * 먼저 수신 버퍼를 준비하고 HAL_UART_Receive_IT로 수신을 요청해야 합니다.
 * 다음 입력도 받으려면 처리 후 수신을 다시 요청합니다.
 * 현재 표준 입력은 기다리는 방식입니다. 같은 UART의 인터럽트 수신과 함께 쓰지 않습니다.
 */
static void uart2_isr_p6(void)
{
    /* 실습 코드 작성: PC 수신 버퍼를 처리하고 다음 수신을 요청합니다. */
}

/* 4. FPGA 통신 UART3의 요청한 수신이 완료되면 호출됩니다. 우선순위: 5.
 * 먼저 수신 버퍼를 준비하고 HAL_UART_Receive_IT로 수신을 요청해야 합니다.
 * 다음 입력도 받으려면 처리 후 수신을 다시 요청합니다.
 */
static void uart3_isr_p5(void)
{
    /* 실습 코드 작성: FPGA 수신 버퍼를 처리하고 다음 수신을 요청합니다. */
}

/* PA0: 풀업 입력의 HIGH -> LOW 변화. 우선순위: 8. */
static void gpio_in_pullup_isr_p8_ece2(void)
{
    /* 실습 코드 작성 */
}

/* PA1: 풀다운 입력의 LOW -> HIGH 변화. 우선순위: 9. */
static void gpio_in_pulldown_isr_p9_ece2(void)
{
    /* 실습 코드 작성 */
}

/* 아래는 HAL 콜백과 실습 함수를 연결하는 코드입니다. */

/* CubeMX의 EXTI 처리 코드에서 버튼 실습 함수로 연결합니다. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BUTTON_ece2_Pin)
    {
        button_isr_p7();
    }
    else if (GPIO_Pin == GPIO_IN_PULLUP_ece2_Pin)
    {
        gpio_in_pullup_isr_p8_ece2();
    }
    else if (GPIO_Pin == GPIO_IN_PULLDOWN_ece2_Pin)
    {
        gpio_in_pulldown_isr_p9_ece2();
    }
}

/* CubeMX의 타이머 처리 코드에서 TIM6 실습 함수로 연결합니다. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        tim6_isr_p4();
    }
}

/* UART 수신 완료 시 PC 통신과 FPGA 통신을 구분합니다. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uart2_isr_p6();
    }
    else if (huart->Instance == USART3)
    {
        uart3_isr_p5();
    }
}
