/*Message header showing the fields used*/

struct message
{
    unsigned int type;	// 0 means message, 1 means token			
    unsigned int seq;		
    unsigned int messageNum;				
    int senderPID;
    char senderIP[100];
    int sender_listening_port; //So that broadcast_server can broadcast to it.
};

struct token_struct
{
    int seq;
    int front,rear;
    struct message m[100];
};
     
struct buffer
{
    int front,rear;
    struct message m[100];
};