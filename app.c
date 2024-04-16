/*Author: Stanly Samuel*/

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h> 
#include "common.h"
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>

#define MULTICAST_GROUP "239.0.0.1"

int sock = 0, m_sock_fd, broadcast_server_port, listening_port;
struct sockaddr_in serv_addr, m_address;
int slen = sizeof(serv_addr);
int m_addrlen = sizeof(m_address);
char hostIP[100];
struct ip_mreq mreq; //For Multicast
int check_recv_seq[100000];

void getOwnIP();
void *send_messages(void *);
void *receive_messages(void *);

int main(int argc, char **argv)
{   
    getOwnIP();
    if(argc!=3)
    {
	fprintf(stderr,"Usage:  %s <Broadcast Server Port> <Multicast Listening Port>\n", argv[0]);
	exit(1);
    }

   broadcast_server_port = atoi(argv[1]);
   listening_port = atoi(argv[2]);
    
    //Socket for sending messages
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset((char *)&serv_addr, '0', sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(broadcast_server_port); //Broadcast_Server Port

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_aton("127.0.0.1", &serv_addr.sin_addr) == 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    //Socket for receiving messages (Listening)
        /* Construct local address structure for listening for messages */
	memset((char *)&m_address, 0, sizeof(m_address));   /* Zero out structure */
	m_address.sin_family = AF_INET;
	m_address.sin_addr.s_addr = htonl(INADDR_ANY);
	m_address.sin_port = htons(listening_port);

	if((m_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))== -1)		/*Socket*/
	{
            perror("m_socket() failed");
            exit(1);
        }
        //Allow port reuse in UDP
        int one = 1; 
        setsockopt(m_sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        if(bind(m_sock_fd, (struct sockaddr *) &m_address, m_addrlen)==-1)	/*Bind*/
        {
            perror("m Bind error");
        }
       
        mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);         
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        //Add membership of this socket (m_sock_fd) to this multicast group
        if (setsockopt(m_sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		     &mreq, sizeof(mreq)) < 0) {
	 perror("setsockopt mreq");
	 exit(1);
        }  
        
        pthread_t send_messages_thread;
        pthread_t receive_messages_thread;
        
        if(pthread_create(&send_messages_thread, NULL, send_messages, NULL)) {

            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
        
        if(pthread_create(&receive_messages_thread, NULL, receive_messages, NULL)) {

            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
        
        if(pthread_join(send_messages_thread, NULL)) {

            fprintf(stderr, "Error joining thread\n");
            return 2;

         }

        if(pthread_join(receive_messages_thread, NULL)) {

            fprintf(stderr, "Error joining thread\n");
            return 2;

         }

    close(sock);
    return 0;
}

void* send_messages(void* param)
{    
    struct message m;
    //CReate message which is to be sent to the broadcast servers.
    printf("Send messages parallely to broadcast server %d\n", broadcast_server_port );            
    m.senderPID = getpid();
    m.messageNum = 1;
    m.sender_listening_port = listening_port;
    strcpy(m.senderIP, hostIP);

        
    do
    {
        sleep(rand() % 10);
        sendto(sock,(struct message *)&m,sizeof(m),0,(struct sockaddr *) &serv_addr, slen );
        printf("Sent message number %d from %d on IP %s on port %d \n",m.messageNum, m.senderPID, m.senderIP, broadcast_server_port);
        m.messageNum++;
    }while(m.messageNum<=2);
}

//Multicast Receive
void* receive_messages(void* param)
{
    printf("Parallely listen for messages from broadcast server on port %d\n", listening_port );
        
    while(1)
    {
        struct message message;
        recvfrom(m_sock_fd, (struct message *)&message, sizeof(message),0,(struct sockaddr *) &m_address, &m_addrlen);
        if(/*(strcmp(message.senderIP,hostIP)!=0 &&*/ message.senderPID != getpid() && check_recv_seq[message.seq]!=1) //Ignore messages that I sent
        {
            printf("Received (seq:%d, %d, %d, %s) \n",message.seq, message.messageNum, message.senderPID, message.senderIP);
            check_recv_seq[message.seq] = 1; //Discard duplicates
        }
    }
        
}

void getOwnIP()
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }


    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if( (strcmp(ifa->ifa_name,"wlan0")==0)&&( ifa->ifa_addr->sa_family==AF_INET) )
        {
            if (s != 0)
            {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            printf("\tInterface : <%s>\n",ifa->ifa_name );
            printf("\t  Address : <%s>\n", host);
            
            strcpy(hostIP, host);
        }
        else if( (strcmp(ifa->ifa_name,"enp1s0")==0)&&( ifa->ifa_addr->sa_family==AF_INET) )
        {
            if (s != 0)
            {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            printf("\tInterface : <%s>\n",ifa->ifa_name );
            printf("\t  Address : <%s>\n", host);
            strcpy(hostIP, host);
        }
        else if( (strcmp(ifa->ifa_name,"eth0")==0)&&( ifa->ifa_addr->sa_family==AF_INET) )
        {
            if (s != 0)
            {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            printf("\tInterface : <%s>\n",ifa->ifa_name );
            printf("\t  Address : <%s>\n", host);
            strcpy(hostIP, host);
        }
    }

    freeifaddrs(ifaddr);
}