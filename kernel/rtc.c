#include "types.h"
#include "defs.h"
#include "cv180x_reg.h"
#include "io.h"
#include "printf.h"
#include "spinlock.h"
#include "include/rtc.h"

/* Spinlock for RTC */
struct spinlock rtc_lock;

static void rtc_calibration(void)
{
    int result = -1;
    uint32_t offset = 0x80;
    uint32_t ctime, cvalue, ftime, fvalue, ftune = 0x100;
    uint32_t freq_int, freq_frac;
    double freq;
    int try = 8;

    write32(RTC_ANA_CALIB, (read32(RTC_ANA_CALIB) & ~0x8000ffff) | ftune);
    while (try-- > 0)
    {
        write32(RTC_FC_COARSE_EN, 1);
        ctime = read32(RTC_FC_COARSE_CAL) >> 16;
        //debug("ctime: %d", ctime);
        while ((read32(RTC_FC_COARSE_CAL) >> 16) <= ctime) {}
        write32(RTC_FC_COARSE_EN, 0);
        cvalue = read32(RTC_FC_COARSE_CAL) & 0xffff;
        //debug("[%d] cvalue: %d", try, cvalue);
        if (cvalue  > 770) {
            ftune += offset;
        } else if (cvalue < 755) {
            ftune -= offset;
        } else {
            //debug("coarse calib ok! cvalue=%d", cvalue);
            result = 0;
            break;
        }
        write32(RTC_ANA_CALIB, (read32(RTC_ANA_CALIB) & ~0xffff) | ftune);
        offset >>= 1;
        //debug("ftune: 0x%08x, offset: %d, ana_calib: 0x%08x", ftune, offset, read32(RTC_ANA_CALIB));
        delayus(2000);
    }

    if (result) {
        warn("coarse calib failed");
    }

    /* 2.1 RTC_SEC_PULSE_GEN[31]=0, fc_fine_en=1*/
    write32(RTC_SEC_PULST_GEN, read32(RTC_SEC_PULST_GEN) & ~(1 << 31));
    write32(RTC_FC_FINE_EN, 1);
    /* 2.2 RTC_FC_FINE_TIME */
    ftime = read32(RTC_FC_FINE_CAL) >> 24;
    while((read32(RTC_FC_FINE_CAL) >> 24) <= ftime) {}

    fvalue = read32(RTC_FC_FINE_CAL) & 0xffffff;
    /* 2.4 32KHz */
    freq = (256.0 / ((double)fvalue * 40.0)) * 1000000000.0;
    freq_int = (uint32_t)freq;
    freq_frac = (uint32_t)((freq - (double)freq_int) * 256.0);
    /* 2.5 RTC_SEC_PULSE_GEN */
    write32(RTC_SEC_PULST_GEN, (freq_int << 8) | (freq_frac & 0xff));
    write32(RTC_FC_FINE_EN, 0);
    //debug("fine calib: freq int=%d, frac=%d", freq_int, freq_frac);
}

void rtc_init(void)
{
    initlock(&rtc_lock, "rtc_lock");
    rtc_calibration();

    write32(RTC_POR_DB_MAGIC_KEY, 0x5af0);
    write32(RTC_SET_SEC_CNTR_VALUE, 1728884419);   // 2024-10-14 14:40:19
    write32(RTC_SET_SEC_CNTR_TRIG, 1);
    delayms(500);
    //debug("RTC_SEC_CNTR_VALUE: %d", read32(RTC_SEC_CNTR_VALUE));
    while(read32(RTC_SEC_CNTR_VALUE) < 1728884419) {};
    write32(RTC_PWR_DET_COMP, read32(RTC_PWR_DET_COMP) | 1);
    write32(RTC_PWR_DET_SEL, read32(RTC_PWR_DET_SEL) | 1);
    write32(RTC_POR_RST_CTRL, read32(RTC_POR_RST_CTRL) | 0x2);
    write32(RTC_EN_PWR_VBAT_DET, read32(RTC_EN_PWR_VBAT_DET) & ~0x4);
    //delayms(2000);
    printf("[rtc] current sec: %d\n", read32(RTC_SEC_CNTR_VALUE));
    info("rtc_init ok!");
}

// Read current time
uint64_t rtc_get_time(void)
{
    return (uint64_t)read32(RTC_SEC_CNTR_VALUE);
}
