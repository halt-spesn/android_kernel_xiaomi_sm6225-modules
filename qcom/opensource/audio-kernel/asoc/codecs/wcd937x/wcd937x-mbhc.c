// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include "wcd937x-registers.h"
#include <asoc/wcdcal-hwdep.h>
#include <asoc/wcd-mbhc-v2-api.h>
#include "internal.h"

#define WCD937X_ZDET_SUPPORTED          true
/* Z value defined in milliohm */
#define WCD937X_ZDET_VAL_32             32000
#define WCD937X_ZDET_VAL_400            400000
#define WCD937X_ZDET_VAL_1200           1200000
#define WCD937X_ZDET_VAL_100K           100000000
/* Z floating defined in ohms */
#define WCD937X_ZDET_FLOATING_IMPEDANCE 0x0FFFFFFE

#define WCD937X_ZDET_NUM_MEASUREMENTS   900
#define WCD937X_MBHC_GET_C1(c)          ((c & 0xC000) >> 14)
#define WCD937X_MBHC_GET_X1(x)          (x & 0x3FFF)
/* Z value compared in milliOhm */
#define WCD937X_MBHC_IS_SECOND_RAMP_REQUIRED(z) ((z > 400000) || (z < 32000))
#define WCD937X_MBHC_ZDET_CONST         (86 * 16384)
#define WCD937X_MBHC_MOISTURE_RREF      R_24_KOHM

static struct wcd_mbhc_register
	wcd_mbhc_registers[WCD_MBHC_REG_FUNC_MAX] = {
	WCD_MBHC_REGISTER("WCD_MBHC_L_DET_EN",
			  WCD937X_ANA_MBHC_MECH, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_DET_EN",
			  WCD937X_ANA_MBHC_MECH, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MECH_DETECTION_TYPE",
			  WCD937X_ANA_MBHC_MECH, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_CLAMP_CTL",
			  WCD937X_MBHC_NEW_PLUG_DETECT_CTL, 0x30, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_DETECTION_TYPE",
			  WCD937X_ANA_MBHC_ELECT, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_CTRL",
			  WCD937X_MBHC_NEW_INT_MECH_DET_CURRENT, 0x1F, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_COMP_CTRL",
			  WCD937X_ANA_MBHC_MECH, 0x04, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PLUG_TYPE",
			  WCD937X_ANA_MBHC_MECH, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_PLUG_TYPE",
			  WCD937X_ANA_MBHC_MECH, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SW_HPH_LP_100K_TO_GND",
			  WCD937X_ANA_MBHC_MECH, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_SCHMT_ISRC",
			  WCD937X_ANA_MBHC_ELECT, 0x06, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_EN",
			  WCD937X_ANA_MBHC_ELECT, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_INSREM_DBNC",
			  WCD937X_MBHC_NEW_PLUG_DETECT_CTL, 0x0F, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_DBNC",
			  WCD937X_MBHC_NEW_CTL_1, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_VREF",
			  WCD937X_MBHC_NEW_CTL_2, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_COMP_RESULT",
			  WCD937X_ANA_MBHC_RESULT_3, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_IN2P_CLAMP_STATE",
			  WCD937X_ANA_MBHC_RESULT_3, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_SCHMT_RESULT",
			  WCD937X_ANA_MBHC_RESULT_3, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_SCHMT_RESULT",
			  WCD937X_ANA_MBHC_RESULT_3, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_SCHMT_RESULT",
			  WCD937X_ANA_MBHC_RESULT_3, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_OCP_FSM_EN",
			  WCD937X_HPH_OCP_CTL, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_RESULT",
			  WCD937X_ANA_MBHC_RESULT_3, 0x07, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_ISRC_CTL",
			  WCD937X_ANA_MBHC_ELECT, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_RESULT",
			  WCD937X_ANA_MBHC_RESULT_3, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MICB_CTRL",
			  WCD937X_ANA_MICB2, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_CNP_WG_TIME",
			  WCD937X_HPH_CNP_WG_TIME, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_PA_EN",
			  WCD937X_ANA_HPH, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PA_EN",
			  WCD937X_ANA_HPH, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_PA_EN",
			  WCD937X_ANA_HPH, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SWCH_LEVEL_REMOVE",
			  WCD937X_ANA_MBHC_RESULT_3, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_PULLDOWN_CTRL",
			  0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ANC_DET_EN",
			  WCD937X_MBHC_CTL_BCS, 0x02, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_STATUS",
			  WCD937X_MBHC_NEW_FSM_STATUS, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MUX_CTL",
			  WCD937X_MBHC_NEW_CTL_2, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MOISTURE_STATUS",
			  WCD937X_MBHC_NEW_FSM_STATUS, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_GND",
			  WCD937X_HPH_PA_CTL2, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_GND",
			  WCD937X_HPH_PA_CTL2, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_OCP_DET_EN",
			  WCD937X_HPH_L_TEST, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_OCP_DET_EN",
			  WCD937X_HPH_R_TEST, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_OCP_STATUS",
			  WCD937X_DIGITAL_INTR_STATUS_0, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_OCP_STATUS",
			  WCD937X_DIGITAL_INTR_STATUS_0, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_EN",
			  WCD937X_MBHC_NEW_CTL_1, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_COMPLETE", WCD937X_MBHC_NEW_FSM_STATUS,
			  0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_TIMEOUT", WCD937X_MBHC_NEW_FSM_STATUS,
			  0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_RESULT", WCD937X_MBHC_NEW_ADC_RESULT,
			  0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MICB2_VOUT", WCD937X_ANA_MICB2, 0x3F, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_MODE",
			  WCD937X_MBHC_NEW_CTL_1, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_DETECTION_DONE",
			  WCD937X_MBHC_NEW_CTL_1, 0x04, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_ISRC_EN",
			  WCD937X_ANA_MBHC_ZDET, 0x02, 1, 0),
};

static const struct wcd_mbhc_intr intr_ids = {
	.mbhc_sw_intr =  WCD937X_IRQ_MBHC_SW_DET,
	.mbhc_btn_press_intr = WCD937X_IRQ_MBHC_BUTTON_PRESS_DET,
	.mbhc_btn_release_intr = WCD937X_IRQ_MBHC_BUTTON_RELEASE_DET,
	.mbhc_hs_ins_intr = WCD937X_IRQ_MBHC_ELECT_INS_REM_LEG_DET,
	.mbhc_hs_rem_intr = WCD937X_IRQ_MBHC_ELECT_INS_REM_DET,
	.hph_left_ocp = WCD937X_IRQ_HPHL_OCP_INT,
	.hph_right_ocp = WCD937X_IRQ_HPHR_OCP_INT,
};

struct wcd937x_mbhc_zdet_param {
	u16 ldo_ctl;
	u16 noff;
	u16 nshift;
	u16 btn5;
	u16 btn6;
	u16 btn7;
};

static int wcd937x_mbhc_request_irq(struct snd_soc_component *component,
				  int irq, irq_handler_t handler,
				  const char *name, void *data)
{
	struct wcd937x_priv *wcd937x = dev_get_drvdata(component->dev);

	return wcd_request_irq(&wcd937x->irq_info, irq, name, handler, data);
}

static void wcd937x_mbhc_irq_control(struct snd_soc_component *component,
				   int irq, bool enable)
{
	struct wcd937x_priv *wcd937x = dev_get_drvdata(component->dev);

	if (enable)
		wcd_enable_irq(&wcd937x->irq_info, irq);
	else
		wcd_disable_irq(&wcd937x->irq_info, irq);
}

static int wcd937x_mbhc_free_irq(struct snd_soc_component *component,
			       int irq, void *data)
{
	struct wcd937x_priv *wcd937x = dev_get_drvdata(component->dev);

	wcd_free_irq(&wcd937x->irq_info, irq, data);

	return 0;
}

static void wcd937x_mbhc_clk_setup(struct snd_soc_component *component,
				 bool enable)
{
	if (enable)
		snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_1,
				    0x80, 0x80);
	else
		snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_1,
				    0x80, 0x00);
}

static int wcd937x_mbhc_btn_to_num(struct snd_soc_component *component)
{
	return snd_soc_component_read(component, WCD937X_ANA_MBHC_RESULT_3) &
				0x7;
}

static void wcd937x_mbhc_mbhc_bias_control(struct snd_soc_component *component,
					 bool enable)
{
	if (enable)
		snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_ELECT,
				    0x01, 0x01);
	else
		snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_ELECT,
				    0x01, 0x00);
}

static void wcd937x_mbhc_program_btn_thr(struct snd_soc_component *component,
				       s16 *btn_low, s16 *btn_high,
				       int num_btn, bool is_micbias)
{
	int i;
	int vth;

	if (num_btn > WCD_MBHC_DEF_BUTTONS) {
		dev_err(component->dev, "%s: invalid number of buttons: %d\n",
			__func__, num_btn);
		return;
	}

	for (i = 0; i < num_btn; i++) {
		vth = ((btn_high[i] * 2) / 25) & 0x3F;
		snd_soc_component_update_bits(component,
				WCD937X_ANA_MBHC_BTN0 + i,
				0xFC, vth << 2);
		dev_dbg(component->dev, "%s: btn_high[%d]: %d, vth: %d\n",
			__func__, i, btn_high[i], vth);
	}
}

static bool wcd937x_mbhc_lock_sleep(struct wcd_mbhc *mbhc, bool lock)
{
	struct snd_soc_component *component = mbhc->component;
	struct wcd937x_priv *wcd937x = dev_get_drvdata(component->dev);

	wcd937x->wakeup((void*)wcd937x, lock);
	return true;
}

static int wcd937x_mbhc_register_notifier(struct wcd_mbhc *mbhc,
					struct notifier_block *nblock,
					bool enable)
{
	struct wcd937x_mbhc *wcd937x_mbhc;

	wcd937x_mbhc = container_of(mbhc, struct wcd937x_mbhc, wcd_mbhc);

	if (enable)
		return blocking_notifier_chain_register(&wcd937x_mbhc->notifier,
							nblock);
	else
		return blocking_notifier_chain_unregister(
				&wcd937x_mbhc->notifier, nblock);
}

static bool wcd937x_mbhc_micb_en_status(struct wcd_mbhc *mbhc, int micb_num)
{
	u8 val = 0;

	if (micb_num == MIC_BIAS_2) {
		val = ((snd_soc_component_read(mbhc->component,
				WCD937X_ANA_MICB2) & 0xC0)
			>> 6);
		if (val == 0x01)
			return true;
	}
	return false;
}

static bool wcd937x_mbhc_hph_pa_on_status(struct snd_soc_component *component)
{
	return (snd_soc_component_read(component, WCD937X_ANA_HPH) & 0xC0) ?
			true : false;
}

static void wcd937x_mbhc_hph_l_pull_up_control(
				struct snd_soc_component *component,
				int pull_up_cur)
{
	/* Default pull up current to 2uA */
	if (pull_up_cur > HS_PULLUP_I_OFF || pull_up_cur < HS_PULLUP_I_3P0_UA ||
	    pull_up_cur == HS_PULLUP_I_DEFAULT)
		pull_up_cur = HS_PULLUP_I_2P0_UA;

	dev_dbg(component->dev, "%s: HS pull up current:%d\n",
		__func__, pull_up_cur);

	snd_soc_component_update_bits(component,
				WCD937X_MBHC_NEW_INT_MECH_DET_CURRENT,
				0x1F, pull_up_cur);
}

static int wcd937x_mbhc_request_micbias(struct snd_soc_component *component,
					int micb_num, int req)
{
	int ret = 0;

	ret = wcd937x_micbias_control(component, micb_num, req, false);

	return ret;
}

static void wcd937x_mbhc_micb_ramp_control(struct snd_soc_component *component,
					   bool enable)
{
	if (enable) {
		snd_soc_component_update_bits(component, WCD937X_ANA_MICB2_RAMP,
				    0x1C, 0x0C);
		snd_soc_component_update_bits(component, WCD937X_ANA_MICB2_RAMP,
				    0x80, 0x80);
	} else {
		snd_soc_component_update_bits(component, WCD937X_ANA_MICB2_RAMP,
				    0x80, 0x00);
		snd_soc_component_update_bits(component, WCD937X_ANA_MICB2_RAMP,
				    0x1C, 0x00);
	}
}

static struct firmware_cal *wcd937x_get_hwdep_fw_cal(struct wcd_mbhc *mbhc,
						   enum wcd_cal_type type)
{
	struct wcd937x_mbhc *wcd937x_mbhc;
	struct firmware_cal *hwdep_cal;
	struct snd_soc_component *component = mbhc->component;

	wcd937x_mbhc = container_of(mbhc, struct wcd937x_mbhc, wcd_mbhc);

	if (!component) {
		pr_err("%s: NULL component pointer\n", __func__);
		return NULL;
	}
	hwdep_cal = wcdcal_get_fw_cal(wcd937x_mbhc->fw_data, type);
	if (!hwdep_cal)
		dev_err(component->dev, "%s: cal not sent by %d\n",
			__func__, type);

	return hwdep_cal;
}

static int wcd937x_mbhc_micb_ctrl_threshold_mic(
					struct snd_soc_component *component,
					int micb_num, bool req_en)
{
	struct wcd937x_pdata *pdata = dev_get_platdata(component->dev);
	int rc, micb_mv;

	if (micb_num != MIC_BIAS_2)
		return -EINVAL;
	/*
	 * If device tree micbias level is already above the minimum
	 * voltage needed to detect threshold microphone, then do
	 * not change the micbias, just return.
	 */
	if (pdata->micbias.micb2_mv >= WCD_MBHC_THR_HS_MICB_MV)
		return 0;

	micb_mv = req_en ? WCD_MBHC_THR_HS_MICB_MV : pdata->micbias.micb2_mv;

	rc = wcd937x_mbhc_micb_adjust_voltage(component, micb_mv, MIC_BIAS_2);

	return rc;
}

static inline void wcd937x_mbhc_get_result_params(struct wcd937x_priv *wcd937x,
						s16 *d1_a, u16 noff,
						int32_t *zdet)
{
	int i;
	int val, val1;
	s16 c1;
	s32 x1, d1;
	int32_t denom;
	int minCode_param[] = {
			3277, 1639, 820, 410, 205, 103, 52, 26
	};

	regmap_update_bits(wcd937x->regmap, WCD937X_ANA_MBHC_ZDET, 0x20, 0x20);
	for (i = 0; i < WCD937X_ZDET_NUM_MEASUREMENTS; i++) {
		regmap_read(wcd937x->regmap, WCD937X_ANA_MBHC_RESULT_2, &val);
		if (val & 0x80)
			break;
	}
	val = val << 0x8;
	regmap_read(wcd937x->regmap, WCD937X_ANA_MBHC_RESULT_1, &val1);
	val |= val1;
	regmap_update_bits(wcd937x->regmap, WCD937X_ANA_MBHC_ZDET, 0x20, 0x00);
	x1 = WCD937X_MBHC_GET_X1(val);
	c1 = WCD937X_MBHC_GET_C1(val);
	/* If ramp is not complete, give additional 5ms */
	if ((c1 < 2) && x1)
		usleep_range(5000, 5050);

	if (!c1 || !x1) {
		dev_dbg(wcd937x->dev,
			"%s: Impedance detect ramp error, c1=%d, x1=0x%x\n",
			__func__, c1, x1);
		goto ramp_down;
	}
	d1 = d1_a[c1];
	denom = (x1 * d1) - (1 << (14 - noff));
	if (denom > 0)
		*zdet = (WCD937X_MBHC_ZDET_CONST * 1000) / denom;
	else if (x1 < minCode_param[noff])
		*zdet = WCD937X_ZDET_FLOATING_IMPEDANCE;

	dev_dbg(wcd937x->dev, "%s: d1=%d, c1=%d, x1=0x%x, z_val=%d(milliOhm)\n",
		__func__, d1, c1, x1, *zdet);
ramp_down:
	i = 0;
	while (x1) {
		regmap_read(wcd937x->regmap, WCD937X_ANA_MBHC_RESULT_1, &val);
		regmap_read(wcd937x->regmap, WCD937X_ANA_MBHC_RESULT_2, &val1);
		val = val << 0x8;
		val |= val1;
		x1 = WCD937X_MBHC_GET_X1(val);
		i++;
		if (i == WCD937X_ZDET_NUM_MEASUREMENTS)
			break;
	}
}

static void wcd937x_mbhc_zdet_ramp(struct snd_soc_component *component,
				 struct wcd937x_mbhc_zdet_param *zdet_param,
				 int32_t *zl, int32_t *zr, s16 *d1_a)
{
	struct wcd937x_priv *wcd937x = dev_get_drvdata(component->dev);
	int32_t zdet = 0;

	snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_ZDET_ANA_CTL,
				0x70, zdet_param->ldo_ctl << 4);
	snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_BTN5, 0xFC,
				zdet_param->btn5);
	snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_BTN6, 0xFC,
				zdet_param->btn6);
	snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_BTN7, 0xFC,
				zdet_param->btn7);
	snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_ZDET_ANA_CTL,
				0x0F, zdet_param->noff);
	snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_ZDET_RAMP_CTL,
				0x0F, zdet_param->nshift);

	if (!zl)
		goto z_right;
	/* Start impedance measurement for HPH_L */
	regmap_update_bits(wcd937x->regmap,
			   WCD937X_ANA_MBHC_ZDET, 0x80, 0x80);
	dev_dbg(wcd937x->dev, "%s: ramp for HPH_L, noff = %d\n",
		__func__, zdet_param->noff);
	wcd937x_mbhc_get_result_params(wcd937x, d1_a, zdet_param->noff, &zdet);
	regmap_update_bits(wcd937x->regmap,
			   WCD937X_ANA_MBHC_ZDET, 0x80, 0x00);

	*zl = zdet;

z_right:
	if (!zr)
		return;
	/* Start impedance measurement for HPH_R */
	regmap_update_bits(wcd937x->regmap,
			   WCD937X_ANA_MBHC_ZDET, 0x40, 0x40);
	dev_dbg(wcd937x->dev, "%s: ramp for HPH_R, noff = %d\n",
		__func__, zdet_param->noff);
	wcd937x_mbhc_get_result_params(wcd937x, d1_a, zdet_param->noff, &zdet);
	regmap_update_bits(wcd937x->regmap,
			   WCD937X_ANA_MBHC_ZDET, 0x40, 0x00);

	*zr = zdet;
}

static inline void wcd937x_wcd_mbhc_qfuse_cal(
					struct snd_soc_component *component,
					int32_t *z_val, int flag_l_r)
{
	s16 q1;
	int q1_cal;

	if (*z_val < (WCD937X_ZDET_VAL_400/1000))
		q1 = snd_soc_component_read(component,
			WCD937X_DIGITAL_EFUSE_REG_23 + (2 * flag_l_r));
	else
		q1 = snd_soc_component_read(component,
			WCD937X_DIGITAL_EFUSE_REG_24 + (2 * flag_l_r));
	if (q1 & 0x80)
		q1_cal = (10000 - ((q1 & 0x7F) * 25));
	else
		q1_cal = (10000 + (q1 * 25));
	if (q1_cal > 0)
		*z_val = ((*z_val) * 10000) / q1_cal;
}

static void wcd937x_wcd_mbhc_calc_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
					  uint32_t *zr)
{
	struct snd_soc_component *component = mbhc->component;
	struct wcd937x_priv *wcd937x = dev_get_drvdata(component->dev);
	s16 reg0, reg1, reg2, reg3, reg4;
	int32_t z1L, z1R, z1Ls;
	int zMono, z_diff1, z_diff2;
	bool is_fsm_disable = false;
	struct wcd937x_mbhc_zdet_param zdet_param[] = {
		{4, 0, 4, 0x08, 0x14, 0x18}, /* < 32ohm */
		{2, 0, 3, 0x18, 0x7C, 0x90}, /* 32ohm < Z < 400ohm */
		{1, 4, 5, 0x18, 0x7C, 0x90}, /* 400ohm < Z < 1200ohm */
		{1, 6, 7, 0x18, 0x7C, 0x90}, /* >1200ohm */
	};
	struct wcd937x_mbhc_zdet_param *zdet_param_ptr = NULL;
	s16 d1_a[][4] = {
		{0, 30, 90, 30},
		{0, 30, 30, 5},
		{0, 30, 30, 5},
		{0, 30, 30, 5},
	};
	s16 *d1 = NULL;

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	reg0 = snd_soc_component_read(component, WCD937X_ANA_MBHC_BTN5);
	reg1 = snd_soc_component_read(component, WCD937X_ANA_MBHC_BTN6);
	reg2 = snd_soc_component_read(component, WCD937X_ANA_MBHC_BTN7);
	reg3 = snd_soc_component_read(component, WCD937X_MBHC_CTL_CLK);
	reg4 = snd_soc_component_read(component,
			WCD937X_MBHC_NEW_ZDET_ANA_CTL);

	if (snd_soc_component_read(component, WCD937X_ANA_MBHC_ELECT) &
			0x80) {
		is_fsm_disable = true;
		regmap_update_bits(wcd937x->regmap,
				   WCD937X_ANA_MBHC_ELECT, 0x80, 0x00);
	}

	/* For NO-jack, disable L_DET_EN before Z-det measurements */
	if (mbhc->hphl_swh)
		regmap_update_bits(wcd937x->regmap,
				   WCD937X_ANA_MBHC_MECH, 0x80, 0x00);

	/* Turn off 100k pull down on HPHL */
	regmap_update_bits(wcd937x->regmap,
			   WCD937X_ANA_MBHC_MECH, 0x01, 0x00);

	/* Disable surge protection before impedance detection.
	 * This is done to give correct value for high impedance.
	 */
	regmap_update_bits(wcd937x->regmap,
			   WCD937X_HPH_SURGE_HPHLR_SURGE_EN, 0xC0, 0x00);
	/* 1ms delay needed after disable surge protection */
	usleep_range(1000, 1010);

	/* First get impedance on Left */
	d1 = d1_a[1];
	zdet_param_ptr = &zdet_param[1];
	wcd937x_mbhc_zdet_ramp(component, zdet_param_ptr, &z1L, NULL, d1);

	if (!WCD937X_MBHC_IS_SECOND_RAMP_REQUIRED(z1L))
		goto left_ch_impedance;

	/* Second ramp for left ch */
	if (z1L < WCD937X_ZDET_VAL_32) {
		zdet_param_ptr = &zdet_param[0];
		d1 = d1_a[0];
	} else if ((z1L > WCD937X_ZDET_VAL_400) &&
		  (z1L <= WCD937X_ZDET_VAL_1200)) {
		zdet_param_ptr = &zdet_param[2];
		d1 = d1_a[2];
	} else if (z1L > WCD937X_ZDET_VAL_1200) {
		zdet_param_ptr = &zdet_param[3];
		d1 = d1_a[3];
	}
	wcd937x_mbhc_zdet_ramp(component, zdet_param_ptr, &z1L, NULL, d1);

left_ch_impedance:
	if ((z1L == WCD937X_ZDET_FLOATING_IMPEDANCE) ||
		(z1L > WCD937X_ZDET_VAL_100K)) {
		*zl = WCD937X_ZDET_FLOATING_IMPEDANCE;
		zdet_param_ptr = &zdet_param[1];
		d1 = d1_a[1];
	} else {
		*zl = z1L/1000;
		wcd937x_wcd_mbhc_qfuse_cal(component, zl, 0);
	}
	dev_dbg(component->dev, "%s: impedance on HPH_L = %d(ohms)\n",
		__func__, *zl);

	/* Start of right impedance ramp and calculation */
	wcd937x_mbhc_zdet_ramp(component, zdet_param_ptr, NULL, &z1R, d1);
	if (WCD937X_MBHC_IS_SECOND_RAMP_REQUIRED(z1R)) {
		if (((z1R > WCD937X_ZDET_VAL_1200) &&
			(zdet_param_ptr->noff == 0x6)) ||
			((*zl) != WCD937X_ZDET_FLOATING_IMPEDANCE))
			goto right_ch_impedance;
		/* Second ramp for right ch */
		if (z1R < WCD937X_ZDET_VAL_32) {
			zdet_param_ptr = &zdet_param[0];
			d1 = d1_a[0];
		} else if ((z1R > WCD937X_ZDET_VAL_400) &&
			(z1R <= WCD937X_ZDET_VAL_1200)) {
			zdet_param_ptr = &zdet_param[2];
			d1 = d1_a[2];
		} else if (z1R > WCD937X_ZDET_VAL_1200) {
			zdet_param_ptr = &zdet_param[3];
			d1 = d1_a[3];
		}
		wcd937x_mbhc_zdet_ramp(component, zdet_param_ptr, NULL,
				&z1R, d1);
	}
right_ch_impedance:
	if ((z1R == WCD937X_ZDET_FLOATING_IMPEDANCE) ||
		(z1R > WCD937X_ZDET_VAL_100K)) {
		*zr = WCD937X_ZDET_FLOATING_IMPEDANCE;
	} else {
		*zr = z1R/1000;
		wcd937x_wcd_mbhc_qfuse_cal(component, zr, 1);
	}
	dev_dbg(component->dev, "%s: impedance on HPH_R = %d(ohms)\n",
		__func__, *zr);

	/* Mono/stereo detection */
	if ((*zl == WCD937X_ZDET_FLOATING_IMPEDANCE) &&
		(*zr == WCD937X_ZDET_FLOATING_IMPEDANCE)) {
		dev_dbg(component->dev,
			"%s: plug type is invalid or extension cable\n",
			__func__);
		goto zdet_complete;
	}
	if ((*zl == WCD937X_ZDET_FLOATING_IMPEDANCE) ||
	    (*zr == WCD937X_ZDET_FLOATING_IMPEDANCE) ||
	    ((*zl < WCD_MONO_HS_MIN_THR) && (*zr > WCD_MONO_HS_MIN_THR)) ||
	    ((*zl > WCD_MONO_HS_MIN_THR) && (*zr < WCD_MONO_HS_MIN_THR))) {
		dev_dbg(component->dev,
			"%s: Mono plug type with one ch floating or shorted to GND\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
		goto zdet_complete;
	}
	snd_soc_component_update_bits(component, WCD937X_HPH_R_ATEST,
				0x02, 0x02);
	snd_soc_component_update_bits(component, WCD937X_HPH_PA_CTL2,
				0x40, 0x01);
	if (*zl < (WCD937X_ZDET_VAL_32/1000))
		wcd937x_mbhc_zdet_ramp(component, &zdet_param[0], &z1Ls,
				NULL, d1);
	else
		wcd937x_mbhc_zdet_ramp(component, &zdet_param[1], &z1Ls,
				NULL, d1);
	snd_soc_component_update_bits(component, WCD937X_HPH_PA_CTL2,
				0x40, 0x00);
	snd_soc_component_update_bits(component, WCD937X_HPH_R_ATEST,
				0x02, 0x00);
	z1Ls /= 1000;
	wcd937x_wcd_mbhc_qfuse_cal(component, &z1Ls, 0);
	/* Parallel of left Z and 9 ohm pull down resistor */
	zMono = ((*zl) * 9) / ((*zl) + 9);
	z_diff1 = (z1Ls > zMono) ? (z1Ls - zMono) : (zMono - z1Ls);
	z_diff2 = ((*zl) > z1Ls) ? ((*zl) - z1Ls) : (z1Ls - (*zl));
	if ((z_diff1 * (*zl + z1Ls)) > (z_diff2 * (z1Ls + zMono))) {
		dev_dbg(component->dev, "%s: stereo plug type detected\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_STEREO;
	} else {
		dev_dbg(component->dev, "%s: MONO plug type detected\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
	}

	/* Enable surge protection again after impedance detection */
	regmap_update_bits(wcd937x->regmap,
			   WCD937X_HPH_SURGE_HPHLR_SURGE_EN, 0xC0, 0xC0);
zdet_complete:
	snd_soc_component_write(component, WCD937X_ANA_MBHC_BTN5, reg0);
	snd_soc_component_write(component, WCD937X_ANA_MBHC_BTN6, reg1);
	snd_soc_component_write(component, WCD937X_ANA_MBHC_BTN7, reg2);
	/* Turn on 100k pull down on HPHL */
	regmap_update_bits(wcd937x->regmap,
			   WCD937X_ANA_MBHC_MECH, 0x01, 0x01);

	/* For NO-jack, re-enable L_DET_EN after Z-det measurements */
	if (mbhc->hphl_swh)
		regmap_update_bits(wcd937x->regmap,
				   WCD937X_ANA_MBHC_MECH, 0x80, 0x80);

	snd_soc_component_write(component, WCD937X_MBHC_NEW_ZDET_ANA_CTL, reg4);
	snd_soc_component_write(component, WCD937X_MBHC_CTL_CLK, reg3);
	if (is_fsm_disable)
		regmap_update_bits(wcd937x->regmap,
				   WCD937X_ANA_MBHC_ELECT, 0x80, 0x80);
}

static void wcd937x_mbhc_gnd_det_ctrl(struct snd_soc_component *component,
			bool enable)
{
	if (enable) {
		snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_MECH,
				    0x02, 0x02);
		snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_MECH,
				    0x40, 0x40);
	} else {
		snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_MECH,
				    0x40, 0x00);
		snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_MECH,
				    0x02, 0x00);
	}
}

static void wcd937x_mbhc_hph_pull_down_ctrl(struct snd_soc_component *component,
					  bool enable)
{
	if (enable) {
		snd_soc_component_update_bits(component, WCD937X_HPH_PA_CTL2,
				    0x40, 0x40);
		snd_soc_component_update_bits(component, WCD937X_HPH_PA_CTL2,
				    0x10, 0x10);
	} else {
		snd_soc_component_update_bits(component, WCD937X_HPH_PA_CTL2,
				    0x40, 0x00);
		snd_soc_component_update_bits(component, WCD937X_HPH_PA_CTL2,
				    0x10, 0x00);
	}
}

static void wcd937x_mbhc_moisture_config(struct wcd_mbhc *mbhc)
{
	struct snd_soc_component *component = mbhc->component;

	if ((mbhc->moist_rref == R_OFF) ||
	    (mbhc->mbhc_cfg->enable_usbc_analog)) {
		snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_2,
				    0x0C, R_OFF << 2);
		return;
	}

	/* Do not enable moisture detection if jack type is NC */
	if (!mbhc->hphl_swh) {
		dev_dbg(component->dev, "%s: disable moisture detection for NC\n",
			__func__);
		snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_2,
				    0x0C, R_OFF << 2);
		return;
	}

	snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_2,
			    0x0C, mbhc->moist_rref << 2);
}

static void wcd937x_mbhc_moisture_detect_en(struct wcd_mbhc *mbhc, bool enable)
{
	struct snd_soc_component *component = mbhc->component;

	if (enable)
		snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_2,
					0x0C, mbhc->moist_rref << 2);
	else
		snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_2,
				    0x0C, R_OFF << 2);
}

static bool wcd937x_mbhc_get_moisture_status(struct wcd_mbhc *mbhc)
{
	struct snd_soc_component *component = mbhc->component;
	bool ret = false;

	if ((mbhc->moist_rref == R_OFF) ||
	    (mbhc->mbhc_cfg->enable_usbc_analog)) {
		snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_2,
				    0x0C, R_OFF << 2);
		goto done;
	}

	/* Do not enable moisture detection if jack type is NC */
	if (!mbhc->hphl_swh) {
		dev_dbg(component->dev, "%s: disable moisture detection for NC\n",
			__func__);
		snd_soc_component_update_bits(component, WCD937X_MBHC_NEW_CTL_2,
				    0x0C, R_OFF << 2);
		goto done;
	}

	/* If moisture_en is already enabled, then skip to plug type
	 * detection.
	 */
	if ((snd_soc_component_read(component, WCD937X_MBHC_NEW_CTL_2) &
			0x0C))
		goto done;

	wcd937x_mbhc_moisture_detect_en(mbhc, true);
	/* Read moisture comparator status */
	ret = ((snd_soc_component_read(component, WCD937X_MBHC_NEW_FSM_STATUS)
				& 0x20) ? 0 : 1);

done:
	return ret;

}

static void wcd937x_mbhc_moisture_polling_ctrl(struct wcd_mbhc *mbhc,
						bool enable)
{
	struct snd_soc_component *component = mbhc->component;

	snd_soc_component_update_bits(component,
			WCD937X_MBHC_NEW_INT_MOISTURE_DET_POLLING_CTRL,
			0x04, (enable << 2));
}

static void wcd937x_mbhc_bcs_enable(struct wcd_mbhc *mbhc,
						  bool bcs_enable)
{
	if (bcs_enable)
		wcd937x_disable_bcs_before_slow_insert(mbhc->component, false);
	else
		wcd937x_disable_bcs_before_slow_insert(mbhc->component, true);
}

/* lct modify for 05514178 begin */
void wcd937x_mbhc_test_ctrl(struct wcd_mbhc *mbhc, bool enable)
{
	struct snd_soc_component *component = mbhc->component;
	if(enable) {
		snd_soc_component_update_bits(component, WCD937X_MBHC_TEST_CTL,
				0xFF, 0x00);
	} else {
		snd_soc_component_update_bits(component, WCD937X_MBHC_TEST_CTL,
				0xFF, 0x30);
	}
}
/* lct modify for 05514178 end */

static const struct wcd_mbhc_cb mbhc_cb = {
	.request_irq = wcd937x_mbhc_request_irq,
	.irq_control = wcd937x_mbhc_irq_control,
	.free_irq = wcd937x_mbhc_free_irq,
	.clk_setup = wcd937x_mbhc_clk_setup,
	.map_btn_code_to_num = wcd937x_mbhc_btn_to_num,
	.mbhc_bias = wcd937x_mbhc_mbhc_bias_control,
	.set_btn_thr = wcd937x_mbhc_program_btn_thr,
	.lock_sleep = wcd937x_mbhc_lock_sleep,
	.register_notifier = wcd937x_mbhc_register_notifier,
	.micbias_enable_status = wcd937x_mbhc_micb_en_status,
	.hph_pa_on_status = wcd937x_mbhc_hph_pa_on_status,
	.hph_pull_up_control_v2 = wcd937x_mbhc_hph_l_pull_up_control,
	.mbhc_micbias_control = wcd937x_mbhc_request_micbias,
	.mbhc_micb_ramp_control = wcd937x_mbhc_micb_ramp_control,
	.get_hwdep_fw_cal = wcd937x_get_hwdep_fw_cal,
	.mbhc_micb_ctrl_thr_mic = wcd937x_mbhc_micb_ctrl_threshold_mic,
	.compute_impedance = wcd937x_wcd_mbhc_calc_impedance,
	.mbhc_gnd_det_ctrl = wcd937x_mbhc_gnd_det_ctrl,
	.hph_pull_down_ctrl = wcd937x_mbhc_hph_pull_down_ctrl,
	.mbhc_moisture_config = wcd937x_mbhc_moisture_config,
	.mbhc_get_moisture_status = wcd937x_mbhc_get_moisture_status,
	.mbhc_moisture_polling_ctrl = wcd937x_mbhc_moisture_polling_ctrl,
	.mbhc_moisture_detect_en = wcd937x_mbhc_moisture_detect_en,
	.bcs_enable = wcd937x_mbhc_bcs_enable,
	.mbhc_test_ctrl = wcd937x_mbhc_test_ctrl,/* lct modify for 05514178 */
};

static int wcd937x_get_hph_type(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wcd937x_mbhc *wcd937x_mbhc = wcd937x_soc_get_mbhc(component);
	struct wcd_mbhc *mbhc;

	if (!wcd937x_mbhc) {
		dev_err(component->dev, "%s: mbhc not initialized!\n",
			__func__);
		return -EINVAL;
	}

	mbhc = &wcd937x_mbhc->wcd_mbhc;

	ucontrol->value.integer.value[0] = (u32) mbhc->hph_type;
	dev_dbg(component->dev, "%s: hph_type = %u\n", __func__,
		mbhc->hph_type);

	return 0;
}

static int wcd937x_hph_impedance_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	uint32_t zl, zr;
	bool hphr;
	struct soc_multi_mixer_control *mc;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wcd937x_mbhc *wcd937x_mbhc = wcd937x_soc_get_mbhc(component);

	if (!wcd937x_mbhc) {
		dev_err(component->dev, "%s: mbhc not initialized!\n",
			__func__);
		return -EINVAL;
	}

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);
	hphr = mc->shift;
	wcd_mbhc_get_impedance(&wcd937x_mbhc->wcd_mbhc, &zl, &zr);
	dev_dbg(component->dev, "%s: zl=%u(ohms), zr=%u(ohms)\n", __func__,
		zl, zr);
	ucontrol->value.integer.value[0] = hphr ? zr : zl;

	return 0;
}

static const struct snd_kcontrol_new hph_type_detect_controls[] = {
	SOC_SINGLE_EXT("HPH Type", 0, 0, UINT_MAX, 0,
		       wcd937x_get_hph_type, NULL),
};

static const struct snd_kcontrol_new impedance_detect_controls[] = {
	SOC_SINGLE_EXT("HPHL Impedance", 0, 0, UINT_MAX, 0,
		       wcd937x_hph_impedance_get, NULL),
	SOC_SINGLE_EXT("HPHR Impedance", 0, 1, UINT_MAX, 0,
		       wcd937x_hph_impedance_get, NULL),
};

/*
 * wcd937x_mbhc_get_impedance: get impedance of headphone
 * left and right channels
 * @wcd937x_mbhc: handle to struct wcd937x_mbhc *
 * @zl: handle to left-ch impedance
 * @zr: handle to right-ch impedance
 * return 0 for success or error code in case of failure
 */
int wcd937x_mbhc_get_impedance(struct wcd937x_mbhc *wcd937x_mbhc,
			     uint32_t *zl, uint32_t *zr)
{
	if (!wcd937x_mbhc) {
		pr_err("%s: mbhc not initialized!\n", __func__);
		return -EINVAL;
	}
	if (!zl || !zr) {
		pr_err("%s: zl or zr null!\n", __func__);
		return -EINVAL;
	}

	return wcd_mbhc_get_impedance(&wcd937x_mbhc->wcd_mbhc, zl, zr);
}
EXPORT_SYMBOL(wcd937x_mbhc_get_impedance);

/*
 * wcd937x_mbhc_hs_detect: starts mbhc insertion/removal functionality
 * @component: handle to snd_soc_component *
 * @mbhc_cfg: handle to mbhc configuration structure
 * return 0 if mbhc_start is success or error code in case of failure
 */
int wcd937x_mbhc_hs_detect(struct snd_soc_component *component,
			 struct wcd_mbhc_config *mbhc_cfg)
{
	struct wcd937x_priv *wcd937x = NULL;
	struct wcd937x_mbhc *wcd937x_mbhc = NULL;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	wcd937x = snd_soc_component_get_drvdata(component);
	if (!wcd937x) {
		pr_err("%s: wcd937x is NULL\n", __func__);
		return -EINVAL;
	}

	wcd937x_mbhc = wcd937x->mbhc;
	if (!wcd937x_mbhc) {
		dev_err(component->dev, "%s: mbhc not initialized!\n",
			__func__);
		return -EINVAL;
	}

	return wcd_mbhc_start(&wcd937x_mbhc->wcd_mbhc, mbhc_cfg);
}
EXPORT_SYMBOL(wcd937x_mbhc_hs_detect);

/*
 * wcd937x_mbhc_hs_detect_exit: stop mbhc insertion/removal functionality
 * @component: handle to snd_soc_component *
 */
void wcd937x_mbhc_hs_detect_exit(struct snd_soc_component *component)
{
	struct wcd937x_priv *wcd937x = NULL;
	struct wcd937x_mbhc *wcd937x_mbhc = NULL;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return;
	}

	wcd937x = snd_soc_component_get_drvdata(component);
	if (!wcd937x) {
		pr_err("%s: wcd937x is NULL\n", __func__);
		return;
	}

	wcd937x_mbhc = wcd937x->mbhc;
	if (!wcd937x_mbhc) {
		dev_err(component->dev, "%s: mbhc not initialized!\n",
			__func__);
		return;
	}
	wcd_mbhc_stop(&wcd937x_mbhc->wcd_mbhc);
}
EXPORT_SYMBOL(wcd937x_mbhc_hs_detect_exit);

/*
 * wcd937x_mbhc_ssr_down: stop mbhc during
 * wcd937x subsystem restart
 * @mbhc: pointer to wcd937x_mbhc structure
 * @component: handle to snd_soc_component *
 */
void wcd937x_mbhc_ssr_down(struct wcd937x_mbhc *mbhc,
		         struct snd_soc_component *component)
{
	struct wcd_mbhc *wcd_mbhc = NULL;

	if (!mbhc || !component)
		return;

	wcd_mbhc = &mbhc->wcd_mbhc;
	if (wcd_mbhc == NULL) {
		dev_err(component->dev, "%s: wcd_mbhc is NULL\n", __func__);
		return;
	}

	wcd937x_mbhc_hs_detect_exit(component);
	wcd_mbhc_deinit(wcd_mbhc);
}
EXPORT_SYMBOL(wcd937x_mbhc_ssr_down);

/*
 * wcd937x_mbhc_post_ssr_init: initialize mbhc for
 * wcd937x post subsystem restart
 * @mbhc: poniter to wcd937x_mbhc structure
 * @component: handle to snd_soc_component *
 *
 * return 0 if mbhc_init is success or error code in case of failure
 */
int wcd937x_mbhc_post_ssr_init(struct wcd937x_mbhc *mbhc,
			     struct snd_soc_component *component)
{
	int ret = 0;
	struct wcd_mbhc *wcd_mbhc = NULL;

	if (!mbhc || !component)
		return -EINVAL;

	wcd_mbhc = &mbhc->wcd_mbhc;
	if (wcd_mbhc == NULL) {
		pr_err("%s: wcd_mbhc is NULL\n", __func__);
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, WCD937X_ANA_MBHC_MECH,
				0x20, 0x20);
	ret = wcd_mbhc_init(wcd_mbhc, component, &mbhc_cb, &intr_ids,
			    wcd_mbhc_registers, WCD937X_ZDET_SUPPORTED);
	if (ret) {
		dev_err(component->dev, "%s: mbhc initialization failed\n",
			__func__);
		goto done;
	}

done:
	return ret;
}
EXPORT_SYMBOL(wcd937x_mbhc_post_ssr_init);

/*
 * wcd937x_mbhc_init: initialize mbhc for wcd937x
 * @mbhc: poniter to wcd937x_mbhc struct pointer to store the configs
 * @component: handle to snd_soc_component *
 * @fw_data: handle to firmware data
 *
 * return 0 if mbhc_init is success or error code in case of failure
 */
int wcd937x_mbhc_init(struct wcd937x_mbhc **mbhc,
		      struct snd_soc_component *component,
		      struct fw_info *fw_data)
{
	struct wcd937x_mbhc *wcd937x_mbhc = NULL;
	struct wcd_mbhc *wcd_mbhc = NULL;
	struct wcd937x_pdata *pdata;
	int ret = 0;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	wcd937x_mbhc = devm_kzalloc(component->dev, sizeof(struct wcd937x_mbhc),
				    GFP_KERNEL);
	if (!wcd937x_mbhc)
		return -ENOMEM;

	wcd937x_mbhc->fw_data = fw_data;
	BLOCKING_INIT_NOTIFIER_HEAD(&wcd937x_mbhc->notifier);
	wcd_mbhc = &wcd937x_mbhc->wcd_mbhc;
	if (wcd_mbhc == NULL) {
		pr_err("%s: wcd_mbhc is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}


	/* Setting default mbhc detection logic to ADC */
	wcd_mbhc->mbhc_detection_logic = WCD_DETECTION_ADC;

	pdata = dev_get_platdata(component->dev);
	if (!pdata) {
		dev_err(component->dev, "%s: pdata pointer is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}
	wcd_mbhc->micb_mv = pdata->micbias.micb2_mv;

	ret = wcd_mbhc_init(wcd_mbhc, component, &mbhc_cb,
				&intr_ids, wcd_mbhc_registers,
				WCD937X_ZDET_SUPPORTED);
	if (ret) {
		dev_err(component->dev, "%s: mbhc initialization failed\n",
			__func__);
		goto err;
	}

	(*mbhc) = wcd937x_mbhc;
	snd_soc_add_component_controls(component, impedance_detect_controls,
				   ARRAY_SIZE(impedance_detect_controls));
	snd_soc_add_component_controls(component, hph_type_detect_controls,
				   ARRAY_SIZE(hph_type_detect_controls));

	return 0;
err:
	devm_kfree(component->dev, wcd937x_mbhc);
	return ret;
}
EXPORT_SYMBOL(wcd937x_mbhc_init);

/*
 * wcd937x_mbhc_deinit: deinitialize mbhc for wcd937x
 * @component: handle to snd_soc_component *
 */
void wcd937x_mbhc_deinit(struct snd_soc_component *component)
{
	struct wcd937x_priv *wcd937x;
	struct wcd937x_mbhc *wcd937x_mbhc;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return;
	}

	wcd937x = snd_soc_component_get_drvdata(component);
	if (!wcd937x) {
		pr_err("%s: wcd937x is NULL\n", __func__);
		return;
	}

	wcd937x_mbhc = wcd937x->mbhc;
	if (wcd937x_mbhc) {
		wcd_mbhc_deinit(&wcd937x_mbhc->wcd_mbhc);
		devm_kfree(component->dev, wcd937x_mbhc);
	}
}
EXPORT_SYMBOL(wcd937x_mbhc_deinit);
