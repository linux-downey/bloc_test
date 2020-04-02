def reverse(x,res):
	if x==0:
		return res
	return reverse(x/10,(res*10+x%10))


def main():
	num=-987
	res=0
	val=reverse(num,res)
	print val



if __name__ == '__main__':
	main()
