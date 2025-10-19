#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define ROL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

/**
 * ip_to_uint32()
 */
static inline uint32_t ip_to_uint32(const uint8_t ip[4])
{
    return ((uint32_t)ip[0] << 24) |
           ((uint32_t)ip[1] << 16) |
           ((uint32_t)ip[2] << 8) |
           ((uint32_t)ip[3]);
}

/**
 * F_hash_XOR_and_ROL()
 */
uint32_t F_hash_XOR_and_ROL(const uint8_t ip_src[4],
                            uint16_t port_src,
                            const uint8_t ip_dst[4],
                            uint16_t port_dst)
{
    uint32_t src = ip_to_uint32(ip_src);
    uint32_t dst = ip_to_uint32(ip_dst);

    uint32_t hash = src;
    hash = ROL32(hash, 7) ^ dst;
    hash = ROL32(hash, 13) ^ ((uint32_t)port_src << 16 | port_dst);
    hash *= 0x9E3779B9; // Golden ratio constant for mixing
    hash ^= hash >> 16; // Final avalanche step

    return hash;
}

/**
 * F_hash_XOR()
 */
uint32_t F_hash_XOR(const uint8_t ip_src[4],
                    uint16_t port_src,
                    const uint8_t ip_dst[4],
                    uint16_t port_dst)
{
    uint32_t src = ip_to_uint32(ip_src);
    uint32_t dst = ip_to_uint32(ip_dst);

    uint32_t hash = src ^ dst ^ ((uint32_t)port_src << 16) ^ port_dst;
    hash *= 0x9E3779B9;

    return hash;
}

/**
 * print_ip()
 */
void print_ip(const uint8_t ip[4])
{
    printf("%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void print_separator(void)
{
    printf("=================================================================\n");
}

/**
 * compare_hashes()
 */
void compare_hashes(const char *label,
                    const uint8_t ip_src[4], uint16_t port_src,
                    const uint8_t ip_dst[4], uint16_t port_dst)
{
    uint32_t hash_rol = F_hash_XOR_and_ROL(ip_src, port_src, ip_dst, port_dst);
    uint32_t hash_xor = F_hash_XOR(ip_src, port_src, ip_dst, port_dst);

    printf("%s:\n", label);
    printf("  Flux: ");
    print_ip(ip_src);
    printf(":%u → ", port_src);
    print_ip(ip_dst);
    printf(":%u\n", port_dst);
    printf("  XOR + ROL : 0x%08X (%u)\n", hash_rol, hash_rol);
    printf("  XOR pur   : 0x%08X (%u)\n", hash_xor, hash_xor);
    printf("\n");
}

/**
 * analyze_difference()
 */
void analyze_difference(const char *test_name,
                        const uint8_t ip_src1[4], uint16_t port_src1,
                        const uint8_t ip_dst1[4], uint16_t port_dst1,
                        const uint8_t ip_src2[4], uint16_t port_src2,
                        const uint8_t ip_dst2[4], uint16_t port_dst2)
{
    uint32_t h1_rol = F_hash_XOR_and_ROL(ip_src1, port_src1, ip_dst1, port_dst1);
    uint32_t h2_rol = F_hash_XOR_and_ROL(ip_src2, port_src2, ip_dst2, port_dst2);

    uint32_t h1_xor = F_hash_XOR(ip_src1, port_src1, ip_dst1, port_dst1);
    uint32_t h2_xor = F_hash_XOR(ip_src2, port_src2, ip_dst2, port_dst2);

    int bits_diff_rol = __builtin_popcount(h1_rol ^ h2_rol);
    int bits_diff_xor = __builtin_popcount(h1_xor ^ h2_xor);

    printf("%s\n", test_name);
    printf("-----------------------------------------------------------------\n");
    printf("Flux A: ");
    print_ip(ip_src1);
    printf(":%u → ", port_src1);
    print_ip(ip_dst1);
    printf(":%u\n", port_dst1);

    printf("Flux B: ");
    print_ip(ip_src2);
    printf(":%u → ", port_src2);
    print_ip(ip_dst2);
    printf(":%u\n\n", port_dst2);

    printf("XOR + ROL:\n");
    printf("  Hash A: 0x%08X\n", h1_rol);
    printf("  Hash B: 0x%08X\n", h2_rol);
    printf("  Bits différents: %d/32 (%.1f%%)\n",
           bits_diff_rol,
           (bits_diff_rol * 100.0) / 32);

    printf("\nXOR pur:\n");
    printf("  Hash A: 0x%08X\n", h1_xor);
    printf("  Hash B: 0x%08X\n", h2_xor);
    printf("  Bits différents: %d/32 (%.1f%%)\n\n",
           bits_diff_xor,
           (bits_diff_xor * 100.0) / 32);
}

/**
 * main()
 */
int main(void)
{
    print_separator();
    printf("COMPARAISON DES ALGORITHMES DE HACHAGE\n");
    print_separator();
    printf("\n");

    // Flux 1
    uint8_t ip_src1[4] = {192, 168, 0, 1};
    uint16_t port_src1 = 12345;
    uint8_t ip_dst1[4] = {192, 168, 0, 2};
    uint16_t port_dst1 = 12345;

    // Flux 2 : Port destination +1
    uint8_t ip_src2[4] = {192, 168, 0, 1};
    uint16_t port_src2 = 12345;
    uint8_t ip_dst2[4] = {192, 168, 0, 2};
    uint16_t port_dst2 = 12346;

    // Flux 3 : IP destination +1
    uint8_t ip_src3[4] = {192, 168, 0, 1};
    uint16_t port_src3 = 12345;
    uint8_t ip_dst3[4] = {192, 168, 0, 3};
    uint16_t port_dst3 = 12345;

    // Flux 4 : Port source +1
    uint8_t ip_src4[4] = {192, 168, 0, 1};
    uint16_t port_src4 = 12346;
    uint8_t ip_dst4[4] = {192, 168, 0, 2};
    uint16_t port_dst4 = 12345;

    // Flux 5 : IP destination +1
    uint8_t ip_src5[4] = {192, 168, 0, 3};
    uint16_t port_src5 = 12345;
    uint8_t ip_dst5[4] = {192, 168, 0, 2};
    uint16_t port_dst5 = 12345;

    // Flux 6 : IP et Port complètement différentes
    uint8_t ip_src6[4] = {10, 0, 0, 1};
    uint16_t port_src6 = 80;
    uint8_t ip_dst6[4] = {172, 16, 0, 1};
    uint16_t port_dst6 = 443;

    // TEST 1 : Port destination +1
    analyze_difference("TEST 1 : Port destination +1 (12345 → 12346)",
                       ip_src1, port_src1, ip_dst1, port_dst1,
                       ip_src2, port_src2, ip_dst2, port_dst2);

    // TEST 2 : IP destination +1
    analyze_difference("TEST 2 : IP destination +1 (192.168.0.2 → 192.168.0.3)",
                       ip_src1, port_src1, ip_dst1, port_dst1,
                       ip_src3, port_src3, ip_dst3, port_dst3);

    // TEST 3 : Port source +1
    analyze_difference("TEST 3 : Port source +1 (12345 → 12346)",
                       ip_src1, port_src1, ip_dst1, port_dst1,
                       ip_src4, port_src4, ip_dst4, port_dst4);

    // TEST 4 : IP source +1
    analyze_difference("TEST 4 : IP source +1 (192.168.0.2 → 192.168.0.3)",
                       ip_src1, port_src1, ip_dst1, port_dst1,
                       ip_src5, port_src5, ip_dst5, port_dst5);

    // TEST 5 : Port et IP complètement différentes
    analyze_difference("TEST 5 : Flux complètement différents",
                       ip_src1, port_src1, ip_dst1, port_dst1,
                       ip_src6, port_src6, ip_dst6, port_dst6);

    return 0;
}
