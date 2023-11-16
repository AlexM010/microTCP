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

#include "microtcp.h"
#include "../utils/crc32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#define SYN 2
#define ACK 8
#define SYNACK 10

microtcp_header_t create_header (uint32_t seq, uint16_t control, uint32_t data_len,  uint32_t ack, uint16_t window) {
  microtcp_header_t msg;

  msg.seq_number=htonl(seq);
  msg.control=htons(control);
  msg.data_len=htonl(data_len);
  msg.ack_number=htonl(ack);
  msg.window=htons(window);
  msg.future_use0=0;
  msg.future_use1=0;
  msg.future_use2=0;
  msg.checksum=0;
  return msg;
}

microtcp_header_t reverse(microtcp_header_t msg) {
  msg.seq_number=ntohl(msg.seq_number);
  msg.control=ntohs(msg.control);
  msg.data_len=ntohl(msg.data_len);
  msg.ack_number=ntohl(msg.ack_number);
  msg.window=ntohs(msg.window);
  msg.future_use0=ntohl(msg.future_use0);
  msg.future_use1=ntohl(msg.future_use1);
  msg.future_use2=ntohl(msg.future_use2);
  msg.checksum=ntohl(msg.checksum);

  return msg;
}
microtcp_sock_t
microtcp_socket (int domain, int type, int protocol)
{
  microtcp_sock_t sock;
  sock.state=UNKNOWN;
  if((sock.sd = socket(domain,type,protocol))==-1){
    perror("opening MicroTCP listening socket");
    sock.state=INVALID;
    exit(EXIT_FAILURE);
  }
  sock.init_win_size=MICROTCP_WIN_SIZE;
  sock.curr_win_size=MICROTCP_WIN_SIZE;
  sock.cwnd=MICROTCP_INIT_CWND;
  sock.ssthresh=MICROTCP_INIT_SSTHRESH;
  sock.buf_fill_level=0;

  return sock;

}

int microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len){
  if(bind(socket->sd, address,address_len)==-1){
    perror("binding MicroTCP socket");
    exit(EXIT_FAILURE);
  }
}

int microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len){
  char buf[MICROTCP_RECVBUF_LEN];
  microtcp_header_t tcp_init;
  microtcp_header_t rec;
  microtcp_header_t send;
  uint32_t checksum;
  int status;
  srand(time(NULL)+1);
  socket->seq_number=rand()%10000;
  tcp_init=create_header(socket->seq_number,SYN,0,0,0);
  tcp_init.checksum=htonl(crc32((uint8_t*)&tcp_init,sizeof(microtcp_header_t)));
  printf("Sending SYN packet with sequence number: %lu\n",socket->seq_number);
  if(sendto(socket->sd,(void*)&tcp_init,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len)==-1){
    perror("sending SYN packet");
    exit(EXIT_FAILURE);
  }
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  printf("Waiting SYNACK packet with sequence number\n");
  status=recvfrom(socket->sd,(void*)buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)address,&address_len);
  if(status==-1){
    perror("receiving SYNACK packet");
    exit(EXIT_FAILURE);
  }
  memcpy(&rec,buf,sizeof(microtcp_header_t));
  checksum=ntohl(rec.checksum);
  rec.checksum=0;
  
  if(checksum!=crc32((uint8_t*)&rec,sizeof(microtcp_header_t))){
    perror("checksum error");
    exit(EXIT_FAILURE);
  }
  
  rec=reverse(rec);
  printf("Received SYNACK packet with sequence number: %u and ack_number: %u\n",rec.seq_number,rec.ack_number);
  if(rec.control==SYNACK&&rec.ack_number==socket->seq_number+1){
    socket->ack_number=rec.seq_number+1;
    socket->seq_number++;
    send=create_header(socket->seq_number,ACK,0,socket->ack_number,0);
    send.checksum=htonl(crc32((uint8_t*)&send,sizeof(microtcp_header_t)));
    printf("Sending ACK packet with sequence number: %lu and ack_number: %lu\n",socket->seq_number,socket->ack_number);
    if(sendto(socket->sd,(void*)&send,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len)==-1){
      perror("sending ACK packet");
      exit(EXIT_FAILURE);
    }
  }
  else{
    perror("receiving SYNACK packet");
    exit(EXIT_FAILURE);
  }
  socket->state=ESTABLISHED;
  printf("Connection Established!\n");
  return 0;

}

int microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len){
  char buf[MICROTCP_RECVBUF_LEN];
  microtcp_header_t tcp_init;
  microtcp_header_t rec;
  microtcp_header_t rec2;
  uint32_t checksum;
  int status;

  memset(buf,0,MICROTCP_RECVBUF_LEN);
  status=recvfrom(socket->sd,(void*)buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)address,&address_len);
  if(status==-1){
    perror("receiving SYN packet");
    exit(EXIT_FAILURE);
  }
  memcpy(&rec,buf,sizeof(microtcp_header_t));
  checksum=ntohl(rec.checksum);
  rec.checksum=0;
  if(checksum != crc32((const uint8_t*)&rec,sizeof(microtcp_header_t))){
    perror("checksum error");
    exit(EXIT_FAILURE);
  }

  rec=reverse(rec);
  if(rec.control!=SYN){
    perror("receiving SYN packet");
    exit(EXIT_FAILURE);
  }
  printf("Received SYN packet with sequence number: %u\n",rec.seq_number);
  srand(time(NULL));
  socket->seq_number=rand()%10000;
  socket->ack_number=rec.seq_number+1;

  tcp_init=create_header(socket->seq_number,SYNACK,0,socket->ack_number,0);
  tcp_init.checksum=htonl(crc32((uint8_t*)&tcp_init,sizeof(microtcp_header_t)));
  printf("Sending SYNACK packet with sequence number: %lu and ack_number: %lu\n",socket->seq_number,socket->ack_number);
  if(sendto(socket->sd,(void*)&tcp_init,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len)==-1){
    perror("sending SYNACK packet");
    exit(EXIT_FAILURE);
  }

  memset(buf,0,MICROTCP_RECVBUF_LEN);
  //print
  printf("Waiting ACK packet with sequence number: %lu and ack_number: %lu\n",socket->seq_number,socket->ack_number);
  status=recvfrom(socket->sd,(void*)buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)address,&address_len);
  if(status==-1){
    perror("receiving ACK packet");
    exit(EXIT_FAILURE);
  }
  memcpy(&rec2,buf,sizeof(microtcp_header_t));
  checksum=ntohl(rec2.checksum);
  rec2.checksum=0;
  if(checksum!=crc32((uint8_t*)&rec2,sizeof(microtcp_header_t))){
    perror("checksum error");
    exit(EXIT_FAILURE);
  }
  socket->seq_number++;
  rec2=reverse(rec2);
  if(rec2.control !=ACK || rec2.seq_number != socket->ack_number || rec2.ack_number != socket->seq_number ){
    socket->seq_number--;
    perror("receiving ACK packet");
    exit(EXIT_FAILURE);
  }
  socket->state=ESTABLISHED;
  printf("Connection Established!\n");
  return 0;
}

int
microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  /* Your code here */
}

ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
  /* Your code here */
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  /* Your code here */
}
