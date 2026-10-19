#include <stdio.h>
#include <stdint.h>
#include <string.h>

void generate_short_name(const char* lfn, uint8_t* sfn_out, int attempt) {
    // 1. Initialize 11-byte FAT entry with spaces (8 base + 3 ext)
    for (int i = 0; i < 11; i++) {
        sfn_out[i] = ' ';
    }

    // 2. Extract base name (up to 8 characters, converted to uppercase)
    char base_name[8];
    int base_len = 0, i = 0;

    while (lfn[i] != '\0' && lfn[i] != '.' && base_len < 8) {
        char c = lfn[i];
        if (c != ' ') {
            if (c >= 'a' && c <= 'z') c -= 32;
            base_name[base_len++] = c;
        }
        i++;
    }

    // 3. Format numeric tail: ~1, ~2, ..., ~999
    char suffix[8];
    int suffix_len = 0;
    if (attempt > 0) {
        suffix_len = snprintf(suffix, sizeof(suffix), "~%d", attempt);
    }

    int keep_len = (base_len + suffix_len > 8) ? (8 - suffix_len) : base_len;
    if (keep_len < 0) keep_len = 0;

    for (int j = 0; j < keep_len; j++) {
        sfn_out[j] = (uint8_t)base_name[j];
    }
    for (int j = 0; j < suffix_len; j++) {
        sfn_out[keep_len + j] = (uint8_t)suffix[j];
    }

    // 4. Locate extension after the last dot
    int last_dot = -1;
    for (int len = 0; lfn[len] != '\0'; len++) {
        if (lfn[len] == '.') {
            last_dot = len;
        }
    }

    // 5. Copy up to 3 uppercase characters into extension slots (indices 8-10)
    if (last_dot != -1) {
        int ext_idx = 8;
        i = last_dot + 1;
        while (lfn[i] != '\0' && ext_idx < 11) {
            char c = lfn[i];
            if (c != ' ') {
                if (c >= 'a' && c <= 'z') c -= 32;
                sfn_out[ext_idx++] = (uint8_t)c;
            }
            i++;
        }
    }
}

int main(void) {
    // FAT 8.3 entry is exactly 11 bytes (not null-terminated)
    uint8_t sfn[11];
    generate_short_name("testfile.txt", sfn, 1);

    // Print raw 11-byte representation enclosed in brackets
    printf("Raw FAT buffer: [");
    for (int i = 0; i < 11; i++) {
        putchar(sfn[i]);
    }
    printf("]\n");

    // Print with standard '%.11s' precision specifier
    printf("Formatted (8.3): %.8s.%.3s\n", sfn, sfn + 8);

    return 0;
}