default: sender.c common.h util.c router.c
	gcc -g -o sender sender.c util.c 
	gcc -g -o router router.c util.c 
	gcc -g -o receiver receiver.c util.c 

clean:
	rm -f sender receiver router 
	rm -r *.dSYM