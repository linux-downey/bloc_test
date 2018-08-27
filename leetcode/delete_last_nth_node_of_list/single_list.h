//  
//                                  _oo8oo_
//                                 o8888888o
//                                 88" . "88
//                                 (| -_- |)
//                                 0\  =  /0
//                               ___/'==='\___
//                             .' \\|     |// '.
//                            / \\|||  :  |||// \
//                           / _||||| -:- |||||_ \
//                          |   | \\\  -  /// |   |
//                          | \_|  ''\---/''  |_/ |
//                          \  .-\__  '-'  __/-.  /
//                        ___'. .'  /--.--\  '. .'___
//                     ."" '<  '.___\_<|>_/___.'  >' "".
//                    | | :  `- \`.:`\ _ /`:.`/ -`  : | |
//                    \  \ `-.   \_ __\ /__ _/   .-` /  /
//                =====`-.____`.___ \_____/ ___.`____.-`=====
//                                  `=---=`
// 
//                          佛祖保佑         永无bug
// 

/**@file single_list.h
 * @brief 
 * @author downey
 * @date 2018-06-05
 * 
 * */


#ifndef _SINGLE_LIST_H
#define _SINGLE_LIST_H



#include <iostream>
#include <string>

#ifndef PRE_DEFINE
#define PRE_DEFINE

using std::cout;
using std::cin;
using std::endl;
using std::string;

using u8=unsigned char;
using u16=unsigned short;
using u32=unsigned int;
using s32=int;

#endif

template <typename T_INDEX,typename T_VALUE>
struct List_node
{
    List_node *next;
    T_INDEX index;
    T_VALUE value;
};

template <typename T_INDEX,typename T_VALUE>
struct List_t
{
    List_node<T_INDEX,T_VALUE> *head;
    u32 list_len;
};



template <typename T_INDEX,typename T_VALUE>
class List
{
    public:
        List();
        ~List();
        s32 add_node(T_INDEX index,T_VALUE value);
        s32 delete_node(T_INDEX index);
        List_node<T_INDEX,T_VALUE>* find_node(T_INDEX index);
        s32 destroy_list();
        u32 get_list_len();
        void print_all_node();
        List_t<T_INDEX,T_VALUE> *get_list(){return &list;};

    private:
       List_t<T_INDEX,T_VALUE> list;
};


template <typename T_INDEX,typename T_VALUE>
List<T_INDEX,T_VALUE>::List()
{
    list.head=new List_node<T_INDEX,T_VALUE>;
    list.head->next=NULL;
    list.list_len=0;
}



template <typename T_INDEX,typename T_VALUE>
List<T_INDEX,T_VALUE>::~List()
{
    destroy_list();
}

template <typename T_INDEX,typename T_VALUE>
s32 List<T_INDEX,T_VALUE>::destroy_list()
{
    List_node<T_INDEX,T_VALUE>* temp=list.head->next;
    List_node<T_INDEX,T_VALUE>* temp_del=list.head->next;
    while(temp)
    {
        temp=temp->next;
        delete temp_del;
        temp_del=temp;
    }
    delete list.head;
    list.head=NULL;
    return 0; 
}



template <typename T_INDEX,typename T_VALUE>
s32 List<T_INDEX,T_VALUE>::add_node(T_INDEX index,T_VALUE value)
{
    List_node<T_INDEX,T_VALUE>* temp=new List_node<T_INDEX,T_VALUE>;
    List_node<T_INDEX,T_VALUE>* p;
    p=list.head;

    temp->index=index;
    temp->value=value;
    temp->next=NULL;

    while(p->next)
    {
        p=p->next;
    }
    p->next=temp;
    list.list_len++;
    return 0;
}


template <typename T_INDEX,typename T_VALUE>
List_node<T_INDEX,T_VALUE>* List<T_INDEX,T_VALUE>::find_node(T_INDEX index)
{
    List_node<T_INDEX,T_VALUE>* temp=list.head->next;
    while(temp)
    {
        if(temp->index==index)
        {
            return temp;
        }
        else
        {
            temp=temp->next;
        }
    }
    return NULL;
}



template <typename T_INDEX,typename T_VALUE>
s32 List<T_INDEX,T_VALUE>::delete_node(T_INDEX index)
{
    List_node<T_INDEX,T_VALUE>* p;
    List_node<T_INDEX,T_VALUE>* q;
    p=list.head;
    q=list.head->next;
    while(q)
    {
        if(q->index==index)
        {
            p->next=q->next;
            delete q;
            list.list_len--;
            return 0;
        }
        else
        {
            p=q;
            q=q->next;
        }
    }
    return -1;
}

template <typename T_INDEX,typename T_VALUE>
u32 List<T_INDEX,T_VALUE>::get_list_len()
{
    return list.list_len;
}

template <typename T_INDEX,typename T_VALUE>
void List<T_INDEX,T_VALUE>::print_all_node()
{
    u32 i=0;
    List_node<T_INDEX,T_VALUE>* temp=list.head->next;
    while(temp)
    {
        
        cout<<"node "<<i<<":"<<"index:"<<temp->index<<"  vlaue:"<<temp->value<<endl;
        i++;
        temp=temp->next;
    }
}

#endif
