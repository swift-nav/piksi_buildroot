$(eval TARGET_CFLAGS       := $(TARGET_CFLAGS) -ggdb3)
$(eval TARGET_LDFLAGS      := $(TARGET_LDFLAGS) -ggdb3)

$(eval BR2_TARGET_CFLAGS   := $(BR2_TARGET_CFLAGS) -ggdb3)
$(eval BR2_TARGET_LDFLAGS  := $(BR2_TARGET_LDFLAGS) -ggdb3)

$(eval BR2_TARGET_OPTIMIZATION := $(BR2_TARGET_OPTIMIZATION) -ggdb3)
$(eval TARGET_OPTIMIZATION := $(TARGET_OPTIMIZATION) -ggdb3)
