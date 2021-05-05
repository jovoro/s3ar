CFLAGS=-lcurl -lssl -lcrypto
DBGFLAGS=-g

s3.o: s3.c
	$(CC) $(DBGFLAGS) -c -o s3.o $(CFLAGS) s3.c

b64.o: b64.c
	$(CC) $(DBGFLAGS) -c -o b64.o $(CFLAGS) b64.c

s3ar.o: s3ar.c
	$(CC) $(DBGFLAGS) -c -o s3ar.o $(CFLAGS) s3ar.c

s3ar: b64.o s3.o s3ar.o
	$(CC) $(DBGFLAGS) -o s3ar $(CFLAGS) b64.o s3.o s3ar.o

clean:
	rm -f *.o s3ar
