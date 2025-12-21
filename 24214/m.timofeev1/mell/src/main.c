#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include "shell.h"

char *infile, *outfile, *appfile;
struct command cmds[MAXCMDS];
char bkgrnd;

extern struct job jobs[MAXJOBS];
extern int njobs;
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

int main(int argc, char *argv[])
{
	char line[1024]; /*  allow large command lines  */
	int ncmds;
	char prompt[50];

	shell_terminal = STDIN_FILENO;
	shell_is_interactive = isatty(shell_terminal);

	if (shell_is_interactive)
	{
		shell_pgid = getpid();
		if (setpgid(shell_pgid, shell_pgid) < 0)
		{
			perror("setpgid");
			exit(1);
		}

		tcsetpgrp(shell_terminal, shell_pgid);
		tcgetattr(shell_terminal, &shell_tmodes);
	}

	init_signals();

	sprintf(prompt, "[%s] ", argv[0]);

	while (promptline(prompt, line, sizeof(line)) > 0)
	{
		cleanup_jobs();

		if ((ncmds = parseline(line)) <= 0)
			continue;

		int num_pipelines = 0;
		for (int i = 0; i < ncmds; i++)
		{
			int is_last = (i == ncmds - 1);
			int is_pipeline_end = is_last || !(cmds[i].cmdflag & OUTPIP);
			if (is_pipeline_end)
				num_pipelines++;
		}

		if (bkgrnd && num_pipelines > 1)
		{
			pid_t bg_pid = fork();
			if (bg_pid < 0)
			{
				perror("fork");
				continue;
			}
			else if (bg_pid == 0)
			{
				setpgid(0, 0);
				setup_background_signals();
				bkgrnd = 0;

				int cmd_start = 0;
				for (int i = 0; i < ncmds; i++)
				{
					int is_last = (i == ncmds - 1);
					int is_pipeline_end = is_last || !(cmds[i].cmdflag & OUTPIP);

					if (is_pipeline_end)
					{
						int pipeline_len = i - cmd_start + 1;

						if (is_builtin(cmds[cmd_start].cmdargs[0]))
						{
							execute_builtin(cmds[cmd_start].cmdargs[0], cmds[cmd_start].cmdargs);
						}
						else
						{
							struct command saved_cmds[MAXCMDS];
							memcpy(saved_cmds, cmds, sizeof(cmds));

							for (int j = 0; j < pipeline_len; j++)
							{
								cmds[j] = saved_cmds[cmd_start + j];
							}

							execute_pipeline(pipeline_len, line);

							memcpy(cmds, saved_cmds, sizeof(cmds));
						}

						cmd_start = i + 1;
					}
				}
				_exit(0);
			}
			else
			{
				setpgid(bg_pid, bg_pid);
				printf("[%d]\n", bg_pid);
				add_job(bg_pid, JOB_RUNNING, line);
			}
		}
		else
		{
			int cmd_start = 0;
			for (int i = 0; i < ncmds; i++)
			{
				int is_last = (i == ncmds - 1);
				int is_pipeline_end = is_last || !(cmds[i].cmdflag & OUTPIP);

				if (is_pipeline_end)
				{
					int pipeline_len = i - cmd_start + 1;

					if (is_builtin(cmds[cmd_start].cmdargs[0]))
					{
						execute_builtin(cmds[cmd_start].cmdargs[0], cmds[cmd_start].cmdargs);
					}
					else
					{
						struct command saved_cmds[MAXCMDS];
						memcpy(saved_cmds, cmds, sizeof(cmds));

						for (int j = 0; j < pipeline_len; j++)
						{
							cmds[j] = saved_cmds[cmd_start + j];
						}

						execute_pipeline(pipeline_len, line);

						memcpy(cmds, saved_cmds, sizeof(cmds));
					}

					cmd_start = i + 1;
				}
			}
		}
	}

	terminate_all_jobs();

	return 0;
}
