/*
 * tlv320.h
 *
 *  Created on: Jan 26, 2014
 *      Author: dermeister
 */

#ifndef TLV320_H_
#define TLV320_H_

/*
 * Compatibility defines.  This should work on ATmega8, ATmega16,
 * ATmega163, ATmega323 and ATmega128 (IOW: on all devices that
 * provide a builtin TWI interface).
 *
 * On the 128, it defaults to USART 1.
 */
#ifndef UCSRB
# ifdef UCSR1A		/* ATmega128 */
#  define UCSRA UCSR1A
#  define UCSRB UCSR1B
#  define UBRR UBRR1L
#  define UDR UDR1
# else /* ATmega8 */
#  define UCSRA USR
#  define UCSRB UCR
# endif
#endif
#ifndef UBRR
#  define UBRR UBRRL
#endif

#define I2C_ERROR 1
#define I2C_OK 0

#define TLV_OK 0
#define TLV_ERROR -1

// Address of TLV320 is 00110000
#define I2C_SLA_TLV320	0x30

#define TLC320_AUTO_INCREMENT_DISABLE 0
#define TLV320_AUTO_INCREMENT_ENABLE 0x80

// Register defines
// Page 0
#define TLV320_REG_PAGE_SEL			0x00
#define TLV320_CLK_IN_MUXING		0x04
#define TLV320_PLL_P_R				0x05
#define TLV320_PLL_J				0x06
#define TLV320_PLL_D_MSB			0x07
#define TLV320_PLL_D_LSB			0x08
#define TLV320_DAC_NDAC				0x0B
#define TLV320_DAC_MDAC				0x0C
#define TLV320_DAC_DOSR_MSB			0x0D
#define TLV320_DAC_DOSR_LSB			0x0E
#define TLV320_DAC_IDAC				0x0F
#define TLV320_DAC_INTERPOL			0x10
#define TLV320_ADC_NADC				0x12
#define TLV320_ADC_MADC				0x13
#define TLV320_ADC_AOSR 			0x14
#define TLV320_ADC_IADC 			0x15
#define TLV320_ADC_DECIM 			0x16
#define TLV320_CLK_OUT_MUXING		0x19
#define TLV320_CODEC_INTERFACE_1	0x1B
#define TLV320_CODEC_INTERFACE_2	0x1D
#define TLV320_BLK_N				0x1E
#define TLV320_I2C_CONDITION		0x22
#define TLV320_ADC_FLAG				0x24
#define TLV320_DAC_FLAG_1			0x25
#define TLV320_DAC_FLAG_2			0x26
#define TLV320_GPIO1_CTL			0x33
#define TLV320_DOUT_CTL				0x35
#define TLV320_DIN_CTL				0x36
#define TLV320_DAC_PROC_BLOCK		0x3C
#define TLV320_DAC_DATAPATH			0x3F
#define TLV320_DAC_VOLUME_MUTE		0x40
#define TLV320_DAC_VOLUME			0x41
#define TLV320_DRC_CTL_1			0x44
#define TLV320_DRC_CTL_2			0x45
#define TLV320_DRC_CTL_3			0x46
#define TLV320_BEEP					0x47
#define TLV320_ADC_VOLUME_FINE		0x52
#define TLV320_ADC_VOLUME_COARSE	0x53
#define TLV320_AGC_CTL_1			0x56
#define TLV320_AGC_CTL_2			0x57
#define TLV320_AGC_MAX_GAIN			0x58
#define TLV320_AGC_ATTACK_TIME		0x59
#define TLV320_AGC_DECAY_TIME		0x5A
#define TLV320_AGC_NOISE_DEBOUNCE	0x5B
#define TLV320_AGC_SIGNAL_DEBOUNCE	0x5C
#define TLV320_AGC_GAIN_APP_READING	0x5D


// Page 1
#define TLV320_HP_DRIVER			0x1F
#define TLV320_AMP					0x20
#define TLV320_AMP_POP				0x21
#define TLV320_DRIVER_RAMP_DOWN_CTL	0x22
#define TLV320_DAC_MIXER_ROUTING	0x23
#define TLV320_HP_VOL_ANALOG		0x24
#define TLV320_AMP_VOL_ANALOG		0x26
#define TLV320_HPOUT_DRIVER			0x28
#define TLV320_AMP_OUTPUT_DRIVER	0x2A
#define TLV320_MIC_BIAS				0x2E
#define TLV320_MIC_PGA				0x2F
#define TLV320_ADC_FINE_GAIN_P		0x30
#define TLV320_ADC_INPUT_M			0x31

// Page 3
#define TLV320_MCLK_DIVIDER			0x10

// Page 8
#define TLV320_DAC_COEFF_RAM_CTL	0x01

void tlv320_init();
uint8_t tlv320_read_byte(uint8_t address, uint8_t *data);
uint8_t tlv320_write_byte(uint8_t address, uint8_t data);
void tlv320_mute();
void tlv320_unmute();
int8_t tlv320_set_volume(uint8_t volume);
void tlv320_amp_powerdown();
void tlv320_amp_powerup();

#endif /* TLV320_H_ */s
