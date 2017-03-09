#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> /* memset() */
#include <sys/time.h> /* select() */ 
#include <stdlib.h>
#include <stdbool.h>


#define REMOTE_SERVER_PORT 1500
#define BROADCAST_INTERVAL 5

char BROADCAST_PACKET[] = {'S', 'N', 0xff, 0xff, 0};

const char * BROADCAST_ADDR = "255.255.255.255";

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "error opening %s\n", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    fprintf(stderr, "error reading %s\n", filename);
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  uint16_t sbp_sender_id = 0xFFFF;
  char sbp_sender_id_string[32];
  if (file_read_string("/cfg/sbp_sender_id", sbp_sender_id_string,
                        sizeof(sbp_sender_id_string)) == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }

  memcpy(BROADCAST_PACKET + 2, &sbp_sender_id, 2);
  
  int sd, rc, i;
  struct sockaddr_in cliAddr, remoteServAddr;
  struct hostent *h;
  int broadcast = 1;

  /* get server IP address (no check if input is IP address or DNS name */
  h = gethostbyname(BROADCAST_ADDR);
  if(h==NULL) {
    printf("%s: unknown host '%s' \n", argv[0], BROADCAST_ADDR);
    exit(1);
  }

//   printf("%s: sending data to '%s' (IP : %s) \n", argv[0], h->h_name,
// 	 inet_ntoa(*(struct in_addr *)h->h_addr_list[0]));

  remoteServAddr.sin_family = h->h_addrtype;
  memcpy((char *) &remoteServAddr.sin_addr.s_addr, 
	 h->h_addr_list[0], h->h_length);
  remoteServAddr.sin_port = htons(REMOTE_SERVER_PORT);

  /* socket creation */
  sd = socket(AF_INET,SOCK_DGRAM,0);
  if(sd<0) {
    printf("%s: cannot open socket \n",argv[0]);
    exit(1);
  }
  
  
  if (setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &broadcast,sizeof broadcast) == -1) {
          perror("setsockopt (SO_BROADCAST)");
          exit(1);
  }
  
  /* bind any port */
  cliAddr.sin_family = AF_INET;
  cliAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  cliAddr.sin_port = htons(0);
  
  rc = bind(sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
  if(rc<0) {
    printf("%s: cannot bind port\n", argv[0]);
    exit(1);
  }


  /* send data */
  while(1) {
    rc = sendto(sd, BROADCAST_PACKET, sizeof(BROADCAST_PACKET), 0, 
		(struct sockaddr *) &remoteServAddr, 
		sizeof(remoteServAddr));

    if(rc<0) {
      printf("%s: cannot send data\n", argv[0]);
      close(sd);
      exit(1);
    }
    sleep(BROADCAST_INTERVAL);
  }
  
  return 1;

}