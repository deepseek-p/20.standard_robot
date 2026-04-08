#include <assert.h>
#include <stdint.h>

#include "shoot_logic.h"

static void test_idle_hold_reattaches_target_to_feedback(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    cmd.arm_enable = 1;
    cmd.fric_ready = 1;
    exec.trigger_ecd_fdb = 123456;
    exec.trigger_ecd_set = 654321;

    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);

    assert(cmd.public_mode == SHOOT_LOGIC_MODE_READY_BULLET);
    assert(exec.trigger_ecd_set == exec.trigger_ecd_fdb);
}

static void test_single_fire_steps_exactly_one_grid_once(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    cmd.arm_enable = 1;
    cmd.fric_ready = 1;
    exec.trigger_ecd_fdb = 200000;

    shoot_command_logic_update(&cmd, 0, 1, 1, 1, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.trigger_ecd_set == 200000 + 36864);

    shoot_command_logic_update(&cmd, 0, 1, 0, 1, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.trigger_ecd_set == 200000 + 36864);
}

static void test_microswitch_does_not_drive_ready_state_split(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    cmd.arm_enable = 1;
    cmd.fric_ready = 1;
    exec.trigger_ecd_fdb = 300000;

    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_READY_BULLET);

    shoot_command_logic_update(&cmd, 1, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_READY_BULLET);
}

static void test_idle_hold_does_not_chase_feedback_every_cycle(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    cmd.arm_enable = 1;
    cmd.fric_ready = 1;
    exec.trigger_ecd_fdb = 400000;

    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.trigger_ecd_set == 400000);

    exec.trigger_ecd_fdb = 401000;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.trigger_ecd_set == 400000);
}

static void test_disarmed_or_wait_fric_keeps_target_attached_to_latest_feedback(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);

    exec.trigger_ecd_fdb = 0;
    shoot_command_logic_update(&cmd, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.trigger_ecd_set == 0);

    exec.trigger_ecd_fdb = 123456;
    shoot_command_logic_update(&cmd, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.trigger_ecd_set == 123456);

    exec.trigger_ecd_fdb = 234567;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 0);
    shoot_executor_logic_update(&cmd, &exec);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_READY_FRIC);
    assert(exec.trigger_ecd_set == 234567);
}

static void test_single_fire_waits_for_release_and_tighter_settle_before_done(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};
    int32_t target;

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    exec.trigger_ecd_fdb = 700000;

    shoot_command_logic_update(&cmd, 0, 1, 1, 1, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    target = exec.trigger_ecd_set;
    assert(exec.exec_state == TRIGGER_EXEC_STEP_ONE_GRID);
    assert(target == 700000 + 36864);

    exec.trigger_ecd_fdb = target - 4000;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_STEP_ONE_GRID);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_BULLET);
    assert(exec.bullet_count == 1);
    assert(exec.trigger_ecd_set == target);

    exec.trigger_ecd_fdb = target - 800;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_IDLE_HOLD);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_DONE);
    assert(exec.bullet_count == 1);
    assert(exec.trigger_ecd_set == exec.trigger_ecd_fdb);
}

static void test_single_fire_does_not_done_while_trigger_still_held(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};
    int32_t target;

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    exec.trigger_ecd_fdb = 810000;

    shoot_command_logic_update(&cmd, 0, 1, 1, 1, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    target = exec.trigger_ecd_set;

    exec.trigger_ecd_fdb = target - 500;
    shoot_command_logic_update(&cmd, 0, 1, 0, 1, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_STEP_ONE_GRID);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_BULLET);
    assert(exec.bullet_count == 1);
    assert(exec.trigger_ecd_set == target);
}

static void test_manual_reverse_runs_to_completion_without_counting(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    exec.trigger_ecd_fdb = 500000;

    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 1, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_REVERSE_RECOVER);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_BULLET);
    assert(exec.trigger_ecd_set == 500000 - 36864);
    assert(exec.bullet_count == 0);

    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_REVERSE_RECOVER);
    assert(exec.trigger_ecd_set == 500000 - 36864);
    assert(exec.bullet_count == 0);

    exec.trigger_ecd_fdb = exec.trigger_ecd_set + 1000;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_IDLE_HOLD);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_READY_BULLET);
    assert(exec.trigger_ecd_set == exec.trigger_ecd_fdb);
    assert(exec.bullet_count == 0);
}

static void test_held_reverse_request_does_not_repeat_one_grid(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};
    int32_t reverse_target;

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    exec.trigger_ecd_fdb = 620000;

    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 1, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    reverse_target = exec.trigger_ecd_set;

    exec.trigger_ecd_fdb = reverse_target + 1000;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 1, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_IDLE_HOLD);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_READY_BULLET);
    assert(exec.trigger_ecd_set == exec.trigger_ecd_fdb);

    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 1, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_IDLE_HOLD);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_READY_BULLET);
    assert(exec.trigger_ecd_set == exec.trigger_ecd_fdb);
}

static void test_jam_abandon_returns_idle_hold_and_attaches_target(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    exec.trigger_ecd_fdb = 900000;

    shoot_command_logic_update(&cmd, 0, 1, 1, 1, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_STEP_ONE_GRID);

    exec.trigger_ecd_fdb = 901234;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 0, 0, 0, 1, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_IDLE_HOLD);
    assert(cmd.public_mode == SHOOT_LOGIC_MODE_READY_BULLET);
    assert(exec.trigger_ecd_set == exec.trigger_ecd_fdb);
}

static void test_continuous_run_counts_bullets_by_one_grid_progress(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    exec.trigger_ecd_fdb = 1000000;

    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 1, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_CONTINUOUS_RUN);
    assert(exec.bullet_count == 0);

    exec.trigger_ecd_fdb = 1000000 + 36864 + 1200;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 1, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.bullet_count == 1);

    exec.trigger_ecd_fdb = 1000000 + 2 * 36864 + 1600;
    shoot_command_logic_update(&cmd, 0, 1, 0, 0, 1, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.bullet_count == 2);
}

static void test_single_fire_req_is_cleared_after_consumption(void)
{
    shoot_command_state_t cmd = {0};
    shoot_executor_state_t exec = {0};

    shoot_logic_init(&cmd, &exec, 36864, 5000);
    exec.trigger_ecd_fdb = 1100000;

    shoot_command_logic_update(&cmd, 0, 1, 1, 1, 0, 0, 0, 0, 1);
    shoot_executor_logic_update(&cmd, &exec);
    assert(exec.exec_state == TRIGGER_EXEC_STEP_ONE_GRID);
    assert(cmd.single_fire_req == 0);
}

int main(void)
{
    test_idle_hold_reattaches_target_to_feedback();
    test_single_fire_steps_exactly_one_grid_once();
    test_microswitch_does_not_drive_ready_state_split();
    test_idle_hold_does_not_chase_feedback_every_cycle();
    test_disarmed_or_wait_fric_keeps_target_attached_to_latest_feedback();
    test_single_fire_waits_for_release_and_tighter_settle_before_done();
    test_single_fire_does_not_done_while_trigger_still_held();
    test_manual_reverse_runs_to_completion_without_counting();
    test_held_reverse_request_does_not_repeat_one_grid();
    test_jam_abandon_returns_idle_hold_and_attaches_target();
    test_continuous_run_counts_bullets_by_one_grid_progress();
    test_single_fire_req_is_cleared_after_consumption();
    return 0;
}
