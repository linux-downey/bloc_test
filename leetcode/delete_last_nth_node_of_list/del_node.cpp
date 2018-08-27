#include <iostream>
#include "single_list.h"


using namespace std;


//notice that the count must define &count a pointer;
void traversal_list(int n,int &count,List_node<int,int> *node)
{
    if(NULL==node)  return ;
    else
        traversal_list(n,count,node->next);
    count++;
    if(count==n+1)
    {
        List_node<int,int> *temp =node->next;
        node->next=temp->next;
        delete temp;
    }
}


void del_last_nth_node(int n,List_t<int,int> *L)
{
    int count=0;
    List_node<int,int> *head=L->head;
    traversal_list(n,count,head);
}





void print_list(List_t<int,int> *L)
{
    List_node<int,int> *temp=L->head->next;
    cout<<"The list is head->";
    while(NULL!=temp)
    {
       cout<<temp->value<<"->";
       temp=temp->next;
    }
    cout<<"end"<<endl;
}

List<int,int> L;

int main()
{
    for(int i=0;i<10;i++)
    {
        L.add_node(0,i);
    }
    print_list(L.get_list());
    del_last_nth_node(10,L.get_list());
    print_list(L.get_list());
    return 1;
}