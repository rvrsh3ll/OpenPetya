// salsa20.c
/*
References / Learning Resources:
    - https://github.com/Daeinar/salsa20/blob/master/salsa.py
    - https://cr.yp.to/snuffle/salsafamily-20071225.pdf
    - https://asecuritysite.com/symmetric/salsa20
    - Serious Cryptography (2nd Edition), Jean-Philippe Aumasson
*/

#include "salsa20.h"

static inline uint32_t rotl32(uint32_t v, int n)
{
    return (v << n) | (v >> (32 - n));
}

static inline void qr(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    *b ^= rotl32(*a + *d, 7);
    *c ^= rotl32(*b + *a, 9);
    *d ^= rotl32(*c + *b, 13);
    *a ^= rotl32(*d + *c, 18);
}

/// @brief 
/// @param out 
/// @param in 
static void salsa20_block(uint32_t out[16], const uint32_t in[16])
{
    uint32_t s[16];
    for (int i = 0; i < 16; i++)
        s[i] = in[i];

    // 20 rounds (10 double rounds)
    for (int i = 0; i < 10; i++)
    {
        // column rounds
        qr(&s[0],  &s[4],  &s[8],  &s[12]);
        qr(&s[5],  &s[9],  &s[13], &s[1]);
        qr(&s[10], &s[14], &s[2],  &s[6]);
        qr(&s[15], &s[3],  &s[7],  &s[11]);

        // row rounds
        qr(&s[0],  &s[1],  &s[2],  &s[3]);
        qr(&s[5],  &s[6],  &s[7],  &s[4]);
        qr(&s[10], &s[11], &s[8],  &s[9]);
        qr(&s[15], &s[12], &s[13], &s[14]);
    }

    for (int i = 0; i < 16; i++)
        out[i] = s[i] + in[i];
}

/// @brief 
/// @param p 
/// @return 
static uint32_t load32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void salsa20_init(Salsa20_Ctx *ctx, const uint8_t key[32], const uint8_t nonce[8], uint64_t counter)
{
    // expand 32-byte k
    ctx->state[0] = 0x61707865;
    ctx->state[5] = 0x3320646e;
    ctx->state[10] = 0x79622d32;
    ctx->state[15] = 0x6b206574;

    // key (32 bytes)
    ctx->state[1] = load32_le(key +  0);
    ctx->state[2] = load32_le(key +  4);
    ctx->state[3] = load32_le(key +  8);
    ctx->state[4] = load32_le(key + 12);
    ctx->state[11] = load32_le(key + 16);
    ctx->state[12] = load32_le(key + 20);
    ctx->state[13] = load32_le(key + 24);
    ctx->state[14] = load32_le(key + 28);

    // counter (64-bit)
    ctx->state[8] = (uint32_t)(counter);
    ctx->state[9] = (uint32_t)(counter >> 32);

    // nonce (8 bytes)
    ctx->state[6] = load32_le(nonce + 0);
    ctx->state[7] = load32_le(nonce + 4);
}

void salsa20_encrypt(Salsa20_Ctx *ctx, const uint8_t *in, uint8_t *out, uint32_t len)
{
    uint32_t block[16];
    uint8_t keystream[64];

    while (len > 0)
    {
        salsa20_block(block, ctx->state);
        
        // convert block to bytes (little-endian)
        for (int i = 0; i < 16; i++)
        {
            keystream[i*4] = (uint8_t)block[i];
            keystream[i*4 + 1] = (uint8_t)(block[i] >> 8);
            keystream[i*4 + 2] = (uint8_t)(block[i] >> 16);
            keystream[i*4 + 3] = (uint8_t)(block[i] >> 24);
        }

        uint32_t n = len < 64 ? len : 64;
        for (uint32_t i = 0; i < n; i++)
            out[i] = in[i] ^ keystream[i];

        // increment counter
        ctx->state[8]++;
        if (ctx->state[8] == 0)
            ctx->state[9]++;
        
        in += n;
        out += n;
        len -= n;
    }
}