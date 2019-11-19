
# Target library

targets := libfs.a
allObjs := disk.o fs.o mylibrary.o

CC      := gcc
CFLAGS  := -Wall -Werror
AR      := ar
ARFLAGS := rcs

ifneq ($(V),1)
B = @
endif

all: $(targets)

deps := $(patsubst %.o,%.d,$(allObjs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libfs.a: $(allObjs)
	@echo "AR   $@"
	$(B)$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo "CC   $@"
	$(B)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean: 
	@echo "CLEAN"
	$(B)rm -f $(allObjs) $(targets) $(deps)