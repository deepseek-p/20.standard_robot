#include "rm_ui.h"

#include <string.h>

#include "CRC8_CRC16.h"
#include "chassis_task.h"
#include "detect_task.h"
#include "protocol.h"
#include "referee.h"
#include "shoot.h"
#include "usart.h"

#define RM_UI_STATIC_ASSERT(name, expr) typedef char name[(expr) ? 1 : -1]

#define RM_UI_DELETE_LAYER_CMD_ID      0x0100u
#define RM_UI_DRAW_ONE_CMD_ID          0x0101u
#define RM_UI_DRAW_FIVE_CMD_ID         0x0103u
#define RM_UI_DRAW_CHAR_CMD_ID         0x0110u

#define RM_UI_OP_NULL                  0u
#define RM_UI_OP_ADD                   1u
#define RM_UI_OP_MODIFY                2u
#define RM_UI_OP_DELETE                3u

#define RM_UI_TYPE_LINE                0u
#define RM_UI_TYPE_CIRCLE              2u
#define RM_UI_TYPE_CHAR                7u

#define RM_UI_LAYER_RETICLE            0u
#define RM_UI_LAYER_STATUS             1u
#define RM_UI_LAYER_EVENT              2u

#define RM_UI_COLOR_GREEN              2u
#define RM_UI_COLOR_CYAN               6u
#define RM_UI_COLOR_WHITE              8u

#define RM_UI_SCREEN_CENTER_X          960u
#define RM_UI_RETICLE_CENTER_Y         475u
#define RM_UI_RETICLE_MAIN_TOP_Y       570u
#define RM_UI_RETICLE_MAIN_BOTTOM_Y    360u
#define RM_UI_RETICLE_MAIN_LEFT_X      720u
#define RM_UI_RETICLE_MAIN_RIGHT_X     1200u
#define RM_UI_RETICLE_SUB_LEFT_X       780u
#define RM_UI_RETICLE_SUB_RIGHT_X      1140u
#define RM_UI_RETICLE_MID_Y            475u
#define RM_UI_RETICLE_SUB_Y            430u

#define RM_UI_MODE_TEXT_X              180u
#define RM_UI_GEAR_TEXT_X              1460u
#define RM_UI_TEXT_Y                   430u
#define RM_UI_TEXT_FONT_SIZE           24u
#define RM_UI_TEXT_WIDTH               2u

#define RM_UI_REVERSE_LIGHT_X          960u
#define RM_UI_REVERSE_LIGHT_Y          320u
#define RM_UI_REVERSE_LIGHT_RADIUS     12u
#define RM_UI_REVERSE_LIGHT_WIDTH      4u
#define RM_UI_REVERSE_BLINK_MS         800u
#define RM_UI_REVERSE_BLINK_PERIOD_MS  100u

#define RM_UI_SEND_INTERVAL_MS         100u
#define RM_UI_TX_TIMEOUT_MS            20u
#define RM_UI_TEXT_MAX_LEN             30u
#define RM_UI_SPIN_WZ_THRESHOLD        1.0f

#pragma pack(push, 1)

typedef struct
{
    uint16_t data_cmd_id;
    uint16_t sender_id;
    uint16_t receiver_id;
} rm_ui_interactive_header_t;

typedef struct
{
    uint8_t figure_name[3];
    uint32_t operate_type : 3;
    uint32_t figure_type : 3;
    uint32_t layer : 4;
    uint32_t color : 4;
    uint32_t details_a : 9;
    uint32_t details_b : 9;
    uint32_t width : 10;
    uint32_t start_x : 11;
    uint32_t start_y : 11;
    uint32_t details_c : 10;
    uint32_t details_d : 11;
    uint32_t details_e : 11;
} rm_ui_graphic_t;

typedef struct
{
    uint8_t delete_type;
    uint8_t layer;
} rm_ui_delete_t;

typedef struct
{
    rm_ui_graphic_t graphic[5];
} rm_ui_graphic_5_t;

typedef struct
{
    rm_ui_graphic_t graphic;
    uint8_t text[RM_UI_TEXT_MAX_LEN];
} rm_ui_string_t;

#pragma pack(pop)

typedef enum
{
    RM_UI_INIT_IDLE = 0,
    RM_UI_INIT_DELETE_ALL,
    RM_UI_INIT_DRAW_RETICLE,
    RM_UI_INIT_DRAW_MODE,
    RM_UI_INIT_DRAW_GEAR,
} rm_ui_init_stage_e;

typedef enum
{
    RM_UI_CHASSIS_FOLLOW = 0,
    RM_UI_CHASSIS_SPIN = 1,
} rm_ui_chassis_mode_e;

typedef struct
{
    uint8_t online;
    uint8_t mode_drawn;
    uint8_t gear_drawn;
    uint8_t reverse_light_visible;
    rm_ui_init_stage_e init_stage;
    rm_ui_chassis_mode_e last_mode;
    shoot_ui_gear_e last_gear;
    uint32_t last_send_tick;
    uint32_t reverse_blink_deadline;
} rm_ui_state_t;

RM_UI_STATIC_ASSERT(rm_ui_graphic_size_check, sizeof(rm_ui_graphic_t) == 15);
RM_UI_STATIC_ASSERT(rm_ui_delete_size_check, sizeof(rm_ui_delete_t) == 2);
RM_UI_STATIC_ASSERT(rm_ui_string_size_check, sizeof(rm_ui_string_t) == 45);

static rm_ui_state_t rm_ui_state;
static uint8_t rm_ui_seq = 0u;

static uint16_t rm_ui_get_receiver_id(uint16_t sender_id);
static uint8_t rm_ui_ready_to_send(uint16_t *sender_id, uint16_t *receiver_id);
static void rm_ui_reset_online_state(void);
static rm_ui_chassis_mode_e rm_ui_get_chassis_mode(void);
static shoot_ui_gear_e rm_ui_get_shoot_gear(void);
static void rm_ui_fill_name(uint8_t name[3], char c0, char c1, char c2);
static void rm_ui_clear_graphic(rm_ui_graphic_t *graphic);
static void rm_ui_set_line(rm_ui_graphic_t *graphic, char c0, char c1, char c2,
                           uint32_t operate_type, uint32_t layer, uint32_t color, uint32_t width,
                           uint32_t start_x, uint32_t start_y, uint32_t end_x, uint32_t end_y);
static void rm_ui_set_circle(rm_ui_graphic_t *graphic, char c0, char c1, char c2,
                             uint32_t operate_type, uint32_t layer, uint32_t color, uint32_t width,
                             uint32_t center_x, uint32_t center_y, uint32_t radius);
static void rm_ui_set_text_graphic(rm_ui_graphic_t *graphic, char c0, char c1, char c2,
                                   uint32_t operate_type, uint32_t layer, uint32_t color,
                                   uint32_t font_size, uint32_t width, uint32_t start_x,
                                   uint32_t start_y, uint32_t text_len);
static uint8_t rm_ui_send_interactive(uint16_t data_cmd_id, const void *data, uint16_t data_len);
static uint8_t rm_ui_send_delete_all(void);
static uint8_t rm_ui_send_reticle(void);
static uint8_t rm_ui_send_mode_text(rm_ui_chassis_mode_e mode, uint8_t operate_type);
static uint8_t rm_ui_send_gear_text(shoot_ui_gear_e gear, uint8_t operate_type);
static uint8_t rm_ui_send_reverse_light(uint8_t operate_type);
static uint8_t rm_ui_reverse_light_should_show(uint32_t now_tick);

void rm_ui_init(void)
{
    memset(&rm_ui_state, 0, sizeof(rm_ui_state));
    rm_ui_state.init_stage = RM_UI_INIT_DELETE_ALL;
    rm_ui_state.last_mode = RM_UI_CHASSIS_FOLLOW;
    rm_ui_state.last_gear = SHOOT_UI_GEAR_LOW;
    rm_ui_seq = 0u;
}

void rm_ui_update(void)
{
    uint32_t now_tick;
    uint8_t reverse_visible;
    rm_ui_chassis_mode_e current_mode;
    shoot_ui_gear_e current_gear;

    if (shoot_consume_reverse_success_pulse())
    {
        rm_ui_state.reverse_blink_deadline = HAL_GetTick() + RM_UI_REVERSE_BLINK_MS;
    }

    if (toe_is_error(REFEREE_TOE))
    {
        rm_ui_reset_online_state();
        return;
    }

    now_tick = HAL_GetTick();
    if ((uint32_t)(now_tick - rm_ui_state.last_send_tick) < RM_UI_SEND_INTERVAL_MS)
    {
        return;
    }

    if (!rm_ui_state.online)
    {
        rm_ui_state.online = 1u;
        rm_ui_state.init_stage = RM_UI_INIT_DELETE_ALL;
        rm_ui_state.mode_drawn = 0u;
        rm_ui_state.gear_drawn = 0u;
        rm_ui_state.reverse_light_visible = 0u;
    }

    current_mode = rm_ui_get_chassis_mode();
    current_gear = rm_ui_get_shoot_gear();

    if (rm_ui_state.init_stage == RM_UI_INIT_DELETE_ALL)
    {
        if (rm_ui_send_delete_all())
        {
            rm_ui_state.init_stage = RM_UI_INIT_DRAW_RETICLE;
            rm_ui_state.last_send_tick = now_tick;
        }
        return;
    }

    if (rm_ui_state.init_stage == RM_UI_INIT_DRAW_RETICLE)
    {
        if (rm_ui_send_reticle())
        {
            rm_ui_state.init_stage = RM_UI_INIT_DRAW_MODE;
            rm_ui_state.last_send_tick = now_tick;
        }
        return;
    }

    if (rm_ui_state.init_stage == RM_UI_INIT_DRAW_MODE)
    {
        if (rm_ui_send_mode_text(current_mode, RM_UI_OP_ADD))
        {
            rm_ui_state.last_mode = current_mode;
            rm_ui_state.mode_drawn = 1u;
            rm_ui_state.init_stage = RM_UI_INIT_DRAW_GEAR;
            rm_ui_state.last_send_tick = now_tick;
        }
        return;
    }

    if (rm_ui_state.init_stage == RM_UI_INIT_DRAW_GEAR)
    {
        if (rm_ui_send_gear_text(current_gear, RM_UI_OP_ADD))
        {
            rm_ui_state.last_gear = current_gear;
            rm_ui_state.gear_drawn = 1u;
            rm_ui_state.init_stage = RM_UI_INIT_IDLE;
            rm_ui_state.last_send_tick = now_tick;
        }
        return;
    }

    if (!rm_ui_state.mode_drawn || current_mode != rm_ui_state.last_mode)
    {
        if (rm_ui_send_mode_text(current_mode, rm_ui_state.mode_drawn ? RM_UI_OP_MODIFY : RM_UI_OP_ADD))
        {
            rm_ui_state.last_mode = current_mode;
            rm_ui_state.mode_drawn = 1u;
            rm_ui_state.last_send_tick = now_tick;
        }
        return;
    }

    if (!rm_ui_state.gear_drawn || current_gear != rm_ui_state.last_gear)
    {
        if (rm_ui_send_gear_text(current_gear, rm_ui_state.gear_drawn ? RM_UI_OP_MODIFY : RM_UI_OP_ADD))
        {
            rm_ui_state.last_gear = current_gear;
            rm_ui_state.gear_drawn = 1u;
            rm_ui_state.last_send_tick = now_tick;
        }
        return;
    }

    reverse_visible = rm_ui_reverse_light_should_show(now_tick);
    if (reverse_visible != rm_ui_state.reverse_light_visible)
    {
        if (rm_ui_send_reverse_light(reverse_visible ? RM_UI_OP_ADD : RM_UI_OP_DELETE))
        {
            rm_ui_state.reverse_light_visible = reverse_visible;
            rm_ui_state.last_send_tick = now_tick;
        }
    }
}

static uint16_t rm_ui_get_receiver_id(uint16_t sender_id)
{
    if (sender_id >= 1u && sender_id <= 6u)
    {
        return (uint16_t)(0x0100u + sender_id);
    }

    if (sender_id >= 11u && sender_id <= 16u)
    {
        return (uint16_t)(0x015Au + sender_id);
    }

    if (sender_id >= 101u && sender_id <= 106u)
    {
        return (uint16_t)(0x0100u + sender_id);
    }

    return 0u;
}

static uint8_t rm_ui_ready_to_send(uint16_t *sender_id, uint16_t *receiver_id)
{
    uint16_t sender;
    uint16_t receiver;

    if (sender_id == NULL || receiver_id == NULL)
    {
        return 0u;
    }

    sender = (uint16_t)get_robot_id();
    receiver = rm_ui_get_receiver_id(sender);
    if (sender == 0u || receiver == 0u)
    {
        return 0u;
    }

    *sender_id = sender;
    *receiver_id = receiver;
    return 1u;
}

static void rm_ui_reset_online_state(void)
{
    rm_ui_state.online = 0u;
    rm_ui_state.mode_drawn = 0u;
    rm_ui_state.gear_drawn = 0u;
    rm_ui_state.reverse_light_visible = 0u;
    rm_ui_state.init_stage = RM_UI_INIT_DELETE_ALL;
}

static rm_ui_chassis_mode_e rm_ui_get_chassis_mode(void)
{
    chassis_debug_snapshot_t snapshot;

    if (get_chassis_debug_snapshot(&snapshot))
    {
        if (snapshot.mode == (uint8_t)CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW)
        {
            return RM_UI_CHASSIS_FOLLOW;
        }

        if (snapshot.mode == (uint8_t)CHASSIS_VECTOR_NO_FOLLOW_YAW)
        {
            if (snapshot.wz_set > RM_UI_SPIN_WZ_THRESHOLD || snapshot.wz_set < -RM_UI_SPIN_WZ_THRESHOLD)
            {
                return RM_UI_CHASSIS_SPIN;
            }
        }
    }

    return RM_UI_CHASSIS_FOLLOW;
}

static shoot_ui_gear_e rm_ui_get_shoot_gear(void)
{
    return shoot_get_ui_gear();
}

static void rm_ui_fill_name(uint8_t name[3], char c0, char c1, char c2)
{
    name[0] = (uint8_t)c0;
    name[1] = (uint8_t)c1;
    name[2] = (uint8_t)c2;
}

static void rm_ui_clear_graphic(rm_ui_graphic_t *graphic)
{
    if (graphic != NULL)
    {
        memset(graphic, 0, sizeof(rm_ui_graphic_t));
    }
}

static void rm_ui_set_line(rm_ui_graphic_t *graphic, char c0, char c1, char c2,
                           uint32_t operate_type, uint32_t layer, uint32_t color, uint32_t width,
                           uint32_t start_x, uint32_t start_y, uint32_t end_x, uint32_t end_y)
{
    rm_ui_clear_graphic(graphic);
    rm_ui_fill_name(graphic->figure_name, c0, c1, c2);
    graphic->operate_type = operate_type;
    graphic->figure_type = RM_UI_TYPE_LINE;
    graphic->layer = layer;
    graphic->color = color;
    graphic->width = width;
    graphic->start_x = start_x;
    graphic->start_y = start_y;
    graphic->details_d = end_x;
    graphic->details_e = end_y;
}

static void rm_ui_set_circle(rm_ui_graphic_t *graphic, char c0, char c1, char c2,
                             uint32_t operate_type, uint32_t layer, uint32_t color, uint32_t width,
                             uint32_t center_x, uint32_t center_y, uint32_t radius)
{
    rm_ui_clear_graphic(graphic);
    rm_ui_fill_name(graphic->figure_name, c0, c1, c2);
    graphic->operate_type = operate_type;
    graphic->figure_type = RM_UI_TYPE_CIRCLE;
    graphic->layer = layer;
    graphic->color = color;
    graphic->width = width;
    graphic->start_x = center_x;
    graphic->start_y = center_y;
    graphic->details_c = radius;
}

static void rm_ui_set_text_graphic(rm_ui_graphic_t *graphic, char c0, char c1, char c2,
                                   uint32_t operate_type, uint32_t layer, uint32_t color,
                                   uint32_t font_size, uint32_t width, uint32_t start_x,
                                   uint32_t start_y, uint32_t text_len)
{
    rm_ui_clear_graphic(graphic);
    rm_ui_fill_name(graphic->figure_name, c0, c1, c2);
    graphic->operate_type = operate_type;
    graphic->figure_type = RM_UI_TYPE_CHAR;
    graphic->layer = layer;
    graphic->color = color;
    graphic->details_a = font_size;
    graphic->details_b = text_len;
    graphic->width = width;
    graphic->start_x = start_x;
    graphic->start_y = start_y;
}

static uint8_t rm_ui_send_interactive(uint16_t data_cmd_id, const void *data, uint16_t data_len)
{
    uint8_t frame[REF_PROTOCOL_FRAME_MAX_SIZE];
    frame_header_struct_t *frame_header;
    rm_ui_interactive_header_t interactive_header;
    uint16_t total_len;
    uint16_t sender_id;
    uint16_t receiver_id;
    uint16_t cmd_id;
    uint16_t index;

    if (data == NULL || !rm_ui_ready_to_send(&sender_id, &receiver_id))
    {
        return 0u;
    }

    total_len = (uint16_t)(REF_PROTOCOL_HEADER_SIZE + REF_PROTOCOL_CMD_SIZE
                           + sizeof(rm_ui_interactive_header_t) + data_len
                           + REF_PROTOCOL_CRC16_SIZE);
    if (total_len > REF_PROTOCOL_FRAME_MAX_SIZE)
    {
        return 0u;
    }

    frame_header = (frame_header_struct_t *)frame;
    frame_header->SOF = HEADER_SOF;
    frame_header->data_length = (uint16_t)(REF_PROTOCOL_CMD_SIZE + sizeof(rm_ui_interactive_header_t) + data_len);
    frame_header->seq = rm_ui_seq++;
    append_CRC8_check_sum(frame, REF_PROTOCOL_HEADER_SIZE);

    index = REF_PROTOCOL_HEADER_SIZE;
    cmd_id = STUDENT_INTERACTIVE_DATA_CMD_ID;
    memcpy(frame + index, &cmd_id, sizeof(cmd_id));
    index += sizeof(cmd_id);

    interactive_header.data_cmd_id = data_cmd_id;
    interactive_header.sender_id = sender_id;
    interactive_header.receiver_id = receiver_id;
    memcpy(frame + index, &interactive_header, sizeof(interactive_header));
    index += sizeof(interactive_header);

    memcpy(frame + index, data, data_len);
    append_CRC16_check_sum(frame, total_len);

    if (HAL_UART_Transmit(&huart6, frame, total_len, RM_UI_TX_TIMEOUT_MS) != HAL_OK)
    {
        return 0u;
    }

    return 1u;
}

static uint8_t rm_ui_send_delete_all(void)
{
    rm_ui_delete_t delete_cmd;

    delete_cmd.delete_type = 2u;
    delete_cmd.layer = 0u;
    return rm_ui_send_interactive(RM_UI_DELETE_LAYER_CMD_ID, &delete_cmd, sizeof(delete_cmd));
}

static uint8_t rm_ui_send_reticle(void)
{
    rm_ui_graphic_5_t graphics;

    memset(&graphics, 0, sizeof(graphics));
    rm_ui_set_line(&graphics.graphic[0], 'R', '0', '0', RM_UI_OP_ADD, RM_UI_LAYER_RETICLE, RM_UI_COLOR_WHITE, 2u,
                   RM_UI_SCREEN_CENTER_X, RM_UI_RETICLE_MAIN_BOTTOM_Y, RM_UI_SCREEN_CENTER_X, RM_UI_RETICLE_MAIN_TOP_Y);
    rm_ui_set_line(&graphics.graphic[1], 'R', '0', '1', RM_UI_OP_ADD, RM_UI_LAYER_RETICLE, RM_UI_COLOR_WHITE, 2u,
                   RM_UI_RETICLE_MAIN_LEFT_X, RM_UI_RETICLE_MID_Y, RM_UI_SCREEN_CENTER_X - 60u, RM_UI_RETICLE_MID_Y);
    rm_ui_set_line(&graphics.graphic[2], 'R', '0', '2', RM_UI_OP_ADD, RM_UI_LAYER_RETICLE, RM_UI_COLOR_WHITE, 2u,
                   RM_UI_SCREEN_CENTER_X + 60u, RM_UI_RETICLE_MID_Y, RM_UI_RETICLE_MAIN_RIGHT_X, RM_UI_RETICLE_MID_Y);
    rm_ui_set_line(&graphics.graphic[3], 'R', '0', '3', RM_UI_OP_ADD, RM_UI_LAYER_RETICLE, RM_UI_COLOR_WHITE, 2u,
                   RM_UI_RETICLE_SUB_LEFT_X, RM_UI_RETICLE_SUB_Y, RM_UI_SCREEN_CENTER_X - 50u, RM_UI_RETICLE_SUB_Y);
    rm_ui_set_line(&graphics.graphic[4], 'R', '0', '4', RM_UI_OP_ADD, RM_UI_LAYER_RETICLE, RM_UI_COLOR_WHITE, 2u,
                   RM_UI_SCREEN_CENTER_X + 50u, RM_UI_RETICLE_SUB_Y, RM_UI_RETICLE_SUB_RIGHT_X, RM_UI_RETICLE_SUB_Y);

    return rm_ui_send_interactive(RM_UI_DRAW_FIVE_CMD_ID, &graphics, sizeof(graphics));
}

static uint8_t rm_ui_send_mode_text(rm_ui_chassis_mode_e mode, uint8_t operate_type)
{
    static const char follow_text[] = "FOLLOW";
    static const char spin_text[] = "SPIN";
    const char *text;
    uint32_t text_len;
    rm_ui_string_t string_data;

    text = (mode == RM_UI_CHASSIS_SPIN) ? spin_text : follow_text;
    text_len = (uint32_t)strlen(text);

    memset(&string_data, 0, sizeof(string_data));
    rm_ui_set_text_graphic(&string_data.graphic, 'M', '0', '0', operate_type, RM_UI_LAYER_STATUS,
                           RM_UI_COLOR_CYAN, RM_UI_TEXT_FONT_SIZE, RM_UI_TEXT_WIDTH,
                           RM_UI_MODE_TEXT_X, RM_UI_TEXT_Y, text_len);
    memcpy(string_data.text, text, text_len);

    return rm_ui_send_interactive(RM_UI_DRAW_CHAR_CMD_ID, &string_data, sizeof(string_data));
}

static uint8_t rm_ui_send_gear_text(shoot_ui_gear_e gear, uint8_t operate_type)
{
    static const char low_text[] = "LOW";
    static const char high_text[] = "HIGH";
    const char *text;
    uint32_t text_len;
    rm_ui_string_t string_data;

    text = (gear == SHOOT_UI_GEAR_HIGH) ? high_text : low_text;
    text_len = (uint32_t)strlen(text);

    memset(&string_data, 0, sizeof(string_data));
    rm_ui_set_text_graphic(&string_data.graphic, 'G', '0', '0', operate_type, RM_UI_LAYER_STATUS,
                           RM_UI_COLOR_CYAN, RM_UI_TEXT_FONT_SIZE, RM_UI_TEXT_WIDTH,
                           RM_UI_GEAR_TEXT_X, RM_UI_TEXT_Y, text_len);
    memcpy(string_data.text, text, text_len);

    return rm_ui_send_interactive(RM_UI_DRAW_CHAR_CMD_ID, &string_data, sizeof(string_data));
}

static uint8_t rm_ui_send_reverse_light(uint8_t operate_type)
{
    rm_ui_graphic_t graphic;

    rm_ui_set_circle(&graphic, 'V', '0', '0', operate_type, RM_UI_LAYER_EVENT, RM_UI_COLOR_GREEN,
                     RM_UI_REVERSE_LIGHT_WIDTH, RM_UI_REVERSE_LIGHT_X, RM_UI_REVERSE_LIGHT_Y,
                     RM_UI_REVERSE_LIGHT_RADIUS);
    return rm_ui_send_interactive(RM_UI_DRAW_ONE_CMD_ID, &graphic, sizeof(graphic));
}

static uint8_t rm_ui_reverse_light_should_show(uint32_t now_tick)
{
    if ((int32_t)(rm_ui_state.reverse_blink_deadline - now_tick) <= 0)
    {
        return 0u;
    }

    return (uint8_t)(((now_tick / RM_UI_REVERSE_BLINK_PERIOD_MS) & 0x1u) == 0u);
}
