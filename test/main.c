#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#define	BUFFSIZE	4096

void handle(int id)
{
	printf("handle signal: %d", id);
}

int
main(int argc, char **argv)
{
	int		n;
	char	buf[BUFFSIZE];
	printf("pid: %ld\n", getpid());

	struct sigaction action;
	struct sigaction old_action;
	memset(&action, sizeof(struct sigaction), 0);
	action.sa_handler = handle;
	if (argc > 1) {
		action.sa_flags |= SA_RESTART;
	}
	if (sigaction(SIGUSR1, &action, &old_action)) {
		perror("sigaction error: ");
	}



	while ((n = read(STDIN_FILENO, buf, BUFFSIZE)) > 0)
		if (write(STDOUT_FILENO, buf, n) != n)
			perror("write error");

	if (n < 0)
		perror("read error");

	if (sigaction(SIGUSR1, &old_action, &action)) {
		perror("sigaction error: ");
	}
	return 0;
}