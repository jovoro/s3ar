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
#include <unistd.h>
#include <string.h>
#include "s3.h"

int main(int argc, char *argv[]) {
	char *endpoint;
	char *bucket;
	char *aws_key;
	char *aws_secret;
	char *uploadId = NULL;
	size_t uploadIdLen = 0;
	char *responsehdr = NULL;
	size_t rhlen = 0;
	unsigned int partnum = 0;
	unsigned int i;
	int ret = 1;
	char *buffer;
        size_t buflen;
        struct ETag *et = NULL;
        struct ETag *curr_et = NULL;
	short oktocomplete = 1;
	int c;

	if(argc < 2) {
		fprintf(stderr, "Missing aws_path (/foo.xyz)\n");
		return(1);
	}

	endpoint = getenv("S3AR_ENDPOINT");
	bucket = getenv("S3AR_BUCKET");
	aws_key = getenv("S3AR_KEY");
	aws_secret = getenv("S3AR_SECRET");

	if(endpoint == NULL) {
		fprintf(stderr, "Missing endpoint address. Please provide address in environment variable S3AR_ENDPOINT.\n");
		exit(EXIT_FAILURE);
	}

	if(bucket == NULL) {
		fprintf(stderr, "Missing bucket name. Please provide name in environment variable S3AR_BUCKET.\n");
		exit(EXIT_FAILURE);
	}

	if(aws_key == NULL) {
		fprintf(stderr, "Missing S3 key. Please provide key in environment variable S3AR_KEY.\n");
		exit(EXIT_FAILURE);
	}

	if(aws_secret == NULL) {
		fprintf(stderr, "Missing S3 secret. Please provide secret in environment variable S3AR_SECRET.\n");
		exit(EXIT_FAILURE);
	}

	s3_initpart(endpoint, bucket, argv[1], aws_key, aws_secret, &uploadId, &uploadIdLen);

	if(uploadIdLen < 1 || uploadId == NULL) {
		fprintf(stderr, "Cannot get upload ID.\n");
		return 1;
	}

	fprintf(stderr, "Upload ID: %s\n", uploadId);
	buffer = calloc(S3_PUT_BUFSIZ, sizeof(char));

	if(buffer == NULL) {
		fprintf(stderr, "Cannot allocate memory for buffer.\n");
		exit(EXIT_FAILURE);
	}

	while(!feof(stdin)) {
                if((buflen = fread(buffer, sizeof(char), S3_PUT_BUFSIZ, stdin)) != 0) {
#ifdef S3ARDEBUG
                        fprintf(stderr, " -- s3ar: read %d bytes of stdin\n", buflen);
#endif
			partnum++;
			ret = 1;
			for(i=0; i <= S3_MAX_UPLOAD_RETRY; i++) {
				if(i > 0) {
					fprintf(stderr, "Warning: Upload of part %d has seemingly failed, retrying in 5 seconds... (%d of %d retries) \n", partnum, i, S3_MAX_UPLOAD_RETRY);
					sleep(S3_UPLOAD_RETRY_WAIT);
				}
                        	
				ret = s3_putpart(endpoint, bucket, argv[1], aws_key, aws_secret, uploadId, partnum, buffer, buflen, &responsehdr, &rhlen);
				if(ret == 0)
					break;
			}
			if(i >= 3) {
				fprintf(stderr, "Failed upload of part %d three times, giving up.\n", partnum);
				exit(EXIT_FAILURE);
			}

			fprintf(stderr, "Part %5d: %s\n", partnum, responsehdr);
			fflush(stdout);
                        et = realloc(et, partnum * sizeof(struct ETag));

                        if(et == NULL) {
                                fprintf(stderr, "Failed to realloc() space for ETag");
                                exit(1);
                        }

                        curr_et = et+partnum-1;
                        curr_et->partnum = partnum;
                        curr_et->buffer = responsehdr;
                        curr_et->buflen = rhlen;
			responsehdr = NULL;
			rhlen = 0;
                }
        }

	free(buffer);

        for(i=0; i<partnum; i++) {
                curr_et = et+i;
		if(curr_et->buffer == NULL) {
                	fprintf(stderr, "Part %d has empty ETag\n", curr_et->partnum);
			oktocomplete = 0;
		}
        }
	
	if(oktocomplete < 1) {
			fprintf(stderr, " -- s3ar: Part with unset ETag found, will not complete.\n");
			exit(EXIT_FAILURE);
	}

	sleep(S3_UPLOAD_RETRY_WAIT);
	s3_completepart(endpoint, bucket, argv[1], aws_key, aws_secret, uploadId, et, partnum);

	

        free(et);
	free(uploadId);
	uploadId = NULL;
}
