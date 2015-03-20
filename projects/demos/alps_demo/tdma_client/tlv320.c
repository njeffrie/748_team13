#include <avr/io.h>
#include "i2cmaster.h"
#include "tlv320.h"

void tlv320_init() {
	uint8_t ret, i;

	// Initialize I2C
	i2c_init();

	// Select register page 0
	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x00);
	// Stop PLL
	tlv320_write_byte(TLV320_PLL_P_R, 0x0);
	// Set PLL reference to BCK and codec clock reference
	tlv320_write_byte(TLV320_CLK_IN_MUXING, 0x07);


//	// Set volume L to -10dB
//	tlv320_write_byte(tlv320_REG_VOL_L, 0x64);
//	// Set volume R to -10dB
//	tlv320_write_byte(tlv320_REG_VOL_R, 0x64);
//	//disable auto-mute
//	tlv320_write_byte(tlv320_REG_AUTOMUTE, 0x00);

	//tlv320_write_byte(TLV320_PLL_P_R, 0xB2);
	// Set PLL J = 11
	//tlv320_write_byte(TLV320_PLL_J, 0x0B);
	// Set PLL J = 24
	tlv320_write_byte(TLV320_PLL_J, 0x18);
	// Set PLL P = 1, R = 2 and enable
	//tlv320_write_byte(TLV320_PLL_P_R, 0x92);
	// Set PLL P = 1, R = 1 and enable
	tlv320_write_byte(TLV320_PLL_P_R, 0x91);
	// Set NDAC = 4 and enable
	tlv320_write_byte(TLV320_DAC_NDAC, 0x84);
	// Set MDAC = 4 and enable
	tlv320_write_byte(TLV320_DAC_MDAC, 0x84);
	// Set DOSR = 44
	//tlv320_write_byte(TLV320_DAC_DOSR_MSB, 0x00);
	//tlv320_write_byte(TLV320_DAC_DOSR_LSB, 0x2C);
	// Set DOSR = 48
	tlv320_write_byte(TLV320_DAC_DOSR_MSB, 0x00);
	tlv320_write_byte(TLV320_DAC_DOSR_LSB, 0x30);
	// Set I2S mode to left justified and 16 bit, BCLK input, WCLK input
	tlv320_write_byte(TLV320_CODEC_INTERFACE_1, 0xC0);
	// Select processing block C PRB_P21
	tlv320_write_byte(TLV320_DAC_PROC_BLOCK, 0x0C);
	// Set DAC IDAC_VAL = 132
	//tlv320_write_byte(TLV320_DAC_IDAC, 0x21);
	// Set miniDSP engine interpolation ratio to 11
	//tlv320_write_byte(TLV320_DAC_INTERPOL, 0x0B);


//	// Set fs double speed mode
//	tlv320_write_byte(tlv320_REG_FS_SPEED, 0x01);
//	// Set DSP clock divider to 2
//	tlv320_write_byte(tlv320_REG_DSP_CLKD, 0x01);
//	// Set DAC clock divider to 16
//	tlv320_write_byte(tlv320_REG_DAC_CLKD, 0x0F);
//	// Set CP clock divider to 4
//	tlv320_write_byte(tlv320_REG_CP_CLKD, 0x03);
//	// Set OSR clock divider to 4
//	tlv320_write_byte(tlv320_REG_OSR_CLKD, 0x03);

	// Select page 1
	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x01);
	// De-pop, power-on = 800ms, step time = 4ms
	tlv320_write_byte(TLV320_AMP_POP, 0x4E);
	// 30.5ms speaker power-up wait time
	tlv320_write_byte(TLV320_DRIVER_RAMP_DOWN_CTL, 0x70);

	// Route DAC to mixer
	tlv320_write_byte(TLV320_DAC_MIXER_ROUTING, 0x40);
	// Unmute HP driver, gain = 0dB, high impedance during power down
	tlv320_write_byte(TLV320_HPOUT_DRIVER, 0x06);
	// Unmute Class-D driver, gain = 12dB
	//tlv320_write_byte(TLV320_AMP_OUTPUT_DRIVER, 0x1C);
	tlv320_write_byte(TLV320_AMP_OUTPUT_DRIVER, 0x0C);

	// Enable HP driver
	//tlv320_write_byte(TLV320_HP_DRIVER, 0x84);
	// Enable class-D driver
	tlv320_write_byte(TLV320_AMP, 0xC6);
	// Analog volume control routed to HPOUT driver, -9dB volume gain
	tlv320_write_byte(TLV320_HP_VOL_ANALOG, 0x92);
	// Analog volume control routed to class-D driver, -9dB volume gain
	tlv320_write_byte(TLV320_AMP_VOL_ANALOG, 0x00);
	//tlv320_write_byte(TLV320_AMP_VOL_ANALOG, 0x00);

	// Select register page 8
	//tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x08);
	// Enable adaptive filtering in DAC miniDSP
	//tlv320_write_byte(TLV320_DAC_COEFF_RAM_CTL, 0x04);

//	// Select register page 8
//	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x08);
////////	// Write biquad filter coefficients
//	tlv320_write_byte(0x02, 0x3D);
//	tlv320_write_byte(0x03, 0x8A);
//	tlv320_write_byte(0x04, 0xC2);
//	tlv320_write_byte(0x05, 0x76);
//	tlv320_write_byte(0x06, 0x3D);
//	tlv320_write_byte(0x07, 0x8A);
//	tlv320_write_byte(0x08, 0x2A);
//	tlv320_write_byte(0x09, 0xF1);
//	tlv320_write_byte(0x0A, 0xDF);
//	tlv320_write_byte(0x0B, 0xB7);

	// Wait for > 800ms
	for(i = 0; i < 13; i++)
			nrk_spin_wait_us(65000);
	// Select register page 0
	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x00);
	// DAC digital volume about -20dB
	tlv320_write_byte(TLV320_DAC_VOLUME, 0xF1);
	// agr -> turning it to 11
	//tlv320_write_byte(TLV320_DAC_VOLUME, 0x00);
	// Powerup DAC, data path = left data, soft step disable
	tlv320_write_byte(TLV320_DAC_DATAPATH, 0x94);
	// Unmute DAC
	tlv320_write_byte(TLV320_DAC_VOLUME_MUTE, 0x04);
}

void tlv320_mute(){
//	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x00);
	tlv320_write_byte(TLV320_DAC_VOLUME_MUTE, 0x0C);
}

void tlv320_unmute(){
//	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x00);
	tlv320_write_byte(TLV320_DAC_VOLUME_MUTE, 0x04);
}

void tlv320_amp_powerdown(){
	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x01);
	tlv320_write_byte(TLV320_AMP, 0x06);
}
void tlv320_amp_powerup(){
	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x01);
	tlv320_write_byte(TLV320_AMP, 0xC6);
}


// Set volume 0-100% (0 = 0dB, 117 = -78.3dB)
int8_t tlv320_set_volume(uint8_t volume){
	tlv320_write_byte(TLV320_REG_PAGE_SEL, 0x01);
	if(volume < 117){
		if(tlv320_write_byte(TLV320_AMP_VOL_ANALOG, (uint8_t)((float)117-((float)(117/100)*volume))) == I2C_OK);
			return TLV_OK;
	}
		return TLV_ERROR;
}

uint8_t tlv320_read_byte(uint8_t address, uint8_t *data) {
	uint8_t ret;

	// Set device address and write mode
	ret = i2c_start(I2C_SLA_TLV320 + I2C_WRITE);
	if (ret) {
		i2c_stop();
		return ret;
	}

	// Set register to read from
	ret = i2c_write(address);
	if (ret) {
		i2c_stop();
		return ret;
	}

	// Set device address and read
	ret = i2c_start(I2C_SLA_TLV320 + I2C_READ);
	if (ret) {
		i2c_stop();
		return ret;
	}

	*data = i2c_readNak(); // Receive register data
	i2c_stop();

	return ret;
}

uint8_t tlv320_write_byte(uint8_t address, uint8_t data) {
	uint8_t ret;

	// Set device address and write mode
	ret = i2c_start(I2C_SLA_TLV320 + I2C_WRITE);
	if (ret) {
		i2c_stop();
		return ret;
	}

	// Set register to write to
	ret = i2c_write(address);
	if (ret) {
		i2c_stop();
		return ret;
	}

	// Set device address and write
	ret = i2c_write(data + I2C_WRITE);
	if (ret) {
		i2c_stop();
		return ret;
	}

	i2c_stop();
	return ret;
}

