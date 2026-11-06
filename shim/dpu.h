/*
 * The Tensor G5's display controller as the shim uses it: read, report, start one frame, count frames.
 *
 * Every offset is from Google's vsi-drm driver for the DC9x00 (private/google-modules/display/vsi-drm/verisilicon/9x00/
 * vs_dc_reg_be.h, vs_dc_reg_fe0.h, vs_dc_hw.c at 6f689be7fa23; GPL-2.0, facts only, nothing copied). Read from source, not
 * yet seen on a phone: docs/pixel10-bringup.md in scion-kernel says which parts are guesses.
 */
#ifndef SHIM_DPU_H
#define SHIM_DPU_H

#include <stdint.h>

#ifndef LGA_DPU                                         /* a test build puts it in RAM (qemu-probe-test.sh) */
#define LGA_DPU                 0x0EE00000ULL           /* sswrp_dpu 0x0EC00000 + dc9x00@200000 */
#endif
#define LGA_DPU_SMMU            (LGA_DPU - 0x80000ULL)  /* dpu_smmu 0x0ED80000 */
#define SMMU_CR0                0x20                    /* bit 0 SMMUEN: 0, the DPU reads physical addresses */

#define DPU_L0_ADDRESS          0x01802C                /* layer 0's shadow registers */
#define DPU_L0_HIGH_ADDRESS     0x018030                /* bits 7:0 */
#define DPU_L0_STRIDE           0x018044                /* bits 17:0 */
#define DPU_L0_SIZE             0x01805C                /* width 15:0, height 31:16 */
#define DPU_L0_CONFIG           0x018088                /* enable 0:0, format 29:24 (0: A8R8G8B8) */
#define DPU_LAYER_STEP          0x004000                /* layer 1 at 0x01C088 ... */

#define DPU_CHIP_ID             0x000020                /* 0x9400 in Google's table */
#define DPU_CHIP_REV            0x000024                /* 0x7002 */
#define DPU_CHIP_CUSTOMER       0x000030                /* 0x32A (the other table in Google's tree: 0x316) */
#define DPU_PRODUCT_ID          0x0000A8                /* 0x02094000 */
#define DPU_BE_INTR_ENABLE      0x080004
#define DPU_BE_INTR_STATUS      0x080008                /* bit 0 output 0 frame start, 1 frame done, 2 underrun */
#define DPU_OUT0_START          0x0B6000                /* bit 0: "trigger the output 0 start" */
#define DPU_OUT0_SW_CONFIG      0x0B6004                /* bit 0: software panel start, "ready" */
#define DPU_OUT0_MODE           0x0B6008                /* bit 1: work mode, 0 video, 1 command */
#define DPU_OUT0_CLK_EN         0x0B60B4
#define DPU_DSI0_FRAME_CNT      0x0B6434                /* "DSI0 frame counter in timing gen" */

/* ramoops (mainline lga.dtsi): the console zone is where Android's pstore finds the shim's log after a restart */
#define LGA_RAMOOPS_CONSOLE     0x95200000ULL
#define LGA_RAMOOPS_CONSOLE_MAX 0x200000ULL

struct dpu_fb {
	uint64_t base;
	uint32_t width, height, pixels_per_line;
};

/* the CPU's pointer-authentication, BTI and MTE bits, once, at EL1 */
void dpu_cpu_report(void);
/* the controller's identity, its output 0 and its layers, read only */
void dpu_report(const char *when);
/* one frame of output 0: START, then SW_CONFIG, then wait up to `wait_ms` for the frame counter; 1 if it moved */
int dpu_kick(uint32_t wait_ms);
/* the probe run (scion.probe=): bands painted on a timeline, kicked or not; returns when it is done */
void dpu_probe(const struct dpu_fb *fb, int kick);

#endif
