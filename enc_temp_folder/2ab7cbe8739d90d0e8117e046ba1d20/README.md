# HttpFileServer

WIP: Needs several security features, backups and other features for more production ready code, but should be fine if you run this off a raspberry pi and on your local network.
Feel free to the steal code, it's what I did to make this.

V1.4

Now has search, and a media player type

This is finished for now. 
But V2 should ideally work towards a proper prod ready server
It is currently missing:
	- Concurrent acception: kqueue/io_uring/
		- ThreadPool instead of allocating a new thread everytime
	- Rate limiting
	- 