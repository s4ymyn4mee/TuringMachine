NAME := TuringMachine.out

CC := gcc
CFLAGS := -g

default: $(NAME)

$(NAME): src/TuringMachine.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) $(NAME)