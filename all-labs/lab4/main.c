#include <stdint.h>
#include <stm32f10x.h>

/* Подключение дисплея SSD1306 (4-wire SPI):
   PA5 - CLK (SPI1_SCK)   PA7 - DIN (SPI1_MOSI)
   PA2 - /RES             PA1 - DC      PA4 - /CS            */
#define CS_LOW()    (GPIOA->BSRR = GPIO_BSRR_BR4)
#define CS_HIGH()   (GPIOA->BSRR = GPIO_BSRR_BS4)
#define DC_LOW()    (GPIOA->BSRR = GPIO_BSRR_BR1)
#define DC_HIGH()   (GPIOA->BSRR = GPIO_BSRR_BS1)
#define RES_LOW()   (GPIOA->BSRR = GPIO_BSRR_BR2)
#define RES_HIGH()  (GPIOA->BSRR = GPIO_BSRR_BS2)

#define DISP_WIDTH  128U
#define DISP_PAGES  8U      /* 64 пикселя / 8 */
#define CELL_SIZE   8U      /* клетка 8x8 пикселей -> доска 64x64 */
#define BOARD_X0    ((DISP_WIDTH - 8U * CELL_SIZE) / 2U)  /* по центру */

void delay(uint32_t ticks) {
	for (uint32_t i = 0; i < ticks; i++) {
		__NOP();
	}
}

/* ================= Библиотека SPI1 ================= */

void SPI1_Init(void) {
	/* Тактирование GPIOA, AFIO и SPI1 */
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_SPI1EN;

	/* PA5 (SCK), PA7 (MOSI): альтернативная функция, push-pull, 50 МГц (CNF=10, MODE=11 -> 0xB)
	   PA1 (DC), PA2 (RES), PA4 (CS): обычный выход push-pull, 50 МГц (CNF=00, MODE=11 -> 0x3) */
	GPIOA->CRL &= ~((0xFU << 4) | (0xFU << 8) | (0xFU << 16) |
	                (0xFU << 20) | (0xFU << 28));
	GPIOA->CRL |=  ((0x3U << 4) | (0x3U << 8) | (0x3U << 16) |
	                (0xBU << 20) | (0xBU << 28));

	CS_HIGH();  /* дисплей не выбран */

	/* CPOL=1, CPHA=1, мастер, fPCLK/256, программный NSS.
	   SSI=1 обязателен при SSM=1, иначе модуль уйдёт в режим ошибки (MODF)
	   и сбросит MSTR. SPE включаем отдельно, после настройки. */
	SPI1->CR1 = SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_MSTR |
	            SPI_CR1_BR | SPI_CR1_SSM | SPI_CR1_SSI;
	SPI1->CR1 |= SPI_CR1_SPE;
}

void SPI1_Write(uint8_t data) {
	/* Ждем, пока освободится буфер передатчика */
	while (!(SPI1->SR & SPI_SR_TXE));
	SPI1->DR = data;
	/* Ждем окончания передачи, чтобы можно было безопасно поднять CS */
	while (SPI1->SR & SPI_SR_BSY);
}

uint8_t SPI1_Read(void) {
	while (!(SPI1->SR & SPI_SR_TXE));
	SPI1->DR = 0;                         /* запускаем обмен */
	while (!(SPI1->SR & SPI_SR_RXNE));    /* ждем данные в буфере приемника */
	return (uint8_t)SPI1->DR;
}

/* ================= Драйвер SSD1306 ================= */

void display_cmd(uint8_t cmd) {
	CS_LOW();
	DC_LOW();
	delay(1000);
	SPI1_Write(cmd);
	CS_HIGH();
}

void display_data(uint8_t data) {
	CS_LOW();
	DC_HIGH();
	delay(1000);
	SPI1_Write(data);
	CS_HIGH();
}

void display_init(void) {
	/* Аппаратный сброс */
	RES_HIGH();
	delay(100000);
	RES_LOW();
	delay(100000);
	RES_HIGH();
	delay(100000);

	display_cmd(0xAE);                       /* Display OFF */
	display_cmd(0xD5); display_cmd(0x80);    /* Clock divide / osc freq */
	display_cmd(0xA8); display_cmd(0x3F);    /* Multiplex ratio: 64 строки */
	display_cmd(0xD3); display_cmd(0x00);    /* Display offset = 0 */
	display_cmd(0x40);                       /* Start line = 0 */
	display_cmd(0x8D); display_cmd(0x14);    /* Charge pump ON */
	display_cmd(0x20); display_cmd(0x00);    /* Горизонтальный режим адресации */
	display_cmd(0xA1);                       /* Segment remap: SEG127 -> COL0 */
	display_cmd(0xC8);                       /* COM scan direction: remapped */
	display_cmd(0xDA); display_cmd(0x12);    /* COM pins config */
	display_cmd(0x81); display_cmd(0xCF);    /* Контрастность */
	display_cmd(0xD9); display_cmd(0xF1);    /* Pre-charge period */
	display_cmd(0xDB); display_cmd(0x40);    /* VCOMH deselect level */
	display_cmd(0xA4);                       /* Вывод из RAM */
	display_cmd(0xA6);                       /* Normal (не инверсия) */
	display_cmd(0xAE | 1);                   /* Display ON (0xAF) */
}

/* Шахматная доска 8x8 клеток по CELL_SIZE пикселей, по центру экрана.
   В горизонтальном режиме одна страница = 8 пикселей по высоте,
   поэтому одна строка клеток = одна страница. Байт 0xFF - 8 светлых
   пикселей столбца, 0x00 - 8 тёмных. */
void display_chessboard(void) {
	/* Устанавливаем окно записи на весь экран */
	display_cmd(0x21); display_cmd(0); display_cmd(DISP_WIDTH - 1U);  /* столбцы */
	display_cmd(0x22); display_cmd(0); display_cmd(DISP_PAGES - 1U);  /* страницы */

	for (uint32_t page = 0; page < DISP_PAGES; page++) {
		for (uint32_t col = 0; col < DISP_WIDTH; col++) {
			uint8_t value = 0x00;
			if (col >= BOARD_X0 && col < BOARD_X0 + 8U * CELL_SIZE) {
				uint32_t cell_x = (col - BOARD_X0) / CELL_SIZE;
				uint32_t cell_y = page;   /* высота клетки = 1 страница */
				if (((cell_x + cell_y) & 1U) == 0U) {
					value = 0xFF;
				}
			}
			display_data(value);
		}
	}
}

int __attribute((noreturn)) main(void) {
	SPI1_Init();
	display_init();
	display_chessboard();

	while (1) {
	}
}
