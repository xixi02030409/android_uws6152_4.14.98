
# Linux driver folder

# r30: enable combined-transaction I2C path (no per-op printk).
# Console=ttyS1@115200 stalls ~4ms per printk; touch report path
# does 3-4 I2C ops/frame => 12-16ms/frame eaten by console.
ccflags-y += -DHIGH_SPEED_IIC_TRANSFER
obj-$(CONFIG_TOUCHSCREEN_HYNITRON_TS) += hynitron_touch.o

hynitron_touch-objs	+= hynitron_core.o
hynitron_touch-objs	+= hynitron_i2c.o
hynitron_touch-objs	+= hynitron_esd_check.o
hynitron_touch-objs	+= hynitron_tool_debug.o
hynitron_touch-objs	+= hynitron_update_firmware.o
hynitron_touch-objs	+= hynitron_gesture.o
hynitron_touch-objs	+= hynitron_proximity.o