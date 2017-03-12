#include <xinu.h>
#include <stdio.h>
#include <stdlib.h>

syscall sendMsg(pid32 pid, umsg32 msg);
uint32 sendMsgs(pid32 pid, umsg32* msgs, uint32 msg_count);
umsg32 receiveMsg(void);
syscall receiveMsgs(umsg32* msgs, uint32 msgs_count);
uint32 sendnMsg(uint32 pid_count, pid32* pids, umsg32 msg);

void insertItem (pid32 pid, umsg32 msg);
umsg32 getItem (pid32 pid);
void messageSender (void);
void singleMessageReceiver (void);
void multipleMessageReceiver (uint32 msg_count);

# define QUEUE_SIZE 10
# define QUEUE_HEAD queuePointer[pid][0]
# define QUEUE_TAIL queuePointer[pid][1]
# define SIZE (QUEUE_SIZE - (queuePointer[pid][0] - queuePointer[pid][1])) % QUEUE_SIZE

umsg32 messageBuffer[10][QUEUE_SIZE] = {{0}};	// Message buffer initialized for storing the messages of each process
uint32 queuePointer[10][2] = {{0}};		// Pointer initialized for head and tail of circular queue of each process
pid32 receiver1, receiver2, receiver3;


int main(int argc, char **argv)
{

	receiver1 = create(multipleMessageReceiver, 4096, 50, "Receiver1", 1, 2);
	receiver2 = create(singleMessageReceiver, 4096, 50, "Receiver2", 0, 0);
	receiver3 = create(multipleMessageReceiver, 4096, 50, "Receiver3", 1, 3);
	
	resume(receiver1);
	resume(receiver2);
	resume(receiver3);

	resume(create(messageSender, 4096, 50, "Sender1", 0, 0));

	while(TRUE)
	{
		//Do nothing
	}
	

	return OK;
}

/* Process created for sending messages */
void messageSender(void) {
	umsg32 singleReceiverMessages = 0;
	umsg32 multipleReceiverMessages = 1000;
	uint32 testCount;
	for(testCount = 0; testCount<3; testCount++)
	{
		/* Test case for sending single message to single process (Receiver1) */

		if(sendMsg(receiver1, singleReceiverMessages++) == SYSERR)
		{
			kprintf("Message not sent.\n");
		}

		/* Test case for sending multiple messages to single process (Receiver3) */
		umsg32 msgs[8] = {1, 1, 2, 3, 5, 8, 13, 21};
		if(sendMsgs(receiver3, msgs, 8) == SYSERR)
		{
			kprintf("Multiple message send failed.\n");
		}

		/* Test case for sending single message to multiple processes (Receiver1, Receiver2, Receiver3) */
		pid32 receivers[3] = {receiver1, receiver2, receiver3};
		if (sendnMsg(3, receivers, multipleReceiverMessages++) != 3)
		{
			kprintf("Multiple receiver send failed.\n");
		}
	}
	
}

/* Process created for receiving single message */
void singleMessageReceiver(void) {
	while (TRUE)
	{
		umsg32 msg = receiveMsg();
		if (msg == SYSERR)
		{
			kprintf("Message not received.\n");
		}
	}	
}

/*Process created for receiving multiple messages at a time*/
void multipleMessageReceiver(uint32 msg_count) {
	while (TRUE)
	{
		umsg32 msgs[msg_count];
		if (receiveMsgs(msgs, msg_count) == SYSERR)
		{
			kprintf("%d messages not received.\n", msg_count);
		}
	}	
}

void insertItem (pid32 pid, umsg32 msg)
{
	messageBuffer[pid][QUEUE_TAIL] = msg;
	QUEUE_TAIL++;
	QUEUE_TAIL = QUEUE_TAIL % 8;
}

umsg32 getItem (pid32 pid)
{
	uint32 tempValue = messageBuffer[pid][QUEUE_HEAD];
	messageBuffer[pid][QUEUE_HEAD] = 0; 	// Remove the contents from the location
	QUEUE_HEAD++;
	QUEUE_HEAD = QUEUE_HEAD % 8;
	return tempValue;
}

/*------------------------------------------------------------------------
 *  sendMsg  -  pass a message to a process and start recipient if waiting
 *------------------------------------------------------------------------
 */
syscall	sendMsg (pid32 pid, umsg32 msg)
{
	intmask	mask;				// saved interrupt mask
	struct	procent *prptr;			// ptr to process' table entry

	mask = disable();
	if (isbadpid(pid)) {
		restore(mask);
		kprintf("Bad PID for process %d\t", pid);
		return SYSERR;
	}

	prptr = &proctab[pid];
	if (prptr->prstate == PR_FREE) {
		restore(mask);
		kprintf("Free Process\t");
		return SYSERR;
	}

	if (messageBuffer[pid][QUEUE_TAIL] != 0) //Queue of receiver is full
	{
		restore(mask);
		kprintf("Queue is Full\t");
		return SYSERR;
	}
	else
	{
		insertItem(pid, msg);
		kprintf("Message ""%d"" sent to process %d.\n", msg, pid);
	}

	/* If recipient waiting or in timed-wait make it ready */

	if (prptr->prstate == PR_RECV) {
		ready(pid);
	} else if (prptr->prstate == PR_RECTIM) {
		unsleep(pid);
		ready(pid);
	}
	restore(mask);				// Restore interrupts
	return OK;
}


/*------------------------------------------------------------------------
 *  receiveMsg  -  wait for a message and return the message to the caller
 *------------------------------------------------------------------------
 */
umsg32	receiveMsg(void)
{
	intmask	mask;				// saved interrupt mask		
	struct	procent *prptr;			// ptr to process' table entry	
	umsg32	msg;				// message to return

	pid32 pid = getpid();			//Get current process pid

	mask = disable();
	prptr = &proctab[currpid];

	if (QUEUE_HEAD == QUEUE_TAIL) 		//Queue is empty, No message waiting
	{
		prptr->prstate = PR_RECV;
		resched();			// Block until message arrives
	}
	msg = getItem (pid);			// Retrieve the message
	kprintf("Message ""%d"" received by process %d.\n", msg, pid);
	restore(mask);
	return msg;
}

/*------------------------------------------------------------------------
 *  sendMsgs  -  pass multiple messages to a process and start recipient if waiting
 *------------------------------------------------------------------------
 */
uint32 sendMsgs (pid32 pid, umsg32* msgs, uint32 msg_count)
{
	uint32 count = 0;			// Count initialized for keeping track of how many messages were sent correctly
	uint32 ret, i;

	for (i=0;i<msg_count;i++)
	{
		ret = sendMsg(pid,msgs[i]);
		if (ret == OK)
			count++;
	}

	if (count > 0)				// Return number of correctly sent messages
	{
		kprintf("%d of %d messages sent correctly.\n", count, msg_count);
		return count;
	}	
	else
		return SYSERR;
}


/*------------------------------------------------------------------------
 *  sendnMsg  -  pass message to multiple receiver processes
 *------------------------------------------------------------------------
 */
uint32 sendnMsg (uint32 pid_count, pid32* pids, umsg32 msg)
{
	uint32 count = 0; 			// Count initialized for keeping track of how many messages were sent correctly
	uint32 ret, i;

	for (i=0;i<pid_count;i++)
	{
		ret = sendMsg(pids[i],msg);
		if (ret == OK)
			count++;
	}

	if (count > 0)  			// Return number of correctly sent messages
	{
		kprintf("Message sent correctly to %d of %d processes.\n", count, pid_count);
		return count;
	}
	else
		return SYSERR;
}


/*------------------------------------------------------------------------
 *  receiveMsgs  -  wait then receive messages when queue reaches a certain size
 *------------------------------------------------------------------------
 */
syscall	receiveMsgs (umsg32* msgs, uint32 msg_count)
{
	intmask	mask;				// saved interrupt mask
	struct	procent *prptr;			// ptr to process' table entry

	pid32 pid = getpid(); 			//Get current process pid
	mask = disable();
	prptr = &proctab[currpid];


	if (msg_count > 10)			//Check if msg_count is larger than the queue size
		return SYSERR;
	else if (QUEUE_HEAD == QUEUE_TAIL) 	//Queue is empty, No message waiting
	{
		prptr->prstate = PR_RECV;
		resched();			// block until messages arrives
	}

	while (SIZE < msg_count)
	{
		prptr->prstate = PR_RECV;
		resched();			// block until messages arrives
	}


	int i;
	for(i=0;i<msg_count;i++)		// retrieve messages in order
	{
		msgs[i] = receiveMsg();
	}

	kprintf("%d messages received by process %d.\n", msg_count, pid);

	restore(mask);
	return OK;
}
