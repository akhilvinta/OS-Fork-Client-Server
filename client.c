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
    strm->zalloc = Z_NULL;
    strm->zfree = Z_NULL;
    strm->opaque = Z_NULL;
    if (deflateInit(strm, Z_DEFAULT_COMPRESSION) != Z_OK){
        fprintf(stderr, "compression failed\n");
    }
}

void init_decompress_stream(z_stream *strm)
{
    strm->zalloc = Z_NULL;
    strm->zfree = Z_NULL;
    strm->opaque = Z_NULL;
    if (inflateInit(strm) != Z_OK){
        fprintf(stderr, "decompression failed\n");
        exit(1);
    }
}


int client_connect(char *hostname, unsigned int port)
{
    /* e.g. host_name:”google.com”, port:80, return the socket for subsequent communication */
    int sockfd;
    struct sockaddr_in serv_addr; /* server addr and port info */
    struct hostent *server;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    server = gethostbyname(hostname);                                     /* convert host_name to IP addr */
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length); /* copy ip address from server to serv_addr */
    memset(serv_addr.sin_zero, '\0', sizeof serv_addr.sin_zero);          /* padding zeros*/
                                                                          //3. connect socket with corresponding address
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    return sockfd;
}

struct termios terminal_setup(void)
{ //add error handling eventually
    struct termios tmp;
    struct termios tmp2;
    tcgetattr(0, &tmp);
    tmp2 = tmp;
    tmp.c_iflag = ISTRIP, tmp.c_oflag = 0, tmp.c_lflag = 0;
    tcsetattr(0, TCSANOW, &tmp);
    return tmp2;
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
    static struct option longopts[]=
        {
            {"port", required_argument, NULL, 'p'},
            {"compress", no_argument, NULL, 'c'},
            {"log", required_argument, NULL, 'l'},
            {NULL, 0, NULL, 0}};

    char *port_number = NULL;
    bool compress_flag = false;
    int log_flag = false, log_fd = -1;
    while ((ch = getopt_long(argc, argv, "p:cl:", longopts, NULL)) != -1)
    {
        switch (ch)
        {
        case 'p':
            port_number = optarg;
            break;
        case 'c':
            compress_flag = true;

            break;
        case 'l':
            log_flag = true;
            log_fd = creat(optarg, 0666);
            check("Failed to create a file\n", log_fd);
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
    int sock_fd = client_connect("localhost", port_num);
    check("Failed to create socket between client and server\n", sock_fd);


    struct termios tmp2 = terminal_setup(); //open non-canonical mode babyyyyyyyyyyy

    struct pollfd fddesc[2];
    fddesc[0].fd = 0;
    fddesc[0].events = POLLIN + POLLHUP + POLLERR;
    fddesc[1].fd = sock_fd;
    fddesc[1].events = POLLIN + POLLHUP + POLLERR;

    while (1)
    {
        if (poll(fddesc, 2, 0) > 0)
        {
            int keyboard = fddesc[0].revents;
            int socket_to_server = fddesc[1].revents;

            if (keyboard == POLLIN)
            { //KEYBOARD INPUT -- READ FROM STDIN, WRITE TO STDOUT, WRITE TO sock_fd
                char input[256];
                int x;
                x = read(0, &input, 256);
                check("Failed to read input bytes\n", x);

                for (int i = 0; i < x; i++)
                {
                    if (*(input + i) == 0x04)
                    { //handle ^D case
                        status = write(1, "^D\r\n", 4);
                        check("Failed to write output bytes\n", status);

                        if (!compress_flag)
                        {
                            status = write(sock_fd, input + i, 1);
                            check("Failed to write output bytes\n", status);
                            if (log_flag)
                            {
                                dprintf(log_fd, "SENT %u bytes: ", 1);
                                write(log_fd, input + i, 1);
                                dprintf(log_fd, "\n");
                            }
                        }
                    }
                    else if (*(input + i) == 0x03)
                    { //handle ^C case
                        status = write(1, "^C", 1);
                        check("Failed to write output bytes\n", status);

                        if (!compress_flag)
                        {
                            status = write(sock_fd, input + i, 1);
                            check("Failed to write output bytes\n", status);

                            if (log_flag)
                            {
                                dprintf(log_fd, "SENT %u bytes: ", 1);
                                write(log_fd, input + i, 1);
                                dprintf(log_fd, "\n");
                            }
                        }
                    }
                    else if (*(input + i) == '\n' || *(input + i) == '\r')
                    { //handle \r or \n case
                        if (!compress_flag)
                        {
                            status = write(sock_fd, input + i, 1);
                            check("Failed to write output bytes\n", status);
                            if (log_flag)
                            {
                                dprintf(log_fd, "SENT %u bytes: ", 1);
                                status = write(log_fd, input + i, 1);
                                check("Failed to write output bytes\n", status);
                                dprintf(log_fd, "\n");
                            }
                        }
                        status = write(1, "\r\n", 2);
                        check("Failed to write output bytes\n", status);
                    }
                    else
                    { // handle general case
                        if (!compress_flag)
                        {
                            write(sock_fd, input + i, 1);
                            if (log_flag)
                            {
                                dprintf(log_fd, "SENT %u bytes: ", 1);
                                status = write(log_fd, input + i, 1);
                                check("Failed to write output bytes\n", status);
                                dprintf(log_fd, "\n");
                            }
                        }
                        status = write(1, input + i, 1);
                        check("Failed to write output bytes\n", status);

                    }
                }

                char buffer[1024];

                if (compress_flag)
                {
                    compression.avail_in = x;
                    compression.next_in = (unsigned char *)input;
                    compression.avail_out = 1024;
                    compression.next_out = (unsigned char *)buffer;
                    do{deflate(&compression, Z_SYNC_FLUSH);
                    } while (compression.avail_in > 0);

                    write(sock_fd, buffer, 1024 - compression.avail_out);
                    if (log_flag)
                    {
                        dprintf(log_fd, "SENT %u bytes: ", 1024 - compression.avail_out);
                        status = write(log_fd, buffer, 1024 - compression.avail_out);
                        check("Failed to write output bytes\n", status);
                        dprintf(log_fd, "\n");
                    }
                }
            }

            else if (socket_to_server == POLLIN)
            { //SOCKET INPUT -- read from socket_fd and write to STDOUT
                char output[256];
                int x = read(sock_fd, &output, 256);
                check("Failed to read input bytes\n", x);
                if (log_flag)
                {
                    dprintf(log_fd, "RECEIVED %u bytes: ", x);
                    status = write(log_fd, output, x);
                    check("Failed to write output bytes\n", status);
                    dprintf(log_fd, "\n");
                }
                if (x == 0)
                {
                    if (compress_flag)
                    {
                        inflateEnd(&decompression);
                        deflateEnd(&compression);
                    }
                    tcsetattr(0, TCSANOW, &tmp2);
                    exit(0);
                }

                if (compress_flag)
                {
                    char buffer[1024];
                    decompression.avail_in = x;
                    decompression.next_in = (unsigned char *)output;
                    decompression.avail_out = 1024;
                    decompression.next_out = (unsigned char *)buffer;
                    do {inflate(&decompression, Z_SYNC_FLUSH);
                    } while (decompression.avail_in > 0);
                
                    for (int i = 0; (unsigned int)i < 1024-decompression.avail_out; i++)
                    {
                        char temp;
                        temp = buffer[i];
                        if (temp == '\r' || temp == '\n')
                        {
                            status = write(1, "\r\n", 2);
                            check("Failed to write output bytes\n", status);
                        }
                        else{
                            status = write(1, &temp, 1);
                            check("Failed to write output bytes\n", status);
                        }
                    }
                }

                else
                {
                    for (int i = 0; i < x; i++){
                        if (*(output + i) == 0x04)
                        { //handle ^D case
                            status = write(1, "^D", 2);
                            check("Failed to write output bytes\n", status);
                            break;
                        }
                        else if (*(output + i) == '\n' || *(output + i) == '\r')
                        { //handle return or \n case
                            status = write(1, "\r\n", 2);
                            check("Failed to write output bytes\n", status);
                        }
                        else if (*(output + i) == 0x03)
                        {
                            status = write(1, "^C", 1);
                            check("Failed to write output bytes\n", status);
                        }
                        else{
                            status = write(1, output + i, 1);
                            check("Failed to write output bytes\n", status);
                        }
                    }
                }
            }
            else if (socket_to_server & POLLERR || socket_to_server & POLLHUP)
            {
                if (compress_flag){
                    deflateEnd(&compression);
                    inflateEnd(&decompression);
                }
                tcsetattr(0, TCSANOW, &tmp2);
                exit(0);
            }
        }
    }
}