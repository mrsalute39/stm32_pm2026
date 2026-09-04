#include <stdint.h>
#include <stm32f10x.h>

void delay(uint32_t ticks) {
	for (int i=0; i<ticks; i++) {
		__NOP();
	}
}

int __attribute((noreturn)) main(void) {
	// Enable clock for AFIO
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
	// Enable clock for GPIOC
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
	// Enable PC13 push-pull mode
	GPIOC->CRH &= ~GPIO_CRH_CNF12; //clear cnf bits
	GPIOC->CRH |= GPIO_CRH_MODE12_0; //Max speed = 10Mhz

    while (1) {
	    GPIOC->ODR |= (1U<<12U); //U -- unsigned suffix (to avoid syntax warnings in IDE)
		delay(1000000);
	    GPIOC->ODR &= ~(1U<<12U);
	    delay(1000000);
    }
}
