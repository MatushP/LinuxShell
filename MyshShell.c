#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>

#define READ_FD 0
#define WRITE_FD 1
#define CL_ARG_LIMIT 5

#define MAX_CL_LENGTH 256
#define MAX_STR_LEN 20
#define MAX_COMMANDS 15

#define FALSE 0
#define TRUE 1

#define SPACE ' '
#define NEWLINE '\n'
#define BACKGROUND '&'
#define PIPELINE '|'
#define INPUT_REDIRECT '<'
#define OUTPUT_REDIRECT '>'

typedef struct
{
    char *arguments[CL_ARG_LIMIT];
    int read_fd;
    int write_fd;
    int start_arg;
    int end_arg;
} CL;

void initialize_CL(CL *cl);

int parse_input(char tokens[MAX_COMMANDS][MAX_STR_LEN]);
int check_special_char(char character);
void store_argument(char tokens[MAX_COMMANDS][MAX_STR_LEN], char buffer[], int *args, int *str_len, int *counter, int index);
void store_special_char(char tokens[MAX_COMMANDS][MAX_STR_LEN], char character, int *args);

void get_arguments(char tokens[MAX_COMMANDS][MAX_STR_LEN], CL *cl);
int get_args_count(char tokens[MAX_COMMANDS][MAX_STR_LEN]);
int search_special_char(char tokens[MAX_COMMANDS][MAX_STR_LEN], char symbol, int start, int end);

int convert_redirect_symbol(char redirect);
int open_file(char filepath[], int rw_flag);

void split_input_pipeline(CL *output_cl, CL *input_cl, int pipe_index, int args, int background);
int construct_pipeline(CL *output_cl, CL *input_cl);
int set_pipeline(int pipefd[], int rw_flag, CL *cl);
int set_io(int fd, int rw_flag, CL *cl);

int strlen(char string[]);

char read_error[] = "Failed to read character.\n";
char open_error[] = "Could not open file.\n";
char dup_error[] = "Failed to set standard input/output.\n";
char pipe_construct_error[] = "Failed to create pipe.\n";

int main()
{
    char tokens[MAX_COMMANDS][MAX_STR_LEN];
    CL output_cl;
    CL input_cl;

    int args, background = FALSE, pipeline = FALSE, pipe_index;
    int redirect_index [2];
    initialize_CL(&output_cl);
    initialize_CL(&input_cl);

    if (parse_input(tokens) > 0)
    {
        args = get_args_count(tokens);

        if (tokens[args - 1][0] == BACKGROUND)
            background = TRUE;

        pipe_index = search_special_char(tokens, PIPELINE, 0, args - 1);
        if (pipe_index != -1)
        {
            pipeline = TRUE;
            split_input_pipeline(&output_cl, &input_cl, pipe_index, args, background);

            // get_arguments(tokens, &output_cl);
            // get_arguments(tokens, &input_cl);
            // construct_pipeline(&output_cl, &input_cl); // handle error
        }

        // Input < A | B > output

    }
    else
    {
        //handle read error
    }

    exit(EXIT_SUCCESS);
}

void initialize_CL(CL *cl)
{
    for (int i = 0; i < CL_ARG_LIMIT; i++)
        cl->arguments[i] = NULL;
    cl->read_fd = READ_FD;
    cl->write_fd = WRITE_FD;
}

int parse_input(char tokens[MAX_COMMANDS][MAX_STR_LEN])
{
    char buffer[MAX_CL_LENGTH];
    int args = 0, str_len = 0, counter = 0, bytes_read;

    bytes_read = read(0, buffer, MAX_CL_LENGTH);
    if (bytes_read < 0)
        write(1, read_error, strlen(read_error));
    else
    {
        for (int i = 0; i < bytes_read; i++)
        {
            if (buffer[i] == NEWLINE || buffer[i] == SPACE)
            {
                if (str_len > 0)
                    store_argument(tokens, buffer, &args, &str_len, &counter, i);

                if (buffer[i] == NEWLINE)
                    break;
            }
            else if (check_special_char(buffer[i]) == 1)
            {
                if (str_len > 0)
                    store_argument(tokens, buffer, &args, &str_len, &counter, i);
                store_special_char(tokens, buffer[i], &args);
            }
            else
                str_len++;
        }
    }
    return bytes_read;
}

int check_special_char(char character)
{
    int type = 0;
    if (character == BACKGROUND || character == PIPELINE || character == OUTPUT_REDIRECT || character == INPUT_REDIRECT)
        type = 1;
    return type;
}

void store_argument(char tokens[MAX_COMMANDS][MAX_STR_LEN], char buffer[], int *args, int *str_len, int *counter, int index)
{
    while (*str_len > 0 && *counter < MAX_STR_LEN - 1)
    {
        tokens[*args][*counter] = buffer[index - *str_len];
        *str_len -= 1;
        *counter += 1;
    }

    if (*counter == MAX_STR_LEN - 1)
        *str_len = 0;

    *args += 1;
    *counter = 0;
}

void store_special_char(char tokens[MAX_COMMANDS][MAX_STR_LEN], char character, int *args)
{
    tokens[*args][0] = character;
    *args += 1;
}

void get_arguments(char tokens[MAX_COMMANDS][MAX_STR_LEN], CL *cl)
{
    int args = cl->end_arg - cl->start_arg + 1;

    for (int i = 0; i < args; i++)
        cl->arguments[i] = tokens[i + cl->start_arg];
}

int get_args_count(char tokens[MAX_COMMANDS][MAX_STR_LEN])
{
    int args = 0;

    while (tokens[args][0] != '\0')
        args++;

    return args;
}

int search_special_char(char tokens[MAX_COMMANDS][MAX_STR_LEN], char symbol, int start, int end)
{
    for (int i = start; i <= end; i++)
    {
        if (tokens[i][0] == symbol)
            return i;
    }

    return -1;
}

int convert_redirect_symbol(char redirect)
{
    if (redirect == '<')
        return READ_FD;
    else if (redirect == '>')
        return WRITE_FD;
}

int open_file(char filepath[], int rw_flag)
{
    int fd = -1;

    if (rw_flag == READ_FD)
        fd = open(filepath, O_RDONLY);
    else if (rw_flag == WRITE_FD)
        fd = open(filepath, O_WRONLY);

    if (fd < 0)
        write(1, open_error, strlen(open_error));

    return fd;
}

void split_input_pipeline(CL *output_cl, CL *input_cl, int pipe_index, int args, int background)
{
    output_cl->start_arg = 0;
    output_cl->end_arg = pipe_index - 1;

    input_cl->start_arg = pipe_index + 1;
    input_cl->end_arg = args - 1 - background;
}

int construct_pipeline(CL *output_cl, CL *input_cl)
{
    int pipefd[2];
    int error = pipe(pipefd), pid_output, pid_input;

    if (error == -1)
        write(1, pipe_construct_error, strlen(pipe_construct_error));
    else
    {
        pid_output = fork();
        if (pid_output == 0)
        {
            set_pipeline(pipefd, WRITE_FD, output_cl);
            execvp(output_cl->arguments[0], output_cl->arguments);
        }
        close(pipefd[WRITE_FD]);

        pid_input = fork();
        if (pid_input == 0)
        {
            set_pipeline(pipefd, READ_FD, input_cl);
            execvp(input_cl->arguments[0], input_cl->arguments);
        }
        close(pipefd[READ_FD]);
    }

    return error;
}

int set_pipeline(int pipefd[], int rw_flag, CL *cl)
{
    int new_fd;

    if (rw_flag == READ_FD)
    {
        close(pipefd[WRITE_FD]);
        new_fd = set_io(pipefd[READ_FD], READ_FD, cl);
    }
    else if (rw_flag == WRITE_FD)
    {
        close(pipefd[READ_FD]);
        new_fd = set_io(pipefd[WRITE_FD], WRITE_FD, cl);
    }

    return new_fd;
}

int set_io(int fd, int rw_flag, CL *cl)
{
    int new_fd = -1;

    if (rw_flag == READ_FD)
        new_fd = dup2(fd, cl->read_fd);
    else if (rw_flag == WRITE_FD)
        new_fd = dup2(fd, cl->write_fd);

    if (new_fd < 0)
        write(1, dup_error, strlen(dup_error));

    return new_fd;
}

int strlen(char string[])
{
    int count = 0;

    while (string[count] != '\0')
    {
        count++;
    }

    return count;
}
