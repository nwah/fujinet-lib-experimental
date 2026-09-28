PRODUCT_BASE = $(PRODUCT_BASE)
LIBRARY = $(R2R_PD)/lib$(PRODUCT_BASE).$(PLATFORM).a
EXECUTABLE = $(R2R_PD)/$(PRODUCT_BASE)

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk

include $(MWD)/toolchains/prctools.mk

# No disk image for Palm OS: r2r just builds the executable/library into
# r2r/palmos/. A real .prc app additionally needs pilrc/build-prc, which is
# left to the application project, not this library.
r2r:: $(BUILD_EXEC) $(BUILD_LIB) $(R2R_EXTRA_DEPS)
	make -f $(PLATFORM_MK) $(PLATFORM)/r2r-post
