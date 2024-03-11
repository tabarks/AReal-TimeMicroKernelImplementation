#include "system_sam3x.h"
#include "at91sam3x8.h"
#include "kernel_functions.h"


unsigned int g0 = 1; // for Kernel initialization
unsigned int g1 = 1; // for MAilBoxs
unsigned int g2 = 1; // for creating tasks
unsigned int g3 = 1; // 
unsigned int g4 = 1; // 
unsigned int g5 = 1; // 
unsigned int g6 = 1; // 
/* gate flags for various stages of unit test */

unsigned int low_deadline  = 1000;    
unsigned int high_deadline = 100000;

mailbox *charMbox; 
mailbox *intMbox; 
mailbox *floatMbox;


void task_body_1(){
  char  varChar_t1;
  int   retVal_t1, varInt_t1;
  float varFloat_t1; 
  
  // test on an empty mailbox
  retVal_t1 = receive_no_wait(charMbox, &varChar_t1); // charMbox
  if ( retVal_t1 != FAIL ) { g1 = FAIL; while(1) {}}
  
  retVal_t1 = receive_no_wait(intMbox, &varInt_t1); // intMbox
  if ( retVal_t1 != FAIL ) { g1 = FAIL; while(1) {}}
  
  retVal_t1 = receive_no_wait(floatMbox, &varFloat_t1); // floatMbox
  if ( retVal_t1 != FAIL ) { g1 = FAIL; while(1) {}}
  
  // now send
  varChar_t1 = 'a';
  retVal_t1 = send_no_wait(charMbox, &varChar_t1);
  if ( retVal_t1 != OK ) { g1 = FAIL; while(1) {}}
  varChar_t1 = 'b';
  retVal_t1 = send_no_wait(charMbox, &varChar_t1);
  if ( retVal_t1 != OK ) { g1 = FAIL; while(1) {}}
  varChar_t1 = 'c';
  retVal_t1 = send_no_wait(charMbox, &varChar_t1);
  if ( retVal_t1 != OK ) { g1 = FAIL; while(1) {}}
  
  ////////
  if(charMbox->nBlockedMsg != 0) { g1 = FAIL; while(1) {}}
  if(charMbox->nMessages != 3) { g1 = FAIL; while(1) {}}
  if(charMbox->pHead->pBlock != NULL) { g1 = FAIL; while(1) {}}
  if(charMbox->pHead->Status != SENDER) { g1 = FAIL; while(1) {}}
  if(*charMbox->pHead->pData != 'a') { g1 = FAIL; while(1) {}}
  
  // now read
  retVal_t1 = receive_no_wait(charMbox, &varChar_t1);
  if ( retVal_t1 != OK ) { g1 = FAIL; while(1) {}}
  if (varChar_t1 != 'a') { g1 = FAIL; while(1) {}}
  
  retVal_t1 = receive_no_wait(charMbox, &varChar_t1);
  if ( retVal_t1 != OK ) { g1 = FAIL; while(1) {}}
  if (varChar_t1 != 'b') { g1 = FAIL; while(1) {}}
  
  retVal_t1 = receive_no_wait(charMbox, &varChar_t1);
  if ( retVal_t1 != OK ) { g1 = FAIL; while(1) {}}
  if (varChar_t1 != 'c') { g1 = FAIL; while(1) {}}
  
  ////////
  if(charMbox->nBlockedMsg != 0) { g1 = FAIL; while(1) {}}
  if(charMbox->nMessages != 0) { g1 = FAIL; while(1) {}}
  if(charMbox->pHead != NULL) { g1 = FAIL; while(1) {}}
  if(charMbox->pTail != NULL) { g1 = FAIL; while(1) {}}
  
  varInt_t1 = 100;
  retVal_t1 = send_wait( intMbox, &varInt_t1 );
  if ( retVal_t1 != OK ) { g4 = FAIL; while(1) {}}
  if(Ticks >= deadline()) { g1 = FAIL; while(1) {}}
  
  g3 = FAIL;
  varFloat_t1 = 3.14159;
  retVal_t1 = send_wait( floatMbox, &varFloat_t1 );
  if ( retVal_t1 == OK )  { g4 = FAIL; while(1) {}}
  else { g3 = OK; }
  
  terminate();  /* if communication calls in this task worked */
  
}

void task_body_2(){
  int                 retVal_t2;
  int                 varInt_t2; 
  
  retVal_t2 = receive_wait( intMbox, &varInt_t2 );
  if ( retVal_t2 != OK ) { g4 = FAIL; while(1) {}}
  if ( varInt_t2 != 100 ) { g5 = FAIL; while(1) {}}
  
  retVal_t2 = receive_wait( intMbox, &varInt_t2 );
  if ( retVal_t2 != OK ) { g4 = FAIL; while(1) {}}
  if ( varInt_t2 != 500 ) { g5 = FAIL; while(1) {}}
  
  while(1)
  {
    if ( g3 == OK) break;
  }
  while(TimerList->pHead!=NULL||WaitingList->pHead!=NULL);
  remove_mailbox(charMbox);
  remove_mailbox(intMbox);
  remove_mailbox(floatMbox);
  terminate();
}

void task_body_3(){
  int                 retVal_t3;
  int                 varInt_t3;
  
  if(WaitingList->pHead->pTask->PC != task_body_1) { g6 = FAIL; while(1) {}}
  
  varInt_t3 = 500;
  retVal_t3 = send_wait( intMbox, &varInt_t3 );
  if ( retVal_t3 != OK ) { g4 = FAIL; while(1) {}}
  if(Ticks >= deadline()) { g1 = FAIL; while(1) {}}
  
  terminate();   
}

void task_body_4(){
  int                 retVal_t4;
  int               varInt_t4 = 0;
    
  retVal_t4 = wait ((int *) 900);
  if ( retVal_t4 !=  OK ) { g4 = FAIL; while(1) {}}
  
  retVal_t4 = receive_wait( intMbox, &varInt_t4 );
  if ( varInt_t4 != 255)  { g5 = FAIL; while(1) {}}
  
  retVal_t4 = wait ((int *) 4000);
  if ( retVal_t4 !=  DEADLINE_REACHED ) { g4 = FAIL; while(1) {} }
  terminate();   
}

void task_body_5(){
  int                 retVal_t5;
  int               varInt_t5 = 255;
  
  if(TimerList->pHead->pTask->PC != task_body_4) { g6 = FAIL; while(1) {}}
  
  retVal_t5 = send_wait( intMbox, &varInt_t5 );
  if ( retVal_t5 != OK) { g4 = FAIL; while(1) {} }
  
  retVal_t5 = wait ((int *) 200);
  if ( retVal_t5 !=  OK ) { g4 = FAIL; while(1) {} }
  
  terminate();   
}

void main()
{
  SystemInit(); 
  SysTick_Config(100000); 
  SCB->SHP[((uint32_t)(SysTick_IRQn) & 0xF)-4] =  (0xE0);      
  isr_off();
  
  g0 = OK;
  exception retVal = init_kernel(); 
  
  if (retVal != OK) { g0 = FAIL; while(1) {} }
  if (ReadyList->pHead != ReadyList->pTail ) { g0 = FAIL; while(1) {}}
  if (WaitingList->pHead != WaitingList->pTail ) { g0 = FAIL; while(1) {}}
  if (TimerList->pHead != TimerList->pTail ) { g0 = FAIL;  while(1) {}}
  
  /////////////
  if(Ticks != 0) { g0 = FAIL; while(1) {}}
  if(KernelMode != INIT) { g0 = FAIL; while(1) {}}
  
  charMbox = create_mailbox( 3 , sizeof(char) );
  
  // check that charMbox have correct values
  if(charMbox == NULL) { g1 = FAIL; while(1) {}}
  if(charMbox->pHead != NULL || charMbox->pTail != NULL) { g1 = FAIL; while(1) {}}
  if(charMbox->nDataSize != sizeof(char)) { g1 = FAIL; }
  if(charMbox->nMaxMessages != 3) { g1 = FAIL; while(1) {}}
  if(charMbox->nMessages != 0 || charMbox->nBlockedMsg != 0) { g1 = FAIL; while(1) {}}    
    
  intMbox = create_mailbox( 3 , sizeof(int) );
  
  // check that intMbox have correct values
  if(intMbox == NULL) { g1 = FAIL; while(1) {}}
  if(intMbox->pHead != NULL || intMbox->pTail != NULL) { g1 = FAIL; while(1) {}}
  if(intMbox->nDataSize != sizeof(int)) { g1 = FAIL; while(1) {}}
  if(intMbox->nMaxMessages != 3) { g1 = FAIL; while(1) {}}
  if(intMbox->nMessages != 0 || intMbox->nBlockedMsg != 0) { g1 = FAIL; while(1) {}}
    
  floatMbox = create_mailbox( 2 , sizeof(float) );
  
  // check that floatMbox have correct values
  if(floatMbox == NULL) { g1 = FAIL; while(1) {}}
  if(floatMbox->pHead != NULL || floatMbox->pTail != NULL) { g1 = FAIL; while(1) {}}
  if(floatMbox->nDataSize != sizeof(float)) { g1 = FAIL; while(1) {}}
  if(floatMbox->nMaxMessages != 2) { g1 = FAIL; while(1) {}}
  if(floatMbox->nMessages != 0 || floatMbox->nBlockedMsg != 0) { g1 = FAIL; while(1) {}}
    
  retVal = create_task( task_body_1 , low_deadline );
  
  ///////////
  if(retVal != OK) { g2 = FAIL; while(1) {}}
  if(ReadyList->pHead->pTask->PC != task_body_1) { g2 = FAIL; while(1) {}}
  if(ReadyList->pHead->pTask->Deadline != low_deadline) { g2 = FAIL; while(1) {}}
  
  retVal = create_task( task_body_2 , 8*high_deadline );
  if ( retVal !=  OK ) { g2 = FAIL; while(1) {}}
  
  retVal = create_task( task_body_3 , 2*low_deadline );
  if ( retVal !=  OK ) { g2 = FAIL; while(1) {}}
  
  retVal = create_task( task_body_4 , 3*low_deadline );
  if ( retVal !=  OK ) { g2 = FAIL; while(1) {}}
  
  retVal = create_task( task_body_5 , 4*low_deadline );
  if ( retVal !=  OK ) { g2 = FAIL; while(1) {}}
  
  g3 =FAIL;

  run();
  
  while(1){ /* something is wrong with run */}
}

