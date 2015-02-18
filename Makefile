# Dinu Marian Alexandru 334CC
build:
	mpicc tema4.c -o exec
run1:
	mpirun -np 12 ./exec test1.in mesaj1.in
run2:
	mpirun -np 8 ./exec test2.in mesaj2.in
clean:
	rm exec
