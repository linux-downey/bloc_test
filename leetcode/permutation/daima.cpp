#include <iostream>
#include <vector>

using namespace std;

int factorial(int n)
{
	if(1==n)
		return 1;
	return n*factorial(n-1);
}

template <class Iterator,class T>
void traversal_vec(T t,Iterator &it)
{
	it=t.begin();
	for(;it!=t.end();it++)
	{
		cout<<*it;
	}
	cout<<endl;
}

void permutation(vector<int> &a,int* &p,int n,vector<int> &dst)
{
	if(0==n)
	{
		vector<int>::iterator it=dst.begin();
		traversal_vec(dst,it);
		return;
	}
		
	
	for(int i=0;i<a.size();i++)
	{
		vector<int> tmp=a;
		vector<int>::iterator it=tmp.begin();
		//cout<<a[i]<<endl;
		tmp.erase(it+i);
		dst.push_back(a[i]);
		//*p++=a[i];
		//*p++;
		//cout<<p<<endl;
		permutation(tmp,p,n-1,dst);
		dst.pop_back();
	}
}


void combine(vector<int> &a,int* &p,int n,vector<int> &dst)
{
	if(0==n)
	{
		vector<int>::iterator it=dst.begin();
		traversal_vec(dst,it);
		return;
	}
		
	
	for(int i=0;i<a.size();i++)
	{
		vector<int> tmp=a;
		vector<int>::iterator it=tmp.begin();
		//cout<<a[i]<<endl;
		tmp.erase(it,it+i+1);
		dst.push_back(a[i]);
		//*p++=a[i];
		//*p++;
		//cout<<p<<endl;
		combine(tmp,p,n-1,dst);
		dst.pop_back();
	}
}


/*
x!/(x-1)!+x!/(x-2)!+x!/(x-3)!...
*/
int calculate(int x,int n)
{
	int sum=0;
	for(int i=0;i<n;i++)
	{
		sum+=factorial(x)/factorial(x-1-i);		
	}
	return sum;
}


void permutation(vector<int> &a,int n)
{
	vector<int> dst;
	int m=a.size();
	//size!!!!
	int p_size=calculate(a.size(),n);
	
	int *p=new int[factorial(m)/factorial(m-n)+a.size()];
	//*(p+15)=9;
	//cout<<factorial(m)/factorial(m-n)<<endl;
	permutation(a,p,n,dst);
	
}

void combine(vector<int> &a,int n)
{
	vector<int> dst;
	int m=a.size();
	//size!!!!
	int p_size=calculate(a.size(),n);
	
	int *p=new int[factorial(m)/factorial(m-n)+a.size()];
	//*(p+15)=9;
	//cout<<factorial(m)/factorial(m-n)<<endl;
	combine(a,p,n,dst);
}




int main()
{
	vector<int> a={1,2,3,4,5,6,7};
	vector<int>::iterator it;
	combine(a,3);
	//traversal_vec(a,it);
	//cout<<calculate(4,3)<<endl;
	return 1;
}

