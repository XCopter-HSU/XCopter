/*
 * system.h - SOPC Builder system and BSP software package information
 *
 * Machine generated for CPU 'cpu_s1' in SOPC Builder design 'soc_system'
 * SOPC Builder design path: F:/Projekte/MCAPIforDE1-Soc/MCAPI_DE1-SoC_SoCs/NIOS_MCAPI_Base_v01/soc_system.sopcinfo
 *
 * Generated: Tue Nov 25 19:20:42 CET 2014
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
#define ALT_CPU_NAME "cpu_s1"
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
#define __ALTERA_AVALON_ONCHIP_MEMORY2
#define __ALTERA_AVALON_PIO
#define __ALTERA_AVALON_TIMER
#define __ALTERA_NIOS2_QSYS
#define __CPU_FIFO_BRIDGE


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
#define ALT_STDERR "/dev/jtag_uart_cpu_s1"
#define ALT_STDERR_BASE 0x9000020
#define ALT_STDERR_DEV jtag_uart_cpu_s1
#define ALT_STDERR_IS_JTAG_UART
#define ALT_STDERR_PRESENT
#define ALT_STDERR_TYPE "altera_avalon_jtag_uart"
#define ALT_STDIN "/dev/jtag_uart_cpu_s1"
#define ALT_STDIN_BASE 0x9000020
#define ALT_STDIN_DEV jtag_uart_cpu_s1
#define ALT_STDIN_IS_JTAG_UART
#define ALT_STDIN_PRESENT
#define ALT_STDIN_TYPE "altera_avalon_jtag_uart"
#define ALT_STDOUT "/dev/jtag_uart_cpu_s1"
#define ALT_STDOUT_BASE 0x9000020
#define ALT_STDOUT_DEV jtag_uart_cpu_s1
#define ALT_STDOUT_IS_JTAG_UART
#define ALT_STDOUT_PRESENT
#define ALT_STDOUT_TYPE "altera_avalon_jtag_uart"
#define ALT_SYSTEM_NAME "soc_system"


/*
 * fifo_bridge_cpuM_cpus1 configuration
 *
 */

#define ALT_MODULE_CLASS_fifo_bridge_cpuM_cpus1 CPU_FiFo_Bridge
#define FIFO_BRIDGE_CPUM_CPUS1_BASE 0x9004000
#define FIFO_BRIDGE_CPUM_CPUS1_IRQ 1
#define FIFO_BRIDGE_CPUM_CPUS1_IRQ_INTERRUPT_CONTROLLER_ID 0
#define FIFO_BRIDGE_CPUM_CPUS1_NAME "/dev/fifo_bridge_cpuM_cpus1"
#define FIFO_BRIDGE_CPUM_CPUS1_SPAN 1024
#define FIFO_BRIDGE_CPUM_CPUS1_TYPE "CPU_FiFo_Bridge"


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
#define ALT_SYS_CLK TIMER_CPU_S1
#define ALT_TIMESTAMP_CLK none


/*
 * jtag_uart_cpu_s1 configuration
 *
 */

#define ALT_MODULE_CLASS_jtag_uart_cpu_s1 altera_avalon_jtag_uart
#define JTAG_UART_CPU_S1_BASE 0x9000020
#define JTAG_UART_CPU_S1_IRQ 2
#define JTAG_UART_CPU_S1_IRQ_INTERRUPT_CONTROLLER_ID 0
#define JTAG_UART_CPU_S1_NAME "/dev/jtag_uart_cpu_s1"
#define JTAG_UART_CPU_S1_READ_DEPTH 64
#define JTAG_UART_CPU_S1_READ_THRESHOLD 8
#define JTAG_UART_CPU_S1_SPAN 16
#define JTAG_UART_CPU_S1_TYPE "altera_avalon_jtag_uart"
#define JTAG_UART_CPU_S1_WRITE_DEPTH 64
#define JTAG_UART_CPU_S1_WRITE_THRESHOLD 8


/*
 * onchip_sram configuration
 *
 */

#define ALT_MODULE_CLASS_onchip_sram altera_avalon_onchip_memory2
#define ONCHIP_SRAM_ALLOW_IN_SYSTEM_MEMORY_CONTENT_EDITOR 0
#define ONCHIP_SRAM_ALLOW_MRAM_SIM_CONTENTS_ONLY_FILE 0
#define ONCHIP_SRAM_BASE 0x0
#define ONCHIP_SRAM_CONTENTS_INFO ""
#define ONCHIP_SRAM_DUAL_PORT 0
#define ONCHIP_SRAM_GUI_RAM_BLOCK_TYPE "AUTO"
#define ONCHIP_SRAM_INIT_CONTENTS_FILE "soc_system_onchip_sram"
#define ONCHIP_SRAM_INIT_MEM_CONTENT 1
#define ONCHIP_SRAM_INSTANCE_ID "NONE"
#define ONCHIP_SRAM_IRQ -1
#define ONCHIP_SRAM_IRQ_INTERRUPT_CONTROLLER_ID -1
#define ONCHIP_SRAM_NAME "/dev/onchip_sram"
#define ONCHIP_SRAM_NON_DEFAULT_INIT_FILE_ENABLED 0
#define ONCHIP_SRAM_RAM_BLOCK_TYPE "AUTO"
#define ONCHIP_SRAM_READ_DURING_WRITE_MODE "DONT_CARE"
#define ONCHIP_SRAM_SINGLE_CLOCK_OP 0
#define ONCHIP_SRAM_SIZE_MULTIPLE 1
#define ONCHIP_SRAM_SIZE_VALUE 360448
#define ONCHIP_SRAM_SPAN 360448
#define ONCHIP_SRAM_TYPE "altera_avalon_onchip_memory2"
#define ONCHIP_SRAM_WRITABLE 1


/*
 * pio_aliveTest_cpu_s1 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_aliveTest_cpu_s1 altera_avalon_pio
#define PIO_ALIVETEST_CPU_S1_BASE 0x9000030
#define PIO_ALIVETEST_CPU_S1_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_ALIVETEST_CPU_S1_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_ALIVETEST_CPU_S1_CAPTURE 0
#define PIO_ALIVETEST_CPU_S1_DATA_WIDTH 2
#define PIO_ALIVETEST_CPU_S1_DO_TEST_BENCH_WIRING 0
#define PIO_ALIVETEST_CPU_S1_DRIVEN_SIM_VALUE 0
#define PIO_ALIVETEST_CPU_S1_EDGE_TYPE "NONE"
#define PIO_ALIVETEST_CPU_S1_FREQ 25000000
#define PIO_ALIVETEST_CPU_S1_HAS_IN 0
#define PIO_ALIVETEST_CPU_S1_HAS_OUT 1
#define PIO_ALIVETEST_CPU_S1_HAS_TRI 0
#define PIO_ALIVETEST_CPU_S1_IRQ -1
#define PIO_ALIVETEST_CPU_S1_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_ALIVETEST_CPU_S1_IRQ_TYPE "NONE"
#define PIO_ALIVETEST_CPU_S1_NAME "/dev/pio_aliveTest_cpu_s1"
#define PIO_ALIVETEST_CPU_S1_RESET_VALUE 0
#define PIO_ALIVETEST_CPU_S1_SPAN 32
#define PIO_ALIVETEST_CPU_S1_TYPE "altera_avalon_pio"


/*
 * timer_cpu_s1 configuration
 *
 */

#define ALT_MODULE_CLASS_timer_cpu_s1 altera_avalon_timer
#define TIMER_CPU_S1_ALWAYS_RUN 0
#define TIMER_CPU_S1_BASE 0x9000000
#define TIMER_CPU_S1_COUNTER_SIZE 32
#define TIMER_CPU_S1_FIXED_PERIOD 0
#define TIMER_CPU_S1_FREQ 25000000
#define TIMER_CPU_S1_IRQ 0
#define TIMER_CPU_S1_IRQ_INTERRUPT_CONTROLLER_ID 0
#define TIMER_CPU_S1_LOAD_VALUE 249999
#define TIMER_CPU_S1_MULT 0.0010
#define TIMER_CPU_S1_NAME "/dev/timer_cpu_s1"
#define TIMER_CPU_S1_PERIOD 10
#define TIMER_CPU_S1_PERIOD_UNITS "ms"
#define TIMER_CPU_S1_RESET_OUTPUT 0
#define TIMER_CPU_S1_SNAPSHOT 1
#define TIMER_CPU_S1_SPAN 64
#define TIMER_CPU_S1_TICKS_PER_SEC 100.0
#define TIMER_CPU_S1_TIMEOUT_PULSE_OUTPUT 0
#define TIMER_CPU_S1_TYPE "altera_avalon_timer"


/*
 * ucosii configuration
 *
 */

#define OS_ARG_CHK_EN 1
#define OS_CPU_HOOKS_EN 1
#define OS_DEBUG_EN 1
#define OS_EVENT_NAME_SIZE 32
#define OS_FLAGS_NBITS 16
#define OS_FLAG_ACCEPT_EN 1
#define OS_FLAG_DEL_EN 1
#define OS_FLAG_EN 1
#define OS_FLAG_NAME_SIZE 32
#define OS_FLAG_QUERY_EN 1
#define OS_FLAG_WAIT_CLR_EN 1
#define OS_LOWEST_PRIO 40
#define OS_MAX_EVENTS 60
#define OS_MAX_FLAGS 20
#define OS_MAX_MEM_PART 60
#define OS_MAX_QS 20
#define OS_MAX_TASKS 30
#define OS_MBOX_ACCEPT_EN 1
#define OS_MBOX_DEL_EN 1
#define OS_MBOX_EN 1
#define OS_MBOX_POST_EN 1
#define OS_MBOX_POST_OPT_EN 1
#define OS_MBOX_QUERY_EN 1
#define OS_MEM_EN 1
#define OS_MEM_NAME_SIZE 32
#define OS_MEM_QUERY_EN 1
#define OS_MUTEX_ACCEPT_EN 1
#define OS_MUTEX_DEL_EN 1
#define OS_MUTEX_EN 1
#define OS_MUTEX_QUERY_EN 1
#define OS_Q_ACCEPT_EN 1
#define OS_Q_DEL_EN 1
#define OS_Q_EN 1
#define OS_Q_FLUSH_EN 1
#define OS_Q_POST_EN 1
#define OS_Q_POST_FRONT_EN 1
#define OS_Q_POST_OPT_EN 1
#define OS_Q_QUERY_EN 1
#define OS_SCHED_LOCK_EN 1
#define OS_SEM_ACCEPT_EN 1
#define OS_SEM_DEL_EN 1
#define OS_SEM_EN 1
#define OS_SEM_QUERY_EN 1
#define OS_SEM_SET_EN 1
#define OS_TASK_CHANGE_PRIO_EN 1
#define OS_TASK_CREATE_EN 1
#define OS_TASK_CREATE_EXT_EN 1
#define OS_TASK_DEL_EN 1
#define OS_TASK_IDLE_STK_SIZE 512
#define OS_TASK_NAME_SIZE 32
#define OS_TASK_PROFILE_EN 1
#define OS_TASK_QUERY_EN 1
#define OS_TASK_STAT_EN 1
#define OS_TASK_STAT_STK_CHK_EN 1
#define OS_TASK_STAT_STK_SIZE 512
#define OS_TASK_SUSPEND_EN 1
#define OS_TASK_SW_HOOK_EN 1
#define OS_TASK_TMR_PRIO 0
#define OS_TASK_TMR_STK_SIZE 512
#define OS_THREAD_SAFE_NEWLIB 1
#define OS_TICKS_PER_SEC TIMER_CPU_S1_TICKS_PER_SEC
#define OS_TICK_STEP_EN 1
#define OS_TIME_DLY_HMSM_EN 1
#define OS_TIME_DLY_RESUME_EN 1
#define OS_TIME_GET_SET_EN 1
#define OS_TIME_TICK_HOOK_EN 1
#define OS_TMR_CFG_MAX 16
#define OS_TMR_CFG_NAME_SIZE 16
#define OS_TMR_CFG_TICKS_PER_SEC 10
#define OS_TMR_CFG_WHEEL_SIZE 2
#define OS_TMR_EN 0

#endif /* __SYSTEM_H_ */
