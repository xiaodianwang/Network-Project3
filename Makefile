default: sender1.c sender2.c receiver1.c receiver2.c common.h util.c router.c
	gcc -g -o sender2 sender2.c util.c -lm
	gcc -g -o router router.c util.c 
	gcc -g -o receiver2 receiver2.c util.c
	gcc -g -o sender1 sender1.c util.c
	gcc -g -o receiver1 receiver1.c util.c

clean:
	rm -f sender2 receiver2 router sender1 receiver1
	rm -r *.dSYM