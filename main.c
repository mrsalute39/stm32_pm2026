#include <stdint.h>
#include <stm32f10x.h>

#define BUTTON_ACTIVE_LOW 1

#define LED_PIN     13U   /* PC13 */
#define BUTTON_PIN  3U    /* PA3  */

/* Полупериод мигания в "шагах" по STEP_TICKS тактов задержки.
   Чем меньше число, тем чаще мигает светодиод. */
#define STEP_TICKS  1000U
static const uint32_t half_period[] = { 1000, 500, 250, 100 };
#define LEVELS (sizeof(half_period) / sizeof(half_period[0]))

void delay(uint32_t ticks) {
	for (uint32_t i = 0; i < ticks; i++) {
		__NOP();
	}
}

static inline int button_pressed(void) {
	int level = (GPIOA->IDR & (1U << BUTTON_PIN)) != 0;
#if BUTTON_ACTIVE_LOW
	return !level;
#else
	return level;
#endif
}

int __attribute((noreturn)) main(void) {
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

	/* --- PC13: выход push-pull, 10 МГц ---
	   Пины 8..15 настраиваются в CRH, у каждого пина 4 бита: CNF[1:0] MODE[1:0].
	   Для пина 13 это биты 20..23 (CNF13 / MODE13). */
	GPIOC->CRH &= ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13); /* сбросить CNF и MODE */
	GPIOC->CRH |= GPIO_CRH_MODE13_0;                   /* MODE=01: выход 10 МГц, CNF=00: push-pull */


	GPIOA->CRL &= ~(GPIO_CRL_CNF3 | GPIO_CRL_MODE3);
	GPIOA->CRL |= GPIO_CRL_CNF3_1;
#if BUTTON_ACTIVE_LOW
	GPIOA->ODR |= (1U << BUTTON_PIN);    /* pull-up */
#else
	GPIOA->ODR &= ~(1U << BUTTON_PIN);   /* pull-down */
#endif

	uint32_t level = 0;      /* текущая скорость мигания */
	uint32_t counter = 0;    /* сколько шагов прошло с последнего переключения */
	int was_pressed = 0;     /* состояние кнопки на прошлой итерации */

	while (1) {
		delay(STEP_TICKS);
		counter++;

		/* Реакция на нажатие */
		int pressed = button_pressed();
		if (pressed && !was_pressed) {
			level = (level + 1) % LEVELS;
			counter = 0;
			delay(20000);          
		}
		was_pressed = pressed;

		/* Пора переключать светодиод? */
		if (counter >= half_period[level]) {
			counter = 0;
			GPIOC->ODR ^= (1U << LED_PIN);   /* инвертировать PC13 */
		}
	}
}
