#include <iostream>
#include "string"
#include "list"

/*It is not recommended to do this directly in project*/
/*Here is just for convenience*/
using namespace std;



list<int> L1;
list<int> L2;

typedef unsigned char uint8_t;

void init_lists(list<int> &L1,list<int> &L2)
{
    L1.push_back(2);
    L1.push_back(4);
    L1.push_back(3);
    L1.push_back(6);

    L2.push_back(5);
    L2.push_back(6);
    L2.push_back(4);
}

int get_list_elem(list<int> *L)
{
    if ( L->empty() || (0==L->front()) )
        return 0;
    else 
        return L->front();
}


void delete_list(list<int> *L)
{
    if(!L->empty())
        L->pop_front();
}

list<int>* add_two_num(list<int> *L1,list<int> *L2,list<int> *L_dst,uint8_t &carry)
{
    if(L1->empty() && L2->empty())
    {
        return L_dst;
    }
    L_dst->push_back((get_list_elem(L1)+get_list_elem(L2)+carry)%10);
    carry=(get_list_elem(L1)+get_list_elem(L2))/10;
    delete_list(L1);
    delete_list(L2);

    return add_two_num(L1,L2,L_dst,carry);
}


list<int>* add_two_num(list<int> *L1,list<int> *L2)
{
    uint8_t carry=0;
    
    list<int>* L_dst=new(nothrow) list<int>;
    L_dst=add_two_num(L1,L2,L_dst,carry);

    list<int>::iterator it=L_dst->begin();
    for(;it!=L_dst->end();it++)
    {
        cout<<*it<<endl;
    }

}




int main()
{
    init_lists(L1,L2);
    add_two_num(&L1,&L2);
}



