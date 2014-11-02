/**********************
*
* Progam Name: MP1. Membership Protocol
* 
* Code authors: Yogesh Rane
*
* Current file: mp1_node.c
* About this file: Member Node Implementation
* 
***********************/

#include "mp1_node.h"
#include "emulnet.h"
#include "MPtemplate.h"
#include "log.h"

/*
 *
 * Routines for introducer and current time.
 *
 */

char NULLADDR[] = {0,0,0,0,0,0};
int isnulladdr( address *addr){
    return (memcmp(addr, NULLADDR, 6)==0?1:0);
}

/* 
Return the address of the introducer member. 
*/
address getjoinaddr(void){

    address joinaddr;

    memset(&joinaddr, 0, sizeof(address));
    *(int *)(&joinaddr.addr)=1;
    *(short *)(&joinaddr.addr[4])=0;

    return joinaddr;
}


/*
 *
 * Message Processing routines.
 *
 */

/* 
Received a JOINREQ (joinrequest) message.
*/
void Process_joinreq(void *env, char *data, int size)
{
	/* <your code goes in here> */
// 	Code added by Yogesh 09/21

	address *toaddr = (address *) data;			//Get New node's address
	member *intro = (member *) env;

	if (intro->count == 0)
	{	// Only run for the first time JOINREQ is called to update first element of introducer's list
		memcpy(&(intro->memberlist[intro->count].nodeaddr), &intro->addr, sizeof(address));
		intro->memberlist[intro->count].flag = 0;
		intro->memberlist[intro->count].heartbeat = 1;
		intro->memberlist[intro->count].time = globaltime;
		intro->count++;
	}

//	Add other nodes in introducer's list in the subsequent indices
	memcpy(&(intro->memberlist[intro->count].nodeaddr), toaddr, sizeof(address));
	intro->memberlist[intro->count].flag = 0;
	intro->memberlist[intro->count].heartbeat = 1;
	intro->memberlist[intro->count].time = globaltime;

	address joinaddr = getjoinaddr();
	size_t msgsize = sizeof(messagehdr) + sizeof(mlist *);
	messagehdr *msghdr = malloc(msgsize);

	msghdr->msgtype=JOINREP;  // Compose response message
	memcpy((mlist *)(msghdr+1), &intro->memberlist, sizeof(mlist *));

//	Send message by to the node with pointer to the membership list of the introducer
	MPp2psend(&joinaddr, toaddr, (char *)msghdr, msgsize);

	logNodeAdd(&joinaddr, toaddr); //New node has been added to introducer's member list
	intro->count++;
	return;
}

/* 
Received a JOINREP (joinreply) message. 
Update node's membership list with the
one it received from the introducer.
*/
void Process_joinrep(void *env, char *data, int size)
{
	member *node = (member *)env;		// The node itself

	mlist **introlist = (mlist **)data;	// As data * itself is pointer
	mlist *firstnode = *introlist;      // Get the membership list node had received from introducer

//	At first index of all the nodes' membership list introducer will be stored
//	This way all the nodes will have members in the same order in their membership list
	memcpy(&(node->memberlist[0].nodeaddr), &firstnode->nodeaddr, sizeof(address));
	firstnode += 1;
	node->memberlist[0].flag = 0;
	node->memberlist[0].heartbeat = 1;
	node->memberlist[0].time = globaltime;
	node->ingroup = 1;					// Now the node is in the group
	logNodeAdd(&node->addr, &node->memberlist[0].nodeaddr);

//  Start from the second index and go on adding members from the introducer's membership list
	int i = 1;
//	while (firstnode->nodeaddr.addr[0] != '\0' && i < MAX_NNB)
	while (memcmp(&firstnode->nodeaddr, NULLADDR, sizeof(address)) != 0 && i < MAX_NNB)
	{
		node->memberlist[i].flag = 0;
		node->memberlist[i].heartbeat = firstnode->heartbeat;
		node->memberlist[i].time = globaltime;
		memcpy(&(node->memberlist[i].nodeaddr), &(firstnode->nodeaddr), sizeof(address));

		logNodeAdd(&node->addr, &node->memberlist[i].nodeaddr);
		i++;
		firstnode += 1;
	}

    return;
}


void Process_gossip(void *env, char *data, int size)
{
	member *node = (member *)env;    //the Receiver
	int new = 0;
	mlist **memberlist = (mlist **)data;
	mlist *memlist = *memberlist;    //the membership list of the sender

	int j = 0;
	int loopcount = 0;
		while (memcmp(&memlist->nodeaddr, NULLADDR, sizeof(address)) != 0 && loopcount < MAX_NNB)
		{
				if (memlist->flag == 2) // The node is not deleted
				{
					memlist += 1;
					loopcount++;
					new = 0;
					j = 0;
					continue;
				}
				while ((memcmp(&node->memberlist[j].nodeaddr, &memlist->nodeaddr, 4*sizeof(char)) != 0) && j < MAX_NNB)//((node->memberlist[j].nodeaddr.addr[0]) != (targetnode->nodeaddr.addr[0]))
				{
					if (node->memberlist[j].flag == 2)
					{
						j++;
						new = 0;
						continue;
					}
					if (node->memberlist[j].nodeaddr.addr[0] == '\0')  //New node has to be added to the membership list
					{
						memcpy(&(node->memberlist[j].nodeaddr), &(memlist->nodeaddr), sizeof(address));  //Maybe wrong
//						node->memberlist[j].flag = 0;
						node->memberlist[j].heartbeat = memlist->heartbeat;
						node->memberlist[j].time = globaltime;
						logNodeAdd(&node->addr, &memlist->nodeaddr);
						new = 1;
						printf("\nNode added in gossip: %d", node->memberlist[j].nodeaddr.addr[0]);
						break;
					}
					j++;
				}
				if (new == 0 && j < MAX_NNB)  // Node already existed in the membership list of the target. Update it's entry
				{
					node->memberlist[j].flag = memlist->flag;
					if (node->memberlist[j].heartbeat < memlist->heartbeat)
					{
//						Only update if it receives incremented heartbeat
						node->memberlist[j].heartbeat = memlist->heartbeat;
						node->memberlist[j].time = globaltime;
					}
				}
				j = 0;
				memlist += 1;
				new = 0;
				loopcount++;
		}

	return;
}

/* 
Array of Message handlers. 
*/
void ( ( * MsgHandler [20] ) STDCLLBKARGS )={
/* Message processing operations at the P2P layer. */
    Process_joinreq, 
    Process_joinrep,
    Process_gossip
};

/* 
Called from nodeloop() on each received packet dequeue()-ed from node->inmsgq. 
Parse the packet, extract information and process. 
env is member *node, data is 'messagehdr'. 
*/
int recv_callback(void *env, char *data, int size){

    member *node = (member *) env;
    messagehdr *msghdr = (messagehdr *)data;
    char *pktdata = (char *)(msghdr+1);

    if(size < sizeof(messagehdr)){
#ifdef DEBUGLOG
        LOG(&((member *)env)->addr, "Faulty packet received - ignoring");
#endif
        return -1;
    }

#ifdef DEBUGLOG
    LOG(&((member *)env)->addr, "Received msg type %d with %d B payload", msghdr->msgtype, size - sizeof(messagehdr));
#endif

    if((node->ingroup && msghdr->msgtype >= 0 && msghdr->msgtype <= DUMMYLASTMSGTYPE)
        || (!node->ingroup && msghdr->msgtype==JOINREP))            
            /* if not yet in group, accept only JOINREPs */
        MsgHandler[msghdr->msgtype](env, pktdata, size-sizeof(messagehdr));
    /* else ignore (garbled message) */
    free(data);

    return 0;

}

/*
 *
 * Initialization and cleanup routines.
 *
 */

/* 
Find out who I am, and start up. 
*/
int init_thisnode(member *thisnode, address *joinaddr){
    
    if(MPinit(&thisnode->addr, PORTNUM, (char *)joinaddr)== NULL){ /* Calls ENInit */
#ifdef DEBUGLOG
        LOG(&thisnode->addr, "MPInit failed");
#endif
        exit(1);
    }
#ifdef DEBUGLOG
    else LOG(&thisnode->addr, "MPInit succeeded. Hello.");
#endif

    thisnode->bfailed=0;
    thisnode->inited=1;
    thisnode->ingroup=0;
    thisnode->memberlist = malloc(MAX_NNB * sizeof(mlist)); //Allocate memory for membership list
    /* node is up! */

    return 0;
}


/* 
Clean up this node. 
*/
int finishup_thisnode(member *node){

	/* Free up the membership lists for nodes */
	free(node->memberlist);
    return 0;
}


/* 
 *
 * Main code for a node 
 *
 */

/* 
Introduce self to group. 
*/
int introduceselftogroup(member *node, address *joinaddr){
    
    messagehdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if(memcmp(&node->addr, joinaddr, 4*sizeof(char)) == 0){
        /* I am the group booter (first process to join the group). Boot up the group. */
#ifdef DEBUGLOG
        LOG(&node->addr, "Starting up group...");
#endif

        node->ingroup = 1;
        logNodeAdd(&node->addr, joinaddr);
    }
    else{
        size_t msgsize = sizeof(messagehdr) + sizeof(address);
        msg=malloc(msgsize);

    /* create JOINREQ message: format of data is {struct address myaddr} */
        msg->msgtype=JOINREQ;
        memcpy((char *)(msg+1), &node->addr, sizeof(address));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        LOG(&node->addr, s);
#endif

    /* send JOINREQ message to introducer member. */
        MPp2psend(&node->addr, joinaddr, (char *)msg, msgsize);
        
        free(msg);
    }

    return 1;

}

/* 
Called from nodeloop(). 
*/
void checkmsgs(member *node){
    void *data;
    int size;

    /* Dequeue waiting messages from node->inmsgq and process them. */
	
    while((data = dequeue(&node->inmsgq, &size)) != NULL) {
        recv_callback((void *)node, data, size); 
    }
    return;
}


/* 
Executed periodically for each member. 
Performs necessary periodic operations. 
Called by nodeloop(). 
*/

void nodeloopops(member *node){

	/* <your code goes in here> */
	int j = 0;
	while (memcmp(&node->addr, &node->memberlist[j].nodeaddr, sizeof(char) * 4) != 0)
	{
		j++;
	}
	if (node->memberlist[j].flag == 2 || node->memberlist[j].flag == 1)
		return;

	j = 0;
	while (memcmp(&node->memberlist[j].nodeaddr, NULLADDR, sizeof(address)) != 0 && j < MAX_NNB)
	{
	        if (memcmp(&node->addr, &node->memberlist[j].nodeaddr, sizeof(char) * 4) == 0)
	        {
	            node->memberlist[j].flag = 0;
	        	node->memberlist[j].heartbeat++;
	            node->memberlist[j].time = globaltime;
	        } else {
	        	if (node->memberlist[j].flag == 0)
	        	{
	        		if ((globaltime - node->memberlist[j].time) > FAIL_TIME)
	        		{
	        			node->memberlist[j].flag = 1;
	        		}
	        	}
	        	else {
	        		if ((globaltime - node->memberlist[j].time) > CLEAN_TIME && node->memberlist[j].failed != 1)
	        		{
						logNodeRemove(&node->addr, &node->memberlist[j].nodeaddr);
						printf("\nNode Removed: %d by %d", node->memberlist[j].nodeaddr.addr[0], node->addr.addr[0]);
						node->memberlist[j].heartbeat = 0;
						node->memberlist[j].flag = 2;			//2 means Fatality!
						node->memberlist[j].time = 0;
						node->memberlist[j].failed = 1;
	        		}
	        	}
	        }
	        j++;
	  }

//	Now send gossip message to a random node
	size_t msgsize = sizeof(messagehdr) + sizeof(mlist *);
	messagehdr *msghdr = malloc(msgsize);

	int t = 0;
	int i = 0;
	t = (rand() % EN_GPSZ) + 1;

	while ((t == node->addr.addr[0]) && t <= MAX_NNB)
	{
		t = (rand() % EN_GPSZ) + 1;
	}

	while (memcmp(&node->memberlist[i].nodeaddr, NULLADDR, sizeof(address)) != 0 && i < MAX_NNB)
	{
		if (t == node->memberlist[i].nodeaddr.addr[0] && node->memberlist[i].flag != 2 && node->memberlist[i].failed != 1)
		{
			msghdr->msgtype=GOSSIP;
			memcpy((mlist *)(msghdr+1), &node->memberlist, sizeof(mlist *));
			address *toaddr = &(node->memberlist[i].nodeaddr);
//			printf("\n Gossip Message being sent to %d", t);
			MPp2psend(&node->addr,toaddr, (char *)msghdr, msgsize);
			break;
		}
		else if (memcmp(&node->memberlist[i].nodeaddr, NULLADDR, sizeof(address)) != 0 && node->memberlist[i].flag == 2 && node->memberlist[i].failed != 1)
		{
			break;
		}
		i++;
	}

    return;
}

/* 
Executed periodically at each member. Called from app.c.
*/
void nodeloop(member *node){
    if (node->bfailed) return;

    checkmsgs(node);

    /* Wait until you're in the group... */
    if(!node->ingroup) return ;

    /* ...then jump in and share your responsibilites! */
    nodeloopops(node);
    
    return;
}

/* 
All initialization routines for a member. Called by app.c. 
*/
void nodestart(member *node, char *servaddrstr, short servport){

    address joinaddr=getjoinaddr();

    /* Self booting routines */
    if(init_thisnode(node, &joinaddr) == -1){

#ifdef DEBUGLOG
        LOG(&node->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if(!introduceselftogroup(node, &joinaddr)){
        finishup_thisnode(node);
#ifdef DEBUGLOG
        LOG(&node->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/* 
Enqueue a message (buff) onto the queue env. 
*/
int enqueue_wrppr(void *env, char *buff, int size){    return enqueue((queue *)env, buff, size);}

/* 
Called by a member to receive messages currently waiting for it. 
*/
int recvloop(member *node){
    if (node->bfailed) return -1;
    else return MPrecv(&(node->addr), enqueue_wrppr, NULL, 1, &node->inmsgq); 
    /* Fourth parameter specifies number of times to 'loop'. */
}
