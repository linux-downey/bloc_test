#include <iostream>
#include <vector>


using namespace std;


// void permutaion(vector<int> &a,num)
// {
//     static int result[][num]={};
//     if(0==num)
//         return;
//     for(int i=0;i<len(a),i++)
//     {
        
//     }

// }

void func(int x,int count)
{
    static int a=x;
    count--;
    if(count==0)
        return;
    cout << a << endl;
    func(--a,count);

    
}



int main()
{
    func(5,5);

    cout<<endl;
    return 1;
}