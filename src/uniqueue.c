#include <stdio.h> /* printf, fgets */
#include <unistd.h> /* getopt, fork */
#include <stdlib.h> /* malloc */
#include <string.h> /* strlen, strcmp */
#include <fcntl.h> /* fcntl */
#include <signal.h> /* signal, SIGIO */
#include <sys/wait.h> /* waitpid */
int verbose = 10;	// Verbosity level

typedef struct {
	char *record_name;
	int is_running;
	int is_pending;
} uniq_record;

typedef struct {
	int alloc;
	int number;
	uniq_record *records;
} record_list;

record_list rl;		// Global record list

void InitRecordList(record_list *l) {
	l->alloc = 4;
	l->number = 0;
	l->records = malloc(l->alloc * sizeof(uniq_record));
}

int SearchRecordList(record_list *l, void *record, int (*func)(uniq_record *, void *)) {
	int i;
	uniq_record *cur_record;
	for ( i = 0; i < l->number; i++ ) {
		cur_record = &l->records[i];
		if ( func(&l->records[i], record) == 0 )
			return i;
	}
	return -1;
}

int CompareRecordName(uniq_record *record, void *subject) {
	(char *) subject;
	return strcmp(record->record_name, subject);
}

int CompareRecordPID(uniq_record *record, void *subject) {
	int minus = *(int *)subject;
	return (record->is_running - minus);
}

int AddToRecordList(record_list *l, char *record) {
	if ( l->alloc == l->number ) {
		// Must grow
		l->alloc = l->alloc * 2;
		l->records = realloc(l->records, l->alloc * sizeof(uniq_record));
	}

	uniq_record *r = &l->records[l->number];
	r->record_name = strdup(record);
	r->is_running = 0;
	r->is_pending = 1;

	l->number++;
	return 0;
}

int usage(char *prog_name) {
	printf("Usage: %s [ -v ] < -e executable >", prog_name);
	return 0;
}
/*
int log(int level, char *message) {
	if ( level <= verbose ) {
		printf("%s\n", message);
	}
	return 0;
}
*/

int main(int argc, char **argv) {
	pid_t cpid, pid = getpid();
	printf("%s starting with pid: %d\n", argv[0], pid);
	printf("%s called with %d arguments\n", argv[0], argc-1);

	int opt;
	char *executable = NULL;
	InitRecordList(&rl);	// Initialise global record list

	while ( (opt = getopt(argc, argv, "ve:")) != -1 ) {
		switch (opt) {
		case 'e':
			executable = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage(argv[0]);
		}
	}

	printf("executable: %s, verbosity: %d\n", executable, verbose);



	int i;
//	uniq_record r;
	char *line = malloc(1024);	// Pointer to input line buffer
	uniq_record r;			// Local record
	uniq_record *pr;		// Local record pointer
	int running = 1;
	int children = 0;
	int nrb, j;
//	printf("caught signal: %d\n", sig);
	while (running) {

		if ( fgets(line, 1024, stdin) ) {
			// We've got more input
			fcntl(0, F_SETFL, O_NONBLOCK);

			if ( line[strlen(line) - 1] == '\xa' )
				// Clean the newline char off
				line[strlen(line) - 1] = '\0';

			// Compare line to existing records (if any)
			if ( rl.number != 0 &&
			( j = SearchRecordList(&rl, line, CompareRecordName) ) != -1 ) {
				// A match was found
/*				r = *pr;	// Local record is assigned
						// contents of record pointed
						// to by pr
				r.is_pending = 1;	// Set flag
				memcpy(pr, &r, sizeof(uniq_record));
*/
				rl.records[j].is_pending = 1;
			} else {
				// This is a new unique record
				AddToRecordList(&rl, line);
			}
		} // end of new input

//printf("Running\tPending\tName\n");
		for ( i = 0; i < rl.number; i++ ) {
			// Loop through the records
			r = rl.records[i];

/*			if ( r.is_pending == 0 && r.is_running == 0 )
				// Tidy up
				//DeleteFromRecordsList(&rl, i);
				printf("\n");
*/

//printf("Records: %d\tRecord: %d\tIs pending: %d\tIs running: %d\tName: %s\n", rl.number, i, r.is_pending, r.is_running, r.record_name);
			if ( r.is_pending == 1 && r.is_running == 0 ) {
				// Fork off
				cpid = fork();
				if ( cpid == 0 ) {
					// This is the child
					pid = getpid();
					printf("I am a new child at pid: %d\n", pid);
					char command[1024];
					sprintf(command, "%s %s", executable, r.record_name);
					int ret = system(command);
					exit(ret);
				} else if ( cpid == -1 ) {
					// Fork failed
					fprintf(stderr, "Fork failed\n");
					break;
				}
				printf("I had a baby: %d\n", cpid);
				children++;
				r.is_pending = 0;
				r.is_running = cpid;
				rl.records[i] = r;
			}
//printf("%d\t%d\t%s\n", r.is_running, r.is_pending, r.record_name);
			if ( children > 0 ) {
				int child_status;
				cpid = waitpid(-1, &child_status, WNOHANG);
				switch ( cpid ) {
				case 0:
					// Children, but not finnished
					break;
				case -1:
					// No Children
					break;
				default:
					printf("Child %d returned with status: %d\n", cpid, child_status);
					j = SearchRecordList(&rl, &cpid, CompareRecordPID);
					if ( j != -1 )
						rl.records[j].is_running = 0;
					children--;
					break;
				}
			} else {
				fcntl(0, F_SETFL, !O_NONBLOCK);
			}
		}
	}














	return 0;
}
