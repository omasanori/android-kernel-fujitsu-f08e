GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
EXTRA_CFLAGS += -Idrivers/media/video/msm
EXTRA_CFLAGS += -Idrivers/media/video/msm/io
EXTRA_CFLAGS += -Idrivers/media/video/msm/eeprom
EXTRA_CFLAGS += -Idrivers/media/video/msm/csi
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_sensor_common.o msm_sensor.o msm_sensor_bayer.o msm_sensor_init.o
obj-$(CONFIG_OV5647) += ov5647_v4l2.o
obj-$(CONFIG_OV8825) += ov8825_v4l2.o
obj-$(CONFIG_IMX074) += imx074_v4l2.o
obj-$(CONFIG_S5K3L1YX) += s5k3l1yx.o
obj-$(CONFIG_IMX135) += imx135_v4l2.o
obj-$(CONFIG_OV2720) += ov2720.o
obj-$(CONFIG_MT9M114) += mt9m114_v4l2.o
obj-$(CONFIG_S5K4E1) += s5k4e1_v4l2.o
obj-$(CONFIG_MT9E013) += mt9e013_v4l2.o
obj-$(CONFIG_WEBCAM_OV9726) += ov9726_v4l2.o
obj-$(CONFIG_OV7692) += ov7692_v4l2.o
obj-$(CONFIG_VX6953) += vx6953.o
obj-$(CONFIG_OV9724) += ov9724_v4l2.o
# FUJITSU:2012_11_21 mod camera start
obj-$(CONFIG_M9MO_IU135F3) += m9mo_iu135f3.o camsensor_gpio_back.o
obj-$(CONFIG_M9MO_IU081F2F) += m9mo_iu081f2f.o camsensor_gpio_back.o
obj-$(CONFIG_M7MO_IU134FBF) += m7mo_iu134fbf.o camsensor_gpio_back.o
obj-$(CONFIG_RJ68JA120_FM1449) += rj68ja120_fm1449.o camsensor_gpio_front.o
# FUJITSU:2012_11_21 mod camera end
