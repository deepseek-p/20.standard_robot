#!/usr/bin/env python3
"""解析 STM32CubeMX .ioc 文件，生成 docs/hardware_map.md"""

import re
import sys
from pathlib import Path
from collections import defaultdict

def parse_ioc(path):
    """解析 .ioc 为 {key: value} 字典"""
    data = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if '=' in line and not line.startswith('#'):
            k, v = line.split('=', 1)
            data[k] = v
    return data

def extract_pins(data):
    """提取引脚映射: {pin: {signal, label, mode, ...}}"""
    pins = defaultdict(dict)
    for k, v in data.items():
        m = re.match(r'^(P[A-I]\d+(?:-[A-Z_]+)?)\.(.*)', k)
        if not m:
            continue
        pin, attr = m.group(1), m.group(2)
        pins[pin][attr] = v
    return dict(pins)

def extract_peripherals(data):
    """提取外设配置: {periph: {param: value}}"""
    periphs = defaultdict(dict)
    skip = {'Mcu', 'File', 'PCC', 'ProjectManager', 'VP_', 'SH.', 'NVIC', 'Dma', 'RCC'}
    for k, v in data.items():
        if any(k.startswith(s) for s in skip):
            continue
        m = re.match(r'^([A-Z][A-Z0-9_]+)\.(.*)', k)
        if m:
            periph, param = m.group(1), m.group(2)
            if not re.match(r'^P[A-I]\d', periph):
                periphs[periph][param] = v
    return dict(periphs)

def extract_nvic(data):
    """提取中断优先级"""
    irqs = {}
    for k, v in data.items():
        m = re.match(r'^NVIC\.(\w+_IRQn)$', k)
        if m:
            fields = v.replace('\\:', ':').split(':')
            if len(fields) >= 2:
                irqs[m.group(1)] = {'preempt': fields[1], 'enabled': fields[0]}
    return irqs

def extract_dma(data):
    """提取 DMA 通道分配"""
    channels = {}
    nb = int(data.get('Dma.RequestsNb', '0'))
    for i in range(nb):
        req = data.get(f'Dma.Request{i}', '')
        if req:
            inst = data.get(f'Dma.{req}.{i}.Instance', '?')
            prio = data.get(f'Dma.{req}.{i}.Priority', '?')
            direction = data.get(f'Dma.{req}.{i}.Direction', '?')
            channels[req] = {'instance': inst, 'priority': prio, 'direction': direction}
    return channels

def extract_clocks(data):
    """提取关键时钟频率"""
    keys = ['SYSCLKFreq_VALUE', 'AHBFreq_Value', 'APB1Freq_Value',
            'APB2Freq_Value', 'APB1TimFreq_Value', 'APB2TimFreq_Value',
            'HSE_VALUE', '48MHZClocksFreq_Value']
    clocks = {}
    for k in keys:
        v = data.get(f'RCC.{k}')
        if v:
            clocks[k] = int(v)
    return clocks


# ── Markdown 生成 ──

def fmt_freq(hz):
    if hz >= 1_000_000:
        return f"{hz // 1_000_000} MHz"
    if hz >= 1_000:
        return f"{hz // 1_000} KHz"
    return f"{hz} Hz"

def pin_signal_name(attrs):
    """从引脚属性中提取可读信号名"""
    sig = attrs.get('Signal', '')
    label = attrs.get('GPIO_Label', '')
    mode = attrs.get('Mode', '')
    if label:
        return label
    if sig:
        return sig
    return mode

def build_peripheral_pin_map(pins):
    """构建 外设→引脚列表 的反向映射"""
    periph_pins = defaultdict(list)
    for pin, attrs in sorted(pins.items()):
        sig = attrs.get('Signal', '')
        # 从信号名提取外设: SPI1_MOSI → SPI1, CAN1_RX → CAN1, USART3_TX → USART3
        m = re.match(r'^((?:SPI|CAN|USART|I2C|USB_OTG_FS|TIM)\d*)', sig)
        if m:
            periph_pins[m.group(1)].append((pin, sig, attrs))
        elif 'GPIO_Label' in attrs:
            periph_pins['GPIO'].append((pin, sig, attrs))
    return dict(periph_pins)


def generate_md(data):
    pins = extract_pins(data)
    periphs = extract_peripherals(data)
    nvic = extract_nvic(data)
    dma = extract_dma(data)
    clocks = extract_clocks(data)
    pp_map = build_peripheral_pin_map(pins)

    lines = []
    w = lines.append

    mcu = data.get('Mcu.UserName', '?')
    pkg = data.get('Mcu.Package', '?')
    w(f"# Hardware Map — {mcu} ({pkg})")
    w(f"\n> 由 `tools/parse_ioc.py` 从 `standard_robot.ioc` 自动生成，勿手动编辑。")
    w(f"> 重新生成: `python tools/parse_ioc.py`\n")

    # ── 时钟树 ──
    w("## 时钟树\n")
    w("| 时钟域 | 频率 |")
    w("|--------|------|")
    clock_labels = {
        'HSE_VALUE': 'HSE (外部晶振)',
        'SYSCLKFreq_VALUE': 'SYSCLK',
        'AHBFreq_Value': 'AHB (HCLK)',
        'APB1Freq_Value': 'APB1',
        'APB1TimFreq_Value': 'APB1 Timer',
        'APB2Freq_Value': 'APB2',
        'APB2TimFreq_Value': 'APB2 Timer',
        '48MHZClocksFreq_Value': 'USB 48MHz',
    }
    for k, label in clock_labels.items():
        if k in clocks:
            w(f"| {label} | {fmt_freq(clocks[k])} |")
    w("")

    # ── 通信外设 ──
    w("## 通信外设\n")

    # 项目语义注释
    semantic = {
        'CAN1': '底盘 3508×4 (0x201-0x204) + yaw 6020 (0x205) + pitch 6020 (0x206) + trigger 2006 (0x207)',
        'CAN2': '摩擦轮 3508×2 (0x201/0x202)',
        'USART1': 'ESP32 WiFi 桥接 (115200 默认)',
        'USART3': 'SBUS 遥控器 (100000, EVEN parity)',
        'USART6': '裁判系统',
        'USB_OTG_FS': 'USB CDC — FireWater 遥测 + 在线 PID 调参',
        'SPI1': 'BMI088 IMU (CS_ACCEL=PA4, CS_GYRO=PB0)',
        'SPI2': '备用 SPI (CS=PB12)',
        'I2C1': '(PB8/PB9)',
        'I2C2': '(PF1/PF0)',
        'I2C3': 'IST8310 磁力计 (PA8/PC9)',
    }

    comm_order = ['CAN1', 'CAN2', 'USART1', 'USART3', 'USART6',
                  'USB_OTG_FS', 'SPI1', 'SPI2', 'I2C1', 'I2C2', 'I2C3']

    for name in comm_order:
        cfg = periphs.get(name, {})
        if not cfg and name not in pp_map:
            continue
        sem = semantic.get(name, '')
        w(f"### {name}" + (f" — {sem}" if sem else ""))

        # 引脚
        if name in pp_map:
            for pin, sig, _ in pp_map[name]:
                func = sig.split('_', 1)[-1] if '_' in sig else sig
                w(f"- {pin} → {func}")

        # 关键配置参数
        skip_params = {'IPParameters', 'VirtualType', 'VirtualMode', 'VirtualModeFS'}
        for param, val in sorted(cfg.items()):
            if param in skip_params or param.startswith('IPParam'):
                continue
            w(f"- {param}: `{val}`")

        # CAN 波特率计算
        if name.startswith('CAN') and 'Prescaler' in cfg:
            psc = int(cfg['Prescaler'])
            bs1_str = cfg.get('BS1', '')
            bs2_str = cfg.get('BS2', '')
            bs1 = int(re.search(r'(\d+)TQ', bs1_str).group(1)) if 'TQ' in bs1_str else 0
            bs2 = int(re.search(r'(\d+)TQ', bs2_str).group(1)) if 'TQ' in bs2_str else 0
            apb1 = clocks.get('APB1Freq_Value', 42_000_000)
            if bs1 + bs2 > 0:
                baud = apb1 // (psc * (1 + bs1 + bs2))
                w(f"- **波特率**: {baud // 1000} Kbps (APB1={fmt_freq(apb1)}, PSC={psc}, 1+{bs1}+{bs2}={1+bs1+bs2} TQ)")
        w("")

    # ── DMA ──
    w("## DMA 通道分配\n")
    w("| 请求 | DMA 实例 | 方向 | 优先级 |")
    w("|------|----------|------|--------|")
    dir_map = {'DMA_PERIPH_TO_MEMORY': 'P→M (RX)', 'DMA_MEMORY_TO_PERIPH': 'M→P (TX)'}
    prio_map = {'DMA_PRIORITY_VERY_HIGH': 'Very High', 'DMA_PRIORITY_HIGH': 'High',
                'DMA_PRIORITY_MEDIUM': 'Medium', 'DMA_PRIORITY_LOW': 'Low'}
    for req, ch in dma.items():
        d = dir_map.get(ch['direction'], ch['direction'])
        p = prio_map.get(ch['priority'], ch['priority'])
        w(f"| {req} | {ch['instance']} | {d} | {p} |")
    w("")

    # ── NVIC ──
    w("## 中断优先级 (NVIC_PRIORITYGROUP_4)\n")
    w("| 中断 | 抢占优先级 |")
    w("|------|-----------|")
    sorted_irqs = sorted(nvic.items(), key=lambda x: (int(x[1]['preempt']), x[0]))
    for name, info in sorted_irqs:
        if info['enabled'] == 'true' and int(info['preempt']) > 0:
            w(f"| {name} | {info['preempt']} |")
    w("")

    # ── 定时器 ──
    w("## 定时器\n")
    tim_semantic = {
        'TIM1': 'PWM CH1-CH4 (PE9/PE11/PE13/PE14), PSC=167 → 1MHz, Period=19999 → 50Hz 舵机',
        'TIM3': 'PWM CH3 (PC8), Period=8399',
        'TIM4': 'PWM CH3 (PD14=BUZZER), PSC=167, Period=65535',
        'TIM5': 'PWM CH1-CH3 (PH10=LED_B, PH11=LED_G, PH12=LED_R), PSC=0, Period=65535',
        'TIM8': 'PWM CH1-CH2 (PC6/PI6), PSC=167 → 1MHz, Period=19999 → 50Hz',
        'TIM10': 'PWM CH1 (PF6), Period=4999',
    }
    for tim in ['TIM1', 'TIM3', 'TIM4', 'TIM5', 'TIM8', 'TIM10']:
        if tim in tim_semantic:
            w(f"- **{tim}**: {tim_semantic[tim]}")
    w("")

    # ── GPIO ──
    w("## GPIO（带标签）\n")
    w("| 引脚 | 标签 | 方向 | 备注 |")
    w("|------|------|------|------|")
    gpio_notes = {
        'CS1_ACCEL': 'BMI088 加速度计片选',
        'CS1_GYRO': 'BMI088 陀螺仪片选',
        'SPI2_CS': 'SPI2 片选（备用）',
        'INT1_ACCEL': 'BMI088 加速度计中断 (EXTI4, 下降沿)',
        'INT1_GYRO': 'BMI088 陀螺仪中断 (EXTI5, 下降沿)',
        'DRDY_IST8310': 'IST8310 数据就绪 (EXTI3, 下降沿)',
        'RSTN_IST8310': 'IST8310 复位（高有效）',
        'KEY': '用户按键（上拉）',
        'BUTTON_TRIG': '触发按钮（上拉）',
        'BUZZER': '蜂鸣器 (TIM4_CH3 PWM)',
        'LED_R': '红色 LED (TIM5_CH3)',
        'LED_G': '绿色 LED (TIM5_CH2)',
        'LED_B': '蓝色 LED (TIM5_CH1)',
        'HW0': '硬件版本位 0',
        'HW1': '硬件版本位 1',
        'HW2': '硬件版本位 2',
        'ADC_BAT': '电池电压 ADC (ADC3_IN8)',
    }
    if 'GPIO' in pp_map:
        for pin, sig, attrs in pp_map['GPIO']:
            label = attrs.get('GPIO_Label', '')
            if 'EXTI' in sig or 'GPXTI' in sig:
                direction = 'EXTI'
            elif 'Output' in sig:
                direction = 'OUT'
            elif 'S_TIM' in sig or 'ADC' in sig:
                direction = 'PWM/AF'
            else:
                direction = 'IN'
            note = gpio_notes.get(label, '')
            w(f"| {pin} | {label} | {direction} | {note} |")
    w("")

    # ── CAN ID 映射（代码语义） ──
    w("## CAN ID 映射（来自 CAN_receive.h）\n")
    w("| CAN 总线 | 发送 ID | 电机 ID | 电机类型 | 用途 |")
    w("|----------|---------|---------|----------|------|")
    w("| hcan1 (CAN1) | 0x200 | 0x201-0x204 | M3508 | 底盘轮组 M1-M4 |")
    w("| hcan1 (CAN1) | 0x1FF | 0x205 | GM6020 | Yaw 云台 |")
    w("| hcan1 (CAN1) | 0x1FF | 0x206 | GM6020 | Pitch 云台 |")
    w("| hcan1 (CAN1) | 0x1FF | 0x207 | M2006 | 拨弹电机 |")
    w("| hcan2 (CAN2) | 0x200 | 0x201 | M3508 | 摩擦轮 1 |")
    w("| hcan2 (CAN2) | 0x200 | 0x202 | M3508 | 摩擦轮 2 |")
    w("")

    w("## FreeRTOS\n")
    heap = data.get('FREERTOS.configTOTAL_HEAP_SIZE', '?')
    w(f"- 总堆大小: {heap} bytes")
    w(f"- API: CMSIS-RTOS v1")
    w(f"- Tick: 1ms (configTICK_RATE_HZ=1000)")
    w("")

    return '\n'.join(lines)


def main():
    proj = Path(__file__).resolve().parent.parent
    ioc = proj / 'standard_robot.ioc'
    if not ioc.exists():
        print(f"找不到 {ioc}", file=sys.stderr)
        sys.exit(1)

    data = parse_ioc(ioc)
    md = generate_md(data)

    out = proj / 'docs' / 'hardware_map.md'
    out.write_text(md, encoding='utf-8')
    print(f"已生成: {out}")


if __name__ == '__main__':
    main()