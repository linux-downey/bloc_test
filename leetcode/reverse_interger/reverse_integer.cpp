#include <iostream>

using namespace std;
int reverse_num(int x,int res)
{
	if(!x) return res;
	return reverse_num(x/10,(res*10+x%10));
}



int main()
{
	int val=0;
	val=reverse_num(-9870,val);
	cout<<val<<endl;
}
