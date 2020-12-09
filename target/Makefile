SRCDIR = src
COBJDIR = obj/c
AOBJDIR = obj/s
BINDIR = bin
CSRC += $(wildcard $(SRCDIR)/*.c)
ASRC += $(wildcard $(SRCDIR)/*.s)
COBJ += $(CSRC:$(SRCDIR)/%.c=$(AOBJDIR)/%.o)
AOBJ += $(ASRC:$(SRCDIR)/%.s=$(COBJDIR)/%.o)
DEP += $(OBJ:$(COBJDIR)/%.c=$(COBJDIR)/%.d)

CCPATH = /opt/cross/bin
CC = $(CCPATH)/pdp11-aout-gcc
OBJCOPY = $(CCPATH)/pdp11-aout-objcopy
OBJDUMP = $(CCPATH)/pdp11-aout-objdump
CFLAGS += --std=gnu11 -MMD -Wall -Wpointer-arith -I$(SRCDIR)
CFLAGS += -m45 -nostdlib
LDFLAGS += -nostdlib -Ttext 0x200 -m45 -N
dir_guard = @mkdir -p $(@D)

all: $(BINDIR)/a.bin

$(AOBJDIR)/%.o: $(SRCDIR)/%.c
	$(dir_guard)
	$(CC) $(CFLAGS) -c $< -o $@

$(COBJDIR)/%.o: $(SRCDIR)/%.s
	$(dir_guard)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(AOBJDIR) $(COBJDIR) $(BINDIR)

$(BINDIR)/a.out: $(AOBJ) $(COBJ)
	$(dir_guard)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/a.bin: $(BINDIR)/a.out
	$(dir_guard)
	$(OBJCOPY) -O binary $^ $@
	$(OBJDUMP) -b binary -m pdp11 -D $@

#-include $(DEP)
