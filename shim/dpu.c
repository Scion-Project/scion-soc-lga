/* Display probe: README.md ("The display probe") and docs/pixel10-bringup.md in scion-kernel say what it is for. */
#include "dpu.h"
#include "shim.h"

static inline uint32_t
rd(uint32_t off)
{
	return *(volatile uint32_t *)(uintptr_t)(LGA_DPU + off);
}

static uint64_t
ticks(void)
{
	uint64_t t;
	__asm__ volatile ("isb; mrs %0, cntpct_el0" : "=r" (t));
	return t;
}

static uint64_t
freq(void)
{
	uint64_t f;
	__asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (f));
	return f ? f : 38400000;
}

static void
wait_ms(uint32_t ms)
{
	uint64_t end = ticks() + freq() / 1000 * ms;
	while (ticks() < end) {
	}
}

void
dpu_cpu_report(void)
{
	uint64_t midr, isar1, isar2, pfr1;
	__asm__ volatile ("mrs %0, midr_el1; mrs %1, id_aa64isar1_el1; mrs %2, id_aa64isar2_el1; mrs %3, id_aa64pfr1_el1"
	    : "=r" (midr), "=r" (isar1), "=r" (isar2), "=r" (pfr1));
	/* APA 4:7, API 8:11, GPA 24:27, GPI 28:31 of ISAR1; APA3 12:15, GPA3 8:11 of ISAR2; BT 3:0, MTE 11:8 of PFR1 */
	say("cpu: midr %x isar1 %x isar2 %x pfr1 %x: pauth addr %d generic %d, bti %d, mte %d\n", midr, isar1, isar2, pfr1,
	    ((isar1 >> 4) & 15) | ((isar1 >> 8) & 15) | ((isar2 >> 12) & 15),
	    ((isar1 >> 24) & 15) | ((isar1 >> 28) & 15) | ((isar2 >> 8) & 15), pfr1 & 15, (pfr1 >> 8) & 15);
}

void
dpu_report(const char *when)
{
	say("dpu: %s: chip %x rev %x customer %x product %x\n", when, (uint64_t)rd(DPU_CHIP_ID), (uint64_t)rd(DPU_CHIP_REV),
	    (uint64_t)rd(DPU_CHIP_CUSTOMER), (uint64_t)rd(DPU_PRODUCT_ID));
	say("dpu: out0 start %x sw_config %x mode %x clk_en %x; be intr enable %x status %x; frames %d\n",
	    (uint64_t)rd(DPU_OUT0_START), (uint64_t)rd(DPU_OUT0_SW_CONFIG), (uint64_t)rd(DPU_OUT0_MODE),
	    (uint64_t)rd(DPU_OUT0_CLK_EN), (uint64_t)rd(DPU_BE_INTR_ENABLE), (uint64_t)rd(DPU_BE_INTR_STATUS),
	    (uint64_t)rd(DPU_DSI0_FRAME_CNT));
	say("dpu: smmu cr0 %x\n", (uint64_t)*(volatile uint32_t *)(uintptr_t)(LGA_DPU_SMMU + SMMU_CR0));
}

int
dpu_kick(uint32_t ms)
{
	uint32_t before = rd(DPU_DSI0_FRAME_CNT);
	mmio_write32(LGA_DPU + DPU_OUT0_START, 1);
	mmio_write32(LGA_DPU + DPU_OUT0_SW_CONFIG, 1);
	uint64_t end = ticks() + freq() / 1000 * ms;
	while (ticks() < end) {
		if (rd(DPU_DSI0_FRAME_CNT) != before) {
			return 1;
		}
	}
	return 0;
}

static void
band(const struct dpu_fb *fb, int n, uint32_t rgb)
{
	uint32_t y0 = 1200 + (uint32_t)n * 240;
	for (uint32_t y = y0; y < y0 + 200 && y < fb->height; y++) {
		volatile uint32_t *row = (volatile uint32_t *)(uintptr_t)(fb->base + (uint64_t)y * fb->pixels_per_line * 4);
		for (uint32_t x = 0; x < fb->width; x++) {
			row[x] = rgb;
		}
	}
	say("probe: band %d painted\n", (uint64_t)n);
}

void
dpu_probe(const struct dpu_fb *fb, int kick)
{
	dpu_cpu_report();
	dpu_report("before");
	/* what the panel shows afterwards is the bands that were there at the last frame it was sent (README.md) */
	band(fb, 0, 0x000000ff);                /* blue: before anything else */
	wait_ms(2000);
	dpu_report("2 s after blue, no frame started");
	band(fb, 1, 0x00ffff00);                /* yellow */
	wait_ms(2000);
	dpu_report("2 s after yellow, no frame started");
	band(fb, 2, 0x0000ff00);                /* green: painted just before the kick */
	if (kick) {
		say("probe: kick: frame counter %s\n", dpu_kick(100) ? "moved" : "did not move");
		wait_ms(2000);
		dpu_report("2 s after the kick");
	}
	band(fb, 3, 0x00ffffff);                /* white: after the kick, never sent */
	wait_ms(2000);
	dpu_report("end");
}
