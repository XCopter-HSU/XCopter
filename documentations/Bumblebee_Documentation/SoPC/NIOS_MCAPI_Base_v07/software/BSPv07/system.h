/*
 * system.h - SOPC Builder system and BSP software package information
 *
 * Machine generated for CPU 'cpu_s0' in SOPC Builder design 'soc_system'
 * SOPC Builder design path: ../../soc_system.sopcinfo
 *
 * Generated: Wed Dec 17 13:11:09 CET 2014
 */

/*
 * DO NOT MODIFY THIS FILE
 *
 * Changing this file will have subtle consequences
 * which will almost certainly lead to a nonfunctioning
 * system. If you do modify this file, be aware that your
 * changes will be overwritten and lost when this file
 * is generated again.
 *
 * DO NOT MODIFY THIS FILE
 */

/*
 * License Agreement
 *
 * Copyright (c) 2008
 * Altera Corporation, San Jose, California, USA.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This agreement shall be governed in all respects by the laws of the State
 * of California and by the laws of the United States of America.
 */

#ifndef __SYSTEM_H_
#define __SYSTEM_H_

/* Include definitions from linker script generator */
#include "linker.h"


/*
 * CPU configuration
 *
 */

#define ALT_CPU_ARCHITECTURE "altera_nios2_qsys"
#define ALT_CPU_BIG_ENDIAN 0
#define ALT_CPU_BREAK_ADDR 0x0900f020
#define ALT_CPU_CPU_FREQ 125000000u
#define ALT_CPU_CPU_ID_SIZE 1
#define ALT_CPU_CPU_ID_VALUE 0x00000000
#define ALT_CPU_CPU_IMPLEMENTATION "fast"
#define ALT_CPU_DATA_ADDR_WIDTH 0x1c
#define ALT_CPU_DCACHE_LINE_SIZE 0
#define ALT_CPU_DCACHE_LINE_SIZE_LOG2 0
#define ALT_CPU_DCACHE_SIZE 0
#define ALT_CPU_EXCEPTION_ADDR 0x00000020
#define ALT_CPU_FLUSHDA_SUPPORTED
#define ALT_CPU_FREQ 125000000
#define ALT_CPU_HARDWARE_DIVIDE_PRESENT 0
#define ALT_CPU_HARDWARE_MULTIPLY_PRESENT 1
#define ALT_CPU_HARDWARE_MULX_PRESENT 0
#define ALT_CPU_HAS_DEBUG_CORE 1
#define ALT_CPU_HAS_DEBUG_STUB
#define ALT_CPU_HAS_JMPI_INSTRUCTION
#define ALT_CPU_ICACHE_LINE_SIZE 32
#define ALT_CPU_ICACHE_LINE_SIZE_LOG2 5
#define ALT_CPU_ICACHE_SIZE 4096
#define ALT_CPU_INST_ADDR_WIDTH 0x1c
#define ALT_CPU_NAME "cpu_s0"
#define ALT_CPU_NUM_OF_SHADOW_REG_SETS 0
#define ALT_CPU_RESET_ADDR 0x00000000


/*
 * CPU configuration (with legacy prefix - don't use these anymore)
 *
 */

#define NIOS2_BIG_ENDIAN 0
#define NIOS2_BREAK_ADDR 0x0900f020
#define NIOS2_CPU_FREQ 125000000u
#define NIOS2_CPU_ID_SIZE 1
#define NIOS2_CPU_ID_VALUE 0x00000000
#define NIOS2_CPU_IMPLEMENTATION "fast"
#define NIOS2_DATA_ADDR_WIDTH 0x1c
#define NIOS2_DCACHE_LINE_SIZE 0
#define NIOS2_DCACHE_LINE_SIZE_LOG2 0
#define NIOS2_DCACHE_SIZE 0
#define NIOS2_EXCEPTION_ADDR 0x00000020
#define NIOS2_FLUSHDA_SUPPORTED
#define NIOS2_HARDWARE_DIVIDE_PRESENT 0
#define NIOS2_HARDWARE_MULTIPLY_PRESENT 1
#define NIOS2_HARDWARE_MULX_PRESENT 0
#define NIOS2_HAS_DEBUG_CORE 1
#define NIOS2_HAS_DEBUG_STUB
#define NIOS2_HAS_JMPI_INSTRUCTION
#define NIOS2_ICACHE_LINE_SIZE 32
#define NIOS2_ICACHE_LINE_SIZE_LOG2 5
#define NIOS2_ICACHE_SIZE 4096
#define NIOS2_INST_ADDR_WIDTH 0x1c
#define NIOS2_NUM_OF_SHADOW_REG_SETS 0
#define NIOS2_RESET_ADDR 0x00000000


/*
 * Define for each module class mastered by the CPU
 *
 */

#define __ALTERA_AVALON_JTAG_UART
#define __ALTERA_AVALON_NEW_SDRAM_CONTROLLER
#define __ALTERA_AVALON_PIO
#define __ALTERA_AVALON_TIMER
#define __ALTERA_NIOS2_QSYS
#define __CPU_FIFO_BRIDGE
#define __OC_I2C_MASTER
#define __PWM


/*
 * System configuration
 *
 */

#define ALT_DEVICE_FAMILY "Cyclone V"
#define ALT_IRQ_BASE NULL
#define ALT_LEGACY_INTERRUPT_API_PRESENT
#define ALT_LOG_PORT "/dev/null"
#define ALT_LOG_PORT_BASE 0x0
#define ALT_LOG_PORT_DEV null
#define ALT_LOG_PORT_TYPE ""
#define ALT_NUM_EXTERNAL_INTERRUPT_CONTROLLERS 0
#define ALT_NUM_INTERNAL_INTERRUPT_CONTROLLERS 1
#define ALT_NUM_INTERRUPT_CONTROLLERS 1
#define ALT_STDERR "/dev/jtag_uart_cpu_s0"
#define ALT_STDERR_BASE 0x9000020
#define ALT_STDERR_DEV jtag_uart_cpu_s0
#define ALT_STDERR_IS_JTAG_UART
#define ALT_STDERR_PRESENT
#define ALT_STDERR_TYPE "altera_avalon_jtag_uart"
#define ALT_STDIN "/dev/jtag_uart_cpu_s0"
#define ALT_STDIN_BASE 0x9000020
#define ALT_STDIN_DEV jtag_uart_cpu_s0
#define ALT_STDIN_IS_JTAG_UART
#define ALT_STDIN_PRESENT
#define ALT_STDIN_TYPE "altera_avalon_jtag_uart"
#define ALT_STDOUT "/dev/jtag_uart_cpu_s0"
#define ALT_STDOUT_BASE 0x9000020
#define ALT_STDOUT_DEV jtag_uart_cpu_s0
#define ALT_STDOUT_IS_JTAG_UART
#define ALT_STDOUT_PRESENT
#define ALT_STDOUT_TYPE "altera_avalon_jtag_uart"
#define ALT_SYSTEM_NAME "soc_system"


/*
 * fifo_bridge_cpuM_cpus0 configuration
 *
 */

#define ALT_MODULE_CLASS_fifo_bridge_cpuM_cpus0 CPU_FiFo_Bridge
#define FIFO_BRIDGE_CPUM_CPUS0_BASE 0x9004000
#define FIFO_BRIDGE_CPUM_CPUS0_IRQ 1
#define FIFO_BRIDGE_CPUM_CPUS0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define FIFO_BRIDGE_CPUM_CPUS0_NAME "/dev/fifo_bridge_cpuM_cpus0"
#define FIFO_BRIDGE_CPUM_CPUS0_SPAN 1024
#define FIFO_BRIDGE_CPUM_CPUS0_TYPE "CPU_FiFo_Bridge"


/*
 * fifo_bridge_cpus0_cpus1 configuration
 *
 */

#define ALT_MODULE_CLASS_fifo_bridge_cpus0_cpus1 CPU_FiFo_Bridge
#define FIFO_BRIDGE_CPUS0_CPUS1_BASE 0x9004400
#define FIFO_BRIDGE_CPUS0_CPUS1_IRQ 3
#define FIFO_BRIDGE_CPUS0_CPUS1_IRQ_INTERRUPT_CONTROLLER_ID 0
#define FIFO_BRIDGE_CPUS0_CPUS1_NAME "/dev/fifo_bridge_cpus0_cpus1"
#define FIFO_BRIDGE_CPUS0_CPUS1_SPAN 1024
#define FIFO_BRIDGE_CPUS0_CPUS1_TYPE "CPU_FiFo_Bridge"


/*
 * hal configuration
 *
 */

#define ALT_MAX_FD 32
#define ALT_SYS_CLK TIMER_CPU_S0
#define ALT_TIMESTAMP_CLK none


/*
 * i2c_cpu_s0 configuration
 *
 */

#define ALT_MODULE_CLASS_i2c_cpu_s0 oc_i2c_master
#define I2C_CPU_S0_BASE 0x9000040
#define I2C_CPU_S0_IRQ 4
#define I2C_CPU_S0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define I2C_CPU_S0_NAME "/dev/i2c_cpu_s0"
#define I2C_CPU_S0_SPAN 64
#define I2C_CPU_S0_TYPE "oc_i2c_master"
#define I2C_CPU_S0_FREQ 25000000

/*
 * jtag_uart_cpu_s0 configuration
 *
 */

#define ALT_MODULE_CLASS_jtag_uart_cpu_s0 altera_avalon_jtag_uart
#define JTAG_UART_CPU_S0_BASE 0x9000020
#define JTAG_UART_CPU_S0_IRQ 2
#define JTAG_UART_CPU_S0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define JTAG_UART_CPU_S0_NAME "/dev/jtag_uart_cpu_s0"
#define JTAG_UART_CPU_S0_READ_DEPTH 64
#define JTAG_UART_CPU_S0_READ_THRESHOLD 8
#define JTAG_UART_CPU_S0_SPAN 16
#define JTAG_UART_CPU_S0_TYPE "altera_avalon_jtag_uart"
#define JTAG_UART_CPU_S0_WRITE_DEPTH 64
#define JTAG_UART_CPU_S0_WRITE_THRESHOLD 8


/*
 * pio_aliveTest_cpu_s0 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_aliveTest_cpu_s0 altera_avalon_pio
#define PIO_ALIVETEST_CPU_S0_BASE 0x9000030
#define PIO_ALIVETEST_CPU_S0_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_ALIVETEST_CPU_S0_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_ALIVETEST_CPU_S0_CAPTURE 0
#define PIO_ALIVETEST_CPU_S0_DATA_WIDTH 2
#define PIO_ALIVETEST_CPU_S0_DO_TEST_BENCH_WIRING 0
#define PIO_ALIVETEST_CPU_S0_DRIVEN_SIM_VALUE 0
#define PIO_ALIVETEST_CPU_S0_EDGE_TYPE "NONE"
#define PIO_ALIVETEST_CPU_S0_FREQ 25000000
#define PIO_ALIVETEST_CPU_S0_HAS_IN 0
#define PIO_ALIVETEST_CPU_S0_HAS_OUT 1
#define PIO_ALIVETEST_CPU_S0_HAS_TRI 0
#define PIO_ALIVETEST_CPU_S0_IRQ -1
#define PIO_ALIVETEST_CPU_S0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_ALIVETEST_CPU_S0_IRQ_TYPE "NONE"
#define PIO_ALIVETEST_CPU_S0_NAME "/dev/pio_aliveTest_cpu_s0"
#define PIO_ALIVETEST_CPU_S0_RESET_VALUE 0
#define PIO_ALIVETEST_CPU_S0_SPAN 32
#define PIO_ALIVETEST_CPU_S0_TYPE "altera_avalon_pio"


/*
 * pwm_cpu_s0_1 configuration
 *
 */

#define ALT_MODULE_CLASS_pwm_cpu_s0_1 pwm
#define PWM_CPU_S0_1_BASE 0x9000028
#define PWM_CPU_S0_1_IRQ -1
#define PWM_CPU_S0_1_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PWM_CPU_S0_1_NAME "/dev/pwm_cpu_s0_1"
#define PWM_CPU_S0_1_SPAN 4
#define PWM_CPU_S0_1_TYPE "pwm"


/*
 * sdram configuration
 *
 */

#define ALT_MODULE_CLASS_sdram altera_avalon_new_sdram_controller
#define SDRAM_BASE 0x0
#define SDRAM_CAS_LATENCY 3
#define SDRAM_CONTENTS_INFO
#define SDRAM_INIT_NOP_DELAY 0.0
#define SDRAM_INIT_REFRESH_COMMANDS 2
#define SDRAM_IRQ -1
#define SDRAM_IRQ_INTERRUPT_CONTROLLER_ID -1
#define SDRAM_IS_INITIALIZED 1
#define SDRAM_NAME "/dev/sdram"
#define SDRAM_POWERUP_DELAY 100.0
#define SDRAM_REFRESH_PERIOD 7.8125
#define SDRAM_REGISTER_DATA_IN 1
#define SDRAM_SDRAM_ADDR_WIDTH 0x19
#define SDRAM_SDRAM_BANK_WIDTH 2
#define SDRAM_SDRAM_COL_WIDTH 10
#define SDRAM_SDRAM_DATA_WIDTH 16
#define SDRAM_SDRAM_NUM_BANKS 4
#define SDRAM_SDRAM_NUM_CHIPSELECTS 1
#define SDRAM_SDRAM_ROW_WIDTH 13
#define SDRAM_SHARED_DATA 0
#define SDRAM_SIM_MODEL_BASE 0
#define SDRAM_SPAN 67108864
#define SDRAM_STARVATION_INDICATOR 0
#define SDRAM_TRISTATE_BRIDGE_SLAVE ""
#define SDRAM_TYPE "altera_avalon_new_sdram_controller"
#define SDRAM_T_AC 5.4
#define SDRAM_T_MRD 3
#define SDRAM_T_RCD 15.0
#define SDRAM_T_RFC 70.0
#define SDRAM_T_RP 15.0
#define SDRAM_T_WR 14.0


/*
 * timer_cpu_s0 configuration
 *
 */

#define ALT_MODULE_CLASS_timer_cpu_s0 altera_avalon_timer
#define TIMER_CPU_S0_ALWAYS_RUN 0
#define TIMER_CPU_S0_BASE 0x9000000
#define TIMER_CPU_S0_COUNTER_SIZE 32
#define TIMER_CPU_S0_FIXED_PERIOD 0
#define TIMER_CPU_S0_FREQ 25000000
#define TIMER_CPU_S0_IRQ 0
#define TIMER_CPU_S0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define TIMER_CPU_S0_LOAD_VALUE 249999
#define TIMER_CPU_S0_MULT 0.0010
#define TIMER_CPU_S0_NAME "/dev/timer_cpu_s0"
#define TIMER_CPU_S0_PERIOD 10
#define TIMER_CPU_S0_PERIOD_UNITS "ms"
#define TIMER_CPU_S0_RESET_OUTPUT 0
#define TIMER_CPU_S0_SNAPSHOT 1
#define TIMER_CPU_S0_SPAN 64
#define TIMER_CPU_S0_TICKS_PER_SEC 100.0
#define TIMER_CPU_S0_TIMEOUT_PULSE_OUTPUT 0
#define TIMER_CPU_S0_TYPE "altera_avalon_timer"

#endif /* __SYSTEM_H_ */
