.DEFAULT_GOAL := main

main:
	gcc -Wall -std=c99 -o s-talk main.c send_packets.c terminal_output.c receive_packets.c terminal_input.c list/list.c -pthread

schat:
	gcc -Wall -std=c99 -o schat schat_main.c -lpthread

clean:
	rm -f s-talk main schat *.o *.s