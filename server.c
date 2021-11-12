//Akhil Vinta
//akhil.vinta@gmail.com
//405288527

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>
#include <assert.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <zlib.h>

void init_compress_stream(z_stream *strm)
{
    strm->opaque = Z_NULL;
    strm->zalloc = Z_NULL;
    strm->zfree = Z_NULL;
    if (deflateInit(strm, Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        fprintf(stderr, "compression failed\n");
    }
}

void init_decompress_stream(z_stream *strm)
{
    strm->opaque = Z_NULL;
    strm->zalloc = Z_NULL;
    strm->zfree = Z_NULL;
    if (inflateInit(strm) != Z_OK)
    {
        fprintf(stderr, "decompression failed\n");
        exit(1);
    }
}


int server_connect(unsigned int port_num)
{
    int sockfd, new_fd;            /* listen on sock_fd, new connection on new_fd */
    struct sockaddr_in my_addr;    /* my address */
    struct sockaddr_in their_addr; /* connector addr */
    int sin_size;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    /* set the address info */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port_num); /* short, network byte order */
    my_addr.sin_addr.s_addr = INADDR_ANY;

    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero)); //padding zeros
    bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));
    listen(sockfd, 5); /* maximum 5 pending connections */
    sin_size = sizeof(struct sockaddr_in);
    /* wait for client’s connection, their_addr stores client’s address */
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, (socklen_t *)&sin_size);
    return new_fd; /* new_fd is returned not sock_fd*/
}

void check(char *goat, int status){
    if(status < 0){
        fprintf(stderr, "%s", goat);
        exit(1);
    }
    return;
}


z_stream compression;
z_stream decompression;
int status; 

int main(int argc, char *argv[])
{

    int ch;
    static struct option longopts[] =
        {
            {"port", required_argument, NULL, 'p'},
            {"compress", no_argument, NULL, 'c'},
            {NULL, 0, NULL, 0}};

    char *port_number = NULL;
    bool compress_flag = false;
    while ((ch = getopt_long(argc, argv, "p:c", longopts, NULL)) != -1)
    {
        switch (ch)
        {
        case 'p':
            port_number = optarg;
            break;
        case 'c':
            compress_flag = true;
            break;
        default:
            fprintf(stderr, "Incorrect parameter has been passed. Potential options: --port=port_number --compress --log=filename\n");
            exit(1);
        }
    }

    if(compress_flag){
        init_compress_stream(&compression);
        init_decompress_stream(&decompression);
    }

    if (port_number == NULL)
    {
        fprintf(stderr, "Must specify command line argument in the format \"port=wxyz\"\n");
        exit(1);
    }

    int port_num = atoi(port_number);
    int sock_fd = server_connect(port_num);
    check("Failed to create socket between client and server\n", sock_fd);

    int pipe_to_shell[2];
    status= pipe(pipe_to_shell);
    check("failed to create pipe from parent to shell", status);
    int pipe_from_shell[2];
    status = pipe(pipe_from_shell);
    check("failed to create pipe from shell to parent", status);


    int process_id = fork();
    check("failed to fork and create a new thread", process_id);

    if (process_id == 0)
    {
        status = close(pipe_to_shell[1]);
        check("failed to close pipe", status);

        status = close(pipe_from_shell[0]);
        check("failed to close pipe", status);

        //make pipe_to_shell[0] the stdin that command line writes to.
        status = close(0);
        check("failed to close fd 0", status);
        status = dup(pipe_to_shell[0]);
        check("failed to close pipe", status);
        status = close(pipe_to_shell[0]);
        check("failed to close pipe", status);
        //make pipe_from_shell[1] stdout that execlp writes to
        status = close(1);
        check("failed to close fd 1", status);
        status = dup(pipe_from_shell[1]);
        check("failed to close pipe", status);
        status = close(pipe_from_shell[1]);
        check("failed to close pipe", status);
        status = dup2(1, 2);
        check("failed to dup fd 1 and 2", status);

        //execlp("ls", "ls", "-a", "-l", NULL);
        status = execlp("/bin/bash", "bash", NULL);
        check("failed to exec new thread", status);
    }
    else
    {

        status = close(pipe_to_shell[0]);
        check("failed to close pipe", status);
        status = close(pipe_from_shell[1]);
        check("failed to close pipe", status);

        struct pollfd fddesc[] = {
            {sock_fd, POLLIN, 0},
            {pipe_from_shell[0], POLLIN, 0}};

        bool should_exit = false;
        while (!should_exit)
        {

            if (poll(fddesc, 2, 0) > 0)
            {
                int socket_from_client = fddesc[0].revents;
                int shell = fddesc[1].revents;

                if (socket_from_client == POLLIN)
                { //READ FROM sock_fd, WRITE TO pipe_to_shell[1]
                    //write(1,"skrt",4);

                    char input[256];
                    int x;
                    x = read(sock_fd, &input, 256);
                    check("failed to read input bytes", x);
                    if (compress_flag)
                    {
                        //decompress
                        // printf("in the compress if statement\n");

                        char buffer[1024];
                        decompression.avail_in = x;
                        decompression.next_in = (unsigned char *)input;
                        decompression.avail_out = 1024;
                        decompression.next_out = (unsigned char *)buffer;
                        do{ inflate(&decompression, Z_SYNC_FLUSH);
                        } while (decompression.avail_in > 0);

                        for (unsigned int i = 0; i < 1024 - decompression.avail_out; i++)
                        {
                            char temp;
                            temp = buffer[i];
                            if (temp == 0x04)
                            {
                                status = close(pipe_to_shell[1]);
                                check("failed to close pipe", status);
                                should_exit = true;
                                break;
                            }
                            else if (temp == '\r' || temp == '\n')
                            {
                                //char shellnewline[2] = {'\n', '\0'};
                                status = write(pipe_to_shell[1], "\n", 1);
                                check("failed to write bytes", status);
                            }
                            else
                            {
                                status = write(pipe_to_shell[1], &temp, 1);
                                check("failed to write bytes", status);
                            }
                        }
                    }

                    else
                    {

                        for (int i = 0; i < x; i++)
                        {
                            if (*(input + i) == 0x04)
                            { //handle ^D case
                                status = close(pipe_to_shell[1]);
                                check("failed to close pipe", status);
                                should_exit = true;
                                break;
                            }
                            if (should_exit)
                                break;
                            else if (*(input + i) == 0x03)
                            {
                                status = kill(process_id, SIGINT);
                                check("failed to kill child process with ^C signal", status);
                            }
                            else if (*(input + i) == '\n' || *(input + i) == '\r')
                            { //handle \r or \n case
                                status = write(pipe_to_shell[1], "\n", 2);
                                check("failed to write bytes", status);
                            }
                            else
                            {
                                status = write(pipe_to_shell[1], input + i, 1);
                                check("failed to write bytes", status);
                            }
                        }
                    }
                }
                else if (shell == POLLIN)
                { //SHELL INPUT -- read from pipe_from_shell[0] and write to sock_fd
                    char output[256];
                    int x = read(pipe_from_shell[0], &output, 256);
                    check("failed to read bytes", x);
                    if (compress_flag)
                    {
                        //compress
                        char buffer[256];
                        compression.avail_in = x;
                        compression.next_in = (unsigned char *)output;
                        compression.avail_out = 256;
                        compression.next_out = (unsigned char *)buffer;
                        do {deflate(&compression, Z_SYNC_FLUSH);
                        } while (compression.avail_in > 0);
                        status = write(sock_fd, buffer, 256 - compression.avail_out);
                        check("failed to write bytes", status);
                    }

                    else
                    {
                        for (int i = 0; i < x; i++)
                        {
                            if (*(output + i) == 0x04)
                            { //handle ^D case
                                status = write(sock_fd, "^D", 2);
                                check("failed to write bytes", status);
                                status = close(sock_fd);
                                check("failed to close socket", status);
                                break;
                            }
                            else{
                                status = write(sock_fd, output + i, 1);
                                check("failed to write bytes", status);
                            }
                        }
                    }
                }
                else if (socket_from_client == POLLHUP || socket_from_client == POLLERR)
                {
                    status = close(pipe_to_shell[1]);
                    check("failed to close pipe", status);
                }
                else if (shell & POLLHUP || shell & POLLERR)
                {
                    int status = 0;
                    status= close(pipe_from_shell[0]);
                    check("failed to close pipe", status);
                    status = close(sock_fd);
                    check("failed to close socket", status);
                    waitpid(process_id, &status, 0);
                    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
                    exit(0);
                }
            }
        }
    }
}