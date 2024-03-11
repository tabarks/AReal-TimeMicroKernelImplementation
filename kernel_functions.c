#include "system_sam3x.h"
#include "at91sam3x8.h"
#include "kernel_functions.h"
#include <limits.h>
#include <string.h>
#include <stdlib.h>

int Ticks; /* global sysTick counter */
int KernelMode; /* can be equal to either INIT or RUNNING (constants defined in “kernel_functions.h”)*/
TCB *PreviousTask, *NextTask; /* Pointers to previous and next running tasks */
list *ReadyList, *WaitingList, *TimerList;

/////////////////////////////////////// ( Lab 1 ) ///////////////////////////////////////

exception init_kernel (void){
  set_ticks(0);
  exception ex = OK;
  ex &= init_List(&ReadyList);
  ex &= init_List(&WaitingList);
  ex &= init_List(&TimerList);
  ex &= create_task(&idleTask,UINT_MAX);
  KernelMode = INIT;
  return ex;
}
exception create_task (void (*taskBody)(), unsigned int deadline){
  TCB *new_tcb;
  new_tcb = (TCB *)calloc(1, sizeof(TCB));
  /* you must check if calloc was successful or not! */
  if (new_tcb==NULL)
  {
    return FAIL;
  }
  new_tcb->PC = taskBody;
  new_tcb->SPSR = 0x21000000;
  new_tcb->Deadline = deadline;
  new_tcb->StackSeg [STACK_SIZE - 2] = 0x21000000;
  new_tcb->StackSeg [STACK_SIZE - 3] = (unsigned int) taskBody;
  new_tcb->SP = &(new_tcb->StackSeg [STACK_SIZE - 9]);
  // after the mandatory initialization you can implement the rest of the suggested pseudocode
  
  if (KernelMode==INIT)
  {
    listobj *ListObj;
    exception ex = createTaskObject(&ListObj, &new_tcb);
    if (ex == FAIL)
      return FAIL;
    
    //Insert new task in ReadyList
    insertObject(ReadyList, ListObj);
    
    //Return status
    return OK;
  }
  else
  {
    //Disable interrupts
    isr_off();
    
    //Update PreviousTask
    PreviousTask = NextTask;
    
    //Insert new task in ReadyList
    listobj *ListObj;
    exception ex = createTaskObject(&ListObj, &new_tcb);
    if (ex == FAIL)
      return FAIL;
    insertObject(ReadyList, ListObj);
    
    //Update NextTas
    NextTask = ReadyList->pHead->pTask;
    
    //Switch context
    SwitchContext();
  }
  // Return status
  return OK;
}
void run (void){
  //Initialize interrupt timer
  set_ticks(0);
  
  KernelMode = RUNNING;
  
  //Set NextTask to equal TCB of the task to be loaded
  NextTask = ReadyList->pHead->pTask;
  
  LoadContext_In_Run();
  /* supplied to you in the assembly file
  * does not save any of the registers
  * but simply restores registers from saved values
  * from the TCB of NextTask
  */
  
}
void terminate (void){
  isr_off();
  listobj *leavingObj = removeFirstObject(ReadyList);
  /* detaches the head node from the ReadyList and 
  * returns the list object of the running task */
  
  NextTask = ReadyList->pHead->pTask;
  switch_to_stack_of_next_task();
  
  free(leavingObj->pTask);
  free(leavingObj);
  
  LoadContext_In_Terminate();
  
  /* supplied to you in the assembly file
  * does not save any of the registers. Specifically, does not save the
  * process stack pointer (psp), but
  * simply restores registers from saved values from the TCB of NextTask
  * note: the stack pointer is restored from NextTask->SP
  */
}
void idleTask(){
  while(1);
}

////////////////////////////////////////////////   ( Lab 2 ) /////////////////////////////////

int no_messages(mailbox* mBox){
  if(mBox->nMessages==0)
    return TRUE;
  return FALSE;
}
mailbox* create_mailbox(uint nMessages, uint nDataSize){
  /*
  Allocate memory for the mailbox
  Initialize mailbox structure
  Return mailbox
  */
  mailbox *pMailbox = (mailbox*)calloc(1, sizeof(mailbox));
  if (pMailbox==NULL)
    return NULL;
  pMailbox->pHead = pMailbox->pTail = NULL;
  pMailbox->nDataSize = nDataSize;
  pMailbox->nMaxMessages = nMessages;
  pMailbox->nMessages = pMailbox->nBlockedMsg = 0; // no messages yet
  return pMailbox;
}
exception remove_mailbox (mailbox* mBox){
  if (mBox->nMessages==0)
  {
    free(mBox);
    return OK;
  } else {
    return NOT_EMPTY;
  }
}
exception send_wait(mailbox* mBox, void* pData){
  //Disable interrupt
  isr_off();
  
  // Search for receiver...
  msg *pRec = mBox->pHead;
  //while (pRec!=NULL)
  //{
  //  if(pRec->Status == RECEIVER)
  //    break;
  //  pRec = pRec->pNext;
  //}
  
  if(pRec!=NULL&&pRec->Status == RECEIVER) // if I find receiver:
  {
    //Copy sender’s data to the data area of the receivers Message
    memcpy(pRec->pData, pData, mBox->nDataSize);
    
    //Remove receiving task’s Message struct from the mailbox
    removeMsg(mBox, pRec);
    
    //Update PreviousTask
    PreviousTask = NextTask;
    
    //Insert receiving task in ReadyList
    exception ex = removeObject(WaitingList, pRec->pBlock);
    if(ex==FAIL)
      return FAIL;
    insertObject(ReadyList, pRec->pBlock);
    
    free(pRec);
    
    //Update NextTask
    NextTask = ReadyList->pHead->pTask;
  }
  else
  {
    //Allocate a Message structure Set data pointer
    msg *pMsg = (msg*)calloc(1, sizeof(msg));
    if(pMsg==NULL)
      return FAIL;
    pMsg->pNext = pMsg->pPrevious = NULL; // init the msg...
    pMsg->Status = SENDER; // msg from the sender
    pMsg->pBlock = ReadyList->pHead; // Runnnig Task object
    pMsg->pData = (char*)calloc(mBox->nDataSize, sizeof(char));
    if(pMsg->pData==NULL)
      return FAIL;
    memcpy(pMsg->pData, pData , mBox->nDataSize); // copy data to msg data
    ReadyList->pHead->pMessage = pMsg; // connect the running task to it's msg
    
    //IF mailbox is full THEN
    if(mBox->nMessages >= mBox->nMaxMessages)
    {
      //Remove the oldest Message struct
      msg *rmMsg = removeFirstMsg(mBox);
      free(rmMsg);
    }
    
    //Add Message to the mailbox
    pushMsg(mBox, pMsg);
    
    //Update PreviousTask
    PreviousTask = NextTask;
    
    //Move sending task from ReadyList to WaitingList
    listobj *senderObj = removeFirstObject(ReadyList);
    insertObject(WaitingList,senderObj);
    
    //Update NextTask
    NextTask = ReadyList->pHead->pTask;
  }
  
  
  //Switch context
  SwitchContext();
  
  //IF deadline is reached THEN
  if(Ticks >= deadline())
  {
    //Disable interrupt
    isr_off();
    
    // Remove send Message
    // clean up their Mailbox entry.
    removeMsg(mBox,ReadyList->pHead->pMessage);
    free(ReadyList->pHead->pMessage->pData);
    free(ReadyList->pHead->pMessage);
    
    
    //Enable interrupt
    isr_on();
    
    return DEADLINE_REACHED;
  }
  else
    return OK;
}
exception receive_wait(mailbox* mBox, void* pData){
  //Disable interrupt
  isr_off();
  
  // Search for sender...
  msg *pSend = mBox->pHead;
  //while (pSend!=NULL)
  //{
  //  if(pSend->Status == SENDER)
  //    break;
  //  pSend = pSend->pNext;
  //}
  
  if(pSend!=NULL&&pSend->Status == SENDER) // if I find Sender:
  {
    // Copy sender’s data to receiving task’s data area
    memcpy(pData, pSend->pData, mBox->nDataSize);
    
    // Remove sending task’s Message struct from the mailbox
    removeMsg(mBox, pSend);
    
    listobj *senderObj = pSend->pBlock;
    
    //Free senders data area
    free(pSend->pData);
    free(pSend);
    
    if(senderObj!=NULL) // IF Message was of wait type THEN
    {
      //Update PreviousTask
      PreviousTask = NextTask;
      
      //Move sending task to ReadyList
      removeObject(WaitingList,senderObj);
      insertObject(ReadyList,senderObj);
      
      //Update NextTask
      NextTask = ReadyList->pHead->pTask;
    }
  }
  else
  {
    //Allocate a Message structure
    msg *pMsg = (msg*)calloc(1, sizeof(msg));
    if(pMsg==NULL)
      return FAIL;
    pMsg->pNext = pMsg->pPrevious = NULL; // init the msg...
    pMsg->Status = RECEIVER; // receiver msg!
    pMsg->pBlock = ReadyList->pHead; // Runnnig Task object
    pMsg->pData = pData; // set Data Pointer
    ReadyList->pHead->pMessage = pMsg; // connect the running task to it's msg
    
    
    //IF mailbox is full THEN
    if(mBox->nMessages >= mBox->nMaxMessages)
    {
      //Remove the oldest Message struct
      msg *rmMsg = removeFirstMsg(mBox);
      free(rmMsg);
    }
    
    //Add Message to the mailbox
    pushMsg(mBox, pMsg);
    
    //Update PreviousTask
    PreviousTask = NextTask;
    
    //Move sending task from ReadyList to WaitingList
    listobj *senderObj = removeFirstObject(ReadyList);
    insertObject(WaitingList,senderObj);
    
    //Update NextTask
    NextTask = ReadyList->pHead->pTask;
  }
  SwitchContext();
  //
  if(Ticks >= deadline())
  {
    //Disable interrupt
    isr_off();
    
    // clean up their Mailbox entry.
    // Remove receive Message
    removeMsg(mBox,ReadyList->pHead->pMessage);
    free(ReadyList->pHead->pMessage);
    
    //Enable interrupt
    isr_on();
    return DEADLINE_REACHED;
  }
  else
  {
    return OK;
  }
}
exception send_no_wait(mailbox* mBox, void* pData){
  //Disable interrupt
  isr_off();
  
  // Search for receiver...
  msg *pRec = mBox->pHead;
  //while (pRec!=NULL)
  //{
  //  if(pRec->Status == RECEIVER)
  //    break;
  //  pRec = pRec->pNext;
  //}
  
  if(pRec!=NULL && pRec->Status == RECEIVER) // IF receiving task is waiting THEN
  {
    //Copy data to receiving tasks’ data area.
    memcpy(pRec->pData, pData, mBox->nDataSize);
    
    //Remove receiving task’s Message struct from the mailbox
    removeMsg(mBox, pRec);
    
    //Update PreviousTask
    PreviousTask = NextTask;
    
    //Move receiving task to Readylist
    exception ex = removeObject(WaitingList, pRec->pBlock);
    insertObject(ReadyList, pRec->pBlock);
    
    free(pRec);
    
    //Update NextTask
    NextTask = ReadyList->pHead->pTask;
    
    //Switch context
    SwitchContext();
    
  }
  else
  {
    //Allocate a Message structure
    msg *pMsg = (msg*)calloc(1, sizeof(msg));
    if(pMsg==NULL)
      return FAIL;
    pMsg->pNext = pMsg->pPrevious = NULL; // init the msg...
    pMsg->Status = SENDER; // msg from the sender
    pMsg->pBlock = NULL; // the sender will not wait..
    ReadyList->pHead->pMessage = pMsg; // connect the running task to it's msg
    
    //Copy Data to the Message
    pMsg->pData = (char*)calloc(mBox->nDataSize, sizeof(char));
    if(pMsg->pData==NULL)
      return FAIL;
    memcpy(pMsg->pData, pData , mBox->nDataSize); // copy data to msg data
    
    //IF mailbox is full THEN
    if(mBox->nMessages >= mBox->nMaxMessages)
    {
      //Remove the oldest Message struct
      msg *rmMsg = removeFirstMsg(mBox);
      free(rmMsg);
    }
    
    //Add Message to the mailbox
    pushMsg(mBox, pMsg);
    
    isr_on();
  }
  return OK;
}
int receive_no_wait(mailbox* mBox, void* pData){
  //Disable interrupt
  isr_off();
  
  // Search for sender...
  msg *pSend = mBox->pHead;
  //while (pSend!=NULL)
  //{
  //  if(pSend->Status == SENDER)
  //    break;
  //  pSend = pSend->pNext;
  //}
  
  if(pSend!=NULL && pSend->Status == SENDER) // IF send Message is waiting THEN
  {
    // Copy sender’s data to receiving task’s data area
    memcpy(pData, pSend->pData, mBox->nDataSize);
    
    // Remove sending task’s Message struct from the mailbox
    removeMsg(mBox, pSend);
    
    listobj *senderObj = pSend->pBlock;
    
    //Free sender's data area
    free(pSend->pData);
    free(pSend);
    
    if(senderObj!=NULL) // IF Message was of wait type THEN
    {
      //Update PreviousTask
      PreviousTask = NextTask;
      
      //Move sending task to ReadyList
      removeObject(WaitingList,senderObj);
      insertObject(ReadyList,senderObj);
      
      //Update NextTask
      NextTask = ReadyList->pHead->pTask;
      
      //SwitchContext
      SwitchContext();
    }
    else
    {
      isr_on();
    }
    return OK;
  }
  else
  {
    isr_on();
    return FAIL;
  }
}

///////////////////////////////////// ( Lab 3 ) /////////////////////////////////////////

exception wait(uint nTicks){
  //Disable interrupt
  isr_off();
  
  //Update PreviousTask
  PreviousTask = NextTask;
  
  //Place running task in the TimerList
  listobj *runningOb = removeFirstObject(ReadyList);
  // Set the end time
  runningOb->nTCnt = ticks() + nTicks;
  insertObject(TimerList,runningOb);
  
  //Update NextTask
  NextTask = ReadyList->pHead->pTask;
  
  //Switch context
  SwitchContext();
  //
  
  if(ticks() >= ReadyList->pHead->pTask->Deadline){
    return DEADLINE_REACHED;
  } else {
    return OK;
  }
}
void set_ticks(uint nTicks){
  Ticks = nTicks;
}
uint ticks(void){
  return Ticks;
}
uint deadline(void){
  return ReadyList->pHead->pTask->Deadline;
}
void set_deadline(uint deadline){
  //Disable interrupt
  isr_off();
  
  //Set the deadline field in the calling TCB.
  ReadyList->pHead->pTask->Deadline = deadline;
  
  //Update PreviousTask
  PreviousTask = NextTask;
  
  //Reschedule ReadyList
  listobj *runningOb = removeFirstObject(ReadyList);
  insertObject(ReadyList,runningOb);
  
  //Update NextTask
  NextTask = ReadyList->pHead->pTask;
  
  //Switch context
  SwitchContext();
}
void TimerInt(void){
  //Increment tick counter 
  Ticks++;
  
  /*
  --Check the TimerList for tasks that are ready for execution
  --(a task that was sleeping is ready for execution if either
  --its sleep period is over, OR if its deadline has expired).
  */
  listobj *ob = TimerList->pHead;
  while (ob!=NULL){
    listobj *nxt = ob->pNext;
    if(ticks() >= ob->nTCnt || ticks() >= ob->pTask->Deadline){
      //PreviousTask = NextTask;
      
      //move these to ReadyList.
      removeObject(TimerList, ob);
      insertObject(ReadyList, ob);
      
      NextTask = ReadyList->pHead->pTask;  
    }
    ob = nxt;
  }
  
  /*
  --Check the WaitingList for tasks that have expired deadlines
  */
  ob = WaitingList->pHead;
  while(ob!=NULL){
    listobj *nxt = ob->pNext;
    if(ticks() >= ob->pTask->Deadline){
      //PreviousTask = NextTask;
      
      //move these to ReadyList
      removeObject(WaitingList, ob);
      insertObject(ReadyList, ob);
      
      // clean up their Mailbox entry.
      // done in receve_wait() or send_wait()
      
      NextTask = ReadyList->pHead->pTask;
    }
    ob = nxt;
  }
}

///////////////////////////////// Lists Func ////////////////////////////////////////////

exception init_List(list **ppList){
  *ppList = (list*) calloc(1, sizeof(list));
  if (*ppList==NULL)
  {
    return FAIL;
  }
  else
  {
    (*ppList)->pHead = (*ppList)->pTail = NULL;
    return SUCCESS;
  }
}
exception createTaskObject(listobj **ppListObj, TCB **pptcb){
  *ppListObj = (listobj *) calloc(1, sizeof(listobj));
  if (*ppListObj==NULL)
    return FAIL;
  (*ppListObj)->pTask = *pptcb;
  (*ppListObj)->pNext = NULL;
  (*ppListObj)->pPrevious = NULL;
  (*ppListObj)->pMessage = NULL;
  return OK;
}
void insertObject(list *List, listobj *pOb){
  pOb->pNext = pOb->pPrevious = NULL;
  listobj *lobj = List->pHead;
  if (lobj==NULL)
  {
    List->pHead = List->pTail = pOb;
  }
  else if (lobj->pTask->Deadline >= pOb->pTask->Deadline)
  {
    pOb->pNext = List->pHead;
    List->pHead->pPrevious = pOb;
    List->pHead = pOb;
  }
  else
  {
    while(lobj->pNext != NULL && lobj->pNext->pTask->Deadline < pOb->pTask->Deadline)
    {
      lobj = lobj->pNext;
    }
    pOb->pNext = lobj->pNext;
    pOb->pPrevious = lobj;
    if (lobj->pNext != NULL)
    {
      lobj->pNext->pPrevious = pOb;
    }
    else
    {
      List->pTail= pOb;
    }
    lobj->pNext = pOb;
  }
}
listobj *removeFirstObject(list *List){
  listobj *plistobj;
  plistobj = List->pHead;
  if (List->pHead != NULL)
  {
    List->pHead = List->pHead->pNext;
    if (List->pHead!=NULL)
    {
      List->pHead->pPrevious = NULL;
    }
    else
    {
      List->pTail = NULL;
    }
  }
  return plistobj;
}
listobj *removeLastObject(list *List){
  listobj *plistobj;
  plistobj = List->pTail;
  if (List->pTail != NULL)
  {
    List->pTail = List->pTail->pPrevious;
    if (List->pTail!=NULL)
    {
      List->pTail->pNext = NULL;
    }
    else
    {
      List->pHead = NULL;
    }
  }
  return plistobj;
}
exception removeObject(list *List, listobj *pListObj){
  if(List->pHead==NULL)
    return FAIL;
  listobj *pObj = List->pHead;
  while(pObj!=NULL)
  {
    if(pObj==pListObj)
    {
      if(pObj->pPrevious==NULL)
      {
        removeFirstObject(List);
      }
      else if(pObj->pNext==NULL)
      {
        removeLastObject(List);
      }
      else
      {
        pObj->pNext->pPrevious = pObj->pPrevious;
        pObj->pPrevious->pNext = pObj->pNext;
      }
      return OK;
    }
    pObj = pObj->pNext;
  }
  return FAIL;
}
void pushMsg(mailbox *pMbox, msg *pMsg){
  pMsg->pNext = pMsg->pPrevious = NULL;
  if (pMbox->pHead==NULL)
  {
    pMbox->pHead = pMbox->pTail = pMsg;
  }
  else
  {
    pMsg->pPrevious = pMbox->pTail;
    pMbox->pTail->pNext = pMsg;
    pMbox->pTail = pMsg;
  }
  pMbox->nMessages++;
  if(pMsg->pBlock!=NULL)
    pMbox->nBlockedMsg++;
}
msg *removeFirstMsg(mailbox *pMbox){
  msg *pm;
  pm = pMbox->pHead;
  if (pMbox->pHead != NULL)
  {
    pMbox->pHead = pMbox->pHead->pNext;
    if (pMbox->pHead!=NULL)
    {
      pMbox->pHead->pPrevious = NULL;
    }
    else
    {
      pMbox->pTail = NULL;
    }
  }
  pMbox->nMessages--;
  if(pm->pBlock!=NULL)
    pMbox->nBlockedMsg--;
  return pm;
}
msg *removeLastMsg(mailbox *pMbox){
  msg *pm;
  pm = pMbox->pTail;
  if (pMbox->pTail != NULL)
  {
    pMbox->pTail = pMbox->pTail->pPrevious;
    if (pMbox->pTail!=NULL)
    {
      pMbox->pTail->pNext = NULL;
    }
    else
    {
      pMbox->pHead = NULL;
    }
  }
  pMbox->nMessages--;
  if(pm->pBlock!=NULL)
    pMbox->nBlockedMsg--;
  return pm;
}
exception removeMsg(mailbox *pMbox, msg *pMsg){
  if(pMbox->pHead==NULL)
    return FAIL;
  msg *pm = pMbox->pHead;
  while(pm!=NULL)
  {
    if(pm==pMsg)
    {
      if(pm->pPrevious==NULL)
      {
        removeFirstMsg(pMbox);
      }
      else if(pm->pNext==NULL)
      {
        removeLastMsg(pMbox);
      }
      else
      {
        pm->pNext->pPrevious = pm->pPrevious;
        pm->pPrevious->pNext = pm->pNext;
        pMbox->nMessages--;
        if(pm->pBlock!=NULL)
          pMbox->nBlockedMsg--;
      }
      return OK;
    }
    pm = pm->pNext;
  }
  return FAIL;
}