all: PartA PartB watchdog

PartA: ping.c
	gcc ping.c -o PartA
PartB: new_ping.c
	gcc new_ping.c -o PartB
watchdog: watchdog.c
	gcc watchdog.c -o watchdog

clean:
	rm -f *.o PartA  PartB watchdog
