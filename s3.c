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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <openssl/hmac.h>
#include "s3.h"
#include "b64.h"

size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userp);
size_t header_callback(char *buffer, size_t size, size_t nmemb, void *userp);
size_t write_callback(void *data, size_t size, size_t nmemb, void *userp);

char *strip_content_type(char *contenttype) {
	char *result;
	int i, len;

	len = strlen(contenttype);
	result = malloc(len+1);
	if(result == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return NULL;
	}

	for(i=0; i<len; i++) {
		if(contenttype[i] == ';') {
			result[i] = '\0';
			break;
		}
		
		result[i] = contenttype[i];
	}

	result[len] = '\0';
	return result;
}

char *strip_etag(char *etag, size_t etlen) {
	char *result;
	int i;
	int trim = 6;
	int skip = 0;

	result = malloc(etlen+1-trim);

	if(result == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return NULL;
	}


	for(i=0+trim; i<etlen; i++) {
		if(etag[i] == '"') {
			skip++;
			continue;
		}

		if(etag[i] == '\r' || etag[i] == '\n' || etag[i] == '\0') {
			result[i-trim-skip] = '\0';
			break;
		}
		result[i-trim-skip] = etag[i];
	}

	result[etlen+1-trim-skip] = '\0';
	return result;
}

int s3_talk(char *endpoint, char *bucket, char *aws_path, char *method, char *getparms, char *key, char *secret, char *contenttype, unsigned char *buffer, size_t buflen, char **responsehdr, size_t *responsehdrsiz) {
	char *signature;
	char datestr[100];
	char *b64str;
	char authhdr[BUFSIZ];
	char datehdr[BUFSIZ];
	char hosthdr[BUFSIZ];
	char conthdr[BUFSIZ];
	char pathstr[BUFSIZ];
	char requrl[BUFSIZ];
	char *stripped_ct;
	char *etagstr = NULL;
	int bytes_free;
	CURL *curl;
	CURLcode res;

	time_t now = time(NULL);
	struct tm *t = gmtime(&now);

	unsigned char *m;
	unsigned char *md;
	unsigned int md_len;
	size_t sl;

	struct WriteThis wt;
	struct ETagHeader et;
	et.buflen = 0;
	struct ResponseBuffer resbuf;
	resbuf.size = 0;

#ifdef S3ARDEBUG
	/*
	fprintf(stderr, " -- s3_talk: Content of buffer (%d bytes):\n", buflen);
	for (int i = 0; i < buflen; i++) {
		printf(" %02x", (unsigned char)buffer[i]);
	}
	fprintf(stderr, "\n -- s3_talk: End of content of buffer\n\n");
	*/ 
#endif

	bytes_free = BUFSIZ;
	strncpy(pathstr, aws_path, BUFSIZ-1);
	pathstr[BUFSIZ-1] = '\0';
	sl = strlen(pathstr);
	
	m = malloc(BUFSIZ);
	md = calloc(EVP_MAX_MD_SIZE, sizeof(char));

	if(m == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return 1;
	}

	if(md == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return 1;
	}

	stripped_ct = strip_content_type(contenttype);
	if(stripped_ct == NULL) {
		fprintf(stderr, "strip_content_type() failed\n");
		return 1;
	}

	strftime(datestr, sizeof(datestr)-1, "%a, %d %b %Y %T %z", t);
	snprintf(datehdr, BUFSIZ, "Date: %s", datestr);

	if(strlen(getparms) > 0) {
		snprintf(requrl, BUFSIZ, "https://%s.%s%s?%s", bucket, endpoint, pathstr, getparms);
		snprintf(m, BUFSIZ, "%s\n\n%s\n%s\n/%s%s?%s", method, contenttype, datestr, bucket, pathstr, getparms); 
	} else {
		snprintf(requrl, BUFSIZ, "https://%s.%s%s", bucket, endpoint, pathstr);
		snprintf(m, BUFSIZ, "%s\n\n%s\n%s\n/%s%s", method, contenttype, datestr, bucket, pathstr);
	}

#ifdef S3ARDEBUG
	fprintf(stderr, " -- s3_talk: Contents of m:\n%s\n -- s3_talk: End contents of m\n\n", m);
#endif
	HMAC(EVP_sha1(), secret, strlen(secret), m, strlen(m), md, &md_len);
	b64str = base64_encode(md);
	snprintf(authhdr, BUFSIZ, "Authorization: AWS %s:%s", key, b64str);
	res = curl_global_init(CURL_GLOBAL_DEFAULT);
	
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(res));
		return 1;
	}

	curl = curl_easy_init();

	if(!curl) {
		curl_global_cleanup();
		fprintf(stderr, "curl_easy_init() failed\n");
		return 1;
	}

	snprintf(hosthdr, BUFSIZ, "Host: %s.%s", bucket, endpoint);
	curl_easy_setopt(curl, CURLOPT_URL, requrl);

	if(strncmp(method, "POS", 3) == 0) {
		resbuf.response = NULL;
		resbuf.size = 0;
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)buflen);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resbuf);
	} else if(strncmp(method, "PUT", 3) == 0) {
		wt.readptr = buffer;
		wt.sizeleft = buflen;
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
		curl_easy_setopt(curl, CURLOPT_READDATA, &wt);
		curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, buflen);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &et);
	} else if(strncmp(method, "DEL", 3) == 0) {
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	} else {
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	}

	struct curl_slist *sendheaders = NULL;
	sendheaders = curl_slist_append(sendheaders, authhdr);
	sendheaders = curl_slist_append(sendheaders, datehdr);
	sendheaders = curl_slist_append(sendheaders, hosthdr);

	if(contenttype) {
		snprintf(conthdr, BUFSIZ, "Content-Type: %s", contenttype);
		sendheaders = curl_slist_append(sendheaders, conthdr);
	}

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, sendheaders);
#ifdef S3ARDEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	fprintf(stderr, " -- s3_talk: Header contents:\nrequrl: %s\nhosthdr: %s\nauthhdr: %s\ndatehdr: %s\nconthdr: %s\n -- s3_talk: End header contents\n\n", requrl, hosthdr, authhdr, datehdr, conthdr);
#endif
	res = curl_easy_perform(curl);

	if(res != CURLE_OK) {
		curl_global_cleanup();
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_slist_free_all(sendheaders);
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		if(et.buffer) {
			free(et.buffer);
		}
		return 1;
	}

	if(et.buflen > 0) {
		/* Get ETag Header */
		*responsehdr = et.buffer;
		*responsehdrsiz = et.buflen;
#ifdef S3ARDEBUG
		fprintf(stderr, " -- s3_talk: Content of et.buffer:\n%s\n -- s3_talk: End of content of et.buffer\n\n", et.buffer);
#endif
	}

	if(resbuf.size > 0) {
		/* Get XML (or any other) response */
		*responsehdr = resbuf.response;
		*responsehdrsiz = resbuf.size;
#ifdef S3ARDEBUG
		fprintf(stderr, " -- s3_talk: Content of resbuf.response:\n%s\n -- s3_talk: End of content of resbuf.response\n\n", resbuf.response);
#endif
	}

	free(stripped_ct);
	free(m);
	free(md);
	free(b64str);
	curl_slist_free_all(sendheaders);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return 0;
}

int s3_initpart(char *endpoint, char *bucket, char *aws_path, char *key, char *secret, char **uploadId, size_t *uidsiz) {
	char *response;
	char *sep;
        char *orig;
	char *tmp;
	size_t responselen;
        short takenext = 0;
	char *request = calloc(strlen(aws_path) + 9, sizeof(char));
	snprintf(request, strlen(aws_path) + 9, "%s?uploads", aws_path);
	s3_talk(endpoint, bucket, request, "POST", "", key, secret, "text/plain", NULL, 0, &response, &responselen); 

	/* Get UploadId */
	if(responselen > 0) {
		orig = response;
		while((sep = strsep(&response, ">")) != NULL) {
			if(takenext == 1) {
				tmp = strsep(&sep, "<");
				*uidsiz = strlen(tmp)+1;
				*uploadId = calloc(*uidsiz, sizeof(char));
				if(*uploadId == NULL) {
					fprintf(stderr, "calloc() for uploadId failed.\n");
					return 1;
				}

				strcpy(*uploadId, tmp);
				break;
			}

			if(strcmp(sep, "<UploadId") == 0)
				takenext = 1;
		}

		free(orig);
		orig = response = NULL;
	}
	free(request);
}

int s3_putpart(char *endpoint, char *bucket, char *aws_path, char *key, char *secret, char *uploadid, unsigned int partnum, char *buffer, size_t buflen, char **responsehdr, size_t *responsehdrsiz) {
	char getparms[BUFSIZ];
	char *etagstr = NULL;
	size_t etagstrlen = 0;
	int ret = 0;

	snprintf(getparms, BUFSIZ, "partNumber=%d&uploadId=%s", partnum, uploadid);
	ret = s3_talk(endpoint, bucket, aws_path, "PUT", getparms, key, secret, "application/octet-stream", buffer, buflen, &etagstr, &etagstrlen); 

	if(ret != 0) {
		free(etagstr);
		return 1;
	}

	if(etagstrlen > 0) {
		*responsehdr = strip_etag(etagstr, etagstrlen);
		free(etagstr);
		if(*responsehdr == NULL) {
			fprintf(stderr, "strip_etag() failed.\n");
			return 1;
		}
		*responsehdrsiz = strlen(*responsehdr);
	} else {
		fprintf(stderr, " -- s3_putpart: etagstrlen not greater than zero, assuming error\n");
		return 1;
	}

	return 0;
}

int s3_completepart(char *endpoint, char *bucket, char *aws_path, char *key, char *secret, char *uploadid, struct ETag *et, size_t partnum) {
	unsigned int i;
	char *response = NULL;
	size_t responselen = 0;
	unsigned char *getparms;
	char numbuf[BUFSIZ] = "";
	int numbuflen = 0;
	unsigned char *buffer = NULL;
	unsigned char *tmpbuf = NULL;
	int buflen = 1;
	int getparmlen = 0;
	int ret;
	struct ETag *curr_et;

	unsigned char initbody[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><CompleteMultipartUpload>";
	unsigned char initpart[] = "<Part>";
	unsigned char initpnum[] = "<PartNumber>";
	unsigned char endpnum[] = "</PartNumber>";
	unsigned char initetag[] = "<ETag>";
	unsigned char endetag[] = "</ETag>";
	unsigned char endpart[] = "</Part>";
	unsigned char endbody[] = "</CompleteMultipartUpload>";

	getparmlen = 9 + strlen(uploadid) + 1; /* "uploadId=" + uploadId + NULL*/
	getparms = calloc(getparmlen, sizeof(char));

	if(getparms == NULL) {
		fprintf(stderr, "calloc() for getparms failed.\n");
		return 1;
	}

        snprintf(getparms, getparmlen, "uploadId=%s", uploadid);

	buflen += strlen(initbody);
	buffer = calloc(buflen, sizeof(char));

	if(buffer == NULL) {
		fprintf(stderr, "calloc() for buffer failed.\n");
		return 1;
	}

	strncat(buffer, initbody, strlen(initbody));

	for(i=0; i<partnum; i++) {
		curr_et = et+i;

                if(curr_et->buffer == NULL) {
                        fprintf(stderr, " -- s3_completepart: Part with unset ETag found, aborting.\n");
			free(buffer);
                        return 1;
                }

		if(curr_et->partnum < 1) {
                        fprintf(stderr, " -- s3_completepart: Part with number less than 1 found, aborting.\n");
			free(buffer);
                        return 1;
		}

                numbuflen = snprintf(numbuf, BUFSIZ-1, "%s%s%s%s%s%d%s%s", initpart, initetag, (unsigned char*)curr_et->buffer, endetag, initpnum, curr_et->partnum, endpnum, endpart);
		buflen += numbuflen;
		tmpbuf = realloc(buffer, buflen);

		if(tmpbuf == NULL) {
			fprintf(stderr, "realloc() for tmpbuf failed.\n");
			free(buffer);
			return 1;
		}

		buffer = tmpbuf;
		tmpbuf = NULL;
		strncat(buffer, numbuf, numbuflen);
                free(curr_et->buffer);
	}

	buflen += strlen(endbody);
	tmpbuf = realloc(buffer, buflen);

	if(tmpbuf == NULL) {
		fprintf(stderr, "realloc() for tmpbuf failed.\n");
		free(buffer);
		return 1;
	}

	buffer = tmpbuf;
	tmpbuf = NULL;
	strncat(buffer, endbody, strlen(endbody));
	buffer[buflen-1] = '\0';
#ifdef S3ARDEBUG
	fprintf(stderr, " -- s3_completepart: Content of buffer:\n%s\n -- s3_completepart: End of content of buffer\n\n", buffer);
#endif
	ret = s3_talk(endpoint, bucket, aws_path, "POST", getparms, key, secret, "multipart/form-data;", buffer, buflen-1, &response, &responselen); /* buflen -1: We are transferring raw bytes, so terminating NULL character would be included, which results in a malformed XML */
	if(ret != 0) {
		fprintf(stderr, "Failed to send MultipartUploadComplete request, you might want to send it manually again. See above output for ETags for each part number.\n");
	}
	free(getparms);
	free(buffer);
}

size_t read_callback(char *dest, size_t size, size_t nmemb, void *userp) {
	/* https://curl.se/libcurl/c/post-callback.html */
	struct WriteThis *wt = (struct WriteThis *)userp;
	size_t buffer_size = size*nmemb;

	if(wt->sizeleft) {
		/* copy as much as possible from the source to the destination */
		size_t copy_this_much = wt->sizeleft;
		if(copy_this_much > buffer_size)
			copy_this_much = buffer_size;
		memcpy(dest, wt->readptr, copy_this_much);
	
		wt->readptr += copy_this_much;
		wt->sizeleft -= copy_this_much;
		return copy_this_much; /* we copied this many bytes */
	}

	return 0; /* no more data left to deliver */
}

size_t header_callback(char *buffer, size_t size, size_t nmemb, void *userp) {
	struct ETagHeader *et = (struct ETagHeader *)userp;

	if(strncmp("ETag: ", buffer, 6) == 0) {
		et->buffer = calloc(nmemb+1, size);

		if(et->buffer == NULL) {
			fprintf(stderr, "calloc() for et->buffer failed.\n");
			return 0;
		}

		memcpy(et->buffer, buffer, nmemb*size); 
		et->buflen = nmemb*size;
	}
	return nmemb;
}

size_t write_callback(void *data, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct ResponseBuffer * resbuf = (struct ResponseBuffer *)userp;

	char *ptr = realloc(resbuf->response, resbuf->size + realsize + 1);
	if(ptr == NULL)
		return 0;

	resbuf->response = ptr;
	memcpy(&(resbuf->response[resbuf->size]), data, realsize);
	resbuf->size += realsize;
	resbuf->response[resbuf->size] = 0;

	return realsize;
}
