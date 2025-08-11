
#include "crypto.hh"
#include <openssl/evp.h>
#include <openssl/rand.h>

ssize_t aes_gcm_encrypt(const uint8_t* payload, size_t len, const uint8_t* key, 
													aes_gcm_payload* cipher_payload) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
		return -1;
	}
	
	int out_len;
	ssize_t ciphertext_len = 0;

    // Generate a random IV
    if (!RAND_bytes(cipher_payload->iv, sizeof(cipher_payload->iv))) {
		goto Cipher_Error_Handler;
    }

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, cipher_payload->iv)) {
		goto Cipher_Error_Handler;
	}
    
    if (!EVP_EncryptUpdate(ctx, cipher_payload->payload, &out_len, payload, len)) {
        goto Cipher_Error_Handler;
	}

    ciphertext_len += out_len;

    if (!EVP_EncryptFinal_ex(ctx, cipher_payload->payload + out_len, &out_len)) {
        goto Cipher_Error_Handler;
	}

    ciphertext_len += out_len;

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, sizeof(cipher_payload->tag), 
																	cipher_payload->tag)) {
		goto Cipher_Error_Handler;
	}

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;

Cipher_Error_Handler:
	EVP_CIPHER_CTX_free(ctx);
	return -1;
}


ssize_t aes_gcm_decrypt(aes_gcm_payload* cipher, int ciphertext_len, const unsigned char* key,
                                                                        unsigned char* plaintext) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return -1;
    }

    int len;
    int plaintext_len = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, cipher->iv) != 1) {
        goto Decipher_Error_Handler;
    }

    if (EVP_DecryptUpdate(ctx, plaintext, &len, cipher->payload, ciphertext_len) != 1)  {
        goto Decipher_Error_Handler;
    }

    plaintext_len += len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, sizeof(cipher->tag), (void*)cipher->tag) != 1)  {
        goto Decipher_Error_Handler;
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        goto Decipher_Error_Handler;
    }

    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;

Decipher_Error_Handler:
    EVP_CIPHER_CTX_free(ctx);
    return -1;
}