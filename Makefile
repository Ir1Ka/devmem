CFLAGS := -Wall -Werror -O2 -g
LDFLAGS :=

ifneq ($(DEBUG),)
	CFLAGS += -DDEVMEM_DEBUG
endif

EXEC := devmem

all: $(EXEC)

$(EXEC): $(EXEC).o
	$(CROSS_COMPILE)gcc -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CROSS_COMPILE)gcc -c -o $@ $< $(CFLAGS)

clean:
	rm -rf *.o $(EXEC)
