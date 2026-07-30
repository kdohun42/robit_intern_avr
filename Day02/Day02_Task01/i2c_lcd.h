#ifndef I2C_LCD_H_
#define I2C_LCD_H_

#include <stdint.h>

void i2c_lcd_init(void);
void i2c_lcd_clear(void);
void i2c_lcd_command(uint8_t command);
void i2c_lcd_data(uint8_t data);
void i2c_lcd_goto_xy(uint8_t row, uint8_t column); 
void i2c_lcd_string(const char *string); 

#endif