def reverse(x):
	val=0
	while(x!=0):
		val=val*10+x%10
		x=x/10
	return val


def main():
	num=-987
	val=reverse(num)
	print val




if __name__ == '__main__':
	main()
