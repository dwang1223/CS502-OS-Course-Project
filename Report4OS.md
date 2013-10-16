<h1 style="text-align:center;">Architectural Document </h1>
<section style="text-align:center; color:red"><b>Attention</b>: my program is finished in VS 2010, so please compile & run it in <b>windows</b></section>
##a. Elements in your design
###1. Data Structure:
![Data Structure](http://i.imgur.com/gUJGhQT.jpg "Data Structure")
###2. Custom Function in base.c
1. `void schedule_printer();`   
This is a function I used to print current status(current time, target pid, current action, running pid, ready/timer/suspend queue status ).   <br/><br/>
2. `void ready_queue_print();`
3. `void timer_queue_print();`
4. `void suspend_queue_print();`
5. `void total_queue_print();`
6. `void current_statue_print();`    
2~6 are my custom printer functions before I use `void schedule_printer();`, in my current version, they are not used. -- Feel great to use schedule_printer();

7. `int start_timer(long *);`   
This function is mainly for **SLEEP**. Inside it, it implements:    
	1. inactive the current PCB node, by add it to timerQueue (timerQueue is sorted by wakeUpTime); 
	2. After inserting the node into timerQueue, then call dispatcher() to get a new node for current running node <br/><br/>
	
8. `void dispatcher();`    
Assign the first node from readyQueue to current running node. Following some special cases, dispatcher() should handles when readyQueue has no nodes available now, :    
	1. if timerQueue has no node, but suspendQueue has, then move first node from suspendQueue to readyQueue.
	2. call idle() until interrupt moves nodes from timerQueue to readyQueue<br/><br/>

9. `long get_pid_by_name(char *);`  
Get PCD node pid for specified node's name. If the name is null, just return the current running PCB node's pid.

10. `PCB * PCB_item_generator(SYSTEM_CALL_DATA *);`    
This function just uses current information to generate a PCB node, then return it.

11. `void new_node_add_to_readyQueue(Queue , int );`     
This function adds new node into readyQueue. The second parameter indicates how to insert this node into queue[***add to tail*** or sort it by ***priority***].

12. `long process_creater(PCB *);`      
This function calls the above two functions to create a process, then add it to queues.

13. `void myself_teminator( );`    
This function is used to terminate current running process.

14. `int process_teminator_by_pid(long );`    
This function is used to terminate  process by pid.

15. `int suspend_by_PID(long );`   
This function will suspend a process by pid(Direct calling this function to suspend itself is not legal in my program).

16. `int resume_by_PID(long );`   
This function will resume a process by pid. If the first parameter is -1, it will resume the first process in suspendQueue.

17. `int priority_changer(long , int );`     
This function will change the priority of a process, if the process changed is in readyQueue, then resorts readyQueue. 

18. `int msg_sender(long , long , char *, int );`     
This function will add a message node into msgQueue. What's more, it will resume the target process if target process is be suspended.

19. `int msg_receiver(long , char *, int , long *, long *);`    
This function will try to receive a message from msgQueue, if no message found for current process self, it will suspend itself(Exception: if the timerQueue has nodes, if will call idle() instead of suspending itself, after time interruption, making the first process in readyQueue to be current process by adding the "current process" to readyQueue).

###3. High Level Design
**To make things easier, I will use images to demonstrate how my system calls work:**    

1. `SYSNUM_GET_TIME_OF_DAY`    
![SYSNUM_GET_TIME_OF_DAY](http://i.imgur.com/t4zzHIU.jpg "SYSNUM_GET_TIME_OF_DAY")
2. `SYSNUM_SLEEP`  
![Sleep Call](http://i.imgur.com/eMo8yci.jpg "SYSNUM_SLEEP")
3. `SYSNUM_CREATE_PROCESS`    
![PROCESS_CREATE](http://i.imgur.com/g3I03ly.jpg "SYSNUM_CREATE_PROCESS")
4. `SYSNUM_GET_PROCESS_ID`   
![](http://i.imgur.com/iccFYab.jpg "SYSNUM_GET_PROCESS_ID")
5. `SYSNUM_SUSPEND_PROCESS`    
![SYSNUM_SUSPEND_PROCESS](http://i.imgur.com/b5SM0Rc.jpg "SYSNUM_SUSPEND_PROCESS")
6. `SYSNUM_RESUME_PROCESS`     
![SYSNUM_RESUME_PROCESS](http://i.imgur.com/EzldeKq.jpg "SYSNUM_RESUME_PROCESS")
7. `SYSNUM_CHANGE_PRIORITY`    
![SYSNUM_CHANGE_PRIORITY](http://i.imgur.com/WgoilY7.jpg "SYSNUM_CHANGE_PRIORITY")
8. `SYSNUM_SEND_MESSAGE`     
![SYSNUM_SEND_MESSAGE](http://i.imgur.com/Tt2sl15.jpg "SYSNUM_SEND_MESSAGE")
9. `SYSNUM_RECEIVE_MESSAGE`     
![SYSNUM_RECEIVE_MESSAGE](http://i.imgur.com/BmlcX7G.jpg "SYSNUM_RECEIVE_MESSAGE")
10. `SYSNUM_TERMINATE_PROCESS`     
![SYSNUM_TERMINATE_PROCESS](http://i.imgur.com/46gEVXd.jpg "SYSNUM_TERMINATE_PROCESS")  
11. `interrupt_handler`    
![interrupt_handler](http://i.imgur.com/grvMXg4.jpg "interrupt_handler")
12. `fault_handler`    
![fault_handler](http://i.imgur.com/nOIz5ts.jpg "fault_handler")
###4. Justification of High Level Design
In the user model(test.c), multiple system calls are called. So that in SVC (base.c) of kernel model, I define multiple routines to deal with different system calls.   

For example, when some function in test.c calls SLEEP, in base.c, startTimer() is invoked. It will add the current process to timerQueue, then calls dispatcher(), which will check whether readyQueue is empty or not, if readyQueue is not empty, then extracts the first node from readyQueue as current running process, then the program continues. Otherwise, it will call idle() until some process is actived from timerQueue and ready to use and then extracts the first process from the readyQueue, so that the program continues.

I also describe how other routines work above with images and details for each functions. I am trying to make my ideas as clear as possible, and wish I make it. By the way, you can also find more details in my code, as I have add tons of comments there.   

###5. Additional Features
1. I just implement case 1 in test1l, and I feel so shamed that I could implement the whole test1l
2. For test1m, I implement a simulation for "***Will a programmer succeed when he show his love to a beautiful girl?***"   
	a. There are two processes in test1m, one for boy, the other for girl. Both boy and girl have their own collection of sentences for conversation. 
	<pre><code>static char boyWords[5][50] = {  "I love you!", 
                                 	"Haha, I am kidding!", 
                                 	"I give up!", 
                                 	"Today is a good day!", 
                                	"..."};    

	static char girlWords[7][50] = { "Are you kidding?", 
                                 	"I don't love you at all!", 
                                 	"We are just friends",
                                 	"I already have boyfriend!", 
                                 	"Actually, I love girls",
                                 	"You're a good man, you can find a better girl",
                                 	"I love you, too"}; </code></pre>    


	b.  How does it work?   
	1). Conversation mode: boy begins the conversation, and each one gives one sentence for his/her turn, then wait for reply from other.    
	2).  Test1m has three different results:  
	- The girl agrees to be GF for this boy, then they stay together happily[Good result] -- This happens when boy says "I love you", then the girl replies "I love you, too";      
	- The girl refuses this boy, and this boy gives up at last, then the boy does programming for his whole life without GF[Bad result] -- This happens when girl says one of these three sentences of "I don't love you at all!", "I already have boyfriend!", or "Actually, I love girls", then the boy fees hopeless, then replies "I give up!";            
	- After talking to each other for a long time without any result, the boy loses his patience, then he thinks programming is more interesting than having GF, so he goes to continue his work[Not so good, not so bad result].   
	
	3). Why do I do this? I just want to figure out the probability how I can get a GF. Just for fun.
###6. Anomalies and bugs
My program will throw "***Fault_handler: Found vector type 4 with value 0***" error in following two cases:    
1. the `schedule_printer()` is called in some locations which seems not appropriate  
2. in test1l case 2, I stuck on this issue

Source Code
========

     a) base.c -- Almost all my logic issues are handled here
     b) custom.h -- I define my own data structure here, also some macros are defined here
	 c) test.c -- Because my custom test1m() is here.

Test Format
=======
     I have added schedule_printer() in my code.

Test Results
======
	a) Test program 1a runs and gives expected output.
	b) Test program 1b runs and gives expected output.
    c) Test program 1c runs and gives expected output.
    d) Test program 1d runs and gives expected output.
    e) Test program 1e runs and gives expected output.
    f) Test program 1f runs and gives expected output.
    g) Test program 1g runs and gives expected output.
    h) Test program 1h runs and gives expected output.
    i) Test program 1i runs and gives expected output.
    j) Test program 1j runs and gives expected output.
    k) Test program 1k runs and gives expected output.
    l) New feature-- Test program 1l runs, but only case 1 gives expected output (case 2, 3 fail).
	m) New feature-- Test program 1k runs and gives expected output.
