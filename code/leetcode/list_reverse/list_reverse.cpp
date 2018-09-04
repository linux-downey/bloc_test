#include <iostream>
#include "single_list.h"



List<int,int> list;

void list_reverse(List_t<int,int>* L)
{
    if(NULL==L)
        return;
    List_node<int,int> *p,*q,*temp;
    p=L->head;
    if(p->next)
    {
        q=p->next;
    }     
    //if list has no node,return;
    else   return;
    //if list has only one node,there is no need to reverse.
    while(NULL!=q->next)
    {
        temp=q->next;
        q->next=p;
        p=q;
        q=temp;
    }
    q->next=p;
    //At this time,the head->next still point to previous first node.the previous first node becomes the tail node
    //so set the node's next-point field to null.
    L->head->next->next=NULL;
    //set the head's next-point field to new first node.
    L->head->next=q;
}



void traversal_list(List_t<int,int>* L)
{
    List_node<int,int>* p=L->head->next;
    cout<<"head->";
    while(NULL!=p)
    {
        cout<<p->value<<"->";
        p=p->next;
    }
    cout<<"end"<<endl;
}

int main()
{
    for(int i=0;i<10;i++)
        list.add_node(0,i);
    traversal_list(list.get_head());
    list_reverse(list.get_head());
    traversal_list(list.get_head());
    return 0;
}


