#include <stdio.h>
#include <string.h>
#include <cpuid.h>
#include <openssl/md5.h>

static unsigned int bswap32(unsigned int v) {
    return ((v >> 24) & 0xFF)
         | ((v <<  8) & 0xFF0000)
         | ((v >>  8) & 0xFF00)
         | ((v << 24) & 0xFF000000);
}

int main(void) {
    unsigned int eax, ebx, ecx, edx;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);

    unsigned int part1 = bswap32(eax);
    unsigned int part2 = bswap32(edx);

    char PSN[17];
    snprintf(PSN, 17, "%08X%08X", part1, part2);

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, PSN, 16);
    MD5_Final(digest, &ctx);

    char license[33];
    for (int i = 0; i < 16; i++)
        sprintf(license + i * 2, "%02x", digest[15 - i]);
    license[32] = '\0';

    printf("HWID:    %s\n", PSN);
    printf("License: %s\n", license);
    return 0;
}
