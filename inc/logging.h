/* Do not modify this file */
#ifndef LOGGING_H
#define LOGGING_H

#define LOG_FG      0
#define LOG_BG      1

#define LOG_STATE_READY      0
#define LOG_STATE_RUN_FG     1
#define LOG_STATE_RUN_BG     2
#define LOG_STATE_SUSPENDED  3
#define LOG_STATE_FINISHED   4
#define LOG_STATE_KILLED     5

#define STATE_READY      0
#define STATE_RUN_FG     1
#define STATE_RUN_BG     2
#define STATE_SUSPENDED  3
#define STATE_FINISHED   4
#define STATE_KILLED     5

#define LOG_CMD_SUSPEND 0
#define LOG_CMD_RESUME  1
#define LOG_CMD_KILL  2

#define LOG_FILE_PIPE "(pipe)"

#define LOG_REDIR_IN   0
#define LOG_REDIR_OUT  1

/* Basic messages */
void log_hiy_intro();
void log_hiy_prompt();
void log_hiy_help();
void log_hiy_quit();

/* Task information and management */
void log_hiy_task_init(int task_num, const char *cmd);
void log_hiy_num_tasks(int num_tasks);
void log_hiy_task_info(int task_num, const char* cmd, int status, int pid, int exit_code);
void log_hiy_delete(int task_num);
void log_hiy_status(int task_num, const char *cmd, int pid, int from, int to);

/* Redirection */
void log_hiy_redir(int task_num, int redir_type, const char *file);
void log_hiy_pipe(int task_num1, int task_num2);

/* Errors */
void log_hiy_task_num_error(int task_num);
void log_hiy_status_error(int task_num, int status);
void log_hiy_start_error(const char *line);
void log_hiy_file_error(int task_num, const char *file);
void log_hiy_pipe_error(int task_num);

/* Signals */
void log_hiy_sig_sent(int sig_type, int task_num, int pid);
void log_hiy_ctrl_c();
void log_hiy_ctrl_z();
void log_hiy_ctrl_bs();

#endif /*LOGGING_H*/
