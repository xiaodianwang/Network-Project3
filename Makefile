default: sender.c common.h util.c router.c
	gcc -g -o sender2 sender2.c util.c -lm
	gcc -g -o router router.c util.c 
	gcc -g -o receiver2 receiver2.c util.c 

clean:
	rm -f sender2 receiver2 router 
	rm -r *.dSYM