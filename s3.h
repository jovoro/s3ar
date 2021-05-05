/* Copyright (c) 2021 J. von Rotz <jr@vrtz.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define S3_PUT_BUFSIZ 134217728 /* 128M per part */
#define S3_BLOCKSIZE 512
#define S3_MAX_PART 16384
#define S3_MAX_UPLOAD_RETRY 3
#define S3_UPLOAD_RETRY_WAIT 5

struct WriteThis {
	const char *readptr;
	size_t sizeleft;
};

struct ETag {
        int partnum;
        char *buffer;
        size_t buflen;
};

struct ETagHeader {
	char *buffer;
	size_t buflen;
};

struct ResponseBuffer {
	char *response;
	size_t size;
};

int s3_talk(char *endpoint, char *bucket, char *aws_path, char *method, char *getparms, char *key, char *secret, char *contenttype, unsigned char *buffer, size_t buflen, char **responsehdr, size_t *responsehdrsiz);
int s3_putpart(char *endpoint, char *bucket, char *aws_path, char *key, char *secret, char *uploadid, unsigned int partnum, char *buffer, size_t buflen, char **responsehdr, size_t *responsehdrsiz);
int s3_initpart(char *endpoint, char *bucket, char *aws_path, char *key, char *secret, char **uploadId, size_t *uidlen);
int s3_completepart(char *endpoint, char *bucket, char *aws_path, char *key, char *secret, char *uploadid, struct ETag *et, size_t partnum);
