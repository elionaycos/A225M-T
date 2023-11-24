
#include "himax_ic_core.h"

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
/*	extern char *i_CTPM_firmware_name;*/
#endif
#ifdef HX_AUTO_UPDATE_FW
extern int g_i_FW_VER;
extern int g_i_CFG_VER;
extern int g_i_CID_MAJ;
extern int g_i_CID_MIN;
extern unsigned char *i_CTPM_FW;
extern int g_i_PANEL_VER;
#endif
#ifdef HX_ZERO_FLASH
extern int g_f_0f_updat;
#endif

extern unsigned long FW_VER_MAJ_FLASH_ADDR;
extern unsigned long FW_VER_MIN_FLASH_ADDR;
extern unsigned long CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long CFG_VER_MIN_FLASH_ADDR;
extern unsigned long CID_VER_MAJ_FLASH_ADDR;
extern unsigned long CID_VER_MIN_FLASH_ADDR;
extern unsigned long PANEL_VERSION_ADDR;

extern unsigned long FW_VER_MAJ_FLASH_LENG;
extern unsigned long FW_VER_MIN_FLASH_LENG;
extern unsigned long CFG_VER_MAJ_FLASH_LENG;
extern unsigned long CFG_VER_MIN_FLASH_LENG;
extern unsigned long CID_VER_MAJ_FLASH_LENG;
extern unsigned long CID_VER_MIN_FLASH_LENG;

extern struct himax_ic_data *ic_data;
extern struct himax_ts_data *private_ts;
extern unsigned char IC_CHECKSUM;

#ifdef HX_ESD_RECOVERY
extern int g_zero_event_count;
#endif

#ifdef HX_RST_PIN_FUNC
extern u8 HX_HW_RESET_ACTIVATE;
extern void himax_rst_gpio_set(int pinnum, uint8_t value);
#endif

#if defined(HX_USB_DETECT_GLOBAL)
extern void himax_cable_detect_func(bool force_renew);
#endif

extern int himax_report_data_init(void);
extern int i2c_error_count;

struct himax_core_command_operation *g_core_cmd_op = NULL;
struct ic_operation *pic_op = NULL;
struct fw_operation *pfw_op = NULL;
struct flash_operation *pflash_op = NULL;
struct sram_operation *psram_op = NULL;
struct driver_operation *pdriver_op = NULL;
#ifdef HX_TP_PROC_GUEST_INFO
struct hx_guest_info *g_guest_info_data = NULL;
#endif
#ifdef HX_ZERO_FLASH
struct zf_operation *pzf_op = NULL;
#endif

extern struct himax_core_fp g_core_fp;

#ifdef CORE_IC
/* IC side start*/
static void himax_mcu_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[DATA_LEN_4];

	tmp_data[0] = pic_op->data_conti[0];

	if (himax_bus_write(pic_op->addr_conti[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		input_err(true, &private_ts->client->dev,
				"%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		return;
	}

	tmp_data[0] = (pic_op->data_incr4[0] | auto_add_4_byte);

	if (himax_bus_write(pic_op->addr_incr4[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		input_err(true, &private_ts->client->dev,
				"%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		return;
	}
}

static int himax_mcu_register_read(uint8_t * read_addr, uint32_t read_length,
					uint8_t * read_data, uint8_t cfg_flag)
{
	uint8_t tmp_data[DATA_LEN_4];
	int i = 0;
	int address = 0;

/*input_info(true, &private_ts->client->dev, "%s %s,Entering \n",HIMAX_LOG_TAG, __func__);*/

	if (cfg_flag == false) {
		if (read_length > FLASH_RW_MAX_LEN) {
			input_err(true, &private_ts->client->dev,
					"%s %s: read len over %d!\n", HIMAX_LOG_TAG,
					__func__, FLASH_RW_MAX_LEN);
			return LENGTH_FAIL;
		}

		if (read_length > DATA_LEN_4) {
			g_core_fp.fp_burst_enable(1);
		} else {
			g_core_fp.fp_burst_enable(0);
		}

		address = (read_addr[3] << 24) + (read_addr[2] << 16) +
					(read_addr[1] << 8) + read_addr[0];
		i = address;
		tmp_data[0] = (uint8_t) i;
		tmp_data[1] = (uint8_t) (i >> 8);
		tmp_data[2] = (uint8_t) (i >> 16);
		tmp_data[3] = (uint8_t) (i >> 24);

		if (himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], tmp_data, DATA_LEN_4,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return I2C_FAIL;
		}

		tmp_data[0] = pic_op->data_ahb_access_direction_read[0];

		if (himax_bus_write(pic_op->addr_ahb_access_direction[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return I2C_FAIL;
		}

		if (himax_bus_read(pic_op->addr_ahb_rdata_byte_0[0], read_data, read_length,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return I2C_FAIL;
		}

		if (read_length > DATA_LEN_4) {
			g_core_fp.fp_burst_enable(0);
		}
	} else {
		if (himax_bus_read(read_addr[0], read_data, read_length,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return I2C_FAIL;
		}
	}
	return NO_ERR;
}

/*
static int himax_mcu_flash_write_burst(uint8_t *reg_byte, uint8_t *write_data)
{
	uint8_t data_byte[FLASH_WRITE_BURST_SZ];
	int i = 0, j = 0;
	int data_byte_sz = sizeof(data_byte);

	for (i = 0; i < ADDR_LEN_4; i++) {
		data_byte[i] = reg_byte[i];
	}

	for (j = ADDR_LEN_4; j < data_byte_sz; j++) {
		data_byte[j] = write_data[j - ADDR_LEN_4];
	}

	if (himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], data_byte, data_byte_sz, HIMAX_I2C_RETRY_TIMES) < 0) {
		input_err(true, &private_ts->client->dev, "%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		return I2C_FAIL;
	}
	return NO_ERR;
}*/

static int himax_mcu_flash_write_burst_lenth(uint8_t *reg_byte, uint8_t *write_data, uint32_t length)
{
	uint8_t *data_byte;
	int i = 0, j = 0;

	/* if (length + ADDR_LEN_4 > FLASH_RW_MAX_LEN) {
		input_err(true, &private_ts->client->dev, "%s %s: write len over %d!\n", HIMAX_LOG_TAG, __func__, FLASH_RW_MAX_LEN);
		return;
	} */

	data_byte = kzalloc(sizeof(uint8_t) * (length + 4), GFP_KERNEL);

	for (i = 0; i < ADDR_LEN_4; i++) {
		data_byte[i] = reg_byte[i];
	}

	for (j = ADDR_LEN_4; j < length + ADDR_LEN_4; j++) {
		data_byte[j] = write_data[j - ADDR_LEN_4];
	}

	if (himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], data_byte, length + ADDR_LEN_4,
		HIMAX_I2C_RETRY_TIMES) < 0) {
		input_err(true, &private_ts->client->dev,
				"%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		kfree(data_byte);
		return I2C_FAIL;
	}
	kfree(data_byte);

	return NO_ERR;
}

static int himax_mcu_register_write(uint8_t * write_addr, uint32_t write_length,
					uint8_t * write_data, uint8_t cfg_flag)
{
	int address;

/*input_info(true, &private_ts->client->dev, "%s,Entering \n", __func__);*/
	if (cfg_flag == 0) {
		address = (write_addr[3] << 24) + (write_addr[2] << 16) +
				(write_addr[1] << 8) + write_addr[0];

		if (write_length > DATA_LEN_4) {
			g_core_fp.fp_burst_enable(1);
		} else {
			g_core_fp.fp_burst_enable(0);
		}

		if (himax_mcu_flash_write_burst_lenth
			(write_addr, write_data, write_length) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return I2C_FAIL;
		}

	} else if (cfg_flag == 1) {
		if (himax_bus_write
			(write_addr[0], write_data, write_length,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return I2C_FAIL;
		}
	} else {
		input_err(true, &private_ts->client->dev,
				"%s %s: cfg_flag = %d, value is wrong!\n",
				HIMAX_LOG_TAG, __func__, cfg_flag);
	}

	return NO_ERR;
}

static int himax_write_read_reg(uint8_t * tmp_addr, uint8_t * tmp_data,
				uint8_t hb, uint8_t lb)
{
	int cnt = 0;

	do {
		g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, 0);
		msleep(10);
		g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, 0);
		/* input_info(true, &private_ts->client->dev, "%s:Now tmp_data[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
		__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);*/
	} while ((tmp_data[1] != hb && tmp_data[0] != lb) && cnt++ < 100);

	if (cnt == 99) {
		return HX_RW_REG_FAIL;
	}

	input_info(true, &private_ts->client->dev,
			"%s Now register 0x%08X : high byte=0x%02X,low byte=0x%02X\n",
			HIMAX_LOG_TAG, tmp_addr[3], tmp_data[1], tmp_data[0]);
	return NO_ERR;
}

static void himax_mcu_interface_on(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_data2[DATA_LEN_4];
	int cnt = 0;

	/* Read a dummy register to wake up I2C.*/
	if (himax_bus_read(pic_op->addr_ahb_rdata_byte_0[0], tmp_data, DATA_LEN_4, HIMAX_I2C_RETRY_TIMES) < 0) {	/* to knock I2C */
		input_err(true, &private_ts->client->dev,
			  "%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		return;
	}

	do {
		tmp_data[0] = pic_op->data_conti[0];

		if (himax_bus_write(pic_op->addr_conti[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return;
		}

		tmp_data[0] = pic_op->data_incr4[0];

		if (himax_bus_write(pic_op->addr_incr4[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return;
		}

		/*Check cmd*/
		himax_bus_read(pic_op->addr_conti[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		himax_bus_read(pic_op->addr_incr4[0], tmp_data2, 1,
				HIMAX_I2C_RETRY_TIMES);

		if (tmp_data[0] == pic_op->data_conti[0]
			&& tmp_data2[0] == pic_op->data_incr4[0]) {
			break;
		}

		msleep(1);
	} while (++cnt < 10);

	if (cnt > 0) {
		input_info(true, &private_ts->client->dev,
				"%s %s:Polling burst mode: %d times\n",
				HIMAX_LOG_TAG, __func__, cnt);
	}
}

static bool himax_mcu_wait_wip(int Timing)
{
	uint8_t tmp_data[DATA_LEN_4];
	int retry_cnt = 0;

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
					DATA_LEN_4,
					pflash_op->data_spi200_trans_fmt, 0);
	tmp_data[0] = 0x01;

	do {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
				DATA_LEN_4, pflash_op->data_spi200_trans_ctrl_1, 0);

		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
				DATA_LEN_4, pflash_op->data_spi200_cmd_1, 0);
		tmp_data[0] = tmp_data[1] = tmp_data[2] = tmp_data[3] = 0xFF;
		g_core_fp.fp_register_read(pflash_op->addr_spi200_data, 4,
					tmp_data, 0);

		if ((tmp_data[0] & 0x01) == 0x00) {
			return true;
		}

		retry_cnt++;

		if (tmp_data[0] != 0x00 || tmp_data[1] != 0x00
			|| tmp_data[2] != 0x00 || tmp_data[3] != 0x00)
			input_info(true, &private_ts->client->dev,
					"%s %s:Wait wip retry_cnt:%d, buffer[0]=%d, buffer[1]=%d, buffer[2]=%d, buffer[3]=%d \n",
					HIMAX_LOG_TAG, __func__, retry_cnt,
					tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		if (retry_cnt > 100) {
			input_err(true, &private_ts->client->dev,
					"%s %s: Wait wip error!\n", HIMAX_LOG_TAG, __func__);
			return false;
		}

		msleep(Timing);
	} while ((tmp_data[0] & 0x01) == 0x01);

	return true;
}

static void himax_mcu_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4];
	int retry = 0;
	input_info(true, &private_ts->client->dev, "%s Enter %s \n",
			HIMAX_LOG_TAG, __func__);
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
					sizeof(pfw_op->data_clear),
					pfw_op->data_clear, 0);
	msleep(20);

	if (!FlashMode) {
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_system_reset();
#endif
	} else {
		do {
			g_core_fp.fp_register_write(pfw_op->addr_safe_mode_release_pw,
						sizeof(pfw_op->data_safe_mode_release_pw_active),
						pfw_op->data_safe_mode_release_pw_active, 0);

			g_core_fp.fp_register_read(pfw_op->addr_flag_reset_event,
						DATA_LEN_4, tmp_data, 0);
			input_info(true, &private_ts->client->dev,
					"%s %s:Read status from IC = %X,%X\n",
					HIMAX_LOG_TAG, __func__, tmp_data[0],
					tmp_data[1]);
		} while ((tmp_data[1] != 0x01 || tmp_data[0] != 0x00)
			 && retry++ < 5);

		if (retry >= 5) {
			input_err(true, &private_ts->client->dev,
					"%s %s: Fail:\n", HIMAX_LOG_TAG, __func__);
#ifdef HX_RST_PIN_FUNC
			g_core_fp.fp_ic_reset(false, false);
#else
			g_core_fp.fp_system_reset();
#endif
		} else {
			input_info(true, &private_ts->client->dev,
					"%s %s:OK and Read status from IC = %X,%X\n",
					HIMAX_LOG_TAG, __func__, tmp_data[0],
					tmp_data[1]);
			/* reset code*/
			tmp_data[0] = 0x00;

			if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES) < 0) {
				input_err(true, &private_ts->client->dev,
						"%s %s: i2c access fail!\n",
						HIMAX_LOG_TAG, __func__);
			}

			if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES) < 0) {
				input_err(true, &private_ts->client->dev,
						"%s %s: i2c access fail!\n",
						HIMAX_LOG_TAG, __func__);
			}

			g_core_fp.fp_register_write(pfw_op->addr_safe_mode_release_pw,
						sizeof(pfw_op->data_safe_mode_release_pw_reset),
						pfw_op->data_safe_mode_release_pw_reset, 0);
		}
	}
}

static bool himax_mcu_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_data[DATA_LEN_4];

	do {
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return false;
		}

		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return false;
		}

		g_core_fp.fp_register_read(pic_op->addr_cs_central_state,
						ADDR_LEN_4, tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s: Check enter_save_mode data[0]=%X \n",
				HIMAX_LOG_TAG, __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst,
						DATA_LEN_4, pic_op->data_rst, 0);
			msleep(1);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst,
						DATA_LEN_4, tmp_data, 0);

			g_core_fp.fp_register_write(pic_op->addr_adc_on_rst,
						DATA_LEN_4, pic_op->data_rst, 0);
			msleep(1);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_register_write(pic_op->addr_adc_on_rst,
						DATA_LEN_4, tmp_data, 0);
			return true;
		} else {
			msleep(10);
#ifdef HX_RST_PIN_FUNC
			g_core_fp.fp_ic_reset(false, false);
#else
			g_core_fp.fp_system_reset();
#endif
		}
	} while (cnt++ < 15);

	return false;
}

static void himax_mcu_init_psl(void)
{				/*power saving level */
	g_core_fp.fp_register_write(pic_op->addr_psl, sizeof(pic_op->data_rst),
					pic_op->data_rst, 0);
	input_info(true, &private_ts->client->dev,
			"%s %s: power saving level reset OK!\n", HIMAX_LOG_TAG,
			__func__);
}

static void himax_mcu_resume_ic_action(void)
{
	input_info(true, &private_ts->client->dev, "%s %s: resetting glove_mode,%d\n",
		HIMAX_LOG_TAG, __func__, private_ts->glove_enabled);
	g_core_fp.fp_set_HSEN_enable(private_ts->glove_enabled, false);
}

static void himax_mcu_suspend_ic_action(void)
{
	/* Nothing to do */
}

static void himax_mcu_power_on_init(void)
{
	input_info(true, &private_ts->client->dev, "%s %s:\n", HIMAX_LOG_TAG,
			__func__);
	g_core_fp.fp_touch_information();
/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
					sizeof(pfw_op->data_clear),
					pfw_op->data_clear, 0);
/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);
	g_core_fp.fp_sense_on(0x00);
}

/* IC side end*/
#endif

#ifdef CORE_FW
/* FW side start*/
static void diag_mcu_parse_raw_data(struct himax_report_data *hx_touch_data,
					int mul_num, int self_num, uint8_t diag_cmd,
					int32_t * mutual_data, int32_t * self_data)
{
	int RawDataLen_word;
	int index = 0;
	int temp1, temp2, i;
	int shft_sz = 0;

#ifdef HX_SP_INFO
	shft_sz = hx_touch_data->sp_info_sz;
#else
	shft_sz = HX_EVENT_STCK_HEADR;
#endif

	if (hx_touch_data->hx_rawdata_buf[0] == pfw_op->data_rawdata_ready_lb[0]
		&& hx_touch_data->hx_rawdata_buf[1] ==
		pfw_op->data_rawdata_ready_hb[0]
		&& hx_touch_data->hx_rawdata_buf[2] > 0
		&& hx_touch_data->hx_rawdata_buf[3] == diag_cmd) {
		RawDataLen_word = (hx_touch_data->rawdata_size - HX_SP_INFO_SZ - HX_SP_INFO_CHK_SZ) / 2;
		index = (hx_touch_data->hx_rawdata_buf[2] - 1) * RawDataLen_word;

		for (i = 0; i < RawDataLen_word; i++) {
			temp1 = index + i;

			if (temp1 < mul_num) {	/*mutual */
				mutual_data[index + i] = ((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 + shft_sz + 1]) * 256
										+ hx_touch_data->hx_rawdata_buf[i * 2 + shft_sz];
			} else {	/*self */
				temp1 = i + index;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2) {
					break;
				}

				self_data[i + index - mul_num] = (((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 + shft_sz + 1]) << 8) +
												hx_touch_data->hx_rawdata_buf[i * 2 + shft_sz];
			}
		}
	}
}

static void himax_mcu_system_reset(void)
{
	g_core_fp.fp_register_write(pfw_op->addr_system_reset,
					sizeof(pfw_op->data_system_reset),
					pfw_op->data_system_reset, 0);
}

static int himax_mcu_Calculate_CRC_with_AP(unsigned char *FW_content,
						int CRC_from_FW, int len)
{
	int i, j, length = 0;
	int fw_data;
	int fw_data_2;
	int CRC = 0xFFFFFFFF;
	int PolyNomial = 0x82F63B78;

	input_err(true, &private_ts->client->dev,
				"%s %s: Entering!\n", HIMAX_LOG_TAG, __func__);
	length = len / 4;
	for (i = 0; i < length; i++) {
		fw_data = FW_content[i * 4];
				// + FW_content[i * 4 + 1] << 8
				// + FW_content[i * 4 + 2] << 16
				// + FW_content[i * 4 + 3] << 24;

		for (j = 1; j < 4; j++) {
			fw_data_2 = FW_content[i * 4 + j];
			fw_data += (fw_data_2) << (8 * j);
		}
		// I("times %d: before CRC=0x%08X\n", i, CRC);
		CRC = fw_data ^ CRC;
		// I("times %d: After CRC=0x%08X\n", i, CRC);
		for (j = 0; j < 32; j++) {
			if ((CRC % 2) != 0)
				CRC = ((CRC >> 1) & 0x7FFFFFFF) ^ PolyNomial;
			else
				CRC = (((CRC >> 1) & 0x7FFFFFFF)/*& 0x7FFFFFFF*/);
		}
	}
	return CRC;

}

static uint32_t himax_mcu_check_CRC(uint8_t * start_addr, int reload_length)
{
	uint32_t result = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int cnt = 0, ret = 0;
	int length = reload_length / DATA_LEN_4;

	ret =
		g_core_fp.fp_register_write(pfw_op->addr_reload_addr_from,
					DATA_LEN_4, start_addr, 0);
	if (ret < NO_ERR) {
		input_err(true, &private_ts->client->dev,
				"%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		return HW_CRC_FAIL;
	}

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x99;
	tmp_data[1] = (length >> 8);
	tmp_data[0] = length;
	ret =
		g_core_fp.fp_register_write(pfw_op->addr_reload_addr_cmd_beat,
					DATA_LEN_4, tmp_data, 0);
	if (ret < NO_ERR) {
		input_err(true, &private_ts->client->dev,
				"%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		return HW_CRC_FAIL;
	}
	cnt = 0;

	do {
		ret =
			g_core_fp.fp_register_read(pfw_op->addr_reload_status,
						DATA_LEN_4, tmp_data, 0);
		if (ret < NO_ERR) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return HW_CRC_FAIL;
		}

		if ((tmp_data[0] & 0x01) != 0x01) {
			ret =
				g_core_fp.fp_register_read(pfw_op->
							addr_reload_crc32_result,
							DATA_LEN_4, tmp_data, 0);
			if (ret < NO_ERR) {
				input_err(true, &private_ts->client->dev,
						"%s %s: i2c access fail!\n",
						HIMAX_LOG_TAG, __func__);
				return HW_CRC_FAIL;
			}
			input_info(true, &private_ts->client->dev,
					"%s %s: tmp_data[3]=%X, tmp_data[2]=%X, tmp_data[1]=%X, tmp_data[0]=%X  \n",
					HIMAX_LOG_TAG, __func__, tmp_data[3],
					tmp_data[2], tmp_data[1], tmp_data[0]);
			result =
				((tmp_data[3] << 24) + (tmp_data[2] << 16) +
				(tmp_data[1] << 8) + tmp_data[0]);
			break;
		} else {
			input_info(true, &private_ts->client->dev,
					"%s %s:Waiting for HW ready!\n",
					HIMAX_LOG_TAG, __func__);
			msleep(1);
		}

	} while (cnt++ < 100);

	return result;
}

static void himax_mcu_set_reload_cmd(uint8_t * write_data, int idx,
					uint32_t cmd_from, uint32_t cmd_to,
					uint32_t cmd_beat)
{
	int index = idx * 12;
	int i;

	for (i = 3; i >= 0; i--) {
		write_data[index + i] = (cmd_from >> (8 * i));
		write_data[index + 4 + i] = (cmd_to >> (8 * i));
		write_data[index + 8 + i] = (cmd_beat >> (8 * i));
	}
}

static bool himax_mcu_program_reload(void)
{
	return true;
}

#ifdef HX_P_SENSOR
static int himax_mcu_ulpm_in(void)
{
	uint8_t tmp_data[4];
	int rtimes = 0;

	input_info(true, &private_ts->client->dev, "%s %s:entering\n",
			HIMAX_LOG_TAG, __func__);

/* 34 -> 11 */
	do {
		if (rtimes > 10) {
			input_info(true, &private_ts->client->dev,
					"%s %s:1/7 retry over 10 times!\n",
					HIMAX_LOG_TAG, __func__);
			return false;
		}
		if (himax_bus_write(pfw_op->addr_ulpm_34[0], pfw_op->data_ulpm_11, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi write fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}
		if (himax_bus_read(pfw_op->addr_ulpm_34[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi read fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}

		input_info(true, &private_ts->client->dev,
				"%s %s:retry times %d, addr = 0x34, correct 0x11 = current 0x%2.2X\n",
				HIMAX_LOG_TAG, __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_11[0]);

	rtimes = 0;
	/* 34 -> 11 */
	do {
		if (rtimes > 10) {
			input_info(true, &private_ts->client->dev,
					"%s %s:2/7 retry over 10 times!\n",
					HIMAX_LOG_TAG, __func__);
			return false;
		}
		if (himax_bus_write(pfw_op->addr_ulpm_34[0], pfw_op->data_ulpm_11, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi write fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}
		if (himax_bus_read(pfw_op->addr_ulpm_34[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi read fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}

		input_info(true, &private_ts->client->dev,
				"%s %s:retry times %d, addr = 0x34, correct 0x11 = current 0x%2.2X\n",
				HIMAX_LOG_TAG, __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_11[0]);

	/* 33 -> 33 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			input_info(true, &private_ts->client->dev,
					"%s %s:3/7 retry over 10 times!\n",
					HIMAX_LOG_TAG, __func__);
			return false;
		}
		if (himax_bus_write(pfw_op->addr_ulpm_33[0], pfw_op->data_ulpm_33, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi write fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}
		if (himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi read fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}

		input_info(true, &private_ts->client->dev,
				"%s %s:retry times %d, addr = 0x33, correct 0x33 = current 0x%2.2X\n",
				HIMAX_LOG_TAG, __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_33[0]);

	/* 34 -> 22 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			input_info(true, &private_ts->client->dev,
					"%s %s:4/7 retry over 10 times!\n",
					HIMAX_LOG_TAG, __func__);
			return false;
		}
		if (himax_bus_write(pfw_op->addr_ulpm_34[0], pfw_op->data_ulpm_22, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi write fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}
		if (himax_bus_read(pfw_op->addr_ulpm_34[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi read fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}

		input_info(true, &private_ts->client->dev,
				"%s %s:retry times %d, addr = 0x34, correct 0x22 = current 0x%2.2X\n",
				HIMAX_LOG_TAG, __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_22[0]);

	/* 33 -> AA */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			input_info(true, &private_ts->client->dev,
					"%s %s:5/7 retry over 10 times!\n",
					HIMAX_LOG_TAG, __func__);
			return false;
		}
		if (himax_bus_write(pfw_op->addr_ulpm_33[0], pfw_op->data_ulpm_aa, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi write fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}
		if (himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi read fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}

		input_info(true, &private_ts->client->dev,
				"%s %s:retry times %d, addr = 0x33, correct 0xAA = current 0x%2.2X\n",
				HIMAX_LOG_TAG, __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_aa[0]);

	/* 33 -> 33 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			input_info(true, &private_ts->client->dev,
					"%s %s:6/7 retry over 10 times!\n",
					HIMAX_LOG_TAG, __func__);
			return false;
		}
		if (himax_bus_write(pfw_op->addr_ulpm_33[0], pfw_op->data_ulpm_33, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi write fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}
		if (himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi read fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}

		input_info(true, &private_ts->client->dev,
				"%s %s:retry times %d, addr = 0x33, correct 0x33 = current 0x%2.2X\n",
				HIMAX_LOG_TAG, __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_33[0]);

	/* 33 -> AA */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			input_info(true, &private_ts->client->dev,
					"%s %s:7/7 retry over 10 times!\n",
					HIMAX_LOG_TAG, __func__);
			return false;
		}
		if (himax_bus_write(pfw_op->addr_ulpm_33[0], pfw_op->data_ulpm_aa, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi write fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}
		if (himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_info(true, &private_ts->client->dev,
					"%s %s: spi read fail!\n", HIMAX_LOG_TAG,
					__func__);
			continue;
		}

		input_info(true, &private_ts->client->dev,
				"%s %s:retry times %d, addr = 0x33, correct 0xAA = current 0x%2.2X\n",
				HIMAX_LOG_TAG, __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_aa[0]);

	return true;
	input_info(true, &private_ts->client->dev, "%s %s:END\n", HIMAX_LOG_TAG,
			__func__);
}

static int himax_mcu_black_gest_ctrl(bool enable)
{
	int ret = 0;

	input_info(true, &private_ts->client->dev,
			"%s %s:enable=%d, ts->is_suspended=%d \n", HIMAX_LOG_TAG,
			__func__, enable, private_ts->suspended);
	if (private_ts->shutdown) {
		input_info(true, &private_ts->client->dev, "%s %s:shutdown=%d\n",
			 HIMAX_LOG_TAG, __func__, private_ts->shutdown);
		return -1;
	}

	if (private_ts->suspended) {
		if (enable) {
#ifdef HX_RST_PIN_FUNC
			g_core_fp.fp_ic_reset(false, false);
#else
			input_info(true, &private_ts->client->dev,
					"%s %s: Please enable TP reset define \n",
					HIMAX_LOG_TAG, __func__);
#endif
		} else {
			g_core_fp.fp_ulpm_in();
		}
	} else {
		g_core_fp.fp_sense_on(0);
	}
	return ret;
}
#endif

static void himax_mcu_set_SMWP_enable(uint8_t SMWP_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (SMWP_enable) {
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd,
							tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_smwp_enable,
							DATA_LEN_4, tmp_data, 0);
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd,
							back_data, 4);
		} else {
			himax_in_parse_assign_cmd
				(fw_data_safe_mode_release_pw_reset, tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_smwp_enable,
							DATA_LEN_4, tmp_data, 0);
			himax_in_parse_assign_cmd
				(fw_data_safe_mode_release_pw_reset, back_data, 4);
		}

		g_core_fp.fp_register_read(pfw_op->addr_smwp_enable, DATA_LEN_4,
						tmp_data, 0);
/*input_info(true, &private_ts->client->dev, "%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d \n", __func__, tmp_data[0],SMWP_enable,retry_cnt);*/
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2]
			|| tmp_data[1] != back_data[1] || tmp_data[0] != back_data[0])
			&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_set_HSEN_enable(uint8_t HSEN_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (HSEN_enable) {
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd,
							tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_hsen_enable,
							DATA_LEN_4, tmp_data, 0);
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd,
							back_data, 4);
		} else {
			himax_in_parse_assign_cmd
				(fw_data_safe_mode_release_pw_reset, tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_hsen_enable,
							DATA_LEN_4, tmp_data, 0);
			himax_in_parse_assign_cmd
				(fw_data_safe_mode_release_pw_reset, back_data, 4);
		}

		g_core_fp.fp_register_read(pfw_op->addr_hsen_enable, DATA_LEN_4,
					   tmp_data, 0);
/*input_info(true, &private_ts->client->dev, "%s: tmp_data[0]=%d, HSEN_enable=%d, retry_cnt=%d \n", __func__, tmp_data[0],HSEN_enable,retry_cnt);*/
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2]
			|| tmp_data[1] != back_data[1] || tmp_data[0] != back_data[0])
			&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_usb_detect_set(uint8_t * cable_config)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (cable_config[1] == 0x01) {
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd,
							tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_usb_detect,
							DATA_LEN_4, tmp_data, 0);
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd,
							back_data, 4);
			input_info(true, &private_ts->client->dev,
					"%s %s: USB detect status IN!\n",
					HIMAX_LOG_TAG, __func__);
		} else {
			himax_in_parse_assign_cmd
				(fw_data_safe_mode_release_pw_reset, tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_usb_detect,
							DATA_LEN_4, tmp_data, 0);
			himax_in_parse_assign_cmd
				(fw_data_safe_mode_release_pw_reset, back_data, 4);
			input_info(true, &private_ts->client->dev,
					"%s %s: USB detect status OUT!\n",
					HIMAX_LOG_TAG, __func__);
		}

		g_core_fp.fp_register_read(pfw_op->addr_usb_detect, DATA_LEN_4,
						tmp_data, 0);
/*input_info(true, &private_ts->client->dev, "%s: tmp_data[0]=%d, USB detect=%d, retry_cnt=%d \n", __func__, tmp_data[0],cable_config[1] ,retry_cnt);*/
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2]
			|| tmp_data[1] != back_data[1] || tmp_data[0] != back_data[0])
			&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_diag_register_set(uint8_t diag_command,
					uint8_t storage_type)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t cnt = 50;

	if (diag_command > 0 && storage_type % 8 > 0)
		tmp_data[0] = diag_command + 0x08;
	else
		tmp_data[0] = diag_command;
	input_info(true, &private_ts->client->dev,
			"%s diag_command = %d, tmp_data[0] = %X\n", HIMAX_LOG_TAG,
			diag_command, tmp_data[0]);
	g_core_fp.fp_interface_on();
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	do {
		g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
						DATA_LEN_4, tmp_data, 0);
		g_core_fp.fp_register_read(pfw_op->addr_raw_out_sel, DATA_LEN_4,
						back_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s: back_data[3]=0x%02X,back_data[2]=0x%02X,back_data[1]=0x%02X,back_data[0]=0x%02X!\n",
				HIMAX_LOG_TAG, __func__, back_data[3], back_data[2],
				back_data[1], back_data[0]);
		cnt--;
	} while (tmp_data[0] != back_data[0] && cnt > 0);
}

static int himax_mcu_chip_self_test(void)
{
	uint8_t tmp_data[FLASH_WRITE_BURST_SZ];
	uint8_t self_test_info[20];
	int pf_value = 0x00;
	uint8_t test_result_id = 0;
	int i;
	memset(tmp_data, 0x00, sizeof(tmp_data));
	g_core_fp.fp_interface_on();
	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_burst_enable(1);
	g_core_fp.fp_register_write(pfw_op->addr_selftest_addr_en, DATA_LEN_4,
					pfw_op->data_selftest_request, 0);
/*Set criteria 0x10007F1C [0,1]=aa/up,down=, [2-3]=key/up,down, [4-5]=avg/up,down*/
	tmp_data[0] = pfw_op->data_criteria_aa_top[0];
	tmp_data[1] = pfw_op->data_criteria_aa_bot[0];
	tmp_data[2] = pfw_op->data_criteria_key_top[0];
	tmp_data[3] = pfw_op->data_criteria_key_bot[0];
	tmp_data[4] = pfw_op->data_criteria_avg_top[0];
	tmp_data[5] = pfw_op->data_criteria_avg_bot[0];
	tmp_data[6] = 0x00;
	tmp_data[7] = 0x00;
	g_core_fp.fp_register_write(pfw_op->addr_criteria_addr,
					FLASH_WRITE_BURST_SZ, tmp_data, 0);
	g_core_fp.fp_register_write(pfw_op->addr_set_frame_addr, DATA_LEN_4,
					pfw_op->data_set_frame, 0);
	/*Disable IDLE Mode*/
	g_core_fp.fp_idle_mode(1);
	/*Diable Flash Reload*/
	g_core_fp.fp_reload_disable(1);
	/*start selftest // leave safe mode*/
	g_core_fp.fp_sense_on(0x01);

	/*Hand shaking*/
	for (i = 0; i < 1000; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_selftest_addr_en, 4,
						tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X,tmp_data[3] = 0x%02X, cnt=%d\n",
				HIMAX_LOG_TAG, __func__, tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3], i);
		msleep(10);

		if (tmp_data[1] == pfw_op->data_selftest_ack_hb[0]
			&& tmp_data[0] == pfw_op->data_selftest_ack_lb[0]) {
			input_info(true, &private_ts->client->dev,
					"%s %s Data ready goto moving data\n",
					HIMAX_LOG_TAG, __func__);
			break;
		}
	}

	g_core_fp.fp_sense_off(true);
	msleep(20);
	/*=====================================
	Read test result ==> bit[2][1][0] = [key][AA][avg] => 0xF = PASS
	=====================================*/
	g_core_fp.fp_register_read(pfw_op->addr_selftest_result_addr, 20,
					self_test_info, 0);
	test_result_id = self_test_info[0];
	input_info(true, &private_ts->client->dev,
			"%s%s: check test result, test_result_id=%x, test_result=%x\n",
			HIMAX_LOG_TAG, __func__, test_result_id, self_test_info[0]);
	input_info(true, &private_ts->client->dev, "%s raw top 1 = %d\n",
			HIMAX_LOG_TAG, self_test_info[3] * 256 + self_test_info[2]);
	input_info(true, &private_ts->client->dev, "%s raw top 2 = %d\n",
			HIMAX_LOG_TAG, self_test_info[5] * 256 + self_test_info[4]);
	input_info(true, &private_ts->client->dev, "%s raw top 3 = %d\n",
			HIMAX_LOG_TAG, self_test_info[7] * 256 + self_test_info[6]);
	input_info(true, &private_ts->client->dev, "%s raw last 1 = %d\n",
			HIMAX_LOG_TAG, self_test_info[9] * 256 + self_test_info[8]);
	input_info(true, &private_ts->client->dev, "%s raw last 2 = %d\n",
			HIMAX_LOG_TAG,
			self_test_info[11] * 256 + self_test_info[10]);
	input_info(true, &private_ts->client->dev, "%s raw last 3 = %d\n",
			HIMAX_LOG_TAG,
			self_test_info[13] * 256 + self_test_info[12]);
	input_info(true, &private_ts->client->dev, "%s raw key 1 = %d\n",
			HIMAX_LOG_TAG,
			self_test_info[15] * 256 + self_test_info[14]);
	input_info(true, &private_ts->client->dev, "%s raw key 2 = %d\n",
			HIMAX_LOG_TAG,
			self_test_info[17] * 256 + self_test_info[16]);
	input_info(true, &private_ts->client->dev, "%s raw key 3 = %d\n",
			HIMAX_LOG_TAG,
			self_test_info[19] * 256 + self_test_info[18]);

	if (test_result_id == pfw_op->data_selftest_pass[0]) {
		input_info(true, &private_ts->client->dev,
				"[Himax]: self-test pass\n");
		pf_value = 0x0;
	} else {
		input_err(true, &private_ts->client->dev,
				"[Himax]: self-test fail\n");
		pf_value = 0x1;
	}

	/*Enable IDLE Mode*/
	g_core_fp.fp_idle_mode(0);
#ifndef HX_ZERO_FLASH
	/* Enable Flash Reload //recovery*/
	g_core_fp.fp_reload_disable(0);
#endif
	g_core_fp.fp_sense_on(0x00);
	msleep(120);
	return pf_value;
}

static void himax_mcu_idle_mode(int disable)
{
	int retry = 20;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t switch_cmd = 0x00;
	input_info(true, &private_ts->client->dev, "%s %s:entering\n",
			HIMAX_LOG_TAG, __func__);

	do {
		input_info(true, &private_ts->client->dev,
				"%s %s,now %d times!\n", HIMAX_LOG_TAG, __func__,
				retry);
		g_core_fp.fp_register_read(pfw_op->addr_fw_mode_status,
						DATA_LEN_4, tmp_data, 0);

		if (disable) {
			switch_cmd = tmp_data[0] & 0xF7;
			tmp_data[0] = switch_cmd; /* set third bit to 0*/
		} else {
			switch_cmd = tmp_data[0] | 0x08;
			tmp_data[0] = switch_cmd; /* set third bit to 1*/
		}

		g_core_fp.fp_register_write(pfw_op->addr_fw_mode_status,
						DATA_LEN_4, tmp_data, 0);
		g_core_fp.fp_register_read(pfw_op->addr_fw_mode_status,
						DATA_LEN_4, tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s:After turn ON/OFF IDLE Mode [0] = 0x%02X,[1] = 0x%02X,[2] = 0x%02X,[3] = 0x%02X\n",
				HIMAX_LOG_TAG, __func__, tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);
		retry--;
		msleep(10);
	} while ((tmp_data[0] != switch_cmd) && retry > 0);

	input_info(true, &private_ts->client->dev, "%s: setting OK!\n",
			__func__);
}

static void himax_mcu_reload_disable(int disable)
{
	input_info(true, &private_ts->client->dev, "%s %s:entering\n",
			HIMAX_LOG_TAG, __func__);

	if (disable) {		/*reload disable */
		g_core_fp.fp_register_write(pdriver_op->addr_fw_define_flash_reload,
					DATA_LEN_4,
					pdriver_op->data_fw_define_flash_reload_dis, 0);
	} else {		/*reload enable */
		g_core_fp.fp_register_write(pdriver_op->addr_fw_define_flash_reload,
					DATA_LEN_4,
					pdriver_op->data_fw_define_flash_reload_en, 0);
	}

	input_info(true, &private_ts->client->dev, "%s %s: setting OK!\n",
			HIMAX_LOG_TAG, __func__);
}

static bool himax_mcu_check_chip_version(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t ret_data = false;
	int i = 0;

	for (i = 0; i < 5; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_icid_addr, DATA_LEN_4,
						tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s:Read driver IC ID = %X,%X,%X\n",
				HIMAX_LOG_TAG, __func__, tmp_data[3], tmp_data[2],
				tmp_data[1]);

		if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x10)
			&& (tmp_data[1] == 0x2a)) {
			strlcpy(private_ts->chip_name, HX_83102A_SERIES_PWON,
				30);
			ret_data = true;
			break;
		} else {
			ret_data = false;
			input_err(true, &private_ts->client->dev,
					"%s %s:Read driver ID register Fail:\n",
					HIMAX_LOG_TAG, __func__);
		}
	}

	return ret_data;
}

static int himax_mcu_read_ic_trigger_type(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	int trigger_type = false;
	g_core_fp.fp_register_read(pfw_op->addr_trigger_addr, DATA_LEN_4,
					tmp_data, 0);

	if ((tmp_data[1] & 0x01) == 1) {
		trigger_type = true;
	}

	return trigger_type;
}

static int himax_mcu_read_i2c_status(void)
{
	return i2c_error_count;
}

static void himax_mcu_read_FW_ver(void)
{
	uint8_t data[12];
	uint8_t data_2[DATA_LEN_4];
	int retry = 200;
	int reload_status = 0;

	g_core_fp.fp_sense_on(0x00);

	while (reload_status == 0) {
		g_core_fp.fp_register_read(pdriver_op->
						addr_fw_define_flash_reload,
						DATA_LEN_4, data, 0);
		g_core_fp.fp_register_read(pdriver_op->
						addr_fw_define_2nd_flash_reload,
						DATA_LEN_4, data_2, 0);

		if ((data[1] == 0x3A && data[0] == 0xA3)
			|| (data_2[1] == 0x72 && data_2[0] == 0xC0)) {
			input_info(true, &private_ts->client->dev,
					"%s %s:reload OK! \n", HIMAX_LOG_TAG,
					__func__);
			reload_status = 1;
			break;
		} else if (retry == 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s:reload 20 times! fail \n",
					HIMAX_LOG_TAG, __func__);
			input_err(true, &private_ts->client->dev,
					"%s %s:Maybe NOT have FW in chipset \n",
					HIMAX_LOG_TAG, __func__);
			input_err(true, &private_ts->client->dev,
					"%s %s:Maybe Wrong FW in chipset \n",
					HIMAX_LOG_TAG, __func__);
			ic_data->vendor_panel_ver = 0;
			ic_data->vendor_fw_ver = 0;
			ic_data->vendor_config_ver = 0;
			ic_data->vendor_touch_cfg_ver = 0;
			ic_data->vendor_display_cfg_ver = 0;
			ic_data->vendor_cid_maj_ver = 0;
			ic_data->vendor_cid_min_ver = 0;
			return;
		} else {
			retry--;
			msleep(10);
			if (retry % 10 == 0)
				input_info(true, &private_ts->client->dev,
						"%s %s reload fail ,delay 10ms retry=%d\n",
						HIMAX_LOG_TAG, __func__, retry);
		}
	}

	input_info(true, &private_ts->client->dev,
			"%s %s : data[0]=0x%2.2X,data[1]=0x%2.2X,data_2[0]=0x%2.2X,data_2[1]=0x%2.2X\n",
			HIMAX_LOG_TAG, __func__, data[0], data[1], data_2[0],
			data_2[1]);
	input_info(true, &private_ts->client->dev, "reload_status=%d\n",
			reload_status);
	/*=====================================
	Read FW version
	=====================================*/
	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_register_read(pfw_op->addr_fw_ver_addr, DATA_LEN_4, data,
					0);
	ic_data->vendor_panel_ver = data[0];
	ic_data->vendor_fw_ver = data[1] << 8 | data[2];
	input_info(true, &private_ts->client->dev, "%s PANEL_VER : %X \n",
			HIMAX_LOG_TAG, ic_data->vendor_panel_ver);
	input_info(true, &private_ts->client->dev, "%s FW_VER : %X \n",
			HIMAX_LOG_TAG, ic_data->vendor_fw_ver);
	g_core_fp.fp_register_read(pfw_op->addr_fw_cfg_addr, DATA_LEN_4, data,
					0);
	ic_data->vendor_config_ver = data[2] << 8 | data[3];
	/*input_info(true, &private_ts->client->dev, "CFG_VER : %X \n",ic_data->vendor_config_ver);*/
	ic_data->vendor_touch_cfg_ver = data[2];
	input_info(true, &private_ts->client->dev, "%s TOUCH_VER : %X \n",
			HIMAX_LOG_TAG, ic_data->vendor_touch_cfg_ver);
	ic_data->vendor_display_cfg_ver = data[3];
	input_info(true, &private_ts->client->dev, "%s DISPLAY_VER : %X \n",
			HIMAX_LOG_TAG, ic_data->vendor_display_cfg_ver);
	g_core_fp.fp_register_read(pfw_op->addr_fw_vendor_addr, DATA_LEN_4,
					data, 0);
	ic_data->vendor_cid_maj_ver = data[2];
	ic_data->vendor_cid_min_ver = data[3];
	input_info(true, &private_ts->client->dev, "%s CID_VER : %X \n",
			HIMAX_LOG_TAG,
			(ic_data->vendor_cid_maj_ver << 8 | ic_data->
			vendor_cid_min_ver));
	g_core_fp.fp_register_read(pfw_op->addr_cus_info, 12, data, 0);
	memcpy(ic_data->vendor_cus_info, data, 12);
	input_info(true, &private_ts->client->dev, "%s Cusomer ID = %s \n",
			HIMAX_LOG_TAG, ic_data->vendor_cus_info);
	g_core_fp.fp_register_read(pfw_op->addr_proj_info, 12, data, 0);
	memcpy(ic_data->vendor_proj_info, data, 12);
	input_info(true, &private_ts->client->dev, "%s Project ID = %s \n",
			HIMAX_LOG_TAG, ic_data->vendor_proj_info);
}

static bool himax_mcu_read_event_stack(uint8_t * buf, uint8_t length)
{
	uint8_t cmd[DATA_LEN_4];
	/*  AHB_I2C Burst Read Off */
	cmd[0] = pfw_op->data_ahb_dis[0];

	if (himax_bus_write
		(pfw_op->addr_ahb_addr[0], cmd, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		input_err(true, &private_ts->client->dev,
				"%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		return 0;
	}

	himax_bus_read(pfw_op->addr_event_addr[0], buf, length,
			HIMAX_I2C_RETRY_TIMES);
	/*  AHB_I2C Burst Read On */
	cmd[0] = pfw_op->data_ahb_en[0];

	if (himax_bus_write
		(pfw_op->addr_ahb_addr[0], cmd, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		input_err(true, &private_ts->client->dev,
				"%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
		return 0;
	}

	return 1;
}

static void himax_mcu_return_event_stack(void)
{
	int retry = 20, i;
	uint8_t tmp_data[DATA_LEN_4];
	input_info(true, &private_ts->client->dev, "%s %s:entering\n",
			HIMAX_LOG_TAG, __func__);

	do {
		input_info(true, &private_ts->client->dev, "%s now %d times!\n",
				HIMAX_LOG_TAG, retry);

		for (i = 0; i < DATA_LEN_4; i++) {
			tmp_data[i] = psram_op->addr_rawdata_end[i];
		}

		g_core_fp.fp_register_write(psram_op->addr_rawdata_addr,
						DATA_LEN_4, tmp_data, 0);
		g_core_fp.fp_register_read(psram_op->addr_rawdata_addr,
						DATA_LEN_4, tmp_data, 0);
		retry--;
		msleep(10);
	} while ((tmp_data[1] != psram_op->addr_rawdata_end[1]
			&& tmp_data[0] != psram_op->addr_rawdata_end[0])
			&& retry > 0);

	input_info(true, &private_ts->client->dev, "%s %s: End of setting!\n",
			HIMAX_LOG_TAG, __func__);
}

static bool himax_mcu_calculateChecksum(bool change_iref)
{
	uint8_t CRC_result = 0, i;
	uint8_t tmp_data[DATA_LEN_4];

	for (i = 0; i < DATA_LEN_4; i++) {
		tmp_data[i] = psram_op->addr_rawdata_end[i];
	}
	
	if (strcmp(private_ts->chip_name, HX_83102D_SERIES_PWON) == 0) {
		I("Now size=%d\n", FW_SIZE_128k);
		CRC_result = g_core_fp.fp_check_CRC(tmp_data, FW_SIZE_128k);
	} else {
		I("Now size=%d\n", FW_SIZE_64k);
		CRC_result = g_core_fp.fp_check_CRC(tmp_data, FW_SIZE_64k);
	}
	msleep(50);

	if (CRC_result != 0) {
		input_info(true, &private_ts->client->dev,
				"%s %s: CRC Fail=%d\n", HIMAX_LOG_TAG, __func__,
				CRC_result);
	}
	return (CRC_result == 0) ? true : false;
}

static int himax_mcu_read_FW_status(uint8_t * state_addr, uint8_t * tmp_addr)
{
	uint8_t i;
	uint8_t req_size = 0;
	uint8_t status_addr[DATA_LEN_4];
	uint8_t cmd_addr[DATA_LEN_4];

	if (state_addr[0] == 0x01) {
		state_addr[1] = 0x04;

		for (i = 0; i < DATA_LEN_4; i++) {
			state_addr[i + 2] = pfw_op->addr_fw_dbg_msg_addr[i];
			status_addr[i] = pfw_op->addr_fw_dbg_msg_addr[i];
		}

		req_size = 0x04;
		g_core_fp.fp_register_read(status_addr, req_size, tmp_addr, 0);
	} else if (state_addr[0] == 0x02) {
		state_addr[1] = 0x30;

		for (i = 0; i < DATA_LEN_4; i++) {
			state_addr[i + 2] = pfw_op->addr_fw_dbg_msg_addr[i];
			cmd_addr[i] = pfw_op->addr_fw_dbg_msg_addr[i];
		}

		req_size = 0x30;
		g_core_fp.fp_register_read(cmd_addr, req_size, tmp_addr, 0);
	}

	return NO_ERR;
}

static void himax_mcu_irq_switch(int switch_on)
{
	if (switch_on) {
		if (private_ts->use_irq) {
			himax_int_enable(switch_on);
		} else {
			hrtimer_start(&private_ts->timer, ktime_set(1, 0),
					HRTIMER_MODE_REL);
		}
	} else {
		if (private_ts->use_irq) {
			himax_int_enable(switch_on);
		} else {
			hrtimer_cancel(&private_ts->timer);
			cancel_work_sync(&private_ts->work);
		}
	}
}

static int himax_mcu_assign_sorting_mode(uint8_t * tmp_data)
{

	input_info(true, &private_ts->client->dev,
			"%s %s:Now tmp_data[3]=0x%02X,tmp_data[2]=0x%02X,tmp_data[1]=0x%02X,tmp_data[0]=0x%02X\n",
			HIMAX_LOG_TAG, __func__, tmp_data[3], tmp_data[2],
			tmp_data[1], tmp_data[0]);
	g_core_fp.fp_register_write(pfw_op->addr_sorting_mode_en, DATA_LEN_4,
					tmp_data, 0);

	return NO_ERR;
}

static int himax_mcu_check_sorting_mode(uint8_t * tmp_data)
{

	g_core_fp.fp_register_read(pfw_op->addr_sorting_mode_en, DATA_LEN_4,
					tmp_data, 0);
	input_info(true, &private_ts->client->dev,
			"%s %s: tmp_data[0]=%x,tmp_data[1]=%x\n", HIMAX_LOG_TAG,
			__func__, tmp_data[0], tmp_data[1]);

	return NO_ERR;
}

static int himax_mcu_switch_mode(int mode)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t mode_wirte_cmd;
	uint8_t mode_read_cmd;
	int result = -1;
	int retry = 200;
	input_info(true, &private_ts->client->dev, "%s %s: Entering\n",
			HIMAX_LOG_TAG, __func__);

	if (mode == 0) {	/* normal mode */
		mode_wirte_cmd = pfw_op->data_normal_cmd[0];
		mode_read_cmd = pfw_op->data_normal_status[0];
	} else {		/* sorting mode */
		mode_wirte_cmd = pfw_op->data_sorting_cmd[0];
		mode_read_cmd = pfw_op->data_sorting_status[0];
	}

	g_core_fp.fp_sense_off(true);
	/*g_core_fp.fp_interface_on();*/
	/* clean up FW status */
	g_core_fp.fp_register_write(psram_op->addr_rawdata_addr, DATA_LEN_4,
					psram_op->addr_rawdata_end, 0);
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = mode_wirte_cmd;
	tmp_data[0] = mode_wirte_cmd;
	g_core_fp.fp_assign_sorting_mode(tmp_data);
	g_core_fp.fp_idle_mode(1);
	g_core_fp.fp_reload_disable(1);

	/* To stable the sorting*/
	if (mode) {
		g_core_fp.fp_register_write(pdriver_op->
						addr_fw_define_rxnum_txnum_maxpt,
						DATA_LEN_4,
						pdriver_op->
						data_fw_define_rxnum_txnum_maxpt_sorting,
						0);
	} else {
		g_core_fp.fp_register_write(pfw_op->addr_set_frame_addr,
						DATA_LEN_4, pfw_op->data_set_frame, 0);
		g_core_fp.fp_register_write(pdriver_op->
						addr_fw_define_rxnum_txnum_maxpt,
						DATA_LEN_4,
						pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal,
						0);
	}

	g_core_fp.fp_sense_on(0x01);

	while (retry != 0) {
		input_info(true, &private_ts->client->dev, "%s %s: Read [%d]\n",
				HIMAX_LOG_TAG, __func__, retry);
		g_core_fp.fp_check_sorting_mode(tmp_data);
		msleep(100);
		input_info(true, &private_ts->client->dev,
				"%s mode_read_cmd(0)=0x%2.2X,mode_read_cmd(1)=0x%2.2X\n",
				HIMAX_LOG_TAG, tmp_data[0], tmp_data[1]);

		if (tmp_data[0] == mode_read_cmd
			&& tmp_data[1] == mode_read_cmd) {
			input_info(true, &private_ts->client->dev,
					"%s Read OK!\n", HIMAX_LOG_TAG);
			result = 0;
			break;
		}

		g_core_fp.fp_register_read(pfw_op->addr_chk_fw_status,
						DATA_LEN_4, tmp_data, 0);

		if (tmp_data[0] == 0x00 && tmp_data[1] == 0x00
			&& tmp_data[2] == 0x00 && tmp_data[3] == 0x00) {
			input_err(true, &private_ts->client->dev,
					"%s %s: FW Stop!\n", HIMAX_LOG_TAG, __func__);
			break;
		}

		retry--;
	}

	if (result == 0) {
		if (mode == 0) {	/*normal mode */
			return HX_NORMAL_MODE;
		} else {	/*sorting mode */
			return HX_SORTING_MODE;
		}
	} else {		/*change mode fail */
		return HX_CHANGE_MODE_FAIL;
	}
}

static int himax_mcu_get_max_dc(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	int dc_max = 0;
	g_core_fp.fp_register_read(pfw_op->addr_max_dc, DATA_LEN_4, tmp_data,
					0);
	input_info(true, &private_ts->client->dev,
			"%s %s: tmp_data[0]=%x,tmp_data[1]=%x\n", HIMAX_LOG_TAG,
			__func__, tmp_data[0], tmp_data[1]);
	dc_max = tmp_data[1] << 8 | tmp_data[0];
	input_info(true, &private_ts->client->dev, "%s %s: dc max = %d\n",
			HIMAX_LOG_TAG, __func__, dc_max);
	return dc_max;
}

static uint8_t himax_mcu_read_DD_status(uint8_t * cmd_set, uint8_t * tmp_data)
{
	int cnt = 0;
	uint8_t req_size = cmd_set[0];
	cmd_set[3] = pfw_op->data_dd_request[0];
	g_core_fp.fp_register_write(pfw_op->addr_dd_handshak_addr, DATA_LEN_4,
					cmd_set, 0);
	input_info(true, &private_ts->client->dev,
			"%s %s: cmd_set[0] = 0x%02X,cmd_set[1] = 0x%02X,cmd_set[2] = 0x%02X,cmd_set[3] = 0x%02X\n",
			HIMAX_LOG_TAG, __func__, cmd_set[0], cmd_set[1], cmd_set[2],
			cmd_set[3]);

	/* Doing hand shaking 0xAA -> 0xBB */
	for (cnt = 0; cnt < 100; cnt++) {
		g_core_fp.fp_register_read(pfw_op->addr_dd_handshak_addr,
						DATA_LEN_4, tmp_data, 0);
		msleep(10);

		if (tmp_data[3] == pfw_op->data_dd_ack[0]) {
			input_info(true, &private_ts->client->dev,
					"%s %s Data ready goto moving data\n",
					HIMAX_LOG_TAG, __func__);
			break;
		} else {
			if (cnt >= 99) {
				input_info(true, &private_ts->client->dev,
						"%s %s Data not ready in FW \n",
						HIMAX_LOG_TAG, __func__);
				return FW_NOT_READY;
			}
		}
	}

	g_core_fp.fp_register_read(pfw_op->addr_dd_data_addr, req_size,
					tmp_data, 0);
	return NO_ERR;
}

int hx_mcu_set_edge_border(int set_val)
{
	int ret = NO_ERR;
	int retry = 10;
	uint8_t tmp_data[DATA_LEN_4] = { 0 };

	input_info(true, &private_ts->client->dev, "%s %s:Now set_val=%d\n",
			HIMAX_LOG_TAG, __func__, set_val);
	do {
		g_core_fp.fp_register_read(pfw_op->addr_edge_border, DATA_LEN_4,
						tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s:Now [0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
				__func__, tmp_data[0], tmp_data[1], tmp_data[2],
				tmp_data[3]);

		input_info(true, &private_ts->client->dev,
				"%s Now write value\n", HIMAX_LOG_TAG);

		tmp_data[0] = set_val;
		g_core_fp.fp_register_write(pfw_op->addr_edge_border,
						DATA_LEN_4, tmp_data, false);
		input_info(true, &private_ts->client->dev,
				"%s 1. After write value\n", HIMAX_LOG_TAG);
		g_core_fp.fp_register_read(pfw_op->addr_edge_border, DATA_LEN_4,
						tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s:Now [0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
				HIMAX_LOG_TAG, __func__, tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);

		input_info(true, &private_ts->client->dev,
				"%s 2. After write value\n", HIMAX_LOG_TAG);
		g_core_fp.fp_register_read(pfw_op->addr_edge_border, DATA_LEN_4,
						tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s:Now [0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
				HIMAX_LOG_TAG, __func__, tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);
		input_info(true, &private_ts->client->dev,
				"%s Now_val=%d, retry times=%d\n", HIMAX_LOG_TAG,
				tmp_data[0], retry);
	} while (tmp_data[0] != set_val && retry-- > 0);
	if (retry <= 0 && tmp_data[0] != set_val) {
		input_info(true, &private_ts->client->dev,
				"%s %s:Fail to set value=%d!\n", HIMAX_LOG_TAG,
				__func__, tmp_data[0]);
		ret = -1;
	}
	return ret;
}

int hx_mcu_set_cal_switch(int set_val)
{
	int ret = NO_ERR;
	int retry = 10;
	int now_val = 0;
	uint8_t tmp_data[DATA_LEN_4] = { 0 };

	input_info(true, &private_ts->client->dev, "%s %s:Now set_val=%d\n",
		   HIMAX_LOG_TAG, __func__, set_val);
	do {
		g_core_fp.fp_register_read(pfw_op->addr_cal, DATA_LEN_4,
						tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s:Now [0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
				HIMAX_LOG_TAG, __func__, tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);

		input_info(true, &private_ts->client->dev,
				"%s before write tmp_data[2]=0x%02X\n",
				HIMAX_LOG_TAG, tmp_data[0]);
		input_info(true, &private_ts->client->dev,
				"%s Now write value\n", HIMAX_LOG_TAG);
		input_info(true, &private_ts->client->dev, "%s Sense off\n",
				HIMAX_LOG_TAG);
		g_core_fp.fp_sense_off(true);
		input_info(true, &private_ts->client->dev,
				"%s Disable reload!\n", HIMAX_LOG_TAG);
		g_core_fp.fp_reload_disable(1);
/*clear bit*/
		tmp_data[0] = tmp_data[0] & 0xFB;	/* force value of 2th bit to zero */
		input_info(true, &private_ts->client->dev,
				"%s After clear bit tmp_data[0]=0x%02X\n",
				HIMAX_LOG_TAG, tmp_data[0]);

		tmp_data[0] = tmp_data[0] | (set_val << 2);	/* change the value of 2th bit */
		input_info(true, &private_ts->client->dev,
				"%s After assign tmp_data[0]=0x%02X\n",
				HIMAX_LOG_TAG, tmp_data[0]);
		g_core_fp.fp_register_write(pfw_op->addr_cal, DATA_LEN_4,
						tmp_data, false);

		input_info(true, &private_ts->client->dev,
				"%s After write value\n", HIMAX_LOG_TAG);
		g_core_fp.fp_register_read(pfw_op->addr_cal, DATA_LEN_4,
						tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s:Now [0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
				HIMAX_LOG_TAG, __func__, tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);

		input_info(true, &private_ts->client->dev, "%s Sense on\n",
				HIMAX_LOG_TAG);
		g_core_fp.fp_sense_on(0x00);

		g_core_fp.fp_register_read(pfw_op->addr_cal, DATA_LEN_4,
						tmp_data, 0);
		input_info(true, &private_ts->client->dev,
				"%s %s:Now [0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
				HIMAX_LOG_TAG, __func__, tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);
		now_val = ((tmp_data[0] & 0x04) >> 2);	/* get the 2th bit value of byte[0] */
		input_info(true, &private_ts->client->dev,
				"%s %s:Now_val=%d, retry times=%d\n", HIMAX_LOG_TAG,
				__func__, now_val, retry);
	} while (now_val != set_val && retry-- > 0);
	return ret;
}

int hx_mcu_get_rport_thrsh(void)
{
	int ret = NO_ERR;
	uint8_t tmp_data[DATA_LEN_4] = { 0 };
	input_info(true, &private_ts->client->dev, "%s %s:Entering\n",
			HIMAX_LOG_TAG, __func__);
	g_core_fp.fp_register_read(pfw_op->addr_rport_thrsh, DATA_LEN_4,
					tmp_data, 0);
	input_info(true, &private_ts->client->dev,
			"%s %s:Now [0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
			HIMAX_LOG_TAG, __func__, tmp_data[0], tmp_data[1],
			tmp_data[2], tmp_data[3]);
	ret = tmp_data[3];
	if (ret == 0x00) {
		input_info(true, &private_ts->client->dev,
				"%s Now value=%d, please check FW support this feature or not!\n",
				HIMAX_LOG_TAG, ret);
		ret = -1;
	}
	input_info(true, &private_ts->client->dev, "%s %s:End\n", HIMAX_LOG_TAG,
			__func__);
	return ret;
}

/* FW side end*/
#endif

#ifdef CORE_FLASH
/* FLASH side start*/
static int g_flash_read_ongoing = 0;	/* 0 stop //1 ongoing */
static int himax_flash_read_get_status(void)
{
	return g_flash_read_ongoing;
}

static void himax_flash_read_set_status(int setting)
{
	g_flash_read_ongoing = setting;
}

int hx_write_4k_flash_flow(uint32_t start_addr, uint8_t * write_data,
				uint32_t write_len)
{
	input_info(true, &private_ts->client->dev, "%s %s:Entering\n",
			HIMAX_LOG_TAG, __func__);

	write_len = write_len > 0x1000 ? 0x1000 : write_len;
	input_info(true, &private_ts->client->dev, "%s %s: Now write_len=%d\n",
			HIMAX_LOG_TAG, __func__, write_len);
	if (!himax_flash_read_get_status()) {
		himax_flash_read_set_status(1);

		himax_int_enable(0);
		g_core_fp.fp_sense_off(true);
		//g_core_fp.fp_burst_enable(0);
		g_core_fp.fp_block_erase(start_addr, write_len);
		g_core_fp.fp_flash_programming_sz(start_addr, write_data,
						  write_len);
		//g_core_fp.fp_burst_enable(1);
		g_core_fp.fp_sense_on(0x01);
		himax_int_enable(1);

		himax_flash_read_set_status(0);
	} else {
		input_info(true, &private_ts->client->dev,
				"%s %s:Now operating flash!\n", HIMAX_LOG_TAG,
				__func__);
	}

	input_info(true, &private_ts->client->dev, "%s %s:End\n", HIMAX_LOG_TAG,
			__func__);
	return NO_ERR;
}

static void himax_mcu_chip_erase(void)
{
	g_core_fp.fp_interface_on();

	/* Reset power saving level */
	if (g_core_fp.fp_init_psl != NULL) {
		g_core_fp.fp_init_psl();
	}

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
					DATA_LEN_4,
					pflash_op->data_spi200_trans_fmt, 0);

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
					DATA_LEN_4,
					pflash_op->data_spi200_trans_ctrl_2, 0);
	g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd, DATA_LEN_4,
					pflash_op->data_spi200_cmd_2, 0);

	g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd, DATA_LEN_4,
					pflash_op->data_spi200_cmd_3, 0);
	msleep(2000);

	if (!g_core_fp.fp_wait_wip(100)) {
		input_err(true, &private_ts->client->dev,
				"%s %s: Chip_Erase Fail\n", HIMAX_LOG_TAG, __func__);
	}
}

static bool himax_mcu_block_erase(int start_addr, int length)
{				/*complete not yet */
	uint32_t page_prog_start = 0;
	uint32_t block_size = 0x10000;

	uint8_t tmp_data[4] = { 0 };

	g_core_fp.fp_interface_on();

	g_core_fp.fp_init_psl();

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
					DATA_LEN_4,
					pflash_op->data_spi200_trans_fmt, 0);

	for (page_prog_start = start_addr;
		page_prog_start < start_addr + length;
		page_prog_start = page_prog_start + block_size) {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
						DATA_LEN_4,
						pflash_op->data_spi200_trans_ctrl_2,
						0);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
						DATA_LEN_4,
						pflash_op->data_spi200_cmd_2, 0);

		tmp_data[3] = (page_prog_start >> 24) & 0xFF;
		tmp_data[2] = (page_prog_start >> 16) & 0xFF;
		tmp_data[1] = (page_prog_start >> 8) & 0xFF;
		tmp_data[0] = page_prog_start & 0xFF;
		g_core_fp.fp_register_write(pflash_op->addr_spi200_addr,
						DATA_LEN_4, tmp_data, 0);

		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
						DATA_LEN_4,
						pflash_op->data_spi200_trans_ctrl_3,
						0);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
						DATA_LEN_4,
						pflash_op->data_spi200_cmd_4, 0);
		msleep(1000);

		if (!g_core_fp.fp_wait_wip(100)) {
			input_err(true, &private_ts->client->dev,
					"%s %s:Erase Fail\n", HIMAX_LOG_TAG,
					__func__);
			return false;
		}
	}

	input_info(true, &private_ts->client->dev, "%s %s:END\n", HIMAX_LOG_TAG,
			__func__);
	return true;
}

static bool himax_mcu_sector_erase(int start_addr)
{
	return true;
}

int himax_read_from_ic_flash_flow(uint32_t start_addr, uint8_t * read_buffer,
					uint32_t read_len)
{
	uint8_t tmp_addr[4];
	uint8_t buffer[256];
	int page_prog_start = 0;

	input_info(true, &private_ts->client->dev,
			"%s %s: Start addr = 0x%08X\n", HIMAX_LOG_TAG, __func__,
			start_addr);

	himax_int_enable(0);
	g_core_fp.fp_sense_off(true);

	for (page_prog_start = start_addr;
		page_prog_start < (start_addr + read_len);
		page_prog_start = page_prog_start + 128) {
		input_info(true, &private_ts->client->dev,
				"%s Now page_prog_start=0x%08X\n", HIMAX_LOG_TAG,
				page_prog_start);
		tmp_addr[0] = page_prog_start % 0x100;
		tmp_addr[1] = (page_prog_start >> 8) % 0x100;
		tmp_addr[2] = (page_prog_start >> 16) % 0x100;
		tmp_addr[3] = page_prog_start / 0x1000000;
		g_core_fp.fp_register_read(tmp_addr, 128, buffer, false);
		memcpy(&read_buffer[page_prog_start - start_addr], buffer, 128);
	}

	g_core_fp.fp_sense_on(0x01);
	himax_int_enable(1);
	input_info(true, &private_ts->client->dev,
			"%s %s:After reading now read_buffer size = %d\n",
			HIMAX_LOG_TAG, __func__, (int)strlen(read_buffer));

	return NO_ERR;
}

static void hx_mcu_flash_programming_sz(uint32_t start_addr,
					uint8_t * FW_content, int FW_Size)
{
	int page_prog_start = 0, i = 0, j = 0, k = 0;
	int program_length = PROGRAM_SZ;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t buring_data[FLASH_RW_MAX_LEN];	/* Read for flash data, 128K */
	uint32_t now_tmp_addr = 0;
	/* 4 bytes for padding*/
	g_core_fp.fp_interface_on();

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
					DATA_LEN_4,
					pflash_op->data_spi200_trans_fmt, 0);

	for (page_prog_start = 0; page_prog_start < FW_Size;
		page_prog_start += FLASH_RW_MAX_LEN) {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
						DATA_LEN_4,
						pflash_op->data_spi200_trans_ctrl_2,
						0);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
						DATA_LEN_4,
						pflash_op->data_spi200_cmd_2, 0);

		/*Programmable size = 1 page = 256 bytes, word_number = 256 byte / 4 = 64*/
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
						DATA_LEN_4,
						pflash_op->data_spi200_trans_ctrl_4,
						0);

		now_tmp_addr = start_addr + page_prog_start;

		tmp_data[3] = (uint8_t) (now_tmp_addr >> 24);
		tmp_data[2] = (uint8_t) (now_tmp_addr >> 16);
		tmp_data[1] = (uint8_t) (now_tmp_addr >> 8);
		tmp_data[0] = (uint8_t) (now_tmp_addr);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_addr,
						DATA_LEN_4, tmp_data, 0);

		for (i = 0; i < ADDR_LEN_4; i++) {
			buring_data[i] = pflash_op->addr_spi200_data[i];
		}

		for (i = page_prog_start, j = 0; i < 16 + page_prog_start;
			i++, j++) {
			buring_data[j + ADDR_LEN_4] = FW_content[i];
		}

		if (himax_bus_write
			(pic_op->addr_ahb_addr_byte_0[0], buring_data,
			ADDR_LEN_4 + 16, HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return;
		}

		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
						DATA_LEN_4,
						pflash_op->data_spi200_cmd_6, 0);

		for (j = 0; j < 5; j++) {
			for (i = (page_prog_start + 16 + (j * 48)), k = 0; i <
				(page_prog_start + 16 + (j * 48)) + program_length;
				i++, k++) {
				buring_data[k + ADDR_LEN_4] = FW_content[i];
			}

			if (himax_bus_write
				(pic_op->addr_ahb_addr_byte_0[0], buring_data,
				program_length + ADDR_LEN_4,
				HIMAX_I2C_RETRY_TIMES) < 0) {
				input_err(true, &private_ts->client->dev,
						"%s %s: i2c access fail!\n",
						HIMAX_LOG_TAG, __func__);
				return;
			}
		}

		if (!g_core_fp.fp_wait_wip(1)) {
			input_err(true, &private_ts->client->dev,
					"%s %s:Flash_Programming Fail\n",
					HIMAX_LOG_TAG, __func__);
		}
	}
}

static void himax_mcu_flash_programming(uint8_t * FW_content, int FW_Size)
{
	int page_prog_start = 0, i = 0, j = 0, k = 0;
	int program_length = PROGRAM_SZ;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t buring_data[FLASH_RW_MAX_LEN];	/* Read for flash data, 128K */
	/* 4 bytes for padding*/
	g_core_fp.fp_interface_on();

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
					DATA_LEN_4,
					pflash_op->data_spi200_trans_fmt, 0);

	for (page_prog_start = 0; page_prog_start < FW_Size;
		page_prog_start += FLASH_RW_MAX_LEN) {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
						DATA_LEN_4,
						pflash_op->data_spi200_trans_ctrl_2,
						0);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
						DATA_LEN_4,
						pflash_op->data_spi200_cmd_2, 0);

/*Programmable size = 1 page = 256 bytes, word_number = 256 byte / 4 = 64*/
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
						DATA_LEN_4,
						pflash_op->data_spi200_trans_ctrl_4,
						0);

/* Flash start address 1st : 0x0000_0000*/
		if (page_prog_start < 0x100) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = (uint8_t) page_prog_start;
		} else if (page_prog_start >= 0x100
				&& page_prog_start < 0x10000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = (uint8_t) (page_prog_start >> 8);
			tmp_data[0] = (uint8_t) page_prog_start;
		} else if (page_prog_start >= 0x10000
				&& page_prog_start < 0x1000000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = (uint8_t) (page_prog_start >> 16);
			tmp_data[1] = (uint8_t) (page_prog_start >> 8);
			tmp_data[0] = (uint8_t) page_prog_start;
		}
		g_core_fp.fp_register_write(pflash_op->addr_spi200_addr,
						DATA_LEN_4, tmp_data, 0);

		for (i = 0; i < ADDR_LEN_4; i++) {
			buring_data[i] = pflash_op->addr_spi200_data[i];
		}

		for (i = page_prog_start, j = 0; i < 16 + page_prog_start;
			i++, j++) {
			buring_data[j + ADDR_LEN_4] = FW_content[i];
		}

		if (himax_bus_write
			(pic_op->addr_ahb_addr_byte_0[0], buring_data,
			ADDR_LEN_4 + 16, HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s: i2c access fail!\n", HIMAX_LOG_TAG,
					__func__);
			return;
		}

		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
						DATA_LEN_4,
						pflash_op->data_spi200_cmd_6, 0);

		for (j = 0; j < 5; j++) {
			for (i = (page_prog_start + 16 + (j * 48)), k = 0;
				i < (page_prog_start + 16 + (j * 48)) + program_length;
				i++, k++) {
				buring_data[k + ADDR_LEN_4] = FW_content[i];
			}

			if (himax_bus_write
				(pic_op->addr_ahb_addr_byte_0[0], buring_data,
				program_length + ADDR_LEN_4,
				HIMAX_I2C_RETRY_TIMES) < 0) {
				input_err(true, &private_ts->client->dev,
						"%s %s: i2c access fail!\n",
						HIMAX_LOG_TAG, __func__);
				return;
			}
		}

		if (!g_core_fp.fp_wait_wip(1)) {
			input_err(true, &private_ts->client->dev,
					"%s %s:Flash_Programming Fail\n",
					HIMAX_LOG_TAG, __func__);
		}
	}
}

static void himax_mcu_flash_page_write(uint8_t * write_addr, int length,
				uint8_t * write_data)
{
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_32k(unsigned char *fw,
							int len,
							bool change_iref)
{
/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_60k(unsigned char *fw,
							int len,
							bool change_iref)
{
/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k(unsigned char *fw,
							int len,
							bool change_iref)
{
	int burnFW_success = 0;

	if (len != FW_SIZE_64k) {
		input_err(true, &private_ts->client->dev,
			"%s %s: The file size is not 64K bytes\n",
			HIMAX_LOG_TAG, __func__);
		return false;
	}
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, false);
#else
	g_core_fp.fp_system_reset();
#endif
	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_block_erase(0x00, FW_SIZE_64k);
	g_core_fp.fp_flash_programming(fw, FW_SIZE_64k);

	if (g_core_fp.
		fp_check_CRC(pfw_op->addr_program_reload_from, FW_SIZE_64k) == 0) {
		burnFW_success = 1;
	}

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
					sizeof(pfw_op->data_clear),
					pfw_op->data_clear, 0);
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);

#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, false);
#else
	/*System reset*/
	g_core_fp.fp_system_reset();
#endif
	return burnFW_success;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_124k(unsigned char *fw,
								int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k(unsigned char *fw,
							int len, bool change_iref)
{
	int burnFW_success = 0;
	
	if (len != FW_SIZE_128k) {
		input_err(true, &private_ts->client->dev,
			"%s %s: The file size is not 128K bytes\n",
			HIMAX_LOG_TAG, __func__);
		return false;
	}
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, false);
#else
	g_core_fp.fp_system_reset();
#endif
	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_block_erase(0x00, FW_SIZE_128k);
	g_core_fp.fp_flash_programming(fw, FW_SIZE_128k);
	
	if (g_core_fp.
		fp_check_CRC(pfw_op->addr_program_reload_from, FW_SIZE_128k) == 0) {
		burnFW_success = 1;
	}
	
	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
					sizeof(pfw_op->data_clear),
					pfw_op->data_clear, 0);
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);
	
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, false);
#else
	/*System reset*/
	g_core_fp.fp_system_reset();
#endif
	return burnFW_success;
}

static void himax_mcu_flash_dump_func(uint8_t local_flash_command,
					int Flash_Size, uint8_t * flash_buffer)
{
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t buffer[256];
	int page_prog_start = 0;
	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_burst_enable(1);

	for (page_prog_start = 0; page_prog_start < Flash_Size;
		page_prog_start += 128) {
		tmp_addr[0] = page_prog_start % 0x100;
		tmp_addr[1] = (page_prog_start >> 8) % 0x100;
		tmp_addr[2] = (page_prog_start >> 16) % 0x100;
		tmp_addr[3] = page_prog_start / 0x1000000;
		himax_mcu_register_read(tmp_addr, 128, buffer, 0);
		memcpy(&flash_buffer[page_prog_start], buffer, 128);
	}

	g_core_fp.fp_burst_enable(0);
	g_core_fp.fp_sense_on(0x01);
}

static bool himax_mcu_flash_lastdata_check(void)
{
	uint8_t tmp_addr[4];
	uint32_t start_addr = 0xFF80;
	uint32_t temp_addr = 0;
	uint32_t flash_page_len = 0x80;
	uint8_t flash_tmp_buffer[128];

	if (strcmp(private_ts->chip_name, HX_83102D_SERIES_PWON) == 0)
		start_addr = FW_SIZE_128k - flash_page_len;
	else
		start_addr = FW_SIZE_64k - flash_page_len;

	I("Now start size=0x%08X\n", start_addr);
	
	for (temp_addr = start_addr; temp_addr < (start_addr + flash_page_len);
		temp_addr = temp_addr + flash_page_len) {
		/*input_info(true, &private_ts->client->dev, "temp_addr=%d,tmp_addr[0]=0x%2X, tmp_addr[1]=0x%2X,tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n", temp_addr,tmp_addr[0], tmp_addr[1], tmp_addr[2],tmp_addr[3]);*/
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		g_core_fp.fp_register_read(tmp_addr, flash_page_len,
						&flash_tmp_buffer[0], 0);
	}

	if ((!flash_tmp_buffer[flash_page_len - 4])
		&& (!flash_tmp_buffer[flash_page_len - 3])
		&& (!flash_tmp_buffer[flash_page_len - 2])
		&& (!flash_tmp_buffer[flash_page_len - 1])) {
		input_info(true, &private_ts->client->dev,
				"%s Fail, Last four Bytes are "
				"flash_buffer[FFFC]=0x%2X,flash_buffer[FFFD]=0x%2X,flash_buffer[FFFE]=0x%2X,flash_buffer[FFFF]=0x%2X\n",
				HIMAX_LOG_TAG, flash_tmp_buffer[flash_page_len - 4],
				flash_tmp_buffer[flash_page_len - 3],
				flash_tmp_buffer[flash_page_len - 2],
				flash_tmp_buffer[flash_page_len - 1]);
		return 1;	/*FAIL*/
	} else if ((flash_tmp_buffer[flash_page_len - 4] == 0xFF)
				&& (flash_tmp_buffer[flash_page_len - 3] == 0xFF)
				&& (flash_tmp_buffer[flash_page_len - 2] == 0xFF)
				&& (flash_tmp_buffer[flash_page_len - 1] == 0xFF)) {
		input_info(true, &private_ts->client->dev,
				"%s Fail, Last four Bytes are "
				"flash_buffer[FFFC]=0x%2X,flash_buffer[FFFD]=0x%2X,flash_buffer[FFFE]=0x%2X,flash_buffer[FFFF]=0x%2X\n",
				HIMAX_LOG_TAG, flash_tmp_buffer[flash_page_len - 4],
				flash_tmp_buffer[flash_page_len - 3],
				flash_tmp_buffer[flash_page_len - 2],
				flash_tmp_buffer[flash_page_len - 1]);
		return 1;
	} else {
		input_info(true, &private_ts->client->dev,
				"%s flash_buffer[FFFC]=0x%2X,flash_buffer[FFFD]=0x%2X,flash_buffer[FFFE]=0x%2X,flash_buffer[FFFF]=0x%2X\n",
				HIMAX_LOG_TAG, flash_tmp_buffer[flash_page_len - 4],
				flash_tmp_buffer[flash_page_len - 3],
				flash_tmp_buffer[flash_page_len - 2],
				flash_tmp_buffer[flash_page_len - 1]);
		return 0;	/*PASS*/
	}
}

/* FLASH side end*/
#endif

#ifdef CORE_SRAM
/* SRAM side start*/
static void himax_mcu_sram_write(uint8_t * FW_content)
{
}

static bool himax_mcu_sram_verify(uint8_t * FW_File, int FW_Size)
{
	return true;
}

static bool himax_mcu_get_DSRAM_data(uint8_t * info_data, bool DSRAM_Flag)
{
	int i = 0;
	unsigned char tmp_addr[ADDR_LEN_4];
	unsigned char tmp_data[DATA_LEN_4];
	uint8_t max_i2c_size = MAX_I2C_TRANS_SZ;
	uint8_t x_num = ic_data->HX_RX_NUM;
	uint8_t y_num = ic_data->HX_TX_NUM;
	/*int m_key_num = 0;*/
	int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
	int total_size_temp;
	int total_read_times = 0;
	int address = 0;
	uint8_t *temp_info_data;	/*max mkey size = 8 */
	uint16_t check_sum_cal = 0;
	int fw_run_flag = -1;

	temp_info_data =
		kzalloc(sizeof(uint8_t) * (total_size + 8), GFP_KERNEL);
	/*1. Read number of MKey R100070E8H to determin data size*/
	/*m_key_num = ic_data->HX_BT_NUM;
	input_info(true, &private_ts->client->dev, "%s,m_key_num=%d\n",__func__ ,m_key_num);
	total_size += m_key_num * 2;
	2. Start DSRAM Rawdata and Wait Data Ready */
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = psram_op->passwrd_start[1];
	tmp_data[0] = psram_op->passwrd_start[0];
	fw_run_flag =
		himax_write_read_reg(psram_op->addr_rawdata_addr, tmp_data,
				psram_op->passwrd_end[1],
				psram_op->passwrd_end[0]);

	if (fw_run_flag < 0) {
		input_info(true, &private_ts->client->dev,
				"%s %s Data NOT ready => bypass \n", HIMAX_LOG_TAG,
				__func__);
		kfree(temp_info_data);
		return false;
	}

	/* 3. Read RawData */
	total_size_temp = total_size;
	input_info(true, &private_ts->client->dev,
			"%s %s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X,tmp_data[3] = 0x%02X\n",
			HIMAX_LOG_TAG, __func__, psram_op->addr_rawdata_addr[0],
			psram_op->addr_rawdata_addr[1],
			psram_op->addr_rawdata_addr[2],
			psram_op->addr_rawdata_addr[3]);
	tmp_addr[0] = psram_op->addr_rawdata_addr[0];
	tmp_addr[1] = psram_op->addr_rawdata_addr[1];
	tmp_addr[2] = psram_op->addr_rawdata_addr[2];
	tmp_addr[3] = psram_op->addr_rawdata_addr[3];

	if (total_size % max_i2c_size == 0) {
		total_read_times = total_size / max_i2c_size;
	} else {
		total_read_times = total_size / max_i2c_size + 1;
	}

	for (i = 0; i < total_read_times; i++) {
		address = (psram_op->addr_rawdata_addr[3] << 24) +
			(psram_op->addr_rawdata_addr[2] << 16) +
			(psram_op->addr_rawdata_addr[1] << 8) +
			psram_op->addr_rawdata_addr[0] + i * max_i2c_size;
		/*input_info(true, &private_ts->client->dev, "%s address = %08X \n", __func__, address);*/

		tmp_addr[3] = (uint8_t) ((address >> 24) & 0x00FF);
		tmp_addr[2] = (uint8_t) ((address >> 16) & 0x00FF);
		tmp_addr[1] = (uint8_t) ((address >> 8) & 0x00FF);
		tmp_addr[0] = (uint8_t) ((address) & 0x00FF);

		if (total_size_temp >= max_i2c_size) {
			g_core_fp.fp_register_read(tmp_addr, max_i2c_size,
						&temp_info_data[i * max_i2c_size], 0);
			total_size_temp = total_size_temp - max_i2c_size;
		} else {
			/*input_info(true, &private_ts->client->dev, "last total_size_temp=%d\n",total_size_temp);*/
			g_core_fp.fp_register_read(tmp_addr, total_size_temp % max_i2c_size,
						&temp_info_data[i * max_i2c_size], 0);
		}
	}

	/* 4. FW stop outputing */
	/*input_info(true, &private_ts->client->dev, "DSRAM_Flag=%d\n",DSRAM_Flag);*/
	if (DSRAM_Flag == false || private_ts->diag_cmd == 0) {
	/*input_info(true, &private_ts->client->dev, "Return to Event Stack!\n");*/
		g_core_fp.fp_register_write(psram_op->addr_rawdata_addr,
						DATA_LEN_4, psram_op->data_fin, 0);
	} else {
	/*input_info(true, &private_ts->client->dev, "Continue to SRAM!\n");*/
		g_core_fp.fp_register_write(psram_op->addr_rawdata_addr,
						DATA_LEN_4, psram_op->data_conti,
						0);
	}

	/* 5. Data Checksum Check */
	for (i = 2; i < total_size; i += 2) {	/* 2:PASSWORD NOT included */
		check_sum_cal +=
			(temp_info_data[i + 1] * 256 + temp_info_data[i]);
	}

	if (check_sum_cal % 0x10000 != 0) {
		input_info(true, &private_ts->client->dev,
				"%s %s check_sum_cal fail=%2X \n", HIMAX_LOG_TAG,
				__func__, check_sum_cal);
		kfree(temp_info_data);
		return false;
	} else {
		memcpy(info_data, &temp_info_data[4],
			total_size * sizeof(uint8_t));
		/*input_info(true, &private_ts->client->dev, "%s checksum PASS \n", __func__);*/
	}

	kfree(temp_info_data);
	return true;
}

/* SRAM side end*/
#endif

#ifdef CORE_DRIVER

static void himax_mcu_init_ic(void)
{
	input_info(true, &private_ts->client->dev,
			"%s %s: use default incell init.\n", HIMAX_LOG_TAG,
			__func__);
}

#ifdef HX_AUTO_UPDATE_FW
static int himax_mcu_fw_ver_bin(void)
{
	input_info(true, &private_ts->client->dev,
			"%s %s: use default incell address.\n", HIMAX_LOG_TAG,
			__func__);
	input_info(true, &private_ts->client->dev, "%s %s:Entering!\n",
			HIMAX_LOG_TAG, __func__);
	if (i_CTPM_FW != NULL) {
		input_info(true, &private_ts->client->dev,
				"%s Catch fw version in bin file!\n", HIMAX_LOG_TAG);
		g_i_FW_VER =
			(i_CTPM_FW[FW_VER_MAJ_FLASH_ADDR] << 8) |
			i_CTPM_FW[FW_VER_MIN_FLASH_ADDR];
		g_i_CFG_VER =
			(i_CTPM_FW[CFG_VER_MAJ_FLASH_ADDR] << 8) |
			i_CTPM_FW[CFG_VER_MIN_FLASH_ADDR];
		g_i_CID_MAJ = i_CTPM_FW[CID_VER_MAJ_FLASH_ADDR];
		g_i_CID_MIN = i_CTPM_FW[CID_VER_MIN_FLASH_ADDR];
		g_i_PANEL_VER = i_CTPM_FW[PANEL_VERSION_ADDR];
	} else {
		input_info(true, &private_ts->client->dev,
				"%s %s:FW data is null!\n", HIMAX_LOG_TAG, __func__);
		return 1;
	}
	return NO_ERR;
}
#endif

#ifdef HX_RST_PIN_FUNC
static void himax_mcu_pin_reset(void)
{
	int error;

	input_info(true, &private_ts->client->dev,
			"%s %s: Now reset the Touch chip.\n", HIMAX_LOG_TAG,
			__func__);

	error = gpio_request(private_ts->rst_gpio, "himax-reset");

	if (error < 0) {
		input_err(true, &private_ts->client->dev,
			"%s %s: request reset pin failed\n",
			HIMAX_LOG_TAG, __func__);
		return;
	}

	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	msleep(20);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	msleep(50);

	if (gpio_is_valid(private_ts->rst_gpio))
		gpio_free(private_ts->rst_gpio);
}

static void himax_mcu_ic_reset(uint8_t loadconfig, uint8_t int_off)
{
	struct himax_ts_data *ts = private_ts;
	HX_HW_RESET_ACTIVATE = 0;
	input_info(true, &private_ts->client->dev,
			"%s %s,status: loadconfig=%d,int_off=%d\n", HIMAX_LOG_TAG,
			__func__, loadconfig, int_off);

	if (ts->rst_gpio >= 0) {
		if (int_off) {
			g_core_fp.fp_irq_switch(0);
		}

		g_core_fp.fp_pin_reset();

		if (loadconfig) {
			g_core_fp.fp_reload_config();
		}

		if (int_off) {
			g_core_fp.fp_irq_switch(1);
		}
	}
}
#endif

static uint8_t himax_mcu_tp_info_check(void)
{
	uint8_t rx = pdriver_op->data_df_rx[0];
	uint8_t tx = pdriver_op->data_df_tx[0];
	uint8_t pt = pdriver_op->data_df_pt[0];
	uint16_t x_res =
		pdriver_op->data_df_x_res[1] << 8 | pdriver_op->data_df_x_res[0];
	uint16_t y_res =
		pdriver_op->data_df_y_res[1] << 8 | pdriver_op->data_df_y_res[0];
	uint8_t err_cnt = 0;
	if (ic_data->HX_RX_NUM < (rx / 2) || ic_data->HX_RX_NUM > (rx * 3 / 2)) {
		ic_data->HX_RX_NUM = rx;
		err_cnt |= 0x01;
	}
	if (ic_data->HX_TX_NUM < (tx / 2) || ic_data->HX_TX_NUM > (tx * 3 / 2)) {
		ic_data->HX_TX_NUM = tx;
		err_cnt |= 0x02;
	}
	if (ic_data->HX_MAX_PT < (pt / 2) || ic_data->HX_MAX_PT > (pt * 3 / 2)) {
		ic_data->HX_MAX_PT = pt;
		err_cnt |= 0x04;
	}
	if (ic_data->HX_Y_RES < (y_res / 2)
		|| ic_data->HX_Y_RES > (y_res * 3 / 2)) {
		ic_data->HX_Y_RES = y_res;
		err_cnt |= 0x08;
	}
	if (ic_data->HX_X_RES < (x_res / 2)
		|| ic_data->HX_X_RES > (x_res * 3 / 2)) {
		ic_data->HX_X_RES = x_res;
		err_cnt |= 0x10;
	}
	return err_cnt;
}

static void himax_mcu_touch_information(void)
{
#ifndef HX_FIX_TOUCH_INFO
	char data[DATA_LEN_8] = { 0 };
	uint8_t err_cnt = 0;

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_rxnum_txnum_maxpt,
					DATA_LEN_8, data, 0);
	ic_data->HX_RX_NUM = data[2];
	ic_data->HX_TX_NUM = data[3];
	ic_data->HX_MAX_PT = data[4];
	/*input_info(true, &private_ts->client->dev, "%s : HX_RX_NUM=%d,ic_data->HX_TX_NUM=%d,ic_data->HX_MAX_PT=%d\n",__func__,ic_data->HX_RX_NUM,ic_data->HX_TX_NUM,ic_data->HX_MAX_PT);*/
	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_xy_res_enable,
					DATA_LEN_4, data, 0);

	/*input_info(true, &private_ts->client->dev, "%s : c_data->HX_XY_REVERSE=0x%2.2X\n",__func__,data[1]);*/
	if ((data[1] & 0x04) == 0x04) {
		ic_data->HX_XY_REVERSE = true;
	} else {
		ic_data->HX_XY_REVERSE = false;
	}

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_x_y_res,
					DATA_LEN_4, data, 0);
	ic_data->HX_Y_RES = data[0] * 256 + data[1];
	ic_data->HX_X_RES = data[2] * 256 + data[3];
	/*input_info(true, &private_ts->client->dev, "%s : ic_data->HX_Y_RES=%d,ic_data->HX_X_RES=%d \n",__func__,ic_data->HX_Y_RES,ic_data->HX_X_RES);*/

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_int_is_edge,
					DATA_LEN_4, data, 0);
	/*input_info(true, &private_ts->client->dev, "%s : data[0]=0x%2.2X,data[1]=0x%2.2X,data[2]=0x%2.2X,data[3]=0x%2.2X\n",__func__,data[0],data[1],data[2],data[3]);
	input_info(true, &private_ts->client->dev, "data[0] & 0x01 = %d\n",(data[0] & 0x01));*/
	if ((data[1] & 0x01) == 1) {
		ic_data->HX_INT_IS_EDGE = true;
	} else {
		ic_data->HX_INT_IS_EDGE = false;
	}

	/*1. Read number of MKey R100070E8H to determin data size*/
	g_core_fp.fp_register_read(psram_op->addr_mkey, DATA_LEN_4, data, 0);
	/* input_info(true, &private_ts->client->dev, "%s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X,tmp_data[3] = 0x%02X\n",
	__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);*/
	ic_data->HX_BT_NUM = data[0] & 0x03;
	err_cnt = himax_mcu_tp_info_check();
	if (err_cnt > 0)
		input_err(true, &private_ts->client->dev,
				"%s %s:TP Info from IC is wrong, err_cnt = 0x%X",
				HIMAX_LOG_TAG, __func__, err_cnt);
#else
	ic_data->HX_RX_NUM = FIX_HX_RX_NUM;
	ic_data->HX_TX_NUM = FIX_HX_TX_NUM;
	ic_data->HX_BT_NUM = FIX_HX_BT_NUM;
	ic_data->HX_X_RES = FIX_HX_X_RES;
	ic_data->HX_Y_RES = FIX_HX_Y_RES;
	ic_data->HX_MAX_PT = FIX_HX_MAX_PT;
	ic_data->HX_XY_REVERSE = FIX_HX_XY_REVERSE;
	ic_data->HX_INT_IS_EDGE = FIX_HX_INT_IS_EDGE;
#endif
	input_info(true, &private_ts->client->dev,
			"%s %s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d \n",
			HIMAX_LOG_TAG, __func__, ic_data->HX_RX_NUM,
			ic_data->HX_TX_NUM, ic_data->HX_MAX_PT);
	input_info(true, &private_ts->client->dev,
			"%s %s:HX_XY_REVERSE =%d,HX_Y_RES =%d,HX_X_RES=%d \n",
			HIMAX_LOG_TAG, __func__, ic_data->HX_XY_REVERSE,
			ic_data->HX_Y_RES, ic_data->HX_X_RES);
	input_info(true, &private_ts->client->dev,
			"%s %s:HX_INT_IS_EDGE =%d \n", HIMAX_LOG_TAG, __func__,
			ic_data->HX_INT_IS_EDGE);
}

static void himax_mcu_reload_config(void)
{
	if (himax_report_data_init()) {
		input_err(true, &private_ts->client->dev,
				"%s %s: allocate data fail\n", HIMAX_LOG_TAG,
				__func__);
	}
	g_core_fp.fp_sense_on(0x00);
}

static int himax_mcu_get_touch_data_size(void)
{
	return HIMAX_TOUCH_DATA_SIZE;
}

static int himax_mcu_hand_shaking(void)
{
	/* 0:Running, 1:Stop, 2:I2C Fail */
	int result = 0;
	return result;
}

static int himax_mcu_determin_diag_rawdata(int diag_command)
{
	return diag_command % 10;
}

static int himax_mcu_determin_diag_storage(int diag_command)
{
	return diag_command / 10;
}

static int himax_mcu_cal_data_len(int raw_cnt_rmd, int HX_MAX_PT,
					int raw_cnt_max)
{
	int RawDataLen;

	if (raw_cnt_rmd != 0x00) {
		RawDataLen =
			MAX_I2C_TRANS_SZ - ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 1;
	} else {
		RawDataLen =
			MAX_I2C_TRANS_SZ - ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 1;
	}

	return RawDataLen;
}

static bool himax_mcu_diag_check_sum(struct himax_report_data *hx_touch_data)
{
	uint16_t check_sum_cal = 0;
	int i;

/* Check 128th byte CRC */
	for (i = 0, check_sum_cal = 0;
		i < (hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
		i += 2) {
		check_sum_cal +=
			(hx_touch_data->hx_rawdata_buf[i + 1] * FLASH_RW_MAX_LEN +
			hx_touch_data->hx_rawdata_buf[i]);
	}

	if (check_sum_cal % HX64K != 0) {
		input_info(true, &private_ts->client->dev, "%s %s fail=%2X \n",
				HIMAX_LOG_TAG, __func__, check_sum_cal);
		return 0;
	}

	return 1;
}

static void himax_mcu_diag_parse_raw_data(struct himax_report_data
						*hx_touch_data, int mul_num,
						int self_num, uint8_t diag_cmd,
						int32_t * mutual_data,
						int32_t * self_data)
{
	diag_mcu_parse_raw_data(hx_touch_data, mul_num, self_num, diag_cmd,
				mutual_data, self_data);
}

#ifdef HX_ESD_RECOVERY
static int himax_mcu_ic_esd_recovery(int hx_esd_event, int hx_zero_event,
					int length)
{
	int ret_val = NO_ERR;

	if (g_zero_event_count > 5) {
		g_zero_event_count = 0;
		input_info(true, &private_ts->client->dev,
				"%s [HIMAX TP MSG]: ESD event checked - ALL Zero.\n",
				HIMAX_LOG_TAG);
		ret_val = HX_ESD_EVENT;
		goto END_FUNCTION;
	}

	if (hx_esd_event == length) {
		g_zero_event_count = 0;
		ret_val = HX_ESD_EVENT;
		goto END_FUNCTION;
	} else if (hx_zero_event == length) {
		g_zero_event_count++;
		input_info(true, &private_ts->client->dev,
				"%s [HIMAX TP MSG]: ALL Zero event is %d times.\n",
				HIMAX_LOG_TAG, g_zero_event_count);
		ret_val = HX_ZERO_EVENT_COUNT;
		goto END_FUNCTION;
	}

END_FUNCTION:
	return ret_val;
}

static void himax_mcu_esd_ic_reset(void)
{
	HX_ESD_RESET_ACTIVATE = 0;
#ifdef HX_RST_PIN_FUNC
	himax_mcu_pin_reset();
#else
	g_core_fp.fp_system_reset();
#endif
	input_info(true, &private_ts->client->dev, "%s %s:\n", HIMAX_LOG_TAG,
			__func__);
}
#endif
#ifdef HX_TP_PROC_GUEST_INFO
char *g_checksum_str = "check sum fail";
char *g_guest_info_item[] = {
	"projectID",
	"CGColor",
	"BarCode",
	"Reserve1",
	"Reserve2",
	"Reserve3",
	"Reserve4",
	"Reserve5",
	"VCOM",
	"Vcom-3Gar",
	NULL
};

static int himax_guest_info_get_status(void)
{
	return g_guest_info_data->g_guest_info_ongoing;
}

static void himax_guest_info_set_status(int setting)
{
	g_guest_info_data->g_guest_info_ongoing = setting;
}

static int himax_guest_info_read(uint32_t start_addr,
				 uint8_t * flash_tmp_buffer)
{
	uint32_t temp_addr = 0;
	uint8_t tmp_addr[4];
	uint32_t flash_page_len = 0x1000;
/* uint32_t checksum = 0x00; */
	int result = 0;

	input_info(true, &private_ts->client->dev,
			"%s Reading guest info in start_addr = 0x%08X !\n",
			HIMAX_LOG_TAG, start_addr);

	tmp_addr[0] = start_addr % 0x100;
	tmp_addr[1] = (start_addr >> 8) % 0x100;
	tmp_addr[2] = (start_addr >> 16) % 0x100;
	tmp_addr[3] = start_addr / 0x1000000;
	input_info(true, &private_ts->client->dev,
			"%s %s: Now start addr: tmp_addr[0]=0x%2X,tmp_addr[1]=0x%2X,tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n",
			HIMAX_LOG_TAG, __func__, tmp_addr[0], tmp_addr[1],
			tmp_addr[2], tmp_addr[3]);
	result = g_core_fp.fp_check_CRC(tmp_addr, flash_page_len);
	input_info(true, &private_ts->client->dev, "%s %s: Checksum = 0x%8X\n",
		   HIMAX_LOG_TAG, __func__, result);
	if (result != 0)
		goto END_FUNC;

	for (temp_addr = start_addr; temp_addr < (start_addr + flash_page_len);
		temp_addr = temp_addr + 128) {

		/* input_info(true, &private_ts->client->dev, "temp_addr=%d,tmp_addr[0]=0x%2X,tmp_addr[1]=0x%2X,tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n",temp_addr,tmp_addr[0],tmp_addr[1],tmp_addr[2],tmp_addr[3]); */
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		g_core_fp.fp_register_read(tmp_addr, 128,
						&flash_tmp_buffer[temp_addr - start_addr],
						false);
		/* memcpy(&flash_tmp_buffer[temp_addr - start_addr],buffer,128);*/
	}

END_FUNC:
	return result;
}

static int hx_read_guest_info(void)
{
	/* uint8_t tmp_addr[4]; */
	uint32_t panel_color_addr = 0x10000;	/*64k */

	uint32_t info_len;
	uint32_t flash_page_len = 0x1000;	/*4k */
	uint8_t *flash_tmp_buffer = NULL;
	/* uint32_t temp_addr = 0; */
	uint8_t temp_str[128];
	int i = 0;
	int custom_info_temp = 0;
	int checksum = 0;

	himax_guest_info_set_status(1);

	flash_tmp_buffer =
		kzalloc(HX_GUEST_INFO_SIZE * flash_page_len * sizeof(uint8_t),
			GFP_KERNEL);
	if (flash_tmp_buffer == NULL) {
		input_info(true, &private_ts->client->dev,
				"%s %s: Memory allocate fail!\n", HIMAX_LOG_TAG,
				__func__);
		return MEM_ALLOC_FAIL;
	}

	g_core_fp.fp_sense_off(true);
	/* g_core_fp.fp_burst_enable(1); */

	for (custom_info_temp = 0; custom_info_temp < HX_GUEST_INFO_SIZE;
		custom_info_temp++) {
		checksum =
			himax_guest_info_read(panel_color_addr +
						custom_info_temp * flash_page_len,
						&flash_tmp_buffer[custom_info_temp *
						flash_page_len]);
		if (checksum != 0) {
			input_err(true, &private_ts->client->dev,
					"%s %s:Checksum Fail! g_checksum_str len=%d\n",
					HIMAX_LOG_TAG, __func__,
					(int)strlen(g_checksum_str));
			memcpy(&g_guest_info_data->
				g_guest_str_in_format[custom_info_temp][0],
				g_checksum_str, (int)strlen(g_checksum_str));
			memcpy(&g_guest_info_data->
				g_guest_str[custom_info_temp][0], g_checksum_str,
				(int)strlen(g_checksum_str));
			continue;
		}

		info_len = flash_tmp_buffer[custom_info_temp * flash_page_len]
			+ (flash_tmp_buffer[custom_info_temp * flash_page_len + 1] << 8)
			+ (flash_tmp_buffer[custom_info_temp * flash_page_len + 2] << 16)
			+ (flash_tmp_buffer[custom_info_temp * flash_page_len + 3] << 24);

		input_info(true, &private_ts->client->dev,
				"%s Now custom_info_temp = %d\n", HIMAX_LOG_TAG,
				custom_info_temp);

		input_info(true, &private_ts->client->dev,
				"%s Now size_buff[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
				HIMAX_LOG_TAG,
				flash_tmp_buffer[custom_info_temp * flash_page_len],
				flash_tmp_buffer[custom_info_temp * flash_page_len + 1],
				flash_tmp_buffer[custom_info_temp * flash_page_len + 2],
				flash_tmp_buffer[custom_info_temp * flash_page_len + 3]);

		input_info(true, &private_ts->client->dev,
				"%s Now total length=%d \n", HIMAX_LOG_TAG,
				info_len);

		g_guest_info_data->g_guest_data_len[custom_info_temp] =
			info_len;

		input_info(true, &private_ts->client->dev,
				"%s Now custom_info_id [0]=%d,[1]=%d,[2]=%d,[3]=%d\n",
				HIMAX_LOG_TAG,
				flash_tmp_buffer[custom_info_temp * flash_page_len + 4],
				flash_tmp_buffer[custom_info_temp * flash_page_len + 5],
				flash_tmp_buffer[custom_info_temp * flash_page_len + 6],
				flash_tmp_buffer[custom_info_temp * flash_page_len + 7]);

		g_guest_info_data->g_guest_data_type[custom_info_temp] =
			flash_tmp_buffer[custom_info_temp * flash_page_len + 7];

		/* if(custom_info_temp < 3) { */
		if (info_len > 128) {
			input_info(true, &private_ts->client->dev,
					"%s %s: info_len=%d\n", HIMAX_LOG_TAG,
					__func__, info_len);
			info_len = 128;
		}
		for (i = 0; i < info_len; i++)
			temp_str[i] =
				flash_tmp_buffer[custom_info_temp * flash_page_len +
						HX_GUEST_INFO_LEN_SIZE +
						HX_GUEST_INFO_ID_SIZE + i];
		input_info(true, &private_ts->client->dev,
				"%s g_guest_info_data->g_guest_str_in_format[%d] size = %d\n",
				HIMAX_LOG_TAG, custom_info_temp, info_len);
		memcpy(&g_guest_info_data->
				 g_guest_str_in_format[custom_info_temp][0], temp_str,
				info_len);
		/*}*/

		for (i = 0; i < 128; i++)
			temp_str[i] =
				flash_tmp_buffer[custom_info_temp * flash_page_len + i];

		input_info(true, &private_ts->client->dev,
				"%s g_guest_info_data->g_guest_str[%d] size = %d\n",
				HIMAX_LOG_TAG, custom_info_temp, 128);
		memcpy(&g_guest_info_data->g_guest_str[custom_info_temp][0],
				temp_str, 128);
		/*if(custom_info_temp == 0)
		{
			for( i = 0; i< 256 ; i++)
			{
				if(i % 16 == 0 && i > 0)
					input_info(true, &private_ts->client->dev, "\n");
				input_info(true, &private_ts->client->dev, "%s g_guest_info_data->g_guest_str[%d][%d] = 0x%02X", HIMAX_LOG_TAG, custom_info_temp,i,g_guest_info_data->g_guest_str[custom_info_temp][i]);
			}
		}*/
	}
	/* himax_burst_enable(private_ts->client, 0); */

	g_core_fp.fp_sense_on(0x01);

	kfree(flash_tmp_buffer);
	himax_guest_info_set_status(0);
	return NO_ERR;
}
#endif
#endif

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) || defined(HX_USB_DETECT_GLOBAL)
static void himax_mcu_resend_cmd_func(bool suspended)
{
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE)
	struct himax_ts_data *ts = private_ts;
#endif
#ifdef HX_SMART_WAKEUP
	g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable, suspended);
#endif
#ifdef HX_HIGH_SENSE
	g_core_fp.fp_set_HSEN_enable(ts->HSEN_enable, suspended);
#endif
#ifdef HX_USB_DETECT_GLOBAL
	himax_cable_detect_func(true);
#endif
}
#endif

#ifdef HX_ZERO_FLASH
int G_POWERONOF = 1;

void hx_dis_rload_0f(int disable)
{
/*Diable Flash Reload*/
	g_core_fp.fp_register_write(pzf_op->addr_dis_flash_reload, DATA_LEN_4,
					pzf_op->data_dis_flash_reload, 0);
}

void himax_mcu_clean_sram_0f(uint8_t * addr, int write_len, int type)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;

	uint8_t fix_data = 0x00;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[MAX_I2C_TRANS_SZ] = { 0 };

	input_info(true, &private_ts->client->dev, "%s %s: Entering \n",
			HIMAX_LOG_TAG, __func__);

	total_size = write_len;

	if (total_size > 4096) {
		max_bus_size = 4096;
	}

	total_size_temp = write_len;

	g_core_fp.fp_burst_enable(1);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	input_info(false, &private_ts->client->dev,
			"%s %s, write addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
			HIMAX_LOG_TAG, __func__, tmp_addr[3], tmp_addr[2],
			tmp_addr[1], tmp_addr[0]);

	switch (type) {
	case 0:
		fix_data = 0x00;
		break;
	case 1:
		fix_data = 0xAA;
		break;
	case 2:
		fix_data = 0xBB;
		break;
	}

	for (i = 0; i < MAX_I2C_TRANS_SZ; i++) {
		tmp_data[i] = fix_data;
	}

	input_info(true, &private_ts->client->dev, "%s %s, total size=%d\n",
			HIMAX_LOG_TAG, __func__, total_size);

	if (total_size_temp % max_bus_size == 0) {
		total_read_times = total_size_temp / max_bus_size;
	} else {
		total_read_times = total_size_temp / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		input_info(true, &private_ts->client->dev,
				"%s [log]write %d time start!\n", HIMAX_LOG_TAG, i);
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_register_write(tmp_addr, max_bus_size,
							tmp_data, 0);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			input_info(true, &private_ts->client->dev,
					"%s last total_size_temp=%d\n",
					HIMAX_LOG_TAG, total_size_temp);
			g_core_fp.fp_register_write(tmp_addr,
							total_size_temp %
							max_bus_size, tmp_data, 0);
		}
		address = ((i + 1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t) ((address >> 8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		msleep(10);
	}

	input_info(true, &private_ts->client->dev, "%s %s, END \n",
			HIMAX_LOG_TAG, __func__);
}

void himax_mcu_write_sram_0f(const struct firmware *fw_entry, uint8_t * addr,
				int start_index, uint32_t write_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int address = 0;
	int i = 0;

	uint8_t tmp_addr[4];
	uint8_t *tmp_data;

	total_size_temp = write_len;
	input_info(true, &private_ts->client->dev,
			"%s %s, Entering - total write size=%d\n", HIMAX_LOG_TAG,
			__func__, total_size_temp);

#if defined(HX_SPI_OPERATION)
	if (write_len > 4096) {
		max_bus_size = 4096;
	} else {
		max_bus_size = write_len;
	}
#else
	if (write_len > 240) {
		max_bus_size = 240;
	} else {
		max_bus_size = write_len;
	}
#endif

	g_core_fp.fp_burst_enable(1);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	input_info(true, &private_ts->client->dev,
			"%s %s, write addr = 0x%02X%02X%02X%02X\n", HIMAX_LOG_TAG,
			__func__, tmp_addr[3], tmp_addr[2], tmp_addr[1],
			tmp_addr[0]);

	tmp_data = kzalloc(sizeof(uint8_t) * max_bus_size, GFP_KERNEL);
	if (tmp_data == NULL) {
		input_info(true, &private_ts->client->dev,
				"%s %s: Can't allocate enough buf \n", HIMAX_LOG_TAG,
				__func__);
		return;
	}

	/*
	for(i = 0;i<10;i++)
	{
		input_info(true, &private_ts->client->dev, "[%d] 0x%2.2X", i, tmp_data[i]);
	}
	input_info(true, &private_ts->client->dev, "\n");
	*/
	if (total_size_temp % max_bus_size == 0) {
		total_read_times = total_size_temp / max_bus_size;
	} else {
		total_read_times = total_size_temp / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		/*input_info(true, &private_ts->client->dev, "[log]write %d time start!\n", i);
		input_info(true, &private_ts->client->dev, "[log]addr[3]=0x%02X, addr[2]=0x%02X, addr[1]=0x%02X, addr[0]=0x%02X!\n", tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);*/

		if (total_size_temp >= max_bus_size) {
			memcpy(tmp_data,
					&fw_entry->data[start_index + i * max_bus_size],
					max_bus_size);
			g_core_fp.fp_register_write(tmp_addr, max_bus_size,
							tmp_data, 0);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			memcpy(tmp_data,
					&fw_entry->data[start_index + i * max_bus_size],
					total_size_temp % max_bus_size);
			input_info(true, &private_ts->client->dev,
					"%s last total_size_temp=%d\n",
					HIMAX_LOG_TAG,
					total_size_temp % max_bus_size);
			g_core_fp.fp_register_write(tmp_addr,
							total_size_temp %
							max_bus_size, tmp_data, 0);
		}

		/*input_info(true, &private_ts->client->dev, "[log]write %d time end!\n", i);*/
		address = ((i + 1) * max_bus_size);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		if (tmp_addr[0] < addr[0]) {
			tmp_addr[1] =
				addr[1] + (uint8_t) ((address >> 8) & 0x00FF) + 1;
		} else {
			tmp_addr[1] =
				addr[1] + (uint8_t) ((address >> 8) & 0x00FF);
		}

		udelay(100);
	}
	input_info(true, &private_ts->client->dev, "%s %s, End \n",
			HIMAX_LOG_TAG, __func__);
	kfree(tmp_data);
}

int himax_sram_write_crc_check(const struct firmware *fw_entry, uint8_t * addr,
				int strt_idx, uint32_t len)
{
	int retry = 0;
	int crc = -1;

	do {
		g_core_fp.fp_write_sram_0f(fw_entry, addr, strt_idx, len);
		crc = g_core_fp.fp_check_CRC(addr, len);
		retry++;
		input_info(true, &private_ts->client->dev,
				"%s %s, HW CRC %s in %d time \n", HIMAX_LOG_TAG,
				__func__, (crc == 0) ? "OK" : "Fail", retry);
	} while (crc != 0 && retry < 3);

	return crc;
}

int himax_zf_part_info(const struct firmware *fw_entry)
{
	int part_num = 0;
	int ret = 0;
	int i = 0;
	uint8_t buf[16];
	struct zf_info *zf_info_arr;

	/*1. get number of partition*/
	part_num = fw_entry->data[HX64K + 12];
	input_info(true, &private_ts->client->dev,
			"%s %s, Number of partition is %d\n", HIMAX_LOG_TAG,
			__func__, part_num);
	if (part_num <= 0)
		part_num = 1;

	/*2. initial struct of array*/
	zf_info_arr = kzalloc(part_num * sizeof(struct zf_info), GFP_KERNEL);
	if (zf_info_arr == NULL) {
		input_err(true, &private_ts->client->dev,
				"%s %s, Allocate ZF info array failed!\n",
				HIMAX_LOG_TAG, __func__);
		return MEM_ALLOC_FAIL;
	}

	for (i = 0; i < part_num; i++) {
		/*3. get all partition*/
		memcpy(buf, &fw_entry->data[i * 0x10 + HX64K], 16);
		memcpy(zf_info_arr[i].sram_addr, buf, 4);
		zf_info_arr[i].write_size = buf[5] << 8 | buf[4];
		zf_info_arr[i].fw_addr = buf[9] << 8 | buf[8];
		input_info(true, &private_ts->client->dev,
				"%s %s,[%d] SRAM addr = %02X%02X%02X%02X!\n",
				HIMAX_LOG_TAG, __func__, i,
				zf_info_arr[i].sram_addr[3],
				zf_info_arr[i].sram_addr[2],
				zf_info_arr[i].sram_addr[1],
				zf_info_arr[i].sram_addr[0]);
		input_info(true, &private_ts->client->dev,
				"%s %s,[%d] fw_addr = %04X!\n", HIMAX_LOG_TAG,
				__func__, i, zf_info_arr[i].fw_addr);
		input_info(true, &private_ts->client->dev,
				"%s %s,[%d] write_size = %d!\n", HIMAX_LOG_TAG,
				__func__, i, zf_info_arr[i].write_size);

/*4. write to sram*/
		if (G_POWERONOF == 1) {
			if (himax_sram_write_crc_check
					(fw_entry, zf_info_arr[i].sram_addr,
					zf_info_arr[i].fw_addr,
					zf_info_arr[i].write_size) != 0) {
				input_err(true, &private_ts->client->dev,
						"%s %s, HW CRC FAIL\n", HIMAX_LOG_TAG,
						__func__);
			}
		} else {
			g_core_fp.fp_clean_sram_0f(zf_info_arr[i].sram_addr,
							zf_info_arr[i].write_size, 2);
		}
	}

	kfree(zf_info_arr);

	return ret;
}

void himax_mcu_firmware_update_0f(const struct firmware *fw_entry)
{
	int ret = 0;

	input_info(true, &private_ts->client->dev,
			"%s %s,Entering - total FW size=%d\n", HIMAX_LOG_TAG,
			__func__, (int)fw_entry->size);

	g_core_fp.fp_register_write(pzf_op->addr_system_reset, 4,
					pzf_op->data_system_reset, 0);

	g_core_fp.fp_sense_off(false);

	if ((int)fw_entry->size > HX64K) {
		ret = himax_zf_part_info(fw_entry);
	} else {
		/* first 48K */
		if (himax_sram_write_crc_check
			(fw_entry, pzf_op->data_sram_start_addr, 0, HX_48K_SZ) != 0)
			input_err(true, &private_ts->client->dev,
					"%s %s, HW CRC FAIL - Main SRAM 48K\n",
					HIMAX_LOG_TAG, __func__);

/*config info*/
		if (G_POWERONOF == 1) {
			if (himax_sram_write_crc_check
				(fw_entry, pzf_op->data_cfg_info, 0xC000, 128) != 0)
				input_err(true, &private_ts->client->dev,
						"%s %s: Config info CRC Fail!\n",
						HIMAX_LOG_TAG, __func__);
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_cfg_info, 128, 2);
		}

		if (G_POWERONOF == 1) {
			if (himax_sram_write_crc_check
				(fw_entry, pzf_op->data_fw_cfg_1, 0xC0FE, 528) != 0)
				input_err(true, &private_ts->client->dev,
						"%s %s: FW config 1 CRC Fail!\n",
						HIMAX_LOG_TAG, __func__);
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_fw_cfg_1, 528, 1);
		}

		if (G_POWERONOF == 1) {
			if (himax_sram_write_crc_check
				(fw_entry, pzf_op->data_fw_cfg_3, 0xCA00, 128) != 0)
				input_err(true, &private_ts->client->dev,
						"%s %s: FW config 3 CRC Fail!\n",
						HIMAX_LOG_TAG, __func__);
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_fw_cfg_3, 128, 2);
		}

		/*ADC config*/
		if (G_POWERONOF == 1) {
			if (himax_sram_write_crc_check
				(fw_entry, pzf_op->data_adc_cfg_1, 0xD640, 1200) != 0)
				input_err(true, &private_ts->client->dev,
						"%s %s: ADC config 1 CRC Fail!\n",
						HIMAX_LOG_TAG, __func__);
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_1, 1200, 2);
		}

		if (G_POWERONOF == 1) {
			if (himax_sram_write_crc_check
				(fw_entry, pzf_op->data_adc_cfg_2, 0xD320, 800) != 0)
				input_err(true, &private_ts->client->dev,
						"%s %s: ADC config 2 CRC Fail!\n",
						HIMAX_LOG_TAG, __func__);
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_2, 800, 2);
		}

		/*mapping table*/
		if (G_POWERONOF == 1) {
			if (himax_sram_write_crc_check
				(fw_entry, pzf_op->data_map_table, 0xE000, 1536) != 0)
				input_err(true, &private_ts->client->dev,
						"%s %s: Mapping table CRC Fail!\n",
						HIMAX_LOG_TAG, __func__);
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_map_table, 1536, 2);
		}
	}

	/* set n frame=0*/
	if (G_POWERONOF == 1) {
		g_core_fp.fp_write_sram_0f(fw_entry, pzf_op->data_mode_switch,
						0xC30C, 4);
	} else {
		g_core_fp.fp_clean_sram_0f(pzf_op->data_mode_switch, 4, 2);
	}

	input_info(true, &private_ts->client->dev, "%s %s, End \n",
			HIMAX_LOG_TAG, __func__);
}

int hx_0f_op_file_dirly(char *file_name)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;

	input_info(true, &private_ts->client->dev, "%s %s:file name = %s\n",
			HIMAX_LOG_TAG, __func__, file_name);
	err = request_firmware(&fw_entry, file_name, private_ts->dev);
	if (err < 0) {
		input_err(true, &private_ts->client->dev,
				"%s %s, fail in line%d error code=%d,file maybe fail\n",
				HIMAX_LOG_TAG, __func__, __LINE__, err);
		return err;
	}

	himax_int_enable(0);

	if (g_f_0f_updat == 1) {
		input_info(true, &private_ts->client->dev,
				"%s %s:[Warning]Other thread is updating now!\n",
				HIMAX_LOG_TAG, __func__);
		err = -1;
		return err;
	} else {
		input_info(true, &private_ts->client->dev,
				"%s %s:Entering Update Flow!\n", HIMAX_LOG_TAG,
				__func__);
		g_f_0f_updat = 1;
	}

	g_core_fp.fp_firmware_update_0f(fw_entry);
	release_firmware(fw_entry);

	g_f_0f_updat = 0;
	input_info(true, &private_ts->client->dev, "%s %s, END \n",
			HIMAX_LOG_TAG, __func__);
	return err;
}

int himax_mcu_0f_operation_dirly(void)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;

	input_info(true, &private_ts->client->dev, "%s %s: file name = %s\n",
			HIMAX_LOG_TAG, __func__,
			private_ts->pdata->i_CTPM_firmware_name);
	err =
		request_firmware(&fw_entry, private_ts->pdata->i_CTPM_firmware_name,
				private_ts->dev);
	if (err < 0) {
		input_err(true, &private_ts->client->dev,
				"%s %s, fail in line%d error code=%d,file maybe fail\n",
				HIMAX_LOG_TAG, __func__, __LINE__, err);
		return err;
	}

	himax_int_enable(0);

	if (g_f_0f_updat == 1) {
		input_info(true, &private_ts->client->dev,
				"%s %s:[Warning]Other thread is updating now!\n",
				HIMAX_LOG_TAG, __func__);
		err = -1;
		return err;
	} else {
		input_info(true, &private_ts->client->dev,
				"%s %s:Entering Update Flow!\n", HIMAX_LOG_TAG,
				__func__);
		g_f_0f_updat = 1;
	}

	g_core_fp.fp_firmware_update_0f(fw_entry);
	release_firmware(fw_entry);

	g_f_0f_updat = 0;
	input_info(true, &private_ts->client->dev, "%s %s, END \n",
			HIMAX_LOG_TAG, __func__);
	return err;
}

void himax_mcu_0f_operation(struct work_struct *work)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;

	input_info(true, &private_ts->client->dev, "%s %s: file name = %s\n",
			HIMAX_LOG_TAG, __func__,
			private_ts->pdata->i_CTPM_firmware_name);
	err =
		request_firmware(&fw_entry, private_ts->pdata->i_CTPM_firmware_name,
				private_ts->dev);
	if (err < 0) {
		input_err(true, &private_ts->client->dev,
				"% %s, fail in line%d error code=%d,file maybe fail\n",
				HIMAX_LOG_TAG, __func__, __LINE__, err);
		return;
	}

	if (g_f_0f_updat == 1) {
		input_info(true, &private_ts->client->dev,
				"%s %s:[Warning]Other thread is updating now!\n",
				HIMAX_LOG_TAG, __func__);
		return;
	} else {
		input_info(true, &private_ts->client->dev,
				"%s %s:Entering Update Flow!\n", HIMAX_LOG_TAG,
				__func__);
		g_f_0f_updat = 1;
	}

	himax_int_enable(0);

	g_core_fp.fp_firmware_update_0f(fw_entry);
	release_firmware(fw_entry);

	g_core_fp.fp_reload_disable(0);
	msleep(10);
	g_core_fp.fp_read_FW_ver();
	g_core_fp.fp_touch_information();
	msleep(10);
	g_core_fp.fp_sense_on(0x00);
	msleep(10);
	himax_int_enable(1);

	g_f_0f_updat = 0;
	input_info(true, &private_ts->client->dev, "%s %s, END \n",
			HIMAX_LOG_TAG, __func__);
	return;
}

static int himax_mcu_0f_esd_check(void)
{
	return NO_ERR;
}

#ifdef HX_0F_DEBUG
void himax_mcu_read_sram_0f(const struct firmware *fw_entry, uint8_t * addr,
				int start_index, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0, j = 0;
	int not_same = 0;

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data;
	int *not_same_buff;

	input_info(true, &private_ts->client->dev, "%s %s, Entering \n",
			HIMAX_LOG_TAG, __func__);

	g_core_fp.fp_burst_enable(1);

	total_size = read_len;

	total_size_temp = read_len;

#if defined(HX_SPI_OPERATION)
	if (read_len > 2048) {
		max_bus_size = 2048;
	} else {
		max_bus_size = read_len;
	}
#else
	if (read_len > 240) {
		max_bus_size = 240;
	} else {
		max_bus_size = read_len;
	}
#endif

	temp_info_data = kzalloc(sizeof(uint8_t) * total_size, GFP_KERNEL);
	not_same_buff = kzalloc(sizeof(int) * total_size, GFP_KERNEL);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	input_info(true, &private_ts->client->dev,
			"%s %s,  read addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
			HIMAX_LOG_TAG, __func__, tmp_addr[3], tmp_addr[2],
			tmp_addr[1], tmp_addr[0]);

	input_info(true, &private_ts->client->dev, "%s %s,  total size=%d\n",
			HIMAX_LOG_TAG, __func__, total_size);

	g_core_fp.fp_burst_enable(1);

	if (total_size % max_bus_size == 0) {
		total_read_times = total_size / max_bus_size;
	} else {
		total_read_times = total_size / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_register_read(tmp_addr, max_bus_size,
						&temp_info_data[i * max_bus_size],
						false);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			g_core_fp.fp_register_read(tmp_addr,
						total_size_temp % max_bus_size,
						&temp_info_data[i * max_bus_size],
						false);
		}

		address = ((i + 1) * max_bus_size);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);
		if (tmp_addr[0] < addr[0]) {
			tmp_addr[1] =
				addr[1] + (uint8_t) ((address >> 8) & 0x00FF) + 1;
		} else {
			tmp_addr[1] =
				addr[1] + (uint8_t) ((address >> 8) & 0x00FF);
		}

		msleep(10);
	}
	input_info(true, &private_ts->client->dev, "%s %s, READ Start \n",
			HIMAX_LOG_TAG, __func__);
	input_info(true, &private_ts->client->dev, "%s %s, start_index = %d \n",
			HIMAX_LOG_TAG, __func__, start_index);
	j = start_index;
	for (i = 0; i < read_len; i++, j++) {
		if (fw_entry->data[j] != temp_info_data[i]) {
			not_same++;
			not_same_buff[i] = 1;
		}

		input_info(true, &private_ts->client->dev, "0x%2.2X, ",
				temp_info_data[i]);

		if (i > 0 && i % 16 == 15) {
			printk("\n");
		}
	}
	input_info(true, &private_ts->client->dev, "%s %s, READ END \n",
			HIMAX_LOG_TAG, __func__);
	input_info(true, &private_ts->client->dev, "%s %s, Not Same count=%d\n",
			HIMAX_LOG_TAG, __func__, not_same);
	if (not_same != 0) {
		j = start_index;
		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1) {
				input_info(true, &private_ts->client->dev,
						"%s bin = [%d] 0x%2.2X\n",
						HIMAX_LOG_TAG, i, fw_entry->data[j]);
			}
		}
		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1) {
				input_info(true, &private_ts->client->dev,
						"%s sram = [%d] 0x%2.2X \n",
						HIMAX_LOG_TAG, i, temp_info_data[i]);
			}
		}
	}
	input_info(true, &private_ts->client->dev, "%s %s, READ END \n",
			HIMAX_LOG_TAG, __func__);
	input_info(true, &private_ts->client->dev, "%s %s, Not Same count=%d\n",
			HIMAX_LOG_TAG, __func__, not_same);
	input_info(true, &private_ts->client->dev, "%s %s, END \n",
			HIMAX_LOG_TAG, __func__);

	kfree(not_same_buff);
	kfree(temp_info_data);
}

void himax_mcu_read_all_sram(uint8_t * addr, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;
	/*
	struct file *fn;
	struct filename *vts_name;
	*/

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data;

	input_info(true, &private_ts->client->dev, "%s %s, Entering \n",
			HIMAX_LOG_TAG, __func__);

	g_core_fp.fp_burst_enable(1);

	total_size = read_len;

	total_size_temp = read_len;

	temp_info_data = kzalloc(sizeof(uint8_t) * total_size, GFP_KERNEL);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	input_info(true, &private_ts->client->dev,
			"%s %s,  read addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
			HIMAX_LOG_TAG, __func__, tmp_addr[3], tmp_addr[2],
			tmp_addr[1], tmp_addr[0]);

	input_info(true, &private_ts->client->dev, "%s %s,  total size=%d\n",
			HIMAX_LOG_TAG, __func__, total_size);

	if (total_size % max_bus_size == 0) {
		total_read_times = total_size / max_bus_size;
	} else {
		total_read_times = total_size / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_register_read(tmp_addr, max_bus_size,
						&temp_info_data[i * max_bus_size],
						false);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			g_core_fp.fp_register_read(tmp_addr,
						total_size_temp % max_bus_size,
						&temp_info_data[i * max_bus_size],
						false);
		}

		address = ((i + 1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t) ((address >> 8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		msleep(10);
	}
	input_info(true, &private_ts->client->dev,
			"%s %s,  NOW addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
			HIMAX_LOG_TAG, __func__, tmp_addr[3], tmp_addr[2],
			tmp_addr[1], tmp_addr[0]);
	/*for(i = 0;i<read_len;i++)
	{
		input_info(true, &private_ts->client->dev, "0x%2.2X, ", temp_info_data[i]);

		if (i > 0 && i%16 == 15)
		printk("\n");
	}*/

	/* need modify
	input_info(true, &private_ts->client->dev, "%s Now Write File start!\n", HIMAX_LOG_TAG);
	vts_name = getname_kernel("/sdcard/dump_dsram.txt");
	fn = file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
	if (!IS_ERR (fn)) {
		input_info(true, &private_ts->client->dev, "%s %s create file and ready to write\n", HIMAX_LOG_TAG, __func__);
		fn->f_op->write (fn, temp_info_data, read_len*sizeof (uint8_t), &fn->f_pos);
		filp_close (fn, NULL);
	}
	input_info(true, &private_ts->client->dev, "%s Now Write File End!\n", HIMAX_LOG_TAG);
	*/

	input_info(true, &private_ts->client->dev, "%s %s, END \n",
			HIMAX_LOG_TAG, __func__);

	kfree(temp_info_data);
}

void himax_mcu_firmware_read_0f(const struct firmware *fw_entry, int type)
{
	uint8_t tmp_addr[4];

	input_info(true, &private_ts->client->dev, "%s %s, Entering \n",
			HIMAX_LOG_TAG, __func__);
	if (type == 0) {	/* first 48K */
		g_core_fp.fp_read_sram_0f(fw_entry,
					pzf_op->data_sram_start_addr, 0,
					HX_48K_SZ);
		g_core_fp.fp_read_all_sram(tmp_addr, 0xC000);
	} else {		/*last 16k */
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_cfg_info,
					0xC000, 132);

		/*FW config*/
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_fw_cfg_1,
					0xC0FE, 484);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_fw_cfg_2,
					0xC9DE, 36);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_fw_cfg_3,
					0xCA00, 72);

		/*ADC config*/

		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_adc_cfg_1,
					0xD630, 1188);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_adc_cfg_2,
					0xD318, 792);

		/*mapping table*/
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_map_table,
					0xE000, 1536);

		/* set n frame=0*/
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_mode_switch,
					0xC30C, 4);
	}

	input_info(true, &private_ts->client->dev, "%s %s, END \n",
			HIMAX_LOG_TAG, __func__);
}

void himax_mcu_0f_operation_check(int type)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;
	/* char *firmware_name = "himax.bin"; */

	input_info(true, &private_ts->client->dev, "%s %s:file name = %s\n",
			HIMAX_LOG_TAG, __func__,
			private_ts->pdata->i_CTPM_firmware_name);

	err =
		request_firmware(&fw_entry, private_ts->pdata->i_CTPM_firmware_name,
				private_ts->dev);
	if (err < 0) {
		input_err(true, &private_ts->client->dev,
				"%s %s, fail in line%d error code=%d\n",
				HIMAX_LOG_TAG, __func__, __LINE__, err);
		return;
	}

	input_info(true, &private_ts->client->dev,
			"%s first 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
			HIMAX_LOG_TAG, fw_entry->data[0], fw_entry->data[1],
			fw_entry->data[2], fw_entry->data[3]);
	input_info(true, &private_ts->client->dev,
			"%s next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
			HIMAX_LOG_TAG, fw_entry->data[4], fw_entry->data[5],
			fw_entry->data[6], fw_entry->data[7]);
	input_info(true, &private_ts->client->dev,
			"%s and next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
			HIMAX_LOG_TAG, fw_entry->data[8], fw_entry->data[9],
			fw_entry->data[10], fw_entry->data[11]);

	g_core_fp.fp_firmware_read_0f(fw_entry, type);

	release_firmware(fw_entry);
	input_info(true, &private_ts->client->dev, "%s %s, END \n",
			HIMAX_LOG_TAG, __func__);
	return;
}
#endif

#endif

#ifdef CORE_INIT
/* init start */
static void himax_mcu_fp_init(void)
{
#ifdef CORE_IC
	g_core_fp.fp_burst_enable = himax_mcu_burst_enable;
	g_core_fp.fp_register_read = himax_mcu_register_read;
	/*g_core_fp.fp_flash_write_burst = himax_mcu_flash_write_burst;*/
	/*g_core_fp.fp_flash_write_burst_lenth = himax_mcu_flash_write_burst_lenth;*/
	g_core_fp.fp_register_write = himax_mcu_register_write;
	g_core_fp.fp_interface_on = himax_mcu_interface_on;
	g_core_fp.fp_sense_on = himax_mcu_sense_on;
	g_core_fp.fp_sense_off = himax_mcu_sense_off;
	g_core_fp.fp_wait_wip = himax_mcu_wait_wip;
	g_core_fp.fp_init_psl = himax_mcu_init_psl;
	g_core_fp.fp_resume_ic_action = himax_mcu_resume_ic_action;
	g_core_fp.fp_suspend_ic_action = himax_mcu_suspend_ic_action;
	g_core_fp.fp_power_on_init = himax_mcu_power_on_init;
	g_core_fp.set_edge_border = hx_mcu_set_edge_border;
	g_core_fp.set_cal_switch = hx_mcu_set_cal_switch;
	g_core_fp.get_rport_thrsh = hx_mcu_get_rport_thrsh;
#endif
#ifdef CORE_FW
	g_core_fp.fp_system_reset = himax_mcu_system_reset;
	g_core_fp.fp_Calculate_CRC_with_AP = himax_mcu_Calculate_CRC_with_AP;
	g_core_fp.fp_check_CRC = himax_mcu_check_CRC;
	g_core_fp.fp_set_reload_cmd = himax_mcu_set_reload_cmd;
	g_core_fp.fp_program_reload = himax_mcu_program_reload;
#ifdef HX_P_SENSOR
	g_core_fp.fp_ulpm_in = himax_mcu_ulpm_in;
	g_core_fp.fp_black_gest_ctrl = himax_mcu_black_gest_ctrl;
#endif
	g_core_fp.fp_set_SMWP_enable = himax_mcu_set_SMWP_enable;
	g_core_fp.fp_set_HSEN_enable = himax_mcu_set_HSEN_enable;
	g_core_fp.fp_usb_detect_set = himax_mcu_usb_detect_set;
	g_core_fp.fp_diag_register_set = himax_mcu_diag_register_set;
	g_core_fp.fp_chip_self_test = himax_mcu_chip_self_test;
	g_core_fp.fp_idle_mode = himax_mcu_idle_mode;
	g_core_fp.fp_reload_disable = himax_mcu_reload_disable;
	g_core_fp.fp_check_chip_version = himax_mcu_check_chip_version;
	g_core_fp.fp_read_ic_trigger_type = himax_mcu_read_ic_trigger_type;
	g_core_fp.fp_read_i2c_status = himax_mcu_read_i2c_status;
	g_core_fp.fp_read_FW_ver = himax_mcu_read_FW_ver;
	g_core_fp.fp_read_event_stack = himax_mcu_read_event_stack;
	g_core_fp.fp_return_event_stack = himax_mcu_return_event_stack;
	g_core_fp.fp_calculateChecksum = himax_mcu_calculateChecksum;
	g_core_fp.fp_read_FW_status = himax_mcu_read_FW_status;
	g_core_fp.fp_irq_switch = himax_mcu_irq_switch;
	g_core_fp.fp_assign_sorting_mode = himax_mcu_assign_sorting_mode;
	g_core_fp.fp_check_sorting_mode = himax_mcu_check_sorting_mode;
	g_core_fp.fp_switch_mode = himax_mcu_switch_mode;
	g_core_fp.fp_get_max_dc = himax_mcu_get_max_dc;
	g_core_fp.fp_read_DD_status = himax_mcu_read_DD_status;
#endif
#ifdef CORE_FLASH
	g_core_fp.fp_chip_erase = himax_mcu_chip_erase;
	g_core_fp.fp_block_erase = himax_mcu_block_erase;
	g_core_fp.fp_sector_erase = himax_mcu_sector_erase;
	g_core_fp.fp_flash_programming = himax_mcu_flash_programming;
	g_core_fp.fp_flash_programming_sz = hx_mcu_flash_programming_sz;
	g_core_fp.fp_flash_page_write = himax_mcu_flash_page_write;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k =
		himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_32k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k =
		himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_60k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k =
		himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k =
		himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_124k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k =
		himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k;
	g_core_fp.fp_flash_dump_func = himax_mcu_flash_dump_func;
	g_core_fp.fp_flash_lastdata_check = himax_mcu_flash_lastdata_check;
#endif
#ifdef CORE_SRAM
	g_core_fp.fp_sram_write = himax_mcu_sram_write;
	g_core_fp.fp_sram_verify = himax_mcu_sram_verify;
	g_core_fp.fp_get_DSRAM_data = himax_mcu_get_DSRAM_data;
#endif
#ifdef CORE_DRIVER
	g_core_fp.fp_chip_init = himax_mcu_init_ic;
#ifdef HX_AUTO_UPDATE_FW
	g_core_fp.fp_fw_ver_bin = himax_mcu_fw_ver_bin;
#endif
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_pin_reset = himax_mcu_pin_reset;
	g_core_fp.fp_ic_reset = himax_mcu_ic_reset;
#endif
	g_core_fp.fp_touch_information = himax_mcu_touch_information;
	g_core_fp.fp_reload_config = himax_mcu_reload_config;
	g_core_fp.fp_get_touch_data_size = himax_mcu_get_touch_data_size;
	g_core_fp.fp_hand_shaking = himax_mcu_hand_shaking;
	g_core_fp.fp_determin_diag_rawdata = himax_mcu_determin_diag_rawdata;
	g_core_fp.fp_determin_diag_storage = himax_mcu_determin_diag_storage;
	g_core_fp.fp_cal_data_len = himax_mcu_cal_data_len;
	g_core_fp.fp_diag_check_sum = himax_mcu_diag_check_sum;
	g_core_fp.fp_diag_parse_raw_data = himax_mcu_diag_parse_raw_data;
#ifdef HX_ESD_RECOVERY
	g_core_fp.fp_ic_esd_recovery = himax_mcu_ic_esd_recovery;
	g_core_fp.fp_esd_ic_reset = himax_mcu_esd_ic_reset;
#endif
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) || defined(HX_USB_DETECT_GLOBAL)
	g_core_fp.fp_resend_cmd_func = himax_mcu_resend_cmd_func;
#endif
#if defined(HX_TP_PROC_GUEST_INFO)
	g_core_fp.guest_info_get_status = himax_guest_info_get_status;
	g_core_fp.read_guest_info = hx_read_guest_info;
#endif
#endif
#ifdef HX_ZERO_FLASH
	g_core_fp.fp_reload_disable = hx_dis_rload_0f;
	g_core_fp.fp_clean_sram_0f = himax_mcu_clean_sram_0f;
	g_core_fp.fp_write_sram_0f = himax_mcu_write_sram_0f;
	g_core_fp.fp_firmware_update_0f = himax_mcu_firmware_update_0f;
	g_core_fp.fp_0f_operation = himax_mcu_0f_operation;
	g_core_fp.fp_0f_operation_dirly = himax_mcu_0f_operation_dirly;
	g_core_fp.fp_0f_op_file_dirly = hx_0f_op_file_dirly;
	g_core_fp.fp_0f_esd_check = himax_mcu_0f_esd_check;
#ifdef HX_0F_DEBUG
	g_core_fp.fp_read_sram_0f = himax_mcu_read_sram_0f;
	g_core_fp.fp_read_all_sram = himax_mcu_read_all_sram;
	g_core_fp.fp_firmware_read_0f = himax_mcu_firmware_read_0f;
	g_core_fp.fp_0f_operation_check = himax_mcu_0f_operation_check;
#endif
#endif
}

int himax_mcu_in_cmd_struct_init(void)
{
	int err = 0;
	input_info(true, &private_ts->client->dev, "%s %s: Entering!\n",
			HIMAX_LOG_TAG, __func__);
	g_core_cmd_op =
		kzalloc(sizeof(struct himax_core_command_operation), GFP_KERNEL);
	if (g_core_cmd_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_fail;
	}
	g_core_cmd_op->ic_op = kzalloc(sizeof(struct ic_operation), GFP_KERNEL);
	if (g_core_cmd_op->ic_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_ic_op_fail;
	}
	g_core_cmd_op->fw_op = kzalloc(sizeof(struct fw_operation), GFP_KERNEL);
	if (g_core_cmd_op->fw_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_fw_op_fail;
	}
	g_core_cmd_op->flash_op =
		kzalloc(sizeof(struct flash_operation), GFP_KERNEL);
	if (g_core_cmd_op->flash_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_flash_op_fail;
	}
	g_core_cmd_op->sram_op =
		kzalloc(sizeof(struct sram_operation), GFP_KERNEL);
	if (g_core_cmd_op->sram_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_sram_op_fail;
	}
	g_core_cmd_op->driver_op =
		kzalloc(sizeof(struct driver_operation), GFP_KERNEL);
	if (g_core_cmd_op->driver_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_driver_op_fail;
	}
	pic_op = g_core_cmd_op->ic_op;
	pfw_op = g_core_cmd_op->fw_op;
	pflash_op = g_core_cmd_op->flash_op;
	psram_op = g_core_cmd_op->sram_op;
	pdriver_op = g_core_cmd_op->driver_op;
#ifdef HX_ZERO_FLASH
	g_core_cmd_op->zf_op = kzalloc(sizeof(struct zf_operation), GFP_KERNEL);
	pzf_op = g_core_cmd_op->zf_op;
	if (g_core_cmd_op->zf_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_zf_op_fail;
	}
#endif
	himax_mcu_fp_init();
	return NO_ERR;
#ifdef HX_ZERO_FLASH
err_g_core_cmd_op_zf_op_fail:
#endif
	kfree(g_core_cmd_op->driver_op);
err_g_core_cmd_op_driver_op_fail:
	kfree(g_core_cmd_op->sram_op);
err_g_core_cmd_op_sram_op_fail:
	kfree(g_core_cmd_op->flash_op);
err_g_core_cmd_op_flash_op_fail:
	kfree(g_core_cmd_op->fw_op);
err_g_core_cmd_op_fw_op_fail:
	kfree(g_core_cmd_op->ic_op);
err_g_core_cmd_op_ic_op_fail:
	kfree(g_core_cmd_op);
err_g_core_cmd_op_fail:
	return err;
}

/*
static void himax_mcu_in_cmd_struct_free(void)
{
	pic_op = NULL;
	pfw_op = NULL;
	pflash_op = NULL;
	psram_op = NULL;
	pdriver_op = NULL;
	kfree(g_core_cmd_op);
	kfree(g_core_cmd_op->ic_op);
	kfree(g_core_cmd_op->flash_op);
	kfree(g_core_cmd_op->sram_op);
	kfree(g_core_cmd_op->driver_op);
}
*/

void himax_in_parse_assign_cmd(uint32_t addr, uint8_t * cmd, int len)
{
	/*input_info(true, &private_ts->client->dev, "%s: Entering!\n", __func__);*/
	switch (len) {
	case 1:
		cmd[0] = addr;
		/*input_info(true, &private_ts->client->dev, "%s: cmd[0] = 0x%02X\n", __func__, cmd[0]);*/
		break;

	case 2:
		cmd[0] = addr % 0x100;
		cmd[1] = (addr >> 8) % 0x100;
		/*input_info(true, &private_ts->client->dev, "%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X\n", __func__, cmd[0], cmd[1]);*/
		break;

	case 4:
		cmd[0] = addr % 0x100;
		cmd[1] = (addr >> 8) % 0x100;
		cmd[2] = (addr >> 16) % 0x100;
		cmd[3] = addr / 0x1000000;
		/*  input_info(true, &private_ts->client->dev, "%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X,cmd[2] = 0x%02X,cmd[3] = 0x%02X\n",
		__func__, cmd[0], cmd[1], cmd[2], cmd[3]);*/
		break;

	default:
		input_err(true, &private_ts->client->dev,
				"%s %s: input length fault,len = %d!\n",
				HIMAX_LOG_TAG, __func__, len);
	}
}

void himax_mcu_in_cmd_init(void)
{
	input_info(true, &private_ts->client->dev, "%s %s: Entering!\n",
			HIMAX_LOG_TAG, __func__);
#ifdef CORE_IC
	himax_in_parse_assign_cmd(ic_adr_ahb_addr_byte_0,
				pic_op->addr_ahb_addr_byte_0,
				sizeof(pic_op->addr_ahb_addr_byte_0));
	himax_in_parse_assign_cmd(ic_adr_ahb_rdata_byte_0,
				pic_op->addr_ahb_rdata_byte_0,
				sizeof(pic_op->addr_ahb_rdata_byte_0));
	himax_in_parse_assign_cmd(ic_adr_ahb_access_direction,
				pic_op->addr_ahb_access_direction,
				sizeof(pic_op->addr_ahb_access_direction));
	himax_in_parse_assign_cmd(ic_adr_conti, pic_op->addr_conti,
				sizeof(pic_op->addr_conti));
	himax_in_parse_assign_cmd(ic_adr_incr4, pic_op->addr_incr4,
				sizeof(pic_op->addr_incr4));
	himax_in_parse_assign_cmd(ic_adr_i2c_psw_lb, pic_op->adr_i2c_psw_lb,
				sizeof(pic_op->adr_i2c_psw_lb));
	himax_in_parse_assign_cmd(ic_adr_i2c_psw_ub, pic_op->adr_i2c_psw_ub,
				sizeof(pic_op->adr_i2c_psw_ub));
	himax_in_parse_assign_cmd(ic_cmd_ahb_access_direction_read,
				pic_op->data_ahb_access_direction_read,
				sizeof(pic_op-> data_ahb_access_direction_read));
	himax_in_parse_assign_cmd(ic_cmd_conti, pic_op->data_conti,
				sizeof(pic_op->data_conti));
	himax_in_parse_assign_cmd(ic_cmd_incr4, pic_op->data_incr4,
				sizeof(pic_op->data_incr4));
	himax_in_parse_assign_cmd(ic_cmd_i2c_psw_lb, pic_op->data_i2c_psw_lb,
				sizeof(pic_op->data_i2c_psw_lb));
	himax_in_parse_assign_cmd(ic_cmd_i2c_psw_ub, pic_op->data_i2c_psw_ub,
				sizeof(pic_op->data_i2c_psw_ub));
	himax_in_parse_assign_cmd(ic_adr_tcon_on_rst, pic_op->addr_tcon_on_rst,
				sizeof(pic_op->addr_tcon_on_rst));
	himax_in_parse_assign_cmd(ic_addr_adc_on_rst, pic_op->addr_adc_on_rst,
				sizeof(pic_op->addr_adc_on_rst));
	himax_in_parse_assign_cmd(ic_adr_psl, pic_op->addr_psl,
				sizeof(pic_op->addr_psl));
	himax_in_parse_assign_cmd(ic_adr_cs_central_state,
				pic_op->addr_cs_central_state,
				sizeof(pic_op->addr_cs_central_state));
	himax_in_parse_assign_cmd(ic_cmd_rst, pic_op->data_rst,
				sizeof(pic_op->data_rst));
#endif
#ifdef CORE_FW
	himax_in_parse_assign_cmd(fw_addr_system_reset,
				pfw_op->addr_system_reset,
				sizeof(pfw_op->addr_system_reset));
	himax_in_parse_assign_cmd(fw_addr_safe_mode_release_pw,
				pfw_op->addr_safe_mode_release_pw,
				sizeof(pfw_op->addr_safe_mode_release_pw));
	himax_in_parse_assign_cmd(fw_addr_ctrl_fw, pfw_op->addr_ctrl_fw_isr,
				sizeof(pfw_op->addr_ctrl_fw_isr));
	himax_in_parse_assign_cmd(fw_addr_flag_reset_event,
				pfw_op->addr_flag_reset_event,
				sizeof(pfw_op->addr_flag_reset_event));
	himax_in_parse_assign_cmd(fw_addr_hsen_enable, pfw_op->addr_hsen_enable,
				sizeof(pfw_op->addr_hsen_enable));
	himax_in_parse_assign_cmd(fw_addr_hsen_sts, pfw_op->addr_hsen_sts,
				sizeof(pfw_op->addr_hsen_sts));
	himax_in_parse_assign_cmd(fw_addr_smwp_enable, pfw_op->addr_smwp_enable,
				sizeof(pfw_op->addr_smwp_enable));
	himax_in_parse_assign_cmd(fw_addr_program_reload_from,
				pfw_op->addr_program_reload_from,
				sizeof(pfw_op->addr_program_reload_from));
	himax_in_parse_assign_cmd(fw_addr_program_reload_to,
				pfw_op->addr_program_reload_to,
				sizeof(pfw_op->addr_program_reload_to));
	himax_in_parse_assign_cmd(fw_addr_program_reload_page_write,
				pfw_op->addr_program_reload_page_write,
				sizeof(pfw_op-> addr_program_reload_page_write));
	himax_in_parse_assign_cmd(fw_addr_raw_out_sel, pfw_op->addr_raw_out_sel,
				sizeof(pfw_op->addr_raw_out_sel));
	himax_in_parse_assign_cmd(fw_addr_reload_status,
				pfw_op->addr_reload_status,
				sizeof(pfw_op->addr_reload_status));
	himax_in_parse_assign_cmd(fw_addr_reload_crc32_result,
				pfw_op->addr_reload_crc32_result,
				sizeof(pfw_op->addr_reload_crc32_result));
	himax_in_parse_assign_cmd(fw_addr_reload_addr_from,
				pfw_op->addr_reload_addr_from,
				sizeof(pfw_op->addr_reload_addr_from));
	himax_in_parse_assign_cmd(fw_addr_reload_addr_cmd_beat,
				pfw_op->addr_reload_addr_cmd_beat,
				sizeof(pfw_op->addr_reload_addr_cmd_beat));
	himax_in_parse_assign_cmd(fw_addr_selftest_addr_en,
				pfw_op->addr_selftest_addr_en,
				sizeof(pfw_op->addr_selftest_addr_en));
	himax_in_parse_assign_cmd(fw_addr_criteria_addr,
				pfw_op->addr_criteria_addr,
				sizeof(pfw_op->addr_criteria_addr));
	himax_in_parse_assign_cmd(fw_addr_set_frame_addr,
				pfw_op->addr_set_frame_addr,
				sizeof(pfw_op->addr_set_frame_addr));
	himax_in_parse_assign_cmd(fw_addr_selftest_result_addr,
				pfw_op->addr_selftest_result_addr,
				sizeof(pfw_op->addr_selftest_result_addr));
	himax_in_parse_assign_cmd(fw_addr_sorting_mode_en,
				pfw_op->addr_sorting_mode_en,
				sizeof(pfw_op->addr_sorting_mode_en));
	himax_in_parse_assign_cmd(fw_addr_max_dc, pfw_op->addr_max_dc,
				sizeof(pfw_op->addr_max_dc));
	himax_in_parse_assign_cmd(fw_addr_fw_mode_status,
				pfw_op->addr_fw_mode_status,
				sizeof(pfw_op->addr_fw_mode_status));
	himax_in_parse_assign_cmd(fw_addr_icid_addr, pfw_op->addr_icid_addr,
				sizeof(pfw_op->addr_icid_addr));
	himax_in_parse_assign_cmd(fw_addr_trigger_addr,
				pfw_op->addr_trigger_addr,
				sizeof(pfw_op->addr_trigger_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_ver_addr, pfw_op->addr_fw_ver_addr,
				sizeof(pfw_op->addr_fw_ver_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_cfg_addr, pfw_op->addr_fw_cfg_addr,
				sizeof(pfw_op->addr_fw_cfg_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_vendor_addr,
				pfw_op->addr_fw_vendor_addr,
				sizeof(pfw_op->addr_fw_vendor_addr));
	himax_in_parse_assign_cmd(fw_addr_cus_info, pfw_op->addr_cus_info,
				sizeof(pfw_op->addr_cus_info));
	himax_in_parse_assign_cmd(fw_addr_proj_info, pfw_op->addr_proj_info,
				sizeof(pfw_op->addr_proj_info));
	himax_in_parse_assign_cmd(fw_addr_fw_state_addr,
				pfw_op->addr_fw_state_addr,
				sizeof(pfw_op->addr_fw_state_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_dbg_msg_addr,
				pfw_op->addr_fw_dbg_msg_addr,
				sizeof(pfw_op->addr_fw_dbg_msg_addr));
	himax_in_parse_assign_cmd(fw_addr_chk_fw_status,
				pfw_op->addr_chk_fw_status,
				sizeof(pfw_op->addr_chk_fw_status));
	himax_in_parse_assign_cmd(fw_addr_dd_handshak_addr,
				pfw_op->addr_dd_handshak_addr,
				sizeof(pfw_op->addr_dd_handshak_addr));
	himax_in_parse_assign_cmd(fw_addr_dd_data_addr,
				pfw_op->addr_dd_data_addr,
				sizeof(pfw_op->addr_dd_data_addr));
	himax_in_parse_assign_cmd(fw_data_system_reset,
				pfw_op->data_system_reset,
				sizeof(pfw_op->data_system_reset));
	himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_active,
				pfw_op->data_safe_mode_release_pw_active,
				sizeof(pfw_op->data_safe_mode_release_pw_active));
	himax_in_parse_assign_cmd(fw_data_clear, pfw_op->data_clear,
				sizeof(pfw_op->data_clear));
	himax_in_parse_assign_cmd(fw_data_clear, pfw_op->data_clear,
				sizeof(pfw_op->data_clear));
	himax_in_parse_assign_cmd(fw_data_fw_stop, pfw_op->data_fw_stop,
				sizeof(pfw_op->data_fw_stop));
	himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset,
				pfw_op->data_safe_mode_release_pw_reset,
				sizeof(pfw_op->data_safe_mode_release_pw_reset));
	himax_in_parse_assign_cmd(fw_data_program_reload_start,
				pfw_op->data_program_reload_start,
				sizeof(pfw_op->data_program_reload_start));
	himax_in_parse_assign_cmd(fw_data_program_reload_compare,
				pfw_op->data_program_reload_compare,
				sizeof(pfw_op->data_program_reload_compare));
	himax_in_parse_assign_cmd(fw_data_program_reload_break,
				pfw_op->data_program_reload_break,
				sizeof(pfw_op->data_program_reload_break));
	himax_in_parse_assign_cmd(fw_data_selftest_request,
				pfw_op->data_selftest_request,
				sizeof(pfw_op->data_selftest_request));
	himax_in_parse_assign_cmd(fw_data_criteria_aa_top,
				pfw_op->data_criteria_aa_top,
				sizeof(pfw_op->data_criteria_aa_top));
	himax_in_parse_assign_cmd(fw_data_criteria_aa_bot,
				pfw_op->data_criteria_aa_bot,
				sizeof(pfw_op->data_criteria_aa_bot));
	himax_in_parse_assign_cmd(fw_data_criteria_key_top,
				pfw_op->data_criteria_key_top,
				sizeof(pfw_op->data_criteria_key_top));
	himax_in_parse_assign_cmd(fw_data_criteria_key_bot,
				pfw_op->data_criteria_key_bot,
				sizeof(pfw_op->data_criteria_key_bot));
	himax_in_parse_assign_cmd(fw_data_criteria_avg_top,
				pfw_op->data_criteria_avg_top,
				sizeof(pfw_op->data_criteria_avg_top));
	himax_in_parse_assign_cmd(fw_data_criteria_avg_bot,
				pfw_op->data_criteria_avg_bot,
				sizeof(pfw_op->data_criteria_avg_bot));
	himax_in_parse_assign_cmd(fw_data_set_frame, pfw_op->data_set_frame,
				sizeof(pfw_op->data_set_frame));
	himax_in_parse_assign_cmd(fw_data_selftest_ack_hb,
				pfw_op->data_selftest_ack_hb,
				sizeof(pfw_op->data_selftest_ack_hb));
	himax_in_parse_assign_cmd(fw_data_selftest_ack_lb,
				pfw_op->data_selftest_ack_lb,
				sizeof(pfw_op->data_selftest_ack_lb));
	himax_in_parse_assign_cmd(fw_data_selftest_pass,
				pfw_op->data_selftest_pass,
				sizeof(pfw_op->data_selftest_pass));
	himax_in_parse_assign_cmd(fw_data_normal_cmd, pfw_op->data_normal_cmd,
				sizeof(pfw_op->data_normal_cmd));
	himax_in_parse_assign_cmd(fw_data_normal_status,
				pfw_op->data_normal_status,
				sizeof(pfw_op->data_normal_status));
	himax_in_parse_assign_cmd(fw_data_sorting_cmd, pfw_op->data_sorting_cmd,
				sizeof(pfw_op->data_sorting_cmd));
	himax_in_parse_assign_cmd(fw_data_sorting_status,
				pfw_op->data_sorting_status,
				sizeof(pfw_op->data_sorting_status));
	himax_in_parse_assign_cmd(fw_data_dd_request, pfw_op->data_dd_request,
				sizeof(pfw_op->data_dd_request));
	himax_in_parse_assign_cmd(fw_data_dd_ack, pfw_op->data_dd_ack,
				sizeof(pfw_op->data_dd_ack));
	himax_in_parse_assign_cmd(fw_data_idle_dis_pwd,
				pfw_op->data_idle_dis_pwd,
				sizeof(pfw_op->data_idle_dis_pwd));
	himax_in_parse_assign_cmd(fw_data_idle_en_pwd, pfw_op->data_idle_en_pwd,
				sizeof(pfw_op->data_idle_en_pwd));
	himax_in_parse_assign_cmd(fw_data_rawdata_ready_hb,
				pfw_op->data_rawdata_ready_hb,
				sizeof(pfw_op->data_rawdata_ready_hb));
	himax_in_parse_assign_cmd(fw_data_rawdata_ready_lb,
				pfw_op->data_rawdata_ready_lb,
				sizeof(pfw_op->data_rawdata_ready_lb));
	himax_in_parse_assign_cmd(fw_addr_ahb_addr, pfw_op->addr_ahb_addr,
				sizeof(pfw_op->addr_ahb_addr));
	himax_in_parse_assign_cmd(fw_data_ahb_dis, pfw_op->data_ahb_dis,
				sizeof(pfw_op->data_ahb_dis));
	himax_in_parse_assign_cmd(fw_data_ahb_en, pfw_op->data_ahb_en,
				sizeof(pfw_op->data_ahb_en));
	himax_in_parse_assign_cmd(fw_addr_event_addr, pfw_op->addr_event_addr,
				sizeof(pfw_op->addr_event_addr));
	himax_in_parse_assign_cmd(fw_usb_detect_addr, pfw_op->addr_usb_detect,
				sizeof(pfw_op->addr_usb_detect));
#ifdef HX_P_SENSOR
	himax_in_parse_assign_cmd(fw_addr_ulpm_33, pfw_op->addr_ulpm_33,
				sizeof(pfw_op->addr_ulpm_33));
	himax_in_parse_assign_cmd(fw_addr_ulpm_34, pfw_op->addr_ulpm_34,
				sizeof(pfw_op->addr_ulpm_34));
	himax_in_parse_assign_cmd(fw_data_ulpm_11, pfw_op->data_ulpm_11,
				sizeof(pfw_op->data_ulpm_11));
	himax_in_parse_assign_cmd(fw_data_ulpm_22, pfw_op->data_ulpm_22,
				sizeof(pfw_op->data_ulpm_22));
	himax_in_parse_assign_cmd(fw_data_ulpm_33, pfw_op->data_ulpm_33,
				sizeof(pfw_op->data_ulpm_33));
	himax_in_parse_assign_cmd(fw_data_ulpm_aa, pfw_op->data_ulpm_aa,
				sizeof(pfw_op->data_ulpm_aa));
#endif
	himax_in_parse_assign_cmd(FW_EDGE_BORDER_ADDR, pfw_op->addr_edge_border,
				sizeof(pfw_op->addr_edge_border));
	himax_in_parse_assign_cmd(FW_CAL_ADDR, pfw_op->addr_cal,
				sizeof(pfw_op->addr_cal));
	himax_in_parse_assign_cmd(FW_RPORT_THRSH_ADDR, pfw_op->addr_rport_thrsh,
				sizeof(pfw_op->addr_rport_thrsh));
#endif
#ifdef CORE_FLASH
	himax_in_parse_assign_cmd(flash_addr_spi200_trans_fmt,
				pflash_op->addr_spi200_trans_fmt,
				sizeof(pflash_op->addr_spi200_trans_fmt));
	himax_in_parse_assign_cmd(flash_addr_spi200_trans_ctrl,
				pflash_op->addr_spi200_trans_ctrl,
				sizeof(pflash_op->addr_spi200_trans_ctrl));
	himax_in_parse_assign_cmd(flash_addr_spi200_cmd,
				pflash_op->addr_spi200_cmd,
				sizeof(pflash_op->addr_spi200_cmd));
	himax_in_parse_assign_cmd(flash_addr_spi200_addr,
				pflash_op->addr_spi200_addr,
				sizeof(pflash_op->addr_spi200_addr));
	himax_in_parse_assign_cmd(flash_addr_spi200_data,
				pflash_op->addr_spi200_data,
				sizeof(pflash_op->addr_spi200_data));
	himax_in_parse_assign_cmd(flash_addr_spi200_bt_num,
				pflash_op->addr_spi200_bt_num,
				sizeof(pflash_op->addr_spi200_bt_num));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_fmt,
				pflash_op->data_spi200_trans_fmt,
				sizeof(pflash_op->data_spi200_trans_fmt));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_1,
				pflash_op->data_spi200_trans_ctrl_1,
				sizeof(pflash_op->data_spi200_trans_ctrl_1));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_2,
				pflash_op->data_spi200_trans_ctrl_2,
				sizeof(pflash_op->data_spi200_trans_ctrl_2));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_3,
				pflash_op->data_spi200_trans_ctrl_3,
				sizeof(pflash_op->data_spi200_trans_ctrl_3));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_4,
				pflash_op->data_spi200_trans_ctrl_4,
				sizeof(pflash_op->data_spi200_trans_ctrl_4));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_5,
				pflash_op->data_spi200_trans_ctrl_5,
				sizeof(pflash_op->data_spi200_trans_ctrl_5));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_1,
				pflash_op->data_spi200_cmd_1,
				sizeof(pflash_op->data_spi200_cmd_1));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_2,
				pflash_op->data_spi200_cmd_2,
				sizeof(pflash_op->data_spi200_cmd_2));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_3,
				pflash_op->data_spi200_cmd_3,
				sizeof(pflash_op->data_spi200_cmd_3));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_4,
				pflash_op->data_spi200_cmd_4,
				sizeof(pflash_op->data_spi200_cmd_4));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_5,
				pflash_op->data_spi200_cmd_5,
				sizeof(pflash_op->data_spi200_cmd_5));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_6,
				pflash_op->data_spi200_cmd_6,
				sizeof(pflash_op->data_spi200_cmd_6));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_7,
				pflash_op->data_spi200_cmd_7,
				sizeof(pflash_op->data_spi200_cmd_7));
	himax_in_parse_assign_cmd(flash_data_spi200_addr,
				pflash_op->data_spi200_addr,
				sizeof(pflash_op->data_spi200_addr));
#endif
#ifdef CORE_SRAM
/* sram start*/
	himax_in_parse_assign_cmd(sram_adr_mkey, psram_op->addr_mkey,
				sizeof(psram_op->addr_mkey));
	himax_in_parse_assign_cmd(sram_adr_rawdata_addr,
				psram_op->addr_rawdata_addr,
				sizeof(psram_op->addr_rawdata_addr));
	himax_in_parse_assign_cmd(sram_adr_rawdata_end,
				psram_op->addr_rawdata_end,
				sizeof(psram_op->addr_rawdata_end));
	himax_in_parse_assign_cmd(sram_cmd_conti, psram_op->data_conti,
				sizeof(psram_op->data_conti));
	himax_in_parse_assign_cmd(sram_cmd_fin, psram_op->data_fin,
				sizeof(psram_op->data_fin));
	himax_in_parse_assign_cmd(sram_passwrd_start, psram_op->passwrd_start,
				sizeof(psram_op->passwrd_start));
	himax_in_parse_assign_cmd(sram_passwrd_end, psram_op->passwrd_end,
				sizeof(psram_op->passwrd_end));
/* sram end*/
#endif
#ifdef CORE_DRIVER
	himax_in_parse_assign_cmd(driver_addr_fw_define_flash_reload,
				pdriver_op->addr_fw_define_flash_reload,
				sizeof(pdriver_op->addr_fw_define_flash_reload));
	himax_in_parse_assign_cmd(driver_addr_fw_define_2nd_flash_reload,
				pdriver_op->addr_fw_define_2nd_flash_reload,
				sizeof(pdriver_op->addr_fw_define_2nd_flash_reload));
	himax_in_parse_assign_cmd(driver_addr_fw_define_int_is_edge,
				pdriver_op->addr_fw_define_int_is_edge,
				sizeof(pdriver_op->addr_fw_define_int_is_edge));
	himax_in_parse_assign_cmd(driver_addr_fw_define_rxnum_txnum_maxpt,
				pdriver_op->addr_fw_define_rxnum_txnum_maxpt,
				sizeof(pdriver_op->addr_fw_define_rxnum_txnum_maxpt));
	himax_in_parse_assign_cmd(driver_addr_fw_define_xy_res_enable,
				pdriver_op->addr_fw_define_xy_res_enable,
				sizeof(pdriver_op->addr_fw_define_xy_res_enable));
	himax_in_parse_assign_cmd(driver_addr_fw_define_x_y_res,
				pdriver_op->addr_fw_define_x_y_res,
				sizeof(pdriver_op->addr_fw_define_x_y_res));
	himax_in_parse_assign_cmd(driver_data_df_rx, pdriver_op->data_df_rx,
				sizeof(pdriver_op->data_df_rx));
	himax_in_parse_assign_cmd(driver_data_df_tx, pdriver_op->data_df_tx,
				sizeof(pdriver_op->data_df_tx));
	himax_in_parse_assign_cmd(driver_data_df_pt, pdriver_op->data_df_pt,
				sizeof(pdriver_op->data_df_pt));
	himax_in_parse_assign_cmd(driver_data_df_x_res,
				pdriver_op->data_df_x_res,
				sizeof(pdriver_op->data_df_x_res));
	himax_in_parse_assign_cmd(driver_data_df_y_res,
				pdriver_op->data_df_y_res,
				sizeof(pdriver_op->data_df_y_res));
	himax_in_parse_assign_cmd(driver_data_fw_define_flash_reload_dis,
				pdriver_op->data_fw_define_flash_reload_dis,
				sizeof(pdriver_op->data_fw_define_flash_reload_dis));
	himax_in_parse_assign_cmd(driver_data_fw_define_flash_reload_en,
				pdriver_op->data_fw_define_flash_reload_en,
				sizeof(pdriver_op->data_fw_define_flash_reload_en));
	himax_in_parse_assign_cmd(driver_data_fw_define_rxnum_txnum_maxpt_sorting,
				pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting,
				sizeof(pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting));
	himax_in_parse_assign_cmd(driver_data_fw_define_rxnum_txnum_maxpt_normal,
				pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal,
				sizeof(pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal));
#endif
#ifdef HX_ZERO_FLASH
	himax_in_parse_assign_cmd(zf_addr_dis_flash_reload,
				pzf_op->addr_dis_flash_reload,
				sizeof(pzf_op->addr_dis_flash_reload));
	himax_in_parse_assign_cmd(zf_data_dis_flash_reload,
				pzf_op->data_dis_flash_reload,
				sizeof(pzf_op->data_dis_flash_reload));
	himax_in_parse_assign_cmd(zf_addr_system_reset,
				pzf_op->addr_system_reset,
				sizeof(pzf_op->addr_system_reset));
	himax_in_parse_assign_cmd(zf_data_system_reset,
				pzf_op->data_system_reset,
				sizeof(pzf_op->data_system_reset));
	himax_in_parse_assign_cmd(zf_data_sram_start_addr,
				pzf_op->data_sram_start_addr,
				sizeof(pzf_op->data_sram_start_addr));
	himax_in_parse_assign_cmd(zf_data_sram_clean, pzf_op->data_sram_clean,
				sizeof(pzf_op->data_sram_clean));
	himax_in_parse_assign_cmd(zf_data_cfg_info, pzf_op->data_cfg_info,
				sizeof(pzf_op->data_cfg_info));
	himax_in_parse_assign_cmd(zf_data_fw_cfg_1, pzf_op->data_fw_cfg_1,
				sizeof(pzf_op->data_fw_cfg_1));
	himax_in_parse_assign_cmd(zf_data_fw_cfg_2, pzf_op->data_fw_cfg_2,
				sizeof(pzf_op->data_fw_cfg_2));
	himax_in_parse_assign_cmd(zf_data_fw_cfg_2, pzf_op->data_fw_cfg_3,
				sizeof(pzf_op->data_fw_cfg_3));
	himax_in_parse_assign_cmd(zf_data_adc_cfg_1, pzf_op->data_adc_cfg_1,
				sizeof(pzf_op->data_adc_cfg_1));
	himax_in_parse_assign_cmd(zf_data_adc_cfg_2, pzf_op->data_adc_cfg_2,
				sizeof(pzf_op->data_adc_cfg_2));
	himax_in_parse_assign_cmd(zf_data_adc_cfg_3, pzf_op->data_adc_cfg_3,
				sizeof(pzf_op->data_adc_cfg_3));
	himax_in_parse_assign_cmd(zf_data_map_table, pzf_op->data_map_table,
				sizeof(pzf_op->data_map_table));
	himax_in_parse_assign_cmd(zf_data_mode_switch, pzf_op->data_mode_switch,
				sizeof(pzf_op->data_mode_switch));
	himax_in_parse_assign_cmd(zf_addr_sts_chk, pzf_op->addr_sts_chk,
				sizeof(pzf_op->addr_sts_chk));
	himax_in_parse_assign_cmd(zf_data_activ_sts, pzf_op->data_activ_sts,
				sizeof(pzf_op->data_activ_sts));
	himax_in_parse_assign_cmd(zf_addr_activ_relod, pzf_op->addr_activ_relod,
				sizeof(pzf_op->addr_activ_relod));
	himax_in_parse_assign_cmd(zf_data_activ_in, pzf_op->data_activ_in,
				sizeof(pzf_op->data_activ_in));
#endif
}

/* init end*/
#endif
