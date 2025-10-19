/* Ce code permet simplement de montrer pour deux couples IP différentes assez proches un mélange du xor */

#include <stdint.h>
#include <stdio.h>

/**
 * XOR()
 */
uint32_t XOR(uint32_t input1, uint32_t input2)
{
    return input1 ^ input2;
}

/**
 * ip_to_uint32()
 */
uint32_t ip_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
}

/**
 * print_ip()
 */
void print_ip(uint32_t ip)
{
    uint8_t a = (ip >> 24) & 0xFF;
    uint8_t b = (ip >> 16) & 0xFF;
    uint8_t c = (ip >> 8) & 0xFF;
    uint8_t d = ip & 0xFF;
    printf("%u.%u.%u.%u", a, b, c, d);
}

int main(void)
{
    uint32_t ip1 = ip_to_uint32(192, 168, 0, 1);
    uint32_t ip2 = ip_to_uint32(192, 168, 0, 2);
    uint32_t ip3 = ip_to_uint32(192, 168, 0, 3);

    uint32_t result = XOR(ip1, ip2);
    uint32_t result2 = XOR(ip1, ip3);

    printf("Résultat XOR = ");
    print_ip(result);
    printf("\n");

    printf("Résultat XOR = ");
    print_ip(result2);
    printf("\n");

    return 0;
}