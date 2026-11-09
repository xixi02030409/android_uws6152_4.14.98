
# Linux driver folder

# Enable i2c_transfer based high-speed IIC path (single combined transaction
# for read_register, no per-transaction printk). Factory (divo-63) kernel does
# NOT contain the "cst3xx_i2c_write ret" printk strings, proving the fast path
# is the correct baseline. Without this define every touch report does 2
# separate I2C bus transactions + 2 unconditional printks -> touch lag.
ccflags-y += -DHIGH_SPEED_IIC_TRANSFER

obj-$(CONFIG_TOUCHSCREEN_HYNITRON_TS) += hynitron_touch.o

hynitron_touch-objs	+= hynitron_core.o
hynitron_touch-objs	+= hynitron_i2c.o
hynitron_touch-objs	+= hynitron_esd_check.o
hynitron_touch-objs	+= hynitron_tool_debug.o
hynitron_touch-objs	+= hynitron_update_firmware.o
hynitron_touch-objs	+= hynitron_gesture.o
hynitron_touch-objs	+= hynitron_proximity.o