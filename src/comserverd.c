#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>

//using namespace std;

#define DEAMON_COMSERVER 1
#ifdef DEAMON_COMSERVER
    #undef NO_DEAMON_COMSERVER
#endif

typedef enum SANATIZE_LEVEL {
    SAN_DEFAULT = 0,
    SAN_NUMERAL,
	SAN_PATH
}SANATIZE_LEVEL;

typedef SANATIZE_LEVEL _SANATIZE_LEVEL;
//TODO special group for different goals (e.g. portnumber, encrypt code, etc


void        sanatize_string(char *string,SANATIZE_LEVEL level);
int         open_port(char *portname);
void        setBaudrate(int fd_tty);
void        create_key(char *keypart, uint64_t m_key);
void        build_decoding_table();
int         base64_decode(const char *data, int input_length, char *decoded_data);

static char *decoding_table = NULL;
static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};

#define MAX_BUFFER 1024
int main(int argc, char *argv[])
{
	if (argc !=6){
        printf("\rSyntax: \n\t\tCOMx path\n\t\tHostname\n\t\tPort\n\t\tHigh part of key\n\t\tLow part of key\n\t\tUDP PORT\n\nLicense GPLv2 Based on Qt Example\n\n");
        exit(1);
    }
#ifdef DEAMON_COMSERVER
    pid_t sid, pid = fork();

    if (pid < 0)
        return(EXIT_FAILURE);

    if (pid > 0)
        return(pid);

    sid = setsid();
    if (sid < 0)
        return(EXIT_FAILURE);
#endif
    int             fd_tty=-1;   //tty
    int             fd_tty_udp, n;  //udp-socket
    int             lastChar = 0;
    int             decodedsize = 0;
    int             namesize;
    unsigned int    udp_port = 45454;
    char           *portname;
    bool            same;
    bool            keep_Alive=true;
    char            m_keypart[8];
    char           *name;
    char            charbuff = '\n';
    socklen_t       len;
    struct sockaddr_in saddr, caddr;
    struct timeval  tv;

    char *line        = (char *)malloc(MAX_BUFFER);
    char *decodedline = (char *)malloc(MAX_BUFFER);

    if (argc ==6){
        uint64_t        key;
        portname =      argv[1];
            sanatize_string(portname,SAN_PATH);
        name =          argv[2];
            sanatize_string(name, SAN_DEFAULT);
        namesize =      strlen(name);
        udp_port =      strtoul(argv[5],NULL,10);
        key =           strtoul(argv[3],NULL,16);
        key =           (key << 32) + strtoul(argv[4],NULL,16);
        create_key      (m_keypart, key);
    }
	else
	{
		exit(1);
	}
	fd_tty_udp = socket(AF_INET, SOCK_DGRAM, 0);

    tv.tv_sec = 5;  /* 30 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors

    setsockopt(fd_tty_udp, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(udp_port);

    bind(fd_tty_udp, (struct sockaddr *)&saddr, sizeof(saddr));
    printf("Server running\n");
    while ( keep_Alive ) {          //tty loop, failback temporary tty interuption
        close(fd_tty);
        fd_tty = open_port(portname);
        if (fd_tty == -1){
            sleep(1);
            continue;
        }
        setBaudrate(fd_tty);
        printf("Com connected\n");
        while(1==1) {               //data loop, get, decode, write.
            fsync( fd_tty );
            decodedsize = 0;
            for (int iTemp = 0; iTemp !=MAX_BUFFER; iTemp++){ //cleans data, hope this doens't get optimzed away. TODO, check if this is kept
                decodedline[iTemp] = '\0';
                line[iTemp] = '\0';
            }
            len=sizeof(caddr);
            n=recvfrom(fd_tty_udp, line, MAX_BUFFER-2, 0, (struct sockaddr *)&caddr, &len); //Minus 2 to force end of line and string
            if (n==-1){     //debug,
                charbuff = '.';
                if (write(fd_tty, &charbuff, 1)== -1) break;
                continue;                       //no data loop, write '.' to keep uart alive.
            }
            line[n]='\n';                           //buffer transmit, never higher then max_buffer-1. (Allways closed with \n\0) TODO check if this is true
            fputs(line, stdout);

            if (n < 20)
                continue; //size / 4 is aprox. data size
            decodedsize = base64_decode(line,n,decodedline) -1;

            if (decodedsize < 6)
                continue;                             //go back to data loop

            if (fd_tty == -1){
                printf("connection lost\n");
                break;                                 //go back to connect loop
            }

            lastChar = 0;
            for (int pos = 2;pos < decodedsize;pos++){
                char currentChar = decodedline[pos];
                decodedline[pos] = decodedline[pos] ^ lastChar ^ m_keypart[((pos-2) % 8)];
                lastChar = currentChar;
            }
            decodedline[decodedsize]='\n';
            fputs(decodedline, stdout);
            same=false;
            printf("Checking port status:\t");
            if (access(portname, F_OK) != 0){
                printf("Device not found, USB disconnected?\n");
                break;              //back to TTY loop
            }
            else
                printf("Ok!\n");

            printf("Processing UDP Packet\n");
            for (int iTemp = 0; iTemp < decodedsize-1 ;iTemp++)
            {
                if (decodedline[iTemp]=='|'){
                    fputs(decodedline+iTemp+1, stdout);
                    same=true;
                    for (int jTemp  = 0; jTemp == iTemp; jTemp++) //Scan to --> | reverse order compare hostname <-- if true write everything after | --> uart
                    {
                        if (jTemp >= namesize) break;
                        if (decodedline[iTemp-jTemp]!=name[namesize-jTemp]){
                            same=false;
                            break;
                        }
                    }
                }
                else if(same){
					printf("Write serial command:\n");
                    if ( write(fd_tty, decodedline + iTemp, (decodedsize -1)-iTemp)==-1) break; //go back to connect loop
					if ( write(1, decodedline + iTemp, (decodedsize -1)-iTemp)==-1) break; //go back to connect loop
					break;
                }
            }
            charbuff = '\n';		//force buffer sync, recheck if connection still open.
            if (write(fd_tty, &charbuff, 1)== -1) break; //go back to connect loop
       }
    }
    close (fd_tty);
    free(line);
    free(decodedline);
    free(decoding_table);
    return 0;
}

void sanatize_string(char *data,SANATIZE_LEVEL level) //prevent format string attacks (since its a network application.....
{
    while(*data != '\0'){
        switch ((int)*data)
        {
        case '.':
        case '/':
		case '~':
        if (level == SAN_PATH )
            break;
		//blacklist
        case '!':
        case '\"':
        case '#':
        case '$':
        case '%':
        case '&':
        case '\'':
        case '(':
        case ')':
        case '*':
        case ',':
        case ':':
        case ';':
        case '<':
        case '=':
        case '>':
        case '?':
        case '@':
        case '[':
        case '\\':
        case ']':
        case '^':
        case '_':
        case '`':
        case '{':
        case '|':
        case '}':
            if (level == SAN_NUMERAL )
                *data = '0';
            else
                *data = ' ';
            break;
        //graylist
#ifndef ALLOW_POSNEG_KEY
        case '-':
        case '+':
            if (level != SAN_NUMERAL )
                *data = '0';
            else
                *data = ' ';
            break;
#endif
        //every thing else
        default: break;
        }
        data+=1;
    }
}
int open_port(char *portname)
{
    int fd_tty = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_tty == -1)
        perror("open_port: Unable to open serial port, i'll try again.\n");
    else
        fcntl(fd_tty, F_SETFL, 0);
    return (fd_tty);
}

void setBaudrate(int fd_tty)
{
    struct termios options;

    tcgetattr(fd_tty, &options);

    cfsetispeed(&options, B38400);
    cfsetospeed(&options, B38400);

    options.c_cflag |= (CLOCAL | CREAD);

    options.c_cflag &= ~CSIZE; /* Mask the character size bits */
    options.c_cflag |= CS8;    /* Select 8 data bits */

    tcsetattr(fd_tty, TCSANOW, &options);
}


void create_key(char *keypart,uint64_t m_key){
    for (int i=0;i<8;i++) {
        uint64_t part = m_key;
        for (int j=i; j>0; j--)
            part = part >> 8;
        part = part & 0xff;
        keypart[i] = (char)(part);
    }
}

int base64_decode(const char *data,
                  int input_length,
                  char* decoded_data) {
    if (decoding_table == NULL)
        build_decoding_table();
    if (decoded_data == NULL)
        return 0;

    if (input_length % 4 != 0)
        return 0;

    int output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=')
        (output_length)--;
    if (data[input_length - 2] == '=')
        (output_length)--;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[ (unsigned int) data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[ (unsigned int) data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[ (unsigned int) data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[ (unsigned int) data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }
    return (input_length / 4 * 3);
}

void build_decoding_table() {
    decoding_table = (char* )malloc(256);

    for (int i = 0; i < 64; i++)
        decoding_table[(unsigned char) encoding_table[i]] = i;
}

