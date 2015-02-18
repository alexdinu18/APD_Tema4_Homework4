/*
	Dinu Marian Alexandru 334CC
	Tema 4 - APD
*/
#include <stdio.h>
#include "mpi.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define INF 32767
#define BCAST -100

void read_topology(char* file, int* neighbours, int rank, int size);
void calc_routing_table(int** adjacency_matrix, int** next, int size);
void discover_topology(int* neighbours, int** adjacency_matrix, int rank, int size);
void print_routing_table(int** adjacency_matrix, int* routing_array, int size, int rank);
void send_messages(int* routing_array, int* neighbours, char* file, int rank, int size);
int find_leader(int* neighbours, int rank, int size);
int find_deputy_leader(int* neighbours, int leader, int rank, int size);

int main(int argc, char** argv) {
	int* neighbours;
	int** adjacency_matrix, **matrix_from_neighbours, *routing_array;
	int **next;
	int rank, size, i, j, k;
	MPI_Status stat;

	if (argc != 3) {
		printf("Wrong number of arguments!\n");
		exit(1);	
	} 
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	neighbours = (int*)calloc(size, sizeof(int));
	routing_array = (int*)calloc(size, sizeof(int));

	// etapa 1
	// citește topologia din fișier
	read_topology(argv[1], neighbours, rank, size);
	
	adjacency_matrix = (int**)malloc(size * sizeof(int*));
	matrix_from_neighbours = (int**)malloc(size * sizeof(int*));
	
	for (i = 0; i < size; i++) {
		adjacency_matrix[i] = (int*)calloc(size, sizeof(int));
		matrix_from_neighbours[i] = (int*)calloc(size, sizeof(int));	
	}

	for (i = 0; i < size; i++) {
   		adjacency_matrix[rank][i] = neighbours[i];
	}

	// descoperire topologie
	discover_topology(neighbours, adjacency_matrix, rank, size);


	// calculez tabela de rutare
	if (rank == 0) {
		next = (int**)malloc(size * sizeof(int*));
		for (i = 0; i < size; i++) {
			next[i] = (int*)calloc(size, sizeof(int));
		}
		calc_routing_table(adjacency_matrix, next, size);
	
		// transmit vectorii de rutare celorlalte procese
		for (i = 0; i < size; i++) {
			routing_array[i] = next[0][i];
			if (i > 0)
				MPI_Send(next[i], size, MPI_INT, i, 1, MPI_COMM_WORLD);	
		}
	}
	else {
		MPI_Recv(routing_array, size, MPI_INT, 0, 1, MPI_COMM_WORLD, &stat);
	}

	// afișarea tabelei de rutare pe ecran
	print_routing_table(adjacency_matrix, routing_array, size, rank);

	// etapa 2
	// trimitere mesaje

	send_messages(routing_array, neighbours, argv[2], rank, size); 

	// etapa3
	int leader = find_leader(neighbours, rank, size);
	int deputy_leader = find_deputy_leader(neighbours, leader, rank, size);
	printf("[ETAPA3]%d %d %d\n", rank, leader, deputy_leader);

	MPI_Finalize();
	return 0;
}

void read_topology(char* file, int* neighbours, int rank, int size) {
	/*
		Parsarea fisierului de intrare.	
	*/
	char line[80];
	int i, searched;
	FILE* topology = fopen(file, "r");
	while (!feof(topology)) {
		fgets(line, 80, topology);
		sscanf(line,"%d", &searched);
		if (searched == rank) {
			int cursor = 0;
			while(line[cursor++] != '-');
			cursor++;
			while(line[cursor] != '\0') {
				if(isdigit(line[cursor])) {
					sscanf(line + cursor,"%d", &searched);
					neighbours[searched] = 1;
					while(isdigit(line[cursor++]));
				}
				else
					cursor++;			
			}
			break;
		}
		
	}
	fclose(topology);
}

void calc_routing_table(int** adjacency_matrix, int** next, int size) {
	/*
		Calcularea tabelei de rutare și vecotorul 
		next(next_hop) folosind algoritmul Floyd Warshall. 	
	*/
	int i, j, k;
	int **dist;
	dist = (int**)malloc(size * sizeof(int*));
	
	for (i = 0; i < size; i++) {
		dist[i] = (int*)calloc(size, sizeof(int));	
	}
	for (i = 0; i < size; i++) {
		for (j = 0; j < size; j++) {
			if (adjacency_matrix[i][j] == 0) {
				if (i == j)
					dist[i][j] = 0;
				else
					dist[i][j] = INF;
			}
			else
				dist[i][j] = 1;

			next[i][j] = j;
		}
	}

	for (k = 0; k < size; k++) {
		for (i = 0; i < size; i++) {
			for (j = 0; j < size; j++) {
				if (dist[i][j] > dist[i][k] + dist[k][j]) {
					dist[i][j] = dist[i][k] + dist[k][j];
					next[i][j] = next[i][k];
				}
			}
		}
	}
}

void discover_topology(int* neighbours, int** adjacency_matrix, int rank, int size) {
	/*
		Descoperirea topologiei folosind algortimul ecou.
		Înainte de a trimite informația propriu-zisă, am trimis 
		un char "E" (ECOU) sau "S" (SONDAJ). Dacă primeam sondaj
		când așteptam ecou, ștergeam intrarea respectivă din matricea
		de adiacență. Astfel am reușit să elimin ciclurile.
	*/
	int i, j, k, nr_neigh, parent, flag = 1, temp;
	char type;
	int* aux = (int*)calloc(size, sizeof(int));
	MPI_Status stat;
	
	if (rank == 0) {
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1) {
				type = 'S';
				MPI_Send(&type, 1, MPI_CHAR, i, 1, MPI_COMM_WORLD);
				MPI_Send(&rank,1, MPI_INT, i, 1, MPI_COMM_WORLD);
			}
		}
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1) {
				for (j = 0; j < size; j++) {
					MPI_Recv(&type, 1, MPI_CHAR, i, 1, MPI_COMM_WORLD, &stat);
					if (type == 'E') {
						MPI_Recv(aux, size, MPI_INT, i, 1, MPI_COMM_WORLD, &stat);
						for (k = 0; k < size; k++)
							if (aux[k] == 1)
								adjacency_matrix[j][k] = 1;
					}
					else {
						MPI_Recv(&k, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &stat);
						neighbours[i] = 0;
						adjacency_matrix[rank][i] = 0;
						break;
					}
				}
			}
		}
	}
	else {
		MPI_Recv(&type, 1, MPI_CHAR, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &stat);
		MPI_Recv(&parent, 1, MPI_INT, stat.MPI_SOURCE, 1, MPI_COMM_WORLD, &stat);
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1)
				nr_neigh++;			
		}
		if (nr_neigh == 1) {
			for (i = 0; i < size; i++) {
				type = 'E';
				MPI_Send(&type, 1, MPI_CHAR, parent, 1, MPI_COMM_WORLD);
				MPI_Send(adjacency_matrix[i], size, MPI_INT, parent, 1, MPI_COMM_WORLD);
			}
		}
		else {
			for (i = 0; i < size; ++i) {
				if (i != parent) {
					if (neighbours[i] == 1) {
						type = 'S';
						MPI_Send(&type, 1, MPI_CHAR, i, 1, MPI_COMM_WORLD);
						MPI_Send(&rank, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
					}				
				}
			}
			for (i = 0; i < size; i++) {
				if (i != parent)
					if (neighbours[i] == 1) {
						for (j = 0; j < size; j++) {
							MPI_Recv(&type, 1, MPI_CHAR, i, 1, MPI_COMM_WORLD, &stat);
							if (type == 'E') {
								MPI_Recv(aux, size, MPI_INT, i, 1, MPI_COMM_WORLD, &stat);
								for (k = 0; k < size; k++)
									if (aux[k] == 1)
										adjacency_matrix[j][k] = 1;
							}
							else {
								MPI_Recv(&k, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &stat);
								neighbours[i] = 0;
								adjacency_matrix[rank][i] = 0;
								break;
							}
						}
					}
			}
			for (i = 0; i < size; i++) {
				type = 'E';
				MPI_Send(&type, 1, MPI_CHAR, parent, 1, MPI_COMM_WORLD);
				MPI_Send(adjacency_matrix[i], size, MPI_INT, parent, 1, MPI_COMM_WORLD);
			}
		}
  	}
}

void print_routing_table(int** adjacency_matrix, int* routing_array, int size, int rank) {
	/*
		Afișarea vectorului de rutare. Dacă rank == 0,
		atunci afișez și matricea de adiacență.
	*/	
	int i, j;
	if (rank == 0) {
		for (i = 0; i < size; i++) {
			for	(j = 0; j < size; j++)	
				printf("%d ", adjacency_matrix[i][j]);
			printf("\n");	
		}	
	}
	printf("[ETAPA1]%d\n", rank);
	for (i = 0; i < size; i++) {
		printf("[ETAPA1]%d %d\n", routing_array[i], i);
	}
}

void send_messages(int* routing_array, int* neighbours, char* file, int rank, int size) {
	/*
		Etapa 2 - trimiterea de mesaje între procese.
		Mai întâi parsez fișierul de intrare, apoi
		fiecare proces trimite un broadcast pentru
		a le anunța pe celelalte că sunt gată să înceapă
		trimiterea mesajelor.
		Procesele primesc și rutează mesajele primite
		până când au primit un număr de size - 1 mesaje
		de QUIT. 
	*/
	int i, j, finish = 1, length;
	FILE* messages = fopen(file, "r");
	char line[80];
	int nr_msg;
	char** msg;
	int* dest;
	MPI_Status stat;
	fgets(line, 80, messages);
	sscanf(line,"%d", &nr_msg);
	dest = (int*)malloc(nr_msg * sizeof(int));
	msg = (char**)malloc(nr_msg * sizeof(char*));
	for (i = 0; i < nr_msg; i++)
		msg[i] = (char*)malloc(80 * sizeof(char));
	
	// parsarea fișierului de intrare
	for (i = 0; i < nr_msg; i++) {
		fgets(line, 80, messages);

		int source;
		char temp[10];
		sscanf(line,"%d", &source);
		if (source == rank) {
			sscanf(line,"%d %s %[^\n]", &source, temp, msg[i]);
			if (strcmp(temp,"B") == 0)
				dest[i] = BCAST;
			else
				dest[i] = atoi(temp);
		}
		else {
			dest[i] = -1;
			msg[i] = '\0';
		}

	}
	fclose(messages);

	// începe broadcastul
	char* payload = (char*)malloc(80 * sizeof(char));
	strcpy(payload, "B");
	length = 1;
	for (i = 0; i < size; i++)
		if (i != rank && neighbours[i]) {
			MPI_Send(&length, 1, MPI_INT, routing_array[i], 1, MPI_COMM_WORLD);
			MPI_Send(payload,1, MPI_CHAR, routing_array[i], 1, MPI_COMM_WORLD);
		}	

	// primirea și rutarea mesajelor
	int quit = 0;
	while(1) {
		for (i = 0; i < nr_msg; i++) {
			if (dest[i] >= 0) {
				char c[10];
				sprintf(c, "%d", dest[i]);
				strcpy(payload, c);
				strcat(payload, " ");
				strcat(payload, msg[i]);
				length = strlen(payload);
				MPI_Send(&length, 1, MPI_INT, routing_array[dest[i]] , 1, MPI_COMM_WORLD);
				MPI_Send(payload, strlen(payload), MPI_CHAR, routing_array[dest[i]] , 1, MPI_COMM_WORLD);
				dest[i] = -1;
				free(msg[i]);
			}
			else if (dest[i] == BCAST) {
				for (j = 0; j < size; j++) {
					if (j != rank && neighbours[j]) {
						strcpy(payload, "B");
						strcat(payload, " ");
						strcat(payload, msg[i]);
						length = strlen(payload);
						MPI_Send(&length, 1, MPI_INT, routing_array[j], 1, MPI_COMM_WORLD);
						MPI_Send(payload, strlen(payload), MPI_CHAR, routing_array[j], 1, MPI_COMM_WORLD);
					}
				}
/*
				for (j = 0; j < size; j++) {
					if (j != rank) {
						char c[10];
						sprintf(c, "%d", j);
						strcpy(payload, c);
						strcat(payload, " ");
						strcat(payload, msg[i]);
						length = strlen(payload);
						MPI_Send(&length, 1, MPI_INT, routing_array[j], 1, MPI_COMM_WORLD);
						MPI_Send(payload, strlen(payload), MPI_CHAR, routing_array[j], 1, MPI_COMM_WORLD);
					}
				}
*/
				dest[i] = -1;
				free(msg[i]);
			}
		}
		
		// primirea mesajelor
		MPI_Recv(&length, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &stat);
		MPI_Recv(payload, length, MPI_CHAR, stat.MPI_SOURCE, 1, MPI_COMM_WORLD, &stat);
		payload[length] = '\0';

		if (payload[0] == 'B') {
			if (strlen(payload) < 2) 
				printf("[ETAPA2]rank(%d):Primit de la %d mesajul \"BROADCAST INITIAL\" cu destinatia BROADCAST.\n", rank, stat.MPI_SOURCE);
			else
				printf("[ETAPA2]rank(%d):Primit de la %d mesajul \"%s\" cu destinatia BROADCAST.\n", rank, stat.MPI_SOURCE, payload+2);
			for (i = 0; i < size; i++) {
				if (neighbours[i] && i != stat.MPI_SOURCE && i != rank) {
					MPI_Send(&length, 1, MPI_INT, routing_array[i], 1, MPI_COMM_WORLD);
					MPI_Send(payload, strlen(payload), MPI_CHAR, routing_array[i], 1, MPI_COMM_WORLD);				
				}
			}
		}
		else if (payload[0] == 'Q') {
			quit++;
		}
		else {
			int destination;
			sscanf(payload, "%d", &destination);
			printf("[ETAPA2]rank(%d):Primit de la %d mesajul \"%s\" cu destinatia %d.\n", rank, stat.MPI_SOURCE, payload+2, destination);
			if (destination != rank) {
				length = strlen(payload);
				MPI_Send(&length, 1, MPI_INT, routing_array[destination], 1, MPI_COMM_WORLD);
				MPI_Send(payload, strlen(payload), MPI_CHAR, routing_array[destination], 1, MPI_COMM_WORLD);
			}
		}
		// încheierea etapei
		if (finish == 1) {
			for (i = 0; i < size; i++)
				if (i != rank) {
					length = 1;
					MPI_Send(&length, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
					strcpy(payload, "Q");
					MPI_Send(payload, 1, MPI_CHAR, i, 1, MPI_COMM_WORLD);
				}
			finish = 0;
		}
		if (quit == size - 1)
			break;	
	}
} 

int find_leader(int* neighbours, int rank, int size) {
	/*
		Alegerea liderului.
		Am decis să fac alegerea liderului folosind algoritmul ecou.
		La sfârșit, după ce procesul cu rank == 0 a aflat liderul,
		acesta îl trimite mai departe tuturor celorlalte procese.
		(Mai multe detalii în README)
	*/
	int i, nr_neigh, parent, leader = rank, candidate, tag = 5;
	MPI_Status stat;
	
	if (rank == 0) {
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1) {
				MPI_Send(&rank,1, MPI_INT, i, tag, MPI_COMM_WORLD);
			}
		}
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1) {
				MPI_Recv(&candidate, 1, MPI_INT, i, tag, MPI_COMM_WORLD, &stat);
				if (leader > candidate)
					leader = candidate;
			}
		}
		
		// trimit liderul
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1) {
				MPI_Send(&leader,1, MPI_INT, i, tag, MPI_COMM_WORLD);
			}
		}
	}
	else {
		MPI_Recv(&parent, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &stat);
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1)
				nr_neigh++;			
		}
		if (nr_neigh == 1) {
			if (leader > parent)
				leader = parent;
			MPI_Send(&leader, 1, MPI_INT, parent, tag, MPI_COMM_WORLD);
		}		
		else {
			for (i = 0; i < size; i++) {
				if (i != parent) {
					if (neighbours[i] == 1) {
						MPI_Send(&rank, 1, MPI_INT, i, tag, MPI_COMM_WORLD);
					}				
				}
			}
			for (i = 0; i < size; i++) {
				if (i != parent)
					if (neighbours[i] == 1) {
						MPI_Recv(&candidate, 1, MPI_INT, i, tag, MPI_COMM_WORLD, &stat);
						if (leader > candidate)
							leader = candidate;
					}
			}
			MPI_Send(&leader, 1, MPI_INT, parent, tag, MPI_COMM_WORLD);
		}
		
		// primesc liderul și îl dau mai departe
		MPI_Recv(&leader, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &stat);
		if (nr_neigh != 1)
			for (i = 0; i < size; i++) {
				if (i != parent) {
					if (neighbours[i] == 1) {
						MPI_Send(&leader, 1, MPI_INT, i, tag, MPI_COMM_WORLD);
					}				
				}
			}
	}
	return leader;
	
}

int find_deputy_leader(int* neighbours, int leader, int rank, int size) {
	/*
		Alegerea liderului adjunct.
		(Vezi alegerea liderului)
	*/
	int i, nr_neigh, parent, candidate, tag = 10, deputy_leader;
	MPI_Status stat;
	
	if (rank == 0) {
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1) {
				MPI_Send(&rank,1, MPI_INT, i, tag, MPI_COMM_WORLD);
			}
		}
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1) {
				MPI_Recv(&candidate, 1, MPI_INT, i, tag, MPI_COMM_WORLD, &stat);
				if (deputy_leader > candidate && deputy_leader > leader)
					deputy_leader = candidate;
			}
		}
		// trimit liderul adjunct
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1) {
				MPI_Send(&deputy_leader,1, MPI_INT, i, tag, MPI_COMM_WORLD);
			}
		}
	}
	else {
		MPI_Recv(&parent, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &stat);
		for (i = 0; i < size; i++) {
			if (neighbours[i] == 1)
				nr_neigh++;			
		}
		if (nr_neigh == 1) {
			if (deputy_leader > parent && deputy_leader > parent)
					deputy_leader = parent;
			MPI_Send(&deputy_leader, 1, MPI_INT, parent, tag, MPI_COMM_WORLD);
		}		
		else {
			for (i = 0; i < size; i++) {
				if (i != parent) {
					if (neighbours[i] == 1) {
						MPI_Send(&rank, 1, MPI_INT, i, tag, MPI_COMM_WORLD);
					}				
				}
			}
			for (i = 0; i < size; i++) {
				if (i != parent)
					if (neighbours[i] == 1) {
						MPI_Recv(&candidate, 1, MPI_INT, i, tag, MPI_COMM_WORLD, &stat);
						if (deputy_leader > candidate && deputy_leader > leader)
							deputy_leader = candidate;
					}
			}
			MPI_Send(&deputy_leader, 1, MPI_INT, parent, tag, MPI_COMM_WORLD);
		}
		// primesc liderul adjunct și îl dau mai departe
		MPI_Recv(&deputy_leader, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &stat);
		if (nr_neigh != 1)
			for (i = 0; i < size; i++) {
				if (i != parent) {
					if (neighbours[i] == 1) {
						MPI_Send(&deputy_leader, 1, MPI_INT, i, tag, MPI_COMM_WORLD);
					}				
				}
			}
	}
	return deputy_leader;
	
}
