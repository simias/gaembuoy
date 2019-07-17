NAME = gaembuoy

CFLAGS = -Wall -O2 -MMD -MP

SRC = main.c cpu.c memory.c cart.c gpu.c

OBJ = $(SRC:%.c=%.o)
DEP = $(SRC:%.c=%.d)

$(NAME) : $(OBJ)
	$(info LD $@)
	$(CC) $(LDFLAGS) -o $@ $^

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
