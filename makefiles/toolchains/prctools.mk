CC_DEFAULT ?= m68k-palmos-gcc
AS_DEFAULT ?= $(CC_DEFAULT)
LD_DEFAULT ?= $(CC_DEFAULT)
AR_DEFAULT ?= m68k-palmos-ar

include $(MWD)/tc-common.mk

CFLAGS += -O2 -Wall -palmos3.5
ASFLAGS +=
LDFLAGS +=

DSTRING_OPEN = \"
DSTRING_CLOSE = \"
CFLAGS += -DGIT_VERSION=$(DSTRING_OPEN)$(GIT_VERSION)$(DSTRING_CLOSE)
ifneq ($(FUJINET_LIB_VERSION),)
  CFLAGS += -DFNLIB_VERSION_FULL=\"$(FUJINET_LIB_VERSION)\"
endif

define include-dir-flag
  -I$1
endef

define asm-include-dir-flag
  -I$1
endef

define library-dir-flag
  -L$1
endef

define library-flag
  -l$1
endef

define link-lib
  $(AR) rcs $1 $2
endef

define link-bin
  $(CC) $(CFLAGS) -o $1 $2 $(LIBS)
endef

define compile
  $(CC) $(CFLAGS) -MMD -c -o $1 $2
endef

define assemble
  $(AS) $(ASFLAGS) -c -o $1 $2
endef
