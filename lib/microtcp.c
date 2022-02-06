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
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <ctype.h>
#include <stdlib.h>

#define min(a,b,c) (a<b?(a<c?a:c):(b<c?b:c))
  // 000 0  0 0 0 0 0 0 0 0 ACK=0 0 SYN=1 FIN=0
 
microtcp_header_t create_header (uint32_t seq, uint16_t control, uint32_t data_len,  uint32_t ack, uint16_t window, uint32_t checksum) {
  microtcp_header_t msg;

  msg.seq_number=htonl(seq);
  msg.control=htons(control);
  msg.data_len=htonl(data_len);
  msg.ack_number=htonl(ack);
  msg.window=htons(window);
  msg.future_use0=0;
  msg.future_use1=0;
  msg.future_use2=0;
  msg.checksum=htonl(checksum);

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
  sock.sd=socket (domain, type, protocol);
  if (sock.sd < 0){
     perror ("Socket");
     sock.state=INVALID;
  }
  sock.init_win_size=MICROTCP_WIN_SIZE;
  sock.curr_win_size=MICROTCP_WIN_SIZE;
  sock.cwnd=MICROTCP_INIT_CWND;
  sock.ssthresh=MICROTCP_INIT_SSTHRESH;
  sock.buf_fill_level=0;
  sock.shuts=TRUE;
  return sock;
}

int
microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address,
               socklen_t address_len)
{
  int ret;
  ret=bind (socket->sd, address, address_len);
  if (ret < 0){
     perror ("Bind");
     socket->state=INVALID;
  }
  socket->shuts=FALSE;
  return ret;
}

int
microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address,
                  socklen_t address_len)
{
  srand(time(NULL));
  int tmp;
  ssize_t tmp_len;
  microtcp_header_t msg;
  microtcp_header_t rec;
  uint8_t buf[MICROTCP_RECVBUF_LEN];

  for(int i=0; i<MICROTCP_RECVBUF_LEN; i++)
    buf[i]=0;
  socket->address=(struct sockaddr*)address;
  socket->address_len=address_len;
  socket->seq_number=rand()%10000;

  // 000 0  0 0 0 0 0 0 0 0 ACK=0 0 SYN=1 FIN=0  ==  2
  msg = create_header(socket->seq_number, (uint16_t)2, 0, 0, 0, 0);
  //copy the header to the buffer
  memcpy (buf,&msg, sizeof(microtcp_header_t));
  msg.checksum=htonl( crc32((const uint8_t *)&msg, sizeof(microtcp_header_t)));
  tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,address,address_len);
  if (tmp<0) {
    perror("Cant Establish socket");
    socket->state=INVALID;
    return -1;
  }
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*) address,&address_len)<0){
    socket->state=INVALID;
    perror("Server did not accept connection");
    return -1;
  }

  memcpy(&rec,buf,sizeof(microtcp_header_t));
  int check=rec.checksum;
  rec.checksum=0;

  if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
    perror("Received packet destroyed1");
    socket->state=INVALID;
    return -1;
  }

  rec = reverse(rec);
  if(rec.control!=(uint16_t)10){
    perror("Server did not accept connection");
    socket->state=INVALID;
    return -1;
  }
  if(socket->seq_number!=rec.ack_number-1){
    perror("Received packet destroyed2");
    socket->state=INVALID;
    return -1;
  }

  socket->ack_number=rec.seq_number+1;
  socket->seq_number++;
  socket->curr_win_size=rec.window;
  // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0  ==  8
  msg = create_header(socket->seq_number, (uint16_t)8, 0, socket->ack_number, socket->init_win_size, 0);
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  memcpy(buf,&msg,sizeof(microtcp_header_t));
  msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
  tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len);
  if (tmp<0) {
    perror("Cant Establish socket");
    socket->state=INVALID;
    socket->ack_number=0;
    socket->seq_number--;
    return -1;
  }
  socket->state=ESTABLISHED;
  return 0;
}

int
microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
  srand(time(NULL));
  int tmp,seq,ack;
  ssize_t tmp_len;
  microtcp_header_t msg;
  microtcp_header_t rec;
  uint8_t buf[MICROTCP_RECVBUF_LEN];
  socket->address=address;
  socket->address_len=address_len;
  //receiving packet
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)address,&address_len)<0){
    socket->state=INVALID;
    perror("No packet from client received");
    return -1;
  }
  memcpy(&rec,buf,sizeof(microtcp_header_t));
  int check=rec.checksum;
  rec.checksum=0;

  if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
    perror("Received packet destroyed3");
    socket->state=INVALID;
    return -1;
  }

  rec = reverse(rec);

  if(rec.control!=(uint16_t)2){
    perror("Packet has not SYN=1");
    socket->state=INVALID;
    return -1;
  }
  //creating packet for 2nd handshake
  seq=rand()%10000;
  ack=rec.seq_number+1;

  // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=1 FIN=0  ==  10
  msg = create_header(seq, (uint16_t)10, 0, ack, socket->curr_win_size, 0);
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  memcpy(buf,&msg,sizeof(microtcp_header_t));
  msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
  tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len);
  if (tmp<0) {
    perror("Cant Establish socket");
    socket->state=INVALID;
    return -1;
  }
    //receiving 2nd packet
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)address,&address_len)<0){
    socket->state=INVALID;
    perror("No packet from client received");
    return -1;
  }

  memcpy(&rec,buf,sizeof(microtcp_header_t));
  check=rec.checksum;
  rec.checksum=0;

  if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
    perror("Received packet destroyed4");
    socket->state=INVALID;
    return -1;
  }
  
  rec = reverse(rec);
  if(rec.control!=(uint16_t)8){
    perror("Packet has not ACK=1");
    socket->state=INVALID;
    return -1;
  }
  if(rec.seq_number!=(uint32_t)ack||rec.ack_number!=(uint32_t)seq+1){
    perror("Received packet not valid");
    socket->state=INVALID;
    return -1;
  }
  //adding data received to buffer
  socket->seq_number=seq+1;
  socket->ack_number=ack+1;
  //coping received data to servers buffer for later
  
  socket->recvbuf=malloc(MICROTCP_RECVBUF_LEN);  
  if(socket->recvbuf==NULL){
    perror("Cant allocate memory for recvbuf");
    socket->state=INVALID;
    return -1;
  }
  socket->state=ESTABLISHED;
  return 0;
}

int
microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  int tmp,seq,ack,check;
  ssize_t tmp_len;
  microtcp_header_t msg;
  microtcp_header_t rec;
  uint8_t buf[MICROTCP_RECVBUF_LEN]={0};
  if(socket->state==CLOSING_BY_PEER){
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0  == 8
    msg = create_header(socket->seq_number, (uint16_t)8, 0, socket->ack_number, socket->curr_win_size, 0);
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    memcpy(buf,&msg,sizeof(microtcp_header_t));
    msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
    tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len);
    if (tmp<0) {
      perror("Cant Establish socket");
      socket->state=INVALID;
      return -1;
    }
    socket->state=CLOSING_BY_PEER;
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=1  ==  9
    msg = create_header(socket->seq_number, (uint16_t)9, 0, socket->ack_number, socket->curr_win_size, 0);
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    memcpy(buf,&msg,sizeof(microtcp_header_t));
    msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
    tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len);
    if (tmp<0) {
      perror("Cant Establish socket");
      socket->state=INVALID;
      return -1;
    }
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len)<0){
      socket->state=INVALID;
      perror("No packet from client received");
      return -1;
    }
    memcpy(&rec,buf,sizeof(microtcp_header_t));
    check=rec.checksum;
    rec.checksum=0;
    if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
      perror("Received packet destroyed8");
      socket->state=INVALID;
      return -1;
    }
    rec = reverse(rec);
    if(rec.control!=(uint16_t)8){
      perror("Packet has not ACK=1");
      socket->state=INVALID;
      return -1;
    }
    if(rec.seq_number!=(uint32_t)socket->ack_number+1||rec.ack_number!=(uint32_t)socket->seq_number+1){
      perror("Packet has not correct seq or ACK");
      socket->state=INVALID;
      return -1;
    }
    socket->state=CLOSED;

  }else{
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=1  ==  9
    msg = create_header(socket->seq_number, (uint16_t)9, 0, socket->ack_number, socket->curr_win_size, 0);
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    memcpy(buf,&msg,sizeof(microtcp_header_t));
    msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
    tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,socket->address,socket->address_len);
    if (tmp<0) {
      perror("Cant Establish socket");
      socket->state=INVALID;
      return -1;
    }

    memset(buf,0,MICROTCP_RECVBUF_LEN);
    if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len)<0){
      socket->state=INVALID;
      perror("No packet from client received");
      return -1;
    }
    memcpy(&rec,buf,sizeof(microtcp_header_t));
    check=rec.checksum;
    rec.checksum=0;
    if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
      perror("Received packet destroyed5");
      socket->state=INVALID;
      return -1;
    }
    rec = reverse(rec);
    if(rec.control!=(uint16_t)8){
      perror("Packet has not ACK=1");
      socket->state=INVALID;
      return -1;
    }
    if(rec.ack_number!=(uint32_t)socket->seq_number){
      perror("Received packet not valid");
      socket->state=INVALID;
      return -1;
    }
    socket->seq_number++;
    socket->state=CLOSING_BY_HOST;
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len)<0){
      socket->state=INVALID;
      perror("No packet from client received");
      return -1;
    }

    memcpy(&rec,buf,sizeof(microtcp_header_t));
    check=rec.checksum;
    rec.checksum=0;
    if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
      perror("Received packet destroyed6");
      socket->state=INVALID;
      return -1;
    }
    rec = reverse(rec);
    if(rec.control!=(uint16_t)9){
      perror("Packet has not ACK=1 and FIN=1");
      socket->state=INVALID;
      return -1;
    }
    socket->ack_number=rec.seq_number;
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0  == 8
    msg = create_header(socket->seq_number, (uint16_t)8, 0, ++socket->ack_number, socket->curr_win_size, 0);
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    memcpy(buf,&msg,sizeof(microtcp_header_t));
    msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
    tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len);
    if (tmp<0) {
      perror("Cant Establish socket");
      socket->state=INVALID;
      return -1;
    }
    socket->state=CLOSED;
  }
  close(socket->sd);
  return 0;
}

ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
  microtcp_header_t header;
  uint32_t temp;
  size_t i, tmp_data, sent, res;
  size_t seq_number=socket->seq_number;
  size_t ack_number=socket->ack_number;
  size_t remaining=length;
  size_t data_sent=0;
  size_t bytes_to_send=0;
  size_t chunks=0;
  Bool timeout_b=FALSE;
  void *data=NULL;

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;

  while(data_sent < length){
    bytes_to_send = min(socket->curr_win_size , socket->cwnd, remaining);
    chunks = bytes_to_send / MICROTCP_MSS;
    data = malloc(MICROTCP_MSS+sizeof(microtcp_header_t));
    tmp_data = 0;
    if(data == NULL){
      perror("Cant allocate memory");
      return -1;
    }
    for(i = 0; i < chunks; i++){
      seq_number += MICROTCP_MSS;
      add(seq_number+1);
      /* Initializing Header file "head" with checksum */
      header = create_header(seq_number, (uint16_t)0, 0, 0, MICROTCP_MSS, 0);
      memset(data, 0, MICROTCP_MSS + sizeof(microtcp_header_t));
      memcpy(data, &header, sizeof(microtcp_header_t));
      memcpy(data + sizeof(microtcp_header_t), buffer + data_sent, MICROTCP_MSS);

      header.checksum = htonl(crc32((const uint8_t *)data, MICROTCP_MSS + sizeof(microtcp_header_t)));
      memset(data, 0, MICROTCP_MSS + sizeof(microtcp_header_t));
      memcpy(data, &header, sizeof(microtcp_header_t));
      memcpy(data + sizeof(microtcp_header_t), buffer+data_sent, MICROTCP_MSS);
      /* Sending the chunk */
      if(sendto(socket->sd,(void*)data,MICROTCP_MSS+sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)<0){
        perror("Cant send data");
        socket->state = INVALID;
        return -1;
      }
      data_sent += MICROTCP_MSS;
      sent += MICROTCP_MSS;
    }
    free(data);

    /* Check if there is a semi -filled chunk */
    if(bytes_to_send % MICROTCP_MSS){
      chunks++;
      seq_number += bytes_to_send % MICROTCP_MSS;
      add(seq_number + 1);
      header = create_header(seq_number, (uint16_t) 0, 0, 0, bytes_to_send % MICROTCP_MSS, 0);
      data = malloc((bytes_to_send % MICROTCP_MSS) + sizeof(microtcp_header_t));
      if(data == NULL){
        perror("Cant allocate memory");
        return -1;
      }
      memcpy(data, &header, sizeof(microtcp_header_t));
      memcpy(data + sizeof(microtcp_header_t), buffer + data_sent, bytes_to_send % MICROTCP_MSS);
      header.checksum = htonl(crc32((const uint8_t *) data, MICROTCP_MSS + sizeof(microtcp_header_t)));
      memset(data, 0, (bytes_to_send % MICROTCP_MSS) + sizeof(microtcp_header_t));
      memcpy(data, &header, sizeof(microtcp_header_t));
      memcpy(data + sizeof(microtcp_header_t), buffer + data_sent, bytes_to_send % MICROTCP_MSS);
      if(sendto(socket->sd, (void*)data, (bytes_to_send % MICROTCP_MSS) + sizeof(microtcp_header_t), 0, (struct sockaddr*) socket->address, socket->address_len) < 0){
          perror("Cant send data");
          socket->state = INVALID;
          return -1;
      }
      data_sent += (bytes_to_send % MICROTCP_MSS);
      sent += (bytes_to_send % MICROTCP_MSS);
      free(data);
    }

    /* Get the ACKs */
    for(i = 0; i < chunks; i++){
      if (setsockopt(socket->sd , SOL_SOCKET ,SO_RCVTIMEO , &timeout ,sizeof(struct timeval)) < 0) {
        perror("setsockopt");
        socket->state = INVALID;
        return -1;
      }
      /* Receiving and checking */
      res = recvfrom(socket->sd, ( void *) &header, sizeof(microtcp_header_t), 0, (struct sockaddr*) socket->address, &socket->address_len);
      if(res < 0){
        perror("timeout");
        timeout_b = TRUE;
      }
      /* Checksum checking */
      temp=ntohl(header.checksum);
      header.checksum=0;
      if((uint32_t) temp != crc32((const uint8_t *) &header, sizeof(microtcp_header_t))){
        perror("Received packet destroyed");
        clear();
        i--;
        continue;
      }
      /* Checking for the ACK flag */
      header = reverse(header);
      if(header.control != (uint16_t) 8){
        perror("Packet has not ACK=1");
        i--;
      }else{
        socket->curr_win_size = header.window;
        /* Checking for the amount of duplicate ACK packets */
        if(ack_number == header.ack_number){
          if(d == NOT_DUPLICATE){
            d = DUPLICATE;
          }else if(d == DUPLICATE){
            d = T_DUPLICATE;
            break;
          }
          i--;
        }else{
          /* Checking if the received packet has the same ack number as the one that was sent */
          ack_number = header.ack_number;
          if(ack_number == list->ack){
            tmp_data = tmp_data + header.data_len;
            if(socket->ssthresh >= socket->cwnd){
              socket->cwnd += MICROTCP_MSS;
            }else{
              socket->cwnd++;
            }
            pop();
            d = NOT_DUPLICATE;
            timeout_b = FALSE;
          }
        }
        /* Here will take action the proccesses based on the current state: "3-duplicate ACK's", "timeout" or "none" */
        if(d == T_DUPLICATE){
          /* Congestion avoidance phase */
          socket->ssthresh = socket->cwnd/2;
          socket->cwnd = socket->cwnd/2 + 1;
          seq_number = header.seq_number - chunks + i + 1;
          data_sent = data_sent - sent + tmp_data;
          remaining -= tmp_data;
        }else if(timeout_b){
          /* Slow start phase because of an ACK timeout */
          socket->ssthresh = socket->cwnd/2;
          socket->cwnd = min(MICROTCP_MSS, socket->ssthresh, MICROTCP_RECVBUF_LEN);
          seq_number = header.seq_number - chunks + i + 1;
          data_sent = data_sent - sent + tmp_data;
          remaining -= tmp_data;
          /* Checking if the cwnd is 0 so problems do not occur */
          if(socket->cwnd == 0){
            socket->cwnd = 1;
          }
        }else{
          if(sent == tmp_data){
            remaining -= sent;
            seq_number = header.seq_number-1;
          }else{
            seq_number = header.seq_number-chunks;
            data_sent -= sent;
          }
        }
      }
    }
  }

  return data_sent;
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  ssize_t res=0;
  size_t seq_number=socket->seq_number;
  size_t ack_number=socket->ack_number;
  size_t data_received=0;
  uint32_t tmp;
  void *pkg=malloc(sizeof(microtcp_header_t)+MICROTCP_MSS);
  void *data=malloc(MICROTCP_MSS);
  microtcp_header_t header;
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;
  for(int i=1;TRUE; i++){
    if(setsockopt(socket->sd , SOL_SOCKET ,SO_RCVTIMEO , &timeout, sizeof(struct timeval)) < 0) {
				perror("timeout");
				socket->state = INVALID;
				return 0;
			}
      res=recvfrom(socket->sd,pkg,sizeof(microtcp_header_t)+MICROTCP_MSS,0,(struct sockaddr*)socket->address,&socket->address_len);
      if(res<0){
        perror("timeout");
        header=create_header(0,(uint16_t)8,0,ack_number++,0,0);
        memcpy(data, &header, sizeof(microtcp_header_t));
        header.checksum=htonl(crc32((const uint8_t *)data,sizeof(microtcp_header_t)));
        memset(data,0,MICROTCP_MSS);
        if (sendto(socket->sd,(void *)&header,sizeof(microtcp_header_t),0,(struct sockaddr *)&socket->address,socket->address_len) <0){
						socket->state=INVALID;
						perror("cant sendto");
				}

      }else{
        memcpy(&header,pkg, sizeof(microtcp_header));
        memcpy(data,pkg+sizeof(microtcp_header_t),MICROTCP_MSS);
        tmp=ntohl(header.checksum);
        header.checksum=0;
        if((uint32_t)tmp != crc32((const uint8_t *) &header, sizeof(microtcp_header_t))){
          perror("Received packet destroyed");
        }
        header=reverse(header);

      }
  }
}
void add(uint32_t ack){
  //add elements to end of list and include if list is empty
  if(list==NULL){
    list=malloc(sizeof(struct node));
    list->ack=ack;
    list->next=NULL;
    return;
  }
  struct node *temp=list;
  while(temp->next!=NULL){
    temp=temp->next;
  }
  temp->next=malloc(sizeof(struct node));
  temp->next->ack=ack; 
  temp->next->next=NULL;

}
void clear(){
  //free all elements in list
  struct node *temp=list;
  while(temp!=NULL){
    list=list->next;
    free(temp);
    temp=list;
  }

}
void pop() {
  //remove first element in list
  struct node *temp=list;
  list=list->next;
  free(temp);
}
void print(){
  struct node *temp;
  temp=list;
  while(temp!=NULL){
    printf("%d\n",temp->ack);
    temp=temp->next;
  }
}