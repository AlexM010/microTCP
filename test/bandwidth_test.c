/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <time.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../lib/microtcp.h"

#define CHUNK_SIZE 4096

static inline void
print_statistics (ssize_t received, struct timespec start, struct timespec end)
{
  double elapsed = end.tv_sec - start.tv_sec
      + (end.tv_nsec - start.tv_nsec) * 1e-9;
  double megabytes = received / (1024.0 * 1024.0);
  printf ("Data received: %f MB\n", megabytes);
  printf ("Transfer time: %f seconds\n", elapsed);
  printf ("Throughput achieved: %f MB/s\n", megabytes / elapsed);
}

int
server_tcp (uint16_t listen_port, const char *file)
{
  uint8_t *buffer;
  FILE *fp;
  int sock;
  int accepted;
  ssize_t received;
  ssize_t written;
  ssize_t total_bytes = 0;
  socklen_t client_addr_len;

  struct sockaddr_in sin;
  struct sockaddr client_addr;
  struct timespec start_time;
  struct timespec end_time;

  /* Allocate memory for the application receive buffer */
  buffer = (uint8_t *) malloc (CHUNK_SIZE);
  if (!buffer) {
    perror ("Allocate application receive buffer");
    return -EXIT_FAILURE;
  }

  /* Open the file for writing the data from the network */
  fp = fopen (file, "w");
  if (!fp) {
    perror ("Open file for writing");
    free (buffer);
    return -EXIT_FAILURE;
  }

  if ((sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    perror ("Opening TCP socket");
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }

  memset (&sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_port = htons (listen_port);
  /* Bind to all available network interfaces */
  sin.sin_addr.s_addr = INADDR_ANY;

  if (bind (sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) == -1) {
    perror ("TCP bind");
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }

  if (listen (sock, 1000) == -1) {
    perror ("TCP listen");
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }

  /* Accept a connection from the client */
  client_addr_len = sizeof(struct sockaddr);
  accepted = accept (sock, &client_addr, &client_addr_len);
  if (accepted < 0) {
    perror ("TCP accept");
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }

  /*
   * Start processing the received data.
   *
   * Also start measuring time. Not the most accurate measurement, but
   * it is a good starting point.
   *
   * At hy-435 we deal with bandwidth measurements software in a more
   * right and careful way :-)
   */

  clock_gettime (CLOCK_MONOTONIC_RAW, &start_time);
  while ((received = recv (accepted, buffer, CHUNK_SIZE, 0)) > 0) {
    written = fwrite (buffer, sizeof(uint8_t), received, fp);
    total_bytes += received;
    if (written * sizeof(uint8_t) != (size_t)received) {
      printf ("Failed to write to the file the"
              " amount of data received from the network.\n");
      shutdown (accepted, SHUT_RDWR);
      shutdown (sock, SHUT_RDWR);
      close (accepted);
      close (sock);
      free (buffer);
      fclose (fp);
      return -EXIT_FAILURE;
    }
  }
  clock_gettime (CLOCK_MONOTONIC_RAW, &end_time);
  print_statistics (total_bytes, start_time, end_time);

  shutdown (accepted, SHUT_RDWR);
  shutdown (sock, SHUT_RDWR);
  close (accepted);
  close (sock);
  fclose (fp);
  free (buffer);

  return 0;
}

int
server_microtcp (uint16_t listen_port, const char *file)
{
<<<<<<< HEAD
  /*TODO: Write your code here */
  uint8_t *buffer;
  FILE *fp;
  int sock;
  int accepted;
  ssize_t received;
  ssize_t written;
  ssize_t total_bytes = 0;
  socklen_t client_addr_len;
  microtcp_sock_t socket;
  int domain=AF_INET;
  int type=SOCK_DGRAM;
  int protocol=0;
  socket=microtcp_socket(domain,type,protocol);
=======
  uint8_t *buffer;
  FILE *fp;
  microtcp_sock_t sock;
  int accepted;
  int received;
  ssize_t written;
  ssize_t total_bytes = 0;
  socklen_t client_addr_len;

>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  struct sockaddr_in sin;
  struct sockaddr client_addr;
  struct timespec start_time;
  struct timespec end_time;
<<<<<<< HEAD
=======

>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  /* Allocate memory for the application receive buffer */
  buffer = (uint8_t *) malloc (CHUNK_SIZE);
  if (!buffer) {
    perror ("Allocate application receive buffer");
    return -EXIT_FAILURE;
  }
<<<<<<< HEAD
=======

>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  /* Open the file for writing the data from the network */
  fp = fopen (file, "w");
  if (!fp) {
    perror ("Open file for writing");
    free (buffer);
    return -EXIT_FAILURE;
  }
<<<<<<< HEAD
  //microtcp
  memset(&sin, 0, sizeof(struct sockaddr_in));
=======

  sock = microtcp_socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock.state == INVALID) {
    perror ("Opening microTCP socket");
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }
  

  memset (&sin, 0, sizeof(struct sockaddr_in));
>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  sin.sin_family = AF_INET;
  sin.sin_port = htons (listen_port);
  /* Bind to all available network interfaces */
  sin.sin_addr.s_addr = INADDR_ANY;
<<<<<<< HEAD
  microtcp_bind(&socket,(struct sockaddr*)&sin,sizeof(struct sockaddr_in));
  if(microtcp_accept(&socket,(struct sockaddr *)&client_addr,sizeof(struct sockaddr) ) == -1){
      printf("Cannot accept\n");
      return -1;
  }

  clock_gettime (CLOCK_MONOTONIC_RAW, &start_time);
  while ((received = microtcp_recv (&socket, buffer, CHUNK_SIZE, 0)) > 0) {
    written = fwrite (buffer, sizeof(uint8_t), received, fp);
    total_bytes += received;
    if (written * sizeof(uint8_t) != (size_t)received) {
      printf ("Failed to write to the file the"
              " amount of data received from the network.\n");
      microtcp_shutdown (&socket, SHUT_RDWR);
      close (socket.sd);
      free (buffer);
      fclose (fp);
=======

  if (microtcp_bind (&sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) == -1) {
    perror ("microTCP bind");
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }

  /* Accept a connection from the client */
  client_addr_len = sizeof(struct sockaddr);
  accepted = microtcp_accept (&sock, &client_addr, client_addr_len);
  if (accepted == -1) {
    perror ("microTCP accept");
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }

  /*
   * Start processing the received data.
   *
   * Also start measuring time. Not the most accurate measurement, but
   * it is a good starting point.
   *
   * At hy-435 we deal with bandwidth measurements software in a more
   * right and careful way :-)
   */

  clock_gettime (CLOCK_MONOTONIC_RAW, &start_time);
  while ((received = microtcp_recv (&sock, buffer, CHUNK_SIZE, 0)) > 0) {
    written = fwrite (buffer, sizeof(uint8_t), received, fp);
    total_bytes += received;
    if (written * sizeof(uint8_t) != received) {
      printf ("Failed to write to the file the"
              " amount of data received from the network.\n");
      if(microtcp_shutdown(&sock, 0) == -1){
        perror("microTCP shutdown2");
      }
      free (buffer);
      fclose (fp);

>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
      return -EXIT_FAILURE;
    }
  }
  clock_gettime (CLOCK_MONOTONIC_RAW, &end_time);
  print_statistics (total_bytes, start_time, end_time);
<<<<<<< HEAD
  fclose (fp);
  microtcp_shutdown (&socket, SHUT_RDWR);
  close (socket.sd);
  free (buffer);
=======
  microtcp_shutdown(&sock, 0);
  fclose (fp);
  free (buffer);

>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  return 0;
}

int
client_tcp (const char *serverip, uint16_t server_port, const char *file)
{
  uint8_t *buffer;
  int sock;
  socklen_t client_addr_len;
  FILE *fp;
  size_t read_items = 0;
  size_t data_sent;

  struct sockaddr *client_addr;

  /* Allocate memory for the application receive buffer */
  buffer = (uint8_t *) malloc (CHUNK_SIZE);
  if (!buffer) {
    perror ("Allocate application receive buffer");
    return -EXIT_FAILURE;
  }

  /* Open the file for writing the data from the network */
  fp = fopen (file, "r");
  if (!fp) {
    perror ("Open file for reading");
    free (buffer);
    return -EXIT_FAILURE;
  }

  if ((sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    perror ("Opening TCP socket");
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }

  struct sockaddr_in sin;
  memset (&sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = AF_INET;
  /*Port that server listens at */
  sin.sin_port = htons (server_port);
  /* The server's IP*/
  sin.sin_addr.s_addr = inet_addr (serverip);

  if (connect (sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in))
      == -1) {
    perror ("TCP connect");
    exit (EXIT_FAILURE);
  }

  printf ("Starting sending data...\n");
  /* Start sending the data */
  while (!feof (fp)) {
    read_items = fread (buffer, sizeof(uint8_t), CHUNK_SIZE, fp);
    if (read_items < 1) {
      perror ("Failed read from file");
      shutdown (sock, SHUT_RDWR);
      close (sock);
      free (buffer);
      fclose (fp);
      return -EXIT_FAILURE;
    }

    data_sent = send (sock, buffer, read_items * sizeof(uint8_t), 0);
    if (data_sent != read_items * sizeof(uint8_t)) {
      printf ("Failed to send the"
              " amount of data read from the file.\n");
      shutdown (sock, SHUT_RDWR);
      close (sock);
      free (buffer);
      fclose (fp);
      return -EXIT_FAILURE;
    }

  }

  printf ("Data sent. Terminating...\n");
  shutdown (sock, SHUT_RDWR);
  close (sock);
  free (buffer);
  fclose (fp);
  return 0;
}

int
client_microtcp (const char *serverip, uint16_t server_port, const char *file)
{
<<<<<<< HEAD
  /*TODO: Write your code here */
  FILE* fp;
  uint8_t* buffer;
  size_t read_items = 0;
  ssize_t data_sent;
  microtcp_sock_t socket;
  int domain=AF_INET;
  int type=SOCK_DGRAM;
  int protocol=0;
  socket=microtcp_socket(domain,type,protocol);
  struct sockaddr_in servaddr ;
  memset(&servaddr, 0, sizeof(struct sockaddr_in));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(serverip);
  servaddr.sin_port = htons(server_port);
  if (microtcp_connect(&socket,(struct sockaddr *) &servaddr, sizeof(struct sockaddr_in))) {
      printf("connection with the server failed...\n");
      exit(0);
  }
  printf("TCP Connection Established\n");
=======
  uint8_t *buffer;
  microtcp_sock_t sock;
  socklen_t client_addr_len;
  FILE *fp;
  size_t read_items = 0;
  ssize_t data_sent;

  struct sockaddr *client_addr;

>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  /* Allocate memory for the application receive buffer */
  buffer = (uint8_t *) malloc (CHUNK_SIZE);
  if (!buffer) {
    perror ("Allocate application receive buffer");
    return -EXIT_FAILURE;
  }
<<<<<<< HEAD
  /* Open and read the file that will be transmitted */
=======

  /* Open the file for writing the data from the network */
>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  fp = fopen (file, "r");
  if (!fp) {
    perror ("Open file for reading");
    free (buffer);
    return -EXIT_FAILURE;
  }
<<<<<<< HEAD
=======
  sock = microtcp_socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock.state == INVALID) {
    perror ("Opening microTCP socket");
    if(microtcp_shutdown(&sock, 0) == -1){
      perror("microTCP shutdown3");
    }
    free (buffer);
    fclose (fp);
    return -EXIT_FAILURE;
  }

  struct sockaddr_in sin;
  memset (&sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = AF_INET;
  /*Port that server listens at */
  sin.sin_port = htons (server_port);
  /* The server's IP*/
  sin.sin_addr.s_addr = inet_addr (serverip);

  if (microtcp_connect(&sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) == -1) {
    perror ("microTCP connect");
    exit (EXIT_FAILURE);
  }

  sock.address = &sin;
  sock.address_len = sizeof(sin); 
  
>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  printf ("Starting sending data...\n");
  /* Start sending the data */
  while (!feof (fp)) {
    read_items = fread (buffer, sizeof(uint8_t), CHUNK_SIZE, fp);
    if (read_items < 1) {
      perror ("Failed read from file");
<<<<<<< HEAD
      microtcp_shutdown (&socket, SHUT_RDWR);
      close (socket.sd);
=======
>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
      free (buffer);
      fclose (fp);
      return -EXIT_FAILURE;
    }
<<<<<<< HEAD
    data_sent = microtcp_send (&socket, buffer, read_items * sizeof(uint8_t), 0);
    if ((size_t)data_sent != read_items * sizeof(uint8_t)) {
      printf ("Failed to send the"
              " amount of data read from the file.\n");
      microtcp_shutdown(&socket, SHUT_RDWR);
      close (socket.sd);
      free (buffer);
      fclose (fp);
=======

    data_sent = microtcp_send (&sock, buffer, read_items * sizeof(uint8_t), 0);
    if (data_sent != read_items * sizeof(uint8_t)) {
      printf ("Failed to send the"
              " amount of data read from the file.\n");
      if(microtcp_shutdown(&sock, 0)){
        perror("microTCP shutdown4");
      }
      free (buffer);
      fclose (fp);

>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
      return -EXIT_FAILURE;
    }
  }
  printf ("Data sent. Terminating...\n");
<<<<<<< HEAD
  microtcp_shutdown(&socket, SHUT_RDWR);
  close (socket.sd);
=======
  microtcp_shutdown(&sock, 0);
>>>>>>> aef75777c1c63ed8ba309c7ee1b834628eab0de0
  free (buffer);
  fclose (fp);

  return 0;
}

int
main (int argc, char **argv)
{
  int opt;
  int port;
  int exit_code = 0;
  char *filestr = NULL;
  char *ipstr = NULL;
  uint8_t is_server = 0;
  uint8_t use_microtcp = 0;

  /* A very easy way to parse command line arguments */
  while ((opt = getopt (argc, argv, "hsmf:p:a:")) != -1) {
    switch (opt)
      {
      /* If -s is set, program runs on server mode */
      case 's':
        is_server = 1;
        break;
        /* if -m is set the program should use the microTCP implementation */
      case 'm':
        use_microtcp = 1;
        break;
      case 'f':
        filestr = strdup (optarg);
        /* A few checks will be nice here...*/
        /* Convert the given file to absolute path */
        break;
      case 'p':
        port = atoi (optarg);
        /* To check or not to check? */
        break;
      case 'a':
        ipstr = strdup (optarg);
        break;

      default:
        printf (
            "Usage: bandwidth_test [-s] [-m] -p port -f file"
            "Options:\n"
            "   -s                  If set, the program runs as server. Otherwise as client.\n"
            "   -m                  If set, the program uses the microTCP implementation. Otherwise the normal TCP.\n"
            "   -f <string>         If -s is set the -f option specifies the filename of the file that will be saved.\n"
            "                       If not, is the source file at the client side that will be sent to the server.\n"
            "   -p <int>            The listening port of the server\n"
            "   -a <string>         The IP address of the server. This option is ignored if the tool runs in server mode.\n"
            "   -h                  prints this help\n");
        exit (EXIT_FAILURE);
      }
  }

  /*
   * TODO: Some error checking here???
   */

  /*
   * Depending the use arguments execute the appropriate functions
   */
  if (is_server) {

    if (use_microtcp) {
      exit_code = server_microtcp (port, filestr);
    }
    else {
      exit_code = server_tcp (port, filestr);
    }
  }
  else {
    if (use_microtcp) {
      exit_code = client_microtcp (ipstr, port, filestr);
    }
    else {
      exit_code = client_tcp (ipstr, port, filestr);
    }
  }

  free (filestr);
  free (ipstr);
  return exit_code;
}

