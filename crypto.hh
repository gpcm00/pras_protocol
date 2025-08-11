#ifndef CRYPTO_H
#define CRYPTO_H

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

struct aes_gcm_payload {
	uint8_t iv[12];
	uint8_t tag[16];
	uint8_t payload[0];
};

ssize_t aes_gcm_encrypt( const uint8_t* payload, size_t len, const uint8_t* key, 
													aes_gcm_payload* cipher_payload);

ssize_t aes_gcm_decrypt(aes_gcm_payload* cipher, int ciphertext_len, const unsigned char* key,
                                                                        unsigned char* plaintext);
#endif