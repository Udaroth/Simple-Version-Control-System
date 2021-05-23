
output: svc.o tester.o
	gcc tester.o svc.o -o svc -Wextra -Wall -Werror -g -fsanitize=address

svc.o: svc.c svc.h
	gcc -c svc.c

tester.o: tester.c svc.h svc.c
	gcc -c tester.c

clean:
	rm *.o output