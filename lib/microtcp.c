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
#define  MAX_PAYLOAD_SIZE  (MICROTCP_MSS-sizeof(microtcp_header_t))
//#define  DEBUG

void print_header(microtcp_header_t header);
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
  sock.fin=-1;
  sock.recvbuf=malloc(MICROTCP_RECVBUF_LEN);

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
  int recvbuf_size=0;
  srand(time(NULL)+1);
  socket->seq_number=rand()%10000;
  tcp_init=create_header(socket->seq_number,SYN,0,0,MICROTCP_WIN_SIZE);
  tcp_init.checksum=htonl(crc32((uint8_t*)&tcp_init,sizeof(microtcp_header_t)));

  #ifdef  DEBUG
  printf("Sending SYN packet with sequence number: %lu\n",socket->seq_number);
  #endif  //DEBUG
  
  if(sendto(socket->sd,(void*)&tcp_init,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len)==-1){
    perror("sending SYN packet");
    exit(EXIT_FAILURE);
  }
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  
  #ifdef  DEBUG
  printf("Waiting SYNACK packet with sequence number\n");
  #endif  //DEBUG

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
  #ifdef DEBUG
  printf("Received SYNACK packet with sequence number: %u and ack_number: %u\n",rec.seq_number,rec.ack_number);
  #endif  //DEBUG
  if(rec.control==SYNACK&&rec.ack_number==socket->seq_number+1){
    recvbuf_size=rec.window;
    socket->ack_number=rec.seq_number+1;
    socket->seq_number++;
    send=create_header(socket->seq_number,ACK,0,socket->ack_number,MICROTCP_WIN_SIZE);
    send.checksum=htonl(crc32((uint8_t*)&send,sizeof(microtcp_header_t)));
    #ifdef DEBUG
    printf("Sending ACK packet with sequence number: %lu and ack_number: %lu\n",socket->seq_number,socket->ack_number);
    #endif  //DEBUG
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
  #ifdef  DEBUG
  printf("Connection Established!\n");
  
  #endif  //

  socket->address=malloc(sizeof(struct sockaddr));
  *socket->address=*address;
  socket->address_len=address_len;
  socket->fun=CLIENT;
  socket->init_win_size=recvbuf_size;
  return 0;

}

int microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len){
  char buf[MICROTCP_RECVBUF_LEN];
  int recvbuf_size=0;
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
  #ifdef  DEBUG
  printf("Received SYN packet with sequence number: %u\n",rec.seq_number);
  #endif  //DEG
  socket->seq_number=rand()%10000;
  srand(time(NULL));
  socket->ack_number=rec.seq_number+1;

  tcp_init=create_header(socket->seq_number,SYNACK,0,socket->ack_number,MICROTCP_WIN_SIZE);
  tcp_init.checksum=htonl(crc32((uint8_t*)&tcp_init,sizeof(microtcp_header_t)));
  #ifdef  DEBUG
  printf("Sending SYNACK packet with sequence number: %lu and ack_number: %lu\n",socket->seq_number,socket->ack_number);
  #endif
  if(sendto(socket->sd,(void*)&tcp_init,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len)==-1){
    perror("sending SYNACK packet");
    exit(EXIT_FAILURE);
  }

  memset(buf,0,MICROTCP_RECVBUF_LEN);
  #ifdef  DEBUG
  printf("Waiting ACK packet with sequence number: %lu and ack_number: %lu\n",socket->seq_number,socket->ack_number);
  #endif
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
  recvbuf_size=rec2.window;
  socket->state=ESTABLISHED;
  #ifdef  DEBUG
  printf("Connection Established!\n");
  #endif  //
  socket->address=malloc(sizeof(struct sockaddr));
  *socket->address=*address;
  socket->address_len=address_len;
  socket->fun=SERVER;
  socket->init_win_size=recvbuf_size;
  return 0;
}

int microtcp_shutdown (microtcp_sock_t *socket, int how){
  microtcp_header_t packet;
  char buf[MICROTCP_RECVBUF_LEN];
  int status;
  uint32_t checksum;
  if(socket->fun==SERVER){
    #ifdef  DEBUG
    printf("State has changed to CLOSING_BY_PEER\n");
    #endif
    /* Sending ACK*/
    packet=create_header(socket->seq_number,ACK,0,socket->ack_number,0);
    packet.checksum=htonl(crc32((uint8_t*)&packet,sizeof(microtcp_header_t)));
    #ifdef  DEBUG
    printf("Sending ACK packet with ack number: %lu\n",socket->ack_number);
    #endif
    if(sendto(socket->sd,(void*)&packet,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
      perror("sending SYN packet");
      exit(EXIT_FAILURE);
    }
    /*Sending FINACK*/
    socket->seq_number++;
    packet=create_header(socket->seq_number,FINACK,0,0,0);
    packet.checksum=htonl(crc32((uint8_t*)&packet,sizeof(microtcp_header_t)));
    #ifdef  DEBUG
    printf("Sending FINACK packet with sequence number: %lu\n",socket->seq_number);
    #endif
    if(sendto(socket->sd,(void*)&packet,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
      perror("sending SYN packet");
      exit(EXIT_FAILURE);
    }
    /*Receiving ACK packet*/
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    status=recvfrom(socket->sd,(void*)buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len);
    if(status==-1){
      perror("receiving ACK packet");
      exit(EXIT_FAILURE);
    }
    memcpy(&packet,buf,sizeof(microtcp_header_t));
    checksum=ntohl(packet.checksum);
    packet.checksum=0;
    if(checksum != crc32((const uint8_t*)&packet,sizeof(microtcp_header_t))){
      perror("checksum error");
      exit(EXIT_FAILURE);
    }
    packet=reverse(packet);
    if(packet.control!=ACK){
      perror("receiving ACK packet");
    }
  
    if(packet.seq_number!=socket->ack_number){
      perror("receiving seq number");
      exit(EXIT_FAILURE);
    }
    if(packet.ack_number!= ++socket->seq_number){
      perror("receiving ack number");
      exit(EXIT_FAILURE);
    }
    #ifdef  DEBUG
    printf("Received ACK packet with sequence number: %u and ack_number: %u\n",packet.seq_number,packet.ack_number);
    #endif
    /*Connection Closed*/
    socket->state=CLOSED;
    #ifdef  DEBUG
    printf("State has changed to CLOSED\n");
    printf("TCP Connection Closed\n");
    #endif
  }else if(socket->fun==CLIENT){
    /* Sending 1st packet -> FINACK*/
    packet=create_header(socket->seq_number,FINACK,0,0,0);
    packet.checksum=htonl(crc32((uint8_t*)&packet,sizeof(microtcp_header_t)));
    #ifdef  DEBUG
    printf("Sending FINACK packet with sequence number: %lu\n",socket->seq_number);
    #endif
    if(sendto(socket->sd,(void*)&packet,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
      perror("sending SYN packet");
      exit(EXIT_FAILURE);
    }

    /*Receicing ACK packet*/
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    status=recvfrom(socket->sd,(void*)buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len);
    if(status==-1){
      perror("receiving ACK packet");
      exit(EXIT_FAILURE);
    }
    memcpy(&packet,buf,sizeof(microtcp_header_t));
    checksum=ntohl(packet.checksum);
    packet.checksum=0;
    if(checksum != crc32((const uint8_t*)&packet,sizeof(microtcp_header_t))){
      perror("checksum error 5");
      exit(EXIT_FAILURE);
    }

    packet=reverse(packet);
    if(packet.control!=ACK){
      perror("receiving ACK packet.");
      exit(EXIT_FAILURE);
    }
    if(packet.ack_number!=socket->seq_number+1){
      perror("receiving ack number.");
      exit(EXIT_FAILURE);
    }
    #ifdef  DEBUG
    printf("Received ACK packet ack_number: %u\n",packet.ack_number);
    #endif
    socket->state=CLOSING_BY_HOST;
    #ifdef  DEBUG
    printf("State has changed to CLOSING_BY_HOST\n");
    #endif

    /*Receiving FINACK*/
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    status=recvfrom(socket->sd,(void*)buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len);
    if(status==-1){
      perror("receiving FINACK packet");
      exit(EXIT_FAILURE);
    }
    memcpy(&packet,buf,sizeof(microtcp_header_t));
    checksum=ntohl(packet.checksum);
    packet.checksum=0;
    if(checksum != crc32((const uint8_t*)&packet,sizeof(microtcp_header_t))){
      perror("checksum error 6");
      exit(EXIT_FAILURE);
    }

    packet=reverse(packet);
    if(packet.control!=FINACK){
      perror("receiving SYN packet");
    }
    #ifdef  DEBUG
    printf("Received FINACK packet with sequence number: %u\n",packet.seq_number);
    #endif
    /*Sending ACK packet*/
    socket->ack_number=packet.seq_number+1;
    socket->seq_number++;

    packet=create_header(socket->seq_number,ACK,0,socket->ack_number,0);
    packet.checksum=htonl(crc32((uint8_t*)&packet,sizeof(microtcp_header_t)));
    #ifdef  DEBUG
    printf("Sending ACK packet with sequence number: %lu\n",socket->seq_number);
    #endif
    if(sendto(socket->sd,(void*)&packet,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
      perror("sending SYN packet");
      exit(EXIT_FAILURE);
    }
    /*Connection Closed*/
    socket->state=CLOSED;
    #ifdef  DEBUG
    printf("State has changed to CLOSED\n");
    #endif
  }else{
    perror("Invalid how value");
  }
  free(socket->address);
  shutdown(socket->sd,how);
  return 0;
}
int min(int,int,int);
ssize_t microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length, int flags){
    int i;
    size_t data_sent=0;
    int bytes_to_send;
    int chunks;
    int remaining = length;
    microtcp_header_t header;
    void *send_buf;
    char recv_buf[MICROTCP_RECVBUF_LEN];
    int cwnd;
    int status;
    int cwnd_inc=0;
    struct timeval timeout;

    timeout. tv_sec = 0;
    timeout. tv_usec = MICROTCP_ACK_TIMEOUT_US;
    if (setsockopt(socket->sd, SOL_SOCKET, SO_RCVTIMEO, & timeout ,sizeof( struct timeval)) < 0){
        perror("setsockopt");
    }
    int starting_seq;
    while( data_sent < length){
        starting_seq=socket->seq_number;
        bytes_to_send = min( socket->curr_win_size , socket->cwnd ,remaining);
        chunks = bytes_to_send / MAX_PAYLOAD_SIZE;
        for(i = 0; i < chunks;i++){
            header=create_header(socket->seq_number+MAX_PAYLOAD_SIZE,ACK,MAX_PAYLOAD_SIZE,socket->ack_number,socket->init_win_size-socket->buf_fill_level);
            send_buf=calloc(1,MICROTCP_MSS);
            memcpy(send_buf,&header,sizeof(microtcp_header_t));
            memcpy(send_buf+sizeof(microtcp_header_t),buffer+data_sent+i*MAX_PAYLOAD_SIZE,MAX_PAYLOAD_SIZE);
            header.checksum=htonl(crc32((uint8_t*)send_buf,MICROTCP_MSS));
            memcpy(send_buf,&header,sizeof(microtcp_header_t));
            //print
            #ifdef  DEBUG
            printf("Sending packet with sequence number: %lu, i: %d, chunks: %d\n",socket->seq_number+MAX_PAYLOAD_SIZE,i,chunks);
            #endif
            if(sendto(socket->sd,send_buf,MICROTCP_MSS,flags,(struct sockaddr*)socket->address,socket->address_len)==-1){
                perror("sending packet");
                exit(EXIT_FAILURE);
            }
            socket->seq_number+=MAX_PAYLOAD_SIZE;
            free(send_buf);
        }
    /* Check if there is a semi - filled chunk
    */ 
        int temp_chunks=chunks;
        if(bytes_to_send % (MAX_PAYLOAD_SIZE)!=0){
            #ifdef  DEBUG
            printf("Inside semi-filled chunk\n");
            #endif
            chunks++;
            header=create_header(socket->seq_number+bytes_to_send % MAX_PAYLOAD_SIZE,ACK,bytes_to_send % MAX_PAYLOAD_SIZE,socket->ack_number,socket->init_win_size-socket->buf_fill_level);
            send_buf=calloc(1,sizeof(microtcp_header_t)+bytes_to_send % MAX_PAYLOAD_SIZE);
            memcpy(send_buf,&header,sizeof(microtcp_header_t));
            memcpy(send_buf+sizeof(microtcp_header_t),buffer+data_sent+i*MAX_PAYLOAD_SIZE,bytes_to_send % MAX_PAYLOAD_SIZE);
            header.checksum=htonl(crc32((uint8_t*)send_buf,sizeof(microtcp_header_t)+bytes_to_send %MAX_PAYLOAD_SIZE));
            memcpy(send_buf,&header,sizeof(microtcp_header_t));
            //print
            #ifdef  DEBUG
            printf("Sending packet with sequence number: %lu, i: %d, chunks: %d\n",socket->seq_number+bytes_to_send % MAX_PAYLOAD_SIZE,i,chunks);
            #endif  //DEBUG 

            if(sendto(socket->sd,send_buf,sizeof(microtcp_header_t)+bytes_to_send % MAX_PAYLOAD_SIZE,flags,(struct sockaddr*)socket->address,socket->address_len)==-1){
                perror("sending packet");
                exit(EXIT_FAILURE);
            }
            socket->seq_number+=bytes_to_send % MAX_PAYLOAD_SIZE;
            free(send_buf);
        }
    /* Get the ACKs */
        uint32_t prev_ack=-1;
        int count=0;
        int ack_check=1;
        for(i = 0; i < chunks;i++){
            status=recvfrom(socket->sd,(void*)recv_buf,MICROTCP_RECVBUF_LEN,flags,(struct sockaddr*)socket->address,&socket->address_len);
            if(status==-1){
                socket->ssthresh=socket->cwnd/2;
                socket->cwnd=(MAX_PAYLOAD_SIZE<socket->ssthresh)?MAX_PAYLOAD_SIZE:socket->ssthresh;
                if(i!=0){
                  #ifdef  DEBUG
                  printf("Inside Time Out\n");
                  #endif
                    //fast retransmit
                    data_sent=data_sent+(header.ack_number-starting_seq);
                    remaining=remaining-(header.ack_number-starting_seq);
                    socket->seq_number=header.ack_number;
                    ack_check=0;
                    break;
                }else{
                  socket->seq_number=starting_seq;
                  ack_check=0;
                  break;
                }
            }
            memcpy(&header,recv_buf,sizeof(microtcp_header_t));
            microtcp_header_t temp=reverse(header);
            uint32_t checksum=ntohl(header.checksum);
            int size=sizeof(microtcp_header_t)+temp.data_len;
            header.checksum=0;
            memcpy(recv_buf,&header,sizeof(microtcp_header_t));
            if((uint32_t)checksum != crc32((const uint8_t*)&recv_buf,size)){
                perror("checksum error 7");
                exit(EXIT_FAILURE);
            }
            header=reverse(header);
            socket->curr_win_size=header.window;
            //print
            #ifdef  DEBUG
            printf("Received ACK packet with ack number: %u\n",header.ack_number);
            #endif
            if(header.ack_number==prev_ack){
                ack_check=0;
                count++;
            }else{
                prev_ack=header.ack_number;
                count=0;
                uint32_t calculated_ack;
                if(i!=temp_chunks){
                  //check if ack received is what we sended
                  calculated_ack=(i+1)*(MAX_PAYLOAD_SIZE)+starting_seq;

                }else{
                  calculated_ack=temp_chunks*(MAX_PAYLOAD_SIZE) + (bytes_to_send % (MAX_PAYLOAD_SIZE))+starting_seq;
                }
                if(header.ack_number!=calculated_ack){
                  //print both
                  printf("Wrong ->Expected Acknowledgement Number : %d\n",calculated_ack);
                }
                //slow start
                if(socket->cwnd<=socket->ssthresh){
                  cwnd_inc=1;
                  socket->cwnd+=(MAX_PAYLOAD_SIZE);
                }
            }
            if(count==3){
              //fast retransmit
              data_sent=data_sent+(header.ack_number-starting_seq);
              remaining=remaining-(header.ack_number-starting_seq);
              socket->seq_number=header.ack_number;
              ack_check=0;
              socket->ssthresh=socket->cwnd/2;
              socket->cwnd/2+1;
              break;
            }
            
        }
        if(socket->curr_win_size==0){
          //send a packet without payload
          header=create_header(socket->seq_number,ACK,0,socket->ack_number,socket->init_win_size-socket->buf_fill_level);
          header.checksum=htonl(crc32((uint8_t*)&header,sizeof(microtcp_header_t)));
          #ifdef  DEBUG
          printf("Sending ACK packet with ack number: %lu\n",socket->ack_number);
          #endif
          if(sendto(socket->sd,(void*)&header,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
              perror("sending ACK packet");
              exit(EXIT_FAILURE);
          }
          
          while(recvfrom(socket->sd,(void*)recv_buf,MICROTCP_RECVBUF_LEN,flags,(struct sockaddr*)socket->address,&socket->address_len)==-1){
              header=create_header(socket->seq_number,ACK,0,socket->ack_number,socket->init_win_size-socket->buf_fill_level);
              header.checksum=htonl(crc32((uint8_t*)&header,sizeof(microtcp_header_t)));
              #ifdef  DEBUG
              printf("Sending ACK packet with ack number: %lu\n",socket->ack_number);
              #endif
              if(sendto(socket->sd,(void*)&header,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
                  perror("sending ACK packet");
                  exit(EXIT_FAILURE);
              }
          }
          //copy header of recvbuf to header
          memcpy(&header,recv_buf,sizeof(microtcp_header_t));
          microtcp_header_t temp=reverse(header);
          uint32_t checksum=ntohl(header.checksum);
          int size=sizeof(microtcp_header_t)+temp.data_len;
          header.checksum=0;
          memcpy(recv_buf,&header,sizeof(microtcp_header_t));
          if(checksum != crc32((const uint8_t*)&recv_buf,size)){
              perror("checksum error 8");
              exit(EXIT_FAILURE);
          }
          header=reverse(header);
          socket->curr_win_size=header.window;
        }
        /* Retransmissions */
        /* Update window */
        /* Update congestion control */
        if(ack_check){
          remaining -= bytes_to_send;
          data_sent += bytes_to_send;
        }

        if(!cwnd_inc&&socket->cwnd>socket->ssthresh){
          socket->cwnd+=(MAX_PAYLOAD_SIZE);
        }
        cwnd_inc=0;
     }
    return data_sent;
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags){

    microtcp_header_t packet;
    uint32_t checksum;
    int status;
    int recvbuf_size=0;
    int start_buf_level=socket->buf_fill_level;
    int size;
    struct timeval timeout;
    char recv_buf[MICROTCP_RECVBUF_LEN+sizeof(microtcp_header_t)];
    timeout.tv_sec = 0;
    timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;
    if (setsockopt(socket->sd, SOL_SOCKET, SO_RCVTIMEO, & timeout ,sizeof( struct timeval)) < 0){
        perror("setsockopt");
      }
    if(socket->state==CLOSING_BY_PEER){
      return -1;
    }
    while(1){
      status=recvfrom(socket->sd,recv_buf,(MAX_PAYLOAD_SIZE)+sizeof(microtcp_header_t),flags,(struct sockaddr*)socket->address,&socket->address_len);
      if(status==-1){
          perror("receiving packet");
          return -EXIT_FAILURE;
      }
      memcpy(&packet,recv_buf,sizeof(microtcp_header_t));
      checksum=ntohl(packet.checksum);
      microtcp_header_t temp=reverse(packet);
      size=sizeof(microtcp_header_t)+temp.data_len;
      packet.checksum=0;
      memcpy(recv_buf,&packet,sizeof(microtcp_header_t));
       if(checksum != crc32((const uint8_t*)recv_buf,size)){
           perror("checksum error 9");
           continue;
      }
      packet=reverse(packet);
      if(packet.control==FINACK&&socket->fun==SERVER){
          memcpy(buffer,socket->recvbuf+start_buf_level,socket->buf_fill_level-start_buf_level);
          #ifdef  DEBUG
          printf("Received FINACK packet with sequence number: %u\n",packet.seq_number);
          #endif
          socket->state=CLOSING_BY_PEER;
          socket->ack_number=packet.seq_number+1;
          return socket->buf_fill_level-start_buf_level;
      }
      if(packet.control==ACK){
         #ifdef  DEBUG
          printf("Received ACK packet with sequence number: %u and ack_number: %u\n",packet.seq_number,packet.ack_number);
          #endif
          //window-x
          if(packet.seq_number!=socket->ack_number+packet.data_len){
              //send DUP ACK
              packet=create_header(socket->seq_number,ACK,0,socket->ack_number,socket->init_win_size-socket->buf_fill_level);
              packet.checksum=htonl(crc32((uint8_t*)&packet,sizeof(microtcp_header_t)));
              #ifdef  DEBUG
              printf("Sending ACK packet with ack number: %lu\n",socket->ack_number);
              #endif
              if(sendto(socket->sd,(void*)&packet,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
                  perror("sending ACK packet");
                  exit(EXIT_FAILURE);
              }
              return EXIT_FAILURE;
          }
          //sus
          if(packet.ack_number!= socket->seq_number){
              packet=create_header(socket->seq_number,ACK,0,socket->ack_number,socket->init_win_size-socket->buf_fill_level);
              packet.checksum=htonl(crc32((uint8_t*)&packet,sizeof(microtcp_header_t)));
              #ifdef  DEBUG
              printf("Sending ACK packet with ack number: %lu\n",socket->ack_number);
              #endif
              if(sendto(socket->sd,(void*)&packet,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
                  perror("sending ACK packet");
                  exit(EXIT_FAILURE);
              }
              return EXIT_FAILURE;
          }
          //copy recvbuf to socket->recv_buf
          memcpy(socket->recvbuf+socket->buf_fill_level,recv_buf+sizeof(microtcp_header_t),packet.data_len);
          socket->buf_fill_level+=packet.data_len;
          socket->ack_number=packet.seq_number;
          packet=create_header(socket->seq_number,ACK,0,socket->ack_number,socket->init_win_size-socket->buf_fill_level);
          packet.checksum=htonl(crc32((uint8_t*)&packet,sizeof(microtcp_header_t)));
          #ifdef  DEBUG
          printf("Sending ACK packet with ack number: %lu\n",socket->ack_number);
          #endif
          if(sendto(socket->sd,(void*)&packet,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)==-1){
              perror("sending ACK packet");
          }
          //copy recvbuf to socket->recvbuf
      }
      if(socket->buf_fill_level+(MAX_PAYLOAD_SIZE)>length){
        memcpy(buffer,socket->recvbuf+start_buf_level,socket->buf_fill_level-start_buf_level);
        //print the return
        int retval=socket->buf_fill_level-start_buf_level;
        socket->buf_fill_level=start_buf_level;
        return retval;
      }
      if( socket->buf_fill_level+(MAX_PAYLOAD_SIZE)>MICROTCP_RECVBUF_LEN){
        memcpy(buffer,socket->recvbuf+start_buf_level,socket->buf_fill_level-start_buf_level);
        //print the return
        int retval=socket->buf_fill_level-start_buf_level;
        socket->buf_fill_level=0;
        return retval;
      }

    }
    return 0;
}

void print_header(microtcp_header_t header){
    printf("seq_number: %u\n",header.seq_number);
    printf("ack_number: %u\n",header.ack_number);
    printf("control: %u\n",header.control);
    printf("data_len: %u\n",header.data_len);
    printf("window: %u\n",header.window);
    printf("future_use0: %u\n",header.future_use0);
    printf("future_use1: %u\n",header.future_use1);
    printf("future_use2: %u\n",header.future_use2);
    printf("checksum: %u\n",header.checksum);
}

int min(int a ,int b , int c){
    if(a<b){
        if(a<c){
            return a;
        }else{
            return c;
        }
    }else{
        if(b<c){
            return b;
        }else{
            return c;
        }
    }
    return a;
}
