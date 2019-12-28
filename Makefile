NAME = gaembuoy

CFLAGS = -Wall -O2 -MMD -MP `pkg-config --cflags sdl2`
LDFLAGS = `pkg-config --libs sdl2` -lpthread

SRC = main.c cpu.c memory.c cart.c gpu.c sync.c sdl.c input.c irq.c dma.c \
      timer.c spu.c hdma.c rtc.c

OBJ = $(SRC:%.c=%.o)
DEP = $(SRC:%.c=%.d)

$(NAME) : $(OBJ)
	$(info LD $@)
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(DEP)

%.o: %.c
	$(info CC $@)
	$(CC) -c $(CFLAGS) -o $@ $<

.PHONY : clean
clean:
	$(info CLEAN $(NAME))
	rm -f $(OBJ) $(DEP)

# Be verbose if V is set
$V.SILENT:
