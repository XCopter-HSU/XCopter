#
# oc_i2c_master_sw.tcl
#

# Create a new driver
create_driver oc_i2c_master

# Associate it with some hardware known as "oc_i2c_master"
set_sw_property hw_class_name oc_i2c_master

# The version of this driver
set_sw_property version 9.1

# This driver may be incompatible with versions of hardware less
# than specified below. Updates to hardware and device drivers
# rendering the driver incompatible with older versions of
# hardware are noted with this property assignment.
#
# Multiple-Version compatibility was introduced in version 7.1;
# prior versions are therefore excluded.
set_sw_property min_compatible_hw_version 7.1

# Initialize the driver in alt_sys_init()
set_sw_property auto_initialize true

# Location in generated BSP that above sources will be copied into
set_sw_property bsp_subdirectory drivers

# Interrupt properties: This driver supports both legacy and enhanced
# interrupt APIs, as well as ISR preemption.
set_sw_property isr_preemption_supported true
set_sw_property supported_interrupt_apis "legacy_interrupt_api"

#
# Source file listings...
#

# C/C++ source files
add_sw_property c_source HAL/src/oc_i2c_master_util.c

# Include files
add_sw_property include_source inc/oc_i2c_master_regs.h
add_sw_property include_source HAL/inc/oc_i2c_master.h

# This driver supports HAL
add_sw_property supported_bsp_type HAL

# End of file
