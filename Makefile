EXENAME = dropcopy

CC = g++
CFLAGS = -g -Wall -I./include
LIBS = -lpthread

OBJS = exec_globex.o \
       exec_globex_cmd.o \
       exec_globex_parse.o \
       read_config.o \
       client_message.o \
       trade_socket_list.o \
       drop_copy.o \
       read_position.o \
       main.o \
       package_store.o \
       trace_log.o \
       util.o 

all: $(EXENAME)

$(EXENAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

.PHONY: clean

clean:
	rm -rf $(EXENAME) *.o *.bak *~
