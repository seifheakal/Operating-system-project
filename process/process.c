#include "headers.h"

void continue_handler(int);

int last_update_time;

int main(int argc, char** argv) {
	int remainingTime = atoi(argv[1]);

	printf("[Process] %d started rt=%d\n", getpid(), remainingTime);

	// attach signals
	signal(SIGCONT, continue_handler);

	initClk();

	// stay paused
	raise(SIGTSTP);

	printf("[Process] %d initial wake up\n", getpid());

	// assign prevtime to clk
	last_update_time = getClk();

	while (remainingTime > 0) {
		int now = getClk();
		if (now - last_update_time > 0) {
			printf("PROC PID=%d delta=%d left=%d\n", getpid(), now - last_update_time, remainingTime);

			last_update_time = now;

			remainingTime--;

			// notify scheduler of decrement
			kill(getppid(), SIGUSR2);
		}

		// sleep for a bit
		usleep(200 * 1000);
	}

	destroyClk(false);
	printf("Process %d finished\n", getpid());

	// notify scheduler
	kill(getppid(), SIGUSR1);

	return 0;
}

void continue_handler(int signum)
{
	printf("PROC %d received cont\n", getpid());
	last_update_time = getClk();
}