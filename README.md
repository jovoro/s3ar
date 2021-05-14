# s3ar
Simple utility to write stdin to S3 storage

## Why?
I don't know, man. At $dayjob I was tasked with setting up an S3 compatible solution with Ceph.
To play around with it, I needed to put stuff on it. But soon, I got tired to first tar(1) my desired files before sending them to the S3 storage.
In turn, I was looking for a tool which I can pipe my tar output into. To my surprise, I haven't found any, so I decided to make this a fun exercise.
I'm actually planning on using this to do some one-off backups of my files. 

## Does it work?
After using it for a few days I can say that it works quite well. And after fixing the Base64 implementation it seems to do its job pretty reliably.
I've implemented only the most basic things which are needed to handle multipart uploads with S3.
That means that the feature set is pretty limited to my single use-case. But I think that's quite OK, since there are many other tools to do all the other stuff.
I think what's left to do is to implement some options, e.g. to specify different chunk sizes. Also, it would be nice to be able to create checksums for the uploads.

## What do I need to build it?
Curl and OpenSSL is required. Also you will want some kind of Make and, of course, a C compiler.
To actually build, you can do `make s3ar`.

## How do I run it?
You need some kind of S3 storage, doesn't matter, which. Of course, I've tested with our Ceph solution, so YMMV with regards to compatibility. But the S3 API is pretty straightforward, I guess.
To make your S3 information known to the program, you need to define the following environment variables:

* `S3AR_ENDPOINT`: Hostname of your S3 endpoint, e.g. rados.example.com
* `S3AR_BUCKET`: Name of your S3 bucket, e.g. mydatagrave
* `S3AR_KEY`: Your S3 key, e.g. ABC12345FGE8X7Y6Z
* `S3AR_SECRET`: Your S3 secret, e.g. 1a2b3c4d5e6f7g8h9i0jklmnop987654321qrst 

Then, you call the program with one argument, which is the target object, i.e. `./s3ar /mytestblob.dat`. The program then starts reading from `stdin`. So to do actually do something, you might want pipe the output from some other program into it, like so:

```
cd ~
tar -cf - importantstuff/ | s3ar /importantstuff_backup_20210505.tar
```

If everything works out, you have a new tarfile in your S3 bucket. Since it just reads stdin, you can throw basically anything at it. For example, you could encrypt your tar file before putting it somewhere on the internet:

```
tar -cf - stuff/ | openssl des3 -pass pass:WhyYesThisIsSecureDontYouThink | s3ar /encryptedstuff.tar.des3
```

## It doesn't work at all! Where do I complain?
As always, you may reach me at jr at vrtz dot ch. 
