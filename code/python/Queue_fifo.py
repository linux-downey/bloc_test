import Queue

Q=Queue.Queue(2)
Q.put(12)
Q.put(13,False)
try:
    Q.put(14,True,1)
except :
    print "Queue is full"
    print Q.full()
    print Q.qsize()
    Q.get()
    Q.get()
    try:
        Q.get(True,1)
    except:
        print Q.empty()
        print "Queue is empty"
    else:
        pass
else:
    pass




