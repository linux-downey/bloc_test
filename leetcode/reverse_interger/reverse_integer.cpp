#include <iostream>

using namespace std;
int reverse_num(int x,int &res)
{
	if(!x) return res;
	res=res*10+x%10;
	return reverse_num(x/10,res);
}



int main()
{
	int val=0;
	val=reverse_num(-3245454,val);
	cout<<val<<endl;
}
