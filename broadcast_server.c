/*Author: Stanly Samuel*/

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <semaphore.h>  /* Semaphore */
#include <pthread.h>
#include "common.h"

sem_t mutex;
static struct buffer buffer;
static struct buffer recv_buffer;
int sock_fd,app_sock_fd, m_sock_fd, dest_sock_fd, messageListeningPort, listeningPort, destPort, new_socket;
char* destIP;
struct sockaddr_in m_address, app_address, address, dest_address, sender; /* Broadcast address */
int addrlen = sizeof(address);
int m_addrlen = sizeof(m_address);
int destlen = sizeof(dest_address);
int applen = sizeof(app_address);
struct token_struct token;
struct message message;
struct buffer thread_local_buffer;
int i;
int application_multicast_port;
int flag;
//struct app_ports
//{
//    int front, rear;
//    int array_of_ports[100];
//    int check_port[10000000];
//}app_ports;

#define MULTICAST_GROUP "239.0.0.1"

void *wait_for_message(void *);
void *wait_for_token(void *);
void* init_token(void *);

int main(int argc,char **argv)
{
        sem_init(&mutex, 0, 1);
        flag=0;
        //Initialize
        buffer.front = 0;
        buffer.rear = 0;
        recv_buffer.front = 0;
        recv_buffer.rear = 0;
//        app_ports.front = 0;
//        app_ports.rear = 0;
        
        if(argc<5)
	{
		fprintf(stderr,"Usage:  %s <Message Listening Port> <Token Listening Port> <Destination IP Address> <Destination-Port> \"T\" (Optional: Should be present for token initializer)\n", argv[0]);
		exit(1);
	}
	
        messageListeningPort = atoi(argv[1]);
	listeningPort = atoi(argv[2]);
	destIP = argv[3];
	destPort = atoi(argv[4]);
        
	/* Construct local address structure for listening for token */
	memset((char *)&address, 0, sizeof(address));   /* Zero out structure */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(listeningPort);

	if((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))== -1)		/*Socket*/
	{
            perror("socket() failed");
            exit(1);
        }
        
        if(bind(sock_fd, (struct sockaddr *) &address, addrlen)==-1)	/*Bind*/
        {
            perror("Bind error ");
        }
        
        /* Construct local address structure for listening for messages */
	memset((char *)&m_address, 0, sizeof(m_address));   /* Zero out structure */
	m_address.sin_family = AF_INET;
	m_address.sin_addr.s_addr = htonl(INADDR_ANY);
	m_address.sin_port = htons(messageListeningPort);

	if((m_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))== -1)		/*Socket*/
	{
            perror("m_socket() failed");
            exit(1);
        }
        
        if(bind(m_sock_fd, (struct sockaddr *) &m_address, m_addrlen)==-1)	/*Bind*/
        {
            perror("m Bind error");
        }
        
        pthread_t wait_for_message_thread;
        pthread_t wait_for_token_thread;
        pthread_t init_token_thread;
        
        if(pthread_create(&wait_for_message_thread, NULL, wait_for_message, NULL)) {

            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
        
        if(pthread_create(&wait_for_token_thread, NULL, wait_for_token, NULL)) {

            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
        
        //For initializer thread
        if(argv[5])
        {
            sleep(3);
        if(pthread_create(&init_token_thread, NULL, init_token, NULL)) {

            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
        
        if(pthread_join(init_token_thread, NULL)) {

            fprintf(stderr, "Error joining thread\n");
            return 2;

         }
        else{
            printf("Initializer thread has returned\n");
        }
        }
        if(pthread_join(wait_for_message_thread, NULL)) {

            fprintf(stderr, "Error joining thread\n");
            return 2;

         }

        if(pthread_join(wait_for_token_thread, NULL)) {

            fprintf(stderr, "Error joining thread\n");
            return 2;

         }
        
        
        return 0;
}

void* init_token(void* param)
{
    
    //Initialize token which is to be sent to the broadcast servers.   
    struct token_struct token;
    token.seq = 0;
    token.front = 0;
    token.rear = 0;
    
    if ((dest_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) /*Socket for sending*/
    {
        printf("\n Socket creation error \n");
    }

    memset((char *)&dest_address, '0', sizeof(dest_address));

    dest_address.sin_family = AF_INET;
    dest_address.sin_port = htons(destPort);

    if(inet_aton(destIP, &dest_address.sin_addr) == 0)
    {
        printf("\nInvalid address/ Address not supported \n");
    }

    sleep(2);
    sendto(dest_sock_fd,(struct token_struct *)&token,sizeof(token),0,(struct sockaddr *) &dest_address, destlen );

    printf("First Token sent to IP %s and PORT %d \n\n", destIP, destPort);
    
    close(dest_sock_fd);
}
void* wait_for_message(void* param)
{
    /*This thread waits for messages and stores them in queue*/
           while(1)
            {
            printf("Wait for message on port %d\n", messageListeningPort);
            recvfrom(m_sock_fd,(struct message *)&message, sizeof(message),0,(struct sockaddr *) &m_address, &m_addrlen);
            printf("Received message number %d from %d on IP %s) \n",message.messageNum, message.senderPID, message.senderIP);
            application_multicast_port = message.sender_listening_port;
//  Can record which sender app is listening on which port but not needed as we will be using multicast            
//            if(app_ports.check_port[message.sender_listening_port] != 1)
//            {
//                //Means new port
//                app_ports.array_of_ports[app_ports.rear] = message.sender_listening_port;
//                app_ports.rear++;
//                app_ports.check_port[message.sender_listening_port] = 1;
//            }
//            
//            printf("Unique processes (who are listening on this port currently) sent to this server so far");
//            for (i = app_ports.front; i<app_ports.rear ; i++)
//            {
//                printf("(%d) \n", app_ports.array_of_ports[i]);
//            }
            
            //Store messages as they come in thread local buffer
            thread_local_buffer.m[thread_local_buffer.rear] = message;
            ++thread_local_buffer.rear; 
            
//            printf("Thread Local Buffer contents are \n");
//
//            for (i = thread_local_buffer.front; i<thread_local_buffer.rear ; i++)
//            {
//                printf("(%d, %d, %s) \n", thread_local_buffer.m[i].messageNum, thread_local_buffer.m[i].senderPID, thread_local_buffer.m[i].senderIP);
//            }
//            
//            printf("Thread Local Buffer contents size %d \n", thread_local_buffer.rear - thread_local_buffer.front);
//                        
            //When we get hold of mutex, then transfer these messages to shared buffer. Else messages will get lost.
            
            sem_wait(&mutex);            
            //Critical Section
            for (i = thread_local_buffer.front; i<thread_local_buffer.rear ; i++)
            {
                buffer.m[buffer.rear] = thread_local_buffer.m[i];
                buffer.rear++;
                thread_local_buffer.front++;
            }
                 
            sem_post(&mutex);
            

//            printf("Buffer contents are \n");
//
//            for (i = buffer.front; i<buffer.rear ; i++)
//            {
//                printf("(%d, %d, %s) \n", buffer.m[i].messageNum, buffer.m[i].senderPID, buffer.m[i].senderIP);
//            }
//            
//            printf("Buffer contents size %d \n", buffer.rear - buffer.front);
          }
}

void* wait_for_token(void* param)
{
    /* This thread waits for token and appends messages if present in queue*/
    int prev_token_number = 0;
    int curr_token_number = 0;
            while(1)
            {

                    //Receive Token and store in token struct
                    printf("Wait for token on port %d, and if token received send to port %d on IP %s \n", listeningPort, destPort, destIP );

                    recvfrom(sock_fd,(struct token_struct *)&token,sizeof(token),0,(struct sockaddr *) &address, &addrlen);
                    
                    //RECEIVE MESSAGES FROM TOKEN
                    sem_wait(&mutex);
                    curr_token_number = token.seq;
                    
                    printf("Token contains \n");

                    for (i = token.front; i<token.rear ; i++)
                    {
                          printf("Token Message (sequence: %d, %d, %d, %s) \n",token.m[i].seq, token.m[i].messageNum, token.m[i].senderPID, token.m[i].senderIP);
                          //seq between prev_token_number and curr_token_number
                          if(token.m[i].seq>=prev_token_number && token.m[i].seq<curr_token_number && curr_token_number != prev_token_number)
                          {
                              recv_buffer.m[recv_buffer.rear] = token.m[i];
                              recv_buffer.rear++;
                          }
                    }

                    printf("Token size %d \n", token.rear - token.front);
                    
                    prev_token_number = curr_token_number;
                    sem_post(&mutex);

//                    printf("Receiver Buffer contents are \n");
//
//                    for (i = recv_buffer.front; i<recv_buffer.rear ; i++)
//                    {
//                        printf("RECEIVER BUFFER: (seq: %d, %d, %d, %s) \n", recv_buffer.m[i].seq, recv_buffer.m[i].messageNum, recv_buffer.m[i].senderPID, recv_buffer.m[i].senderIP);
//                    }
//
//                    printf("Receiver buffer contents size %d \n", buffer.rear - buffer.front);                    

                    //SEND MESSAGES TO TOKEN
                    sem_wait(&mutex);
                    //When token is received, check if messages are present the in buffer to be sent
                    if(buffer.front < buffer.rear)
                    {
                        //If buffer not empty
                        //Messages are present, assign sequence number to each message, increment seq of token and send the token to destination.
                        //printf("\n Buffer not empty \n");
                        //Critical Section
                        for (i = buffer.front; i<buffer.rear ; i++)
                            {
                                buffer.m[i].seq = token.seq;
                                token.m[token.rear] = buffer.m[i];
                                token.rear++;
                                buffer.front++;
                                token.seq++;
                            }                        
                    }
                    else
                    {
                        //printf("\n Message Buffer empty \n");
                    }   
                    sem_post(&mutex);
                    
                    if ((dest_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) /*Socket for sending*/
                    {
                        printf("\n Socket creation error \n");
                    }

                    memset((char *)&dest_address, '0', sizeof(dest_address));

                    dest_address.sin_family = AF_INET;
                    dest_address.sin_port = htons(destPort);

                    if(inet_aton(destIP, &dest_address.sin_addr) == 0)
                    {
                        printf("\nInvalid address/ Address not supported \n");
                    }
                    
                    sleep(2);
                    sendto(dest_sock_fd,(struct token_struct *)&token,sizeof(token),0,(struct sockaddr *) &dest_address, destlen );

                    printf("Token sent to IP %s and PORT %d \n\n", destIP, destPort);
                    
                    //If any messages in receiver buffer, then broadcast to all the ports. (Simple for now)
                    //Use Multicast send here
                    
//                    for(i = app_ports.front; i< app_ports.rear; i++)
//                    {
//                        printf("Sending messages to %d ", app_ports.array_of_ports[i]);
//                        
//                  
//                  //Multicast sender      
                    if ((app_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) /*Socket for sending*/
                    {
                        printf("\n Socket creation error \n");
                    }

                    memset((char *)&app_address, '0', sizeof(app_address));

                    app_address.sin_family = AF_INET;
                    app_address.sin_port = htons(application_multicast_port);

                    app_address.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);

                    for(i = recv_buffer.front; i<recv_buffer.rear; i++ )
                    {
                        message = recv_buffer.m[i];
                        sendto(app_sock_fd,(struct message *)&message,sizeof(message),0,(struct sockaddr *) &app_address, applen );
                        recv_buffer.front++; //Since messages are delivered, no need to keep in receive buffer

                    }

                        //Send pending messages in recv_buffer to all processes  waiting
                        
                     close(app_sock_fd);
                    
//                    }
                    
                    }
}
//int split (const char *, char, char ***);


/*
 TCP client                UDP client
 * 
 * socket()             socket()
 * inet_pton()          inet_aton() //Like bind() I guess, Binds address to address struct field
 * connect()            sendto() //Here dest IP address must be specified everytime unlike sender
 * send() / write()     recvfrom()
 * recv() / read()      close()
 * close()
 
 */

/*
 * TCP server           UDP server
 *  
 * socket()             socket()
 * bind()               bind()
 * listen()             sendto()
 * accept()             recvfrom()
 * send() / write()     close()
 * recv() / read()
 * close()
 */
/*
int split (const char *str, char c, char ***arr)
{
    int count = 1;
    int token_len = 1;
    int i = 0;
    char *p;
    char *t; 

    p = str;
    while (*p != '\0')
    {
        if (*p == c)
            count++;
        p++;
    }

    *arr = (char**) malloc(sizeof(char*) * count);
    if (*arr == NULL)
        exit(1);

    p = str;
    while (*p != '\0')
    {
        if (*p == c)
        {
            (*arr)[i] = (char*) malloc( sizeof(char) * token_len );
            if ((*arr)[i] == NULL)
                exit(1);

            token_len = 0;
            i++;
        }
        p++;
        token_len++;
    }
    (*arr)[i] = (char*) malloc( sizeof(char) * token_len );
    if ((*arr)[i] == NULL)
        exit(1);

    i = 0;
    p = str;
    t = ((*arr)[i]);
    while (*p != '\0')
    {
        if (*p != c && *p != '\0')
        {
            *t = *p;
            t++;
        }
        else
        {
            *t = '\0';
            i++;
            t = ((*arr)[i]);
        }
        p++;
    }

    return count;
}
*/

/*

Algorithm:

while(true)
{	
	Store messages recieved by applications in a queue and wait for token.
	
	When Token is received,
		Pick message, increment sequence number and send messages that are pending.
		Attach all pending messages in a format.
		Send ACK for broadcast messages received (for seq no. curr to prev).
 	If none of the above to be done, send token to next in queue.
	
	prev=curr
	
	(Need to create a string processing function.)
}

How to get unique sequence number:
1) Token contains latest sequence number
2) Distributed Mutual Exclusion

How to check stability
1) Two rounds logic
2) Positive and Negative Ack

*/

/*
ROUGH

	c = split(buffer, ':', &arr);

    printf("found %d tokens.\n", c);

    for (i = 0; i < c; i++)
        {printf("string #%d: %s\n", i, arr[i]);}
	//seq = atoi(arr[1]);
	//seq++;
	//char str1[100] = "hi";
	//char str2[100] = "ddd";
	//temp = strcat(str1,str2); //Seg overflow error
	//printf("%s",temp);
	//i=0;
	//while(*(temp)!='\0')
	//{
	//	buffer[i++] = *(temp++);
	//}
	//If message received is a token
    	//if(strstr(binn_object_str(buffer, "type"),"TOKEN") !=NULL) /*Means contains substring TOKEN*/
	//{
       
		//printf("%s Recieved\n", buffer);
		
//		seq=binn_object_int32(buffer, "seq");
//		seq++;
        
		//Use binn to send packets across network
	//binn *obj;
  // create a new object
  	//obj = binn_object();

  // add values to it
  	/*binn_object_set_int32(obj, "seq", seq);
	binn_object_set_int32(obj, "ack", 0);
  	binn_object_set_str(obj, "type", "TOKEN");
	binn_object_set_str(obj, "message", "Hi");
	binn_object_set_str(obj, "destinationIP", "127.0.0.1");
	binn_object_set_str(obj, "destinationPort", "127.0.0.1");
         */
  // send over the network
  	//send(dest_sock_fd, binn_ptr(obj), binn_size(obj),0);
    
        
  // release the buffer
  	//binn_free(obj);

	//}
	
//close(sock_fd);
//close(dest_sock_fd);
