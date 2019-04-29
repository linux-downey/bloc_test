def palindrom(x):
	res=0
	while(x):
		x=res*10+x%10
		x/=10
	