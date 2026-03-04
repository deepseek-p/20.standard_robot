#include "vt03_link.h"

#if VT03_ENABLE

#include "detect_task.h"
#include "CRC8_CRC16.h"

// Keep aligned with RC_CHANNAL_ERROR_VALUE in remote_control.c
#define VT03_CHANNAL_ERROR_VALUE 700

enum
{
    VT03_WAIT_SOF1 = 0,
    VT03_WAIT_SOF2,
    VT03_RECV_DATA
};

static uint8_t vt03_state = VT03_WAIT_SOF1;
static uint8_t vt03_buf[VT03_FRAME_LEN];
static uint8_t vt03_idx = 0u;

extern RC_ctrl_t rc_ctrl;
vt03_ext_t vt03_ext;

static int16_t vt03_abs(int16_t value)
{
    return (value >= 0) ? value : (int16_t)(-value);
}

static void vt03_frame_decode(const uint8_t *frame)
{
    const uint8_t *d;
    uint64_t pk = 0u;
    uint8_t mb;
    uint8_t i;

    if (!verify_CRC16_check_sum((uint8_t *)frame, VT03_FRAME_LEN))
    {
        return;
    }

    d = &frame[2];
    for (i = 0u; i < 8u; i++)
    {
        pk |= ((uint64_t)d[i]) << (i * 8u);
    }

    rc_ctrl.rc.ch[0] = (int16_t)((pk >> 0) & 0x7FFu) - (int16_t)VT03_CH_VALUE_OFFSET;
    rc_ctrl.rc.ch[1] = (int16_t)((pk >> 11) & 0x7FFu) - (int16_t)VT03_CH_VALUE_OFFSET;
    rc_ctrl.rc.ch[2] = (int16_t)((pk >> 33) & 0x7FFu) - (int16_t)VT03_CH_VALUE_OFFSET;
    rc_ctrl.rc.ch[3] = (int16_t)((pk >> 22) & 0x7FFu) - (int16_t)VT03_CH_VALUE_OFFSET;

    vt03_ext.mode_sw = (uint8_t)((pk >> 44) & 0x03u);
    vt03_ext.pause = (uint8_t)((pk >> 46) & 0x01u);
    vt03_ext.fn_1 = (uint8_t)((pk >> 47) & 0x01u);
    vt03_ext.fn_2 = (uint8_t)((pk >> 48) & 0x01u);
    rc_ctrl.rc.ch[4] = (int16_t)((pk >> 49) & 0x7FFu) - (int16_t)VT03_CH_VALUE_OFFSET;
    vt03_ext.trigger = (uint8_t)((pk >> 60) & 0x01u);

    /* VT03 mode_sw -> DBUS s[0] 鍏煎鍊?     * 璇存槑涔︾‘璁ゅ彧鏈?3 涓€?(鏈€澶у€?2):
     *   C=0 -> RC_SW_DOWN(2)  瀹夊叏/鏃犲姏
     *   N=1 -> RC_SW_MID(3)   姝ｅ父鎿嶆帶
     *   S=2 -> RC_SW_UP(1)    灏忛檧铻?     *   3 涓嶄細鍑虹幇锛岄槻寰℃€у～ RC_SW_DOWN */
    {
        static const int8_t mode_map[4] = {
            RC_SW_DOWN, RC_SW_MID, RC_SW_UP, RC_SW_DOWN
        };
        uint8_t idx = (uint8_t)(vt03_ext.mode_sw & 0x03u);
        rc_ctrl.rc.s[0] = mode_map[idx];
        rc_ctrl.rc.s[1] = RC_SW_MID;
    }

    rc_ctrl.mouse.x = (int16_t)(d[8] | ((uint16_t)d[9] << 8));
    rc_ctrl.mouse.y = (int16_t)(d[10] | ((uint16_t)d[11] << 8));
    rc_ctrl.mouse.z = (int16_t)(d[12] | ((uint16_t)d[13] << 8));

    mb = d[14];
    rc_ctrl.mouse.press_l = (uint8_t)(mb & 0x01u);
    rc_ctrl.mouse.press_r = (uint8_t)((mb >> 1) & 0x01u);
    vt03_ext.mouse_mid = (uint8_t)((mb >> 4) & 0x03u);

    rc_ctrl.key.v = (uint16_t)(d[15] | ((uint16_t)d[16] << 8));

    detect_hook(VT03_TOE);
}

void vt03_parse_byte(uint8_t byte)
{
    switch (vt03_state)
    {
    case VT03_WAIT_SOF1:
        if (byte == VT03_SOF_1)
        {
            vt03_buf[0] = byte;
            vt03_state = VT03_WAIT_SOF2;
        }
        break;

    case VT03_WAIT_SOF2:
        if (byte == VT03_SOF_2)
        {
            vt03_buf[1] = byte;
            vt03_idx = 2u;
            vt03_state = VT03_RECV_DATA;
        }
        else if (byte == VT03_SOF_1)
        {
            vt03_buf[0] = byte;
        }
        else
        {
            vt03_state = VT03_WAIT_SOF1;
        }
        break;

    case VT03_RECV_DATA:
        vt03_buf[vt03_idx++] = byte;
        if (vt03_idx >= VT03_FRAME_LEN)
        {
            vt03_frame_decode(vt03_buf);
            vt03_state = VT03_WAIT_SOF1;
            vt03_idx = 0u;
        }
        break;

    default:
        vt03_state = VT03_WAIT_SOF1;
        vt03_idx = 0u;
        break;
    }
}

uint8_t vt03_data_is_error(void)
{
    if (vt03_abs(rc_ctrl.rc.ch[0]) > VT03_CHANNAL_ERROR_VALUE)
    {
        goto error;
    }
    if (vt03_abs(rc_ctrl.rc.ch[1]) > VT03_CHANNAL_ERROR_VALUE)
    {
        goto error;
    }
    if (vt03_abs(rc_ctrl.rc.ch[2]) > VT03_CHANNAL_ERROR_VALUE)
    {
        goto error;
    }
    if (vt03_abs(rc_ctrl.rc.ch[3]) > VT03_CHANNAL_ERROR_VALUE)
    {
        goto error;
    }

    return 0u;

error:
    rc_ctrl.rc.ch[0] = 0;
    rc_ctrl.rc.ch[1] = 0;
    rc_ctrl.rc.ch[2] = 0;
    rc_ctrl.rc.ch[3] = 0;
    rc_ctrl.rc.ch[4] = 0;
    rc_ctrl.rc.s[0] = RC_SW_DOWN;
    rc_ctrl.rc.s[1] = RC_SW_DOWN;
    rc_ctrl.mouse.x = 0;
    rc_ctrl.mouse.y = 0;
    rc_ctrl.mouse.z = 0;
    rc_ctrl.mouse.press_l = 0;
    rc_ctrl.mouse.press_r = 0;
    rc_ctrl.key.v = 0;
    return 1u;
}

#endif
