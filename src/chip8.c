#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <SDL2/SDL.h>
#include <time.h>

#define CHIP8_VREG_COUNT 16     /* V registers count */
#define CHIP8_STACK_CAP  64     /* Stack capacity */
#define CHIP8_DW         64     /* Display Width */
#define CHIP8_DH         32     /* Display Height */
#define CHIP8_RAM_CAP    1024*4 /* 4096 Addressable Memory */
#define CHIP8_WINDOW_HEIGHT 800 /* SDL Window Height */
#define CHIP8_WINDOW_WIDTH  600 /* SDL Window Width */

typedef struct Chip8_Stack {
    uint8_t slots[CHIP8_STACK_CAP];
    uint8_t count;
} Chip8_Stack;

uint8_t      chip8_vregs[CHIP8_VREG_COUNT]; // Registers V0 - V15
Chip8_Stack  chip8_stack = {0};             // 64-Byte Stack
uint8_t      chip8_sp;                      // Stack Pointer
uint16_t     chip8_ir;                      // Index register
uint16_t     chip8_pc;                      // Program Counter

uint8_t chip8_d_timer; // Delay Timer
uint8_t chip8_s_timer; // Sound Timer

static uint8_t chip8_frame_buffer[CHIP8_DH][CHIP8_DW] = {0}; // Frame Buffer
static uint8_t chip8_memory[CHIP8_RAM_CAP] = {0};            // Chip8 RAM

uint8_t chip8_read_memory(const uint16_t loc)
{
    // casting to int16_t because gcc will just wrap the negative values
    if ((int16_t)loc >= 0 && loc <= CHIP8_RAM_CAP) {
        return chip8_memory[loc];
    } else {
        fprintf(stderr, "[ERROR] Index `0X%X` Out of Bounds\n", loc);
        exit(EXIT_FAILURE);
    }
}

bool chip8_write_memory(const uint16_t loc, uint8_t data)
{
    // casting to int16_t because gcc will just wrap the negative values
    if ((int16_t)loc >= 0 && loc <= CHIP8_RAM_CAP) {
        chip8_memory[loc] = data;
        return true;
    } else {
        fprintf(stderr, "[ERROR] Index `0X%X` Out of Bounds\n", loc);
        return false;
    }
}

static inline void chip8_split_uint16_t(uint16_t value, uint8_t *high, uint8_t *low)
{
    *high = value >> 8;   // higher byte
    *low  = value & 0xFF; // low-byte
}

static inline uint16_t chip8_bytes_to_uint16_t(uint8_t high, uint8_t low)
{
    return (high << 8) | low;
}

bool chip8_stack_push(uint16_t value)
{
    if (chip8_stack.count == CHIP8_STACK_CAP) {
        fprintf(stderr, "[ERROR] Stack Full\n");
        return false;
    }

    uint8_t high = 0; // high
    uint8_t low  = 0; // low
    chip8_split_uint16_t(value, &high, &low); // Split the u16 to u8s
    chip8_stack.slots[chip8_stack.count++] = high; // push high
    chip8_stack.slots[chip8_stack.count++] = high; // push low

    return true;
}

uint16_t chip8_stack_pop()
{
    if (chip8_stack.count < 2) {
        fprintf(stderr, "[ERROR] Stack Empty\n");
        exit(1);
    }

    uint8_t low  = chip8_stack.slots[chip8_stack.count--]; // Pop low
    uint8_t high = chip8_stack.slots[chip8_stack.count--]; // Pop high

    return chip8_bytes_to_uint16_t(high, low);
}

typedef enum Chip8_Fonts {
    CHIP8_ZERO = 0,
    CHIP8_ONE,
    CHIP8_TWO,
    CHIP8_THREE,
    CHIP8_FOUR,
    CHIP8_FIVE,
    CHIP8_SIX,
    CHIP8_SEVEN,
    CHIP8_EIGHT,
    CHIP8_NINE,
    CHIP8_A,
    CHIP8_B,
    CHIP8_C,
    CHIP8_D,
    CHIP8_E,
    CHIP8_F,

    // Font Count
    CHIP8_FONT_COUNT
} Chip8_Fonts;

#define CHIP8_FONT_HEIGHT 5

const uint8_t chip8_fontset[CHIP8_FONT_COUNT][CHIP8_FONT_HEIGHT] = {
    [CHIP8_ZERO]  = {0XF0, 0X90, 0X90, 0X90, 0XF0},
    [CHIP8_ONE]   = {0X20, 0X60, 0X20, 0X20, 0X70},
    [CHIP8_TWO]   = {0XF0, 0X10, 0XF0, 0X80, 0XF0},
    [CHIP8_THREE] = {0XF0, 0X10, 0XF0, 0X10, 0XF0},
    [CHIP8_FOUR]  = {0X90, 0X90, 0XF0, 0X10, 0X10},
    [CHIP8_FIVE]  = {0XF0, 0X90, 0XF0, 0X10, 0XF0},
    [CHIP8_SIX]   = {0XF0, 0X80, 0XF0, 0X90, 0XF0},
    [CHIP8_SEVEN] = {0XF0, 0X10, 0X20, 0X20, 0X40},
    [CHIP8_EIGHT] = {0XF0, 0X90, 0XF0, 0X90, 0XF0},
    [CHIP8_NINE]  = {0XF0, 0X90, 0XF0, 0X10, 0XF0},
    [CHIP8_A]     = {0XF0, 0X90, 0XF0, 0X90, 0X90},
    [CHIP8_B]     = {0XE0, 0X90, 0XE0, 0X90, 0XE0},
    [CHIP8_C]     = {0XF0, 0X80, 0X80, 0X80, 0XF0},
    [CHIP8_D]     = {0XE0, 0X90, 0X90, 0X90, 0XE0},
    [CHIP8_E]     = {0XF0, 0X80, 0XF0, 0X80, 0XF0},
    [CHIP8_F]     = {0XF0, 0X80, 0XF0, 0X80, 0X80},
};

bool chip8_load_fontset(void)
{
    for (uint8_t i = 0; i < CHIP8_FONT_COUNT; ++i) {
        for (uint8_t j = 0; j < CHIP8_FONT_HEIGHT; ++j) {
            uint16_t index = i * CHIP8_FONT_HEIGHT + j;
            if (!chip8_write_memory(index, chip8_fontset[i][j])) return false;
        }
    }

    fprintf(stdout, "[INFO] Successfully Loaded the Fontset into Memory\n");
    return true;
}

void chip8_clear_display(void)
{
    memset(chip8_frame_buffer, 0, sizeof(chip8_frame_buffer));
}

static inline uint8_t chip8_gen_random_byte()
{
    return rand() % UINT8_MAX;
}

bool chip8_execute_opcode()
{
    uint8_t  high   = chip8_read_memory(chip8_pc);
    uint8_t  low    = chip8_read_memory(chip8_pc+1);
    uint16_t opcode = chip8_bytes_to_uint16_t(high, low);

    chip8_pc += 2;
    switch (((opcode >> 12) & 0XF)) { // switch on first nibble
    case 0X0: {
        switch ((opcode & 0XFF)) { // switch on last byte
        case 0XEE: { // 0X00EE
            chip8_clear_display();
            printf("00EE, Clear display: 0X%X\n", opcode);
            return true;
        }

        case 0XE0: {  // 0X00E0
            // Pop PC from stack, return from subroutine
            chip8_pc = chip8_stack_pop();
            printf("00E0, Return: 0X%X\n", opcode);
            return true;
        }

        default:
            fprintf(stderr, "[ERROR] Unknown Last Byte `0X%X` For Opcode 0X%X\n", (opcode & 0XFF), opcode);
            return false;
        }
    } break;

    case 0X1: {
        printf("1NNN, JMP to opcode: 0X%X\n", opcode);
        chip8_pc = opcode & 0X0FFF; // JMP instruction 1nnn, set pc to nnn
        return true;
    }

    case 0X2: {
        printf("2NNN, CALL: 0X%X\n", opcode);
        if (!chip8_stack_push(chip8_pc)) return false; // Save  PC
        chip8_pc = opcode & 0X0FFF; // Call subroutine 2nnn, set pc to nnn
        return true;
    }

    case 0x3: {
        // SE Vx, byte
        // Skip Next instruction if Vx == byte
        printf("3XKK, SE: Vx Byte: 0X%X\n", opcode);
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte = opcode & 0XFF;

        if (chip8_vregs[v_index] == low_byte) {
            chip8_pc += 2; // skip two => 2 u8s = u16, next pc
        } else {
            ;
        }
        return true;
    }

    case 0X4: {
        // SNE Vx, byte
        // Skip Next instruction if Vx != byte
        printf("4XKK, SNE: Vx Byte: 0X%X\n", opcode);
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte = opcode & 0XFF;

        if (chip8_vregs[v_index] != low_byte) {
            chip8_pc += 2; // skip two => 2 u8s = u16, next pc
        } else {
            ;
        }
        return true;
    }

    case 0X6: {
        // 0X6XKK
        // put kk into V[X]
        printf("0X6KK, LD Vx, byte: 0X%X\n", opcode);
        uint8_t v_index = ((opcode >> 8) & 0XF);
        uint8_t low_byte = opcode & 0XFF;

        chip8_vregs[v_index] = low_byte;
        return true;
    }

    case 0X7: {
        // 0X79F8
        printf("7XKK, ADD Vx, byte");
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte = opcode & 0XFF;

        chip8_vregs[v_index] += low_byte; // Add Vx + byte, store it back to Vx
        return true;
    }

    case 0X8: {
        // 0X8ED7
        uint8_t vidx_x   = ((opcode >> 8) & 0XF); // 2nd nibble , x index into v
        uint8_t vidx_y   = ((opcode >> 4) & 0XF); // 3rd nibble , y index into v
        uint8_t l_nibble = (opcode & 0XF);

        switch (l_nibble) {
        case 0x7: {
            printf("8XY7, SUBN Vx, Vy: 0X%X\n", opcode);
            if (chip8_vregs[vidx_y] > chip8_vregs[vidx_x]) {
                chip8_vregs[0XF] = 1; // Set Borrow if greater
            } else {
                chip8_vregs[0XF] = 0; // else not set
            }

            chip8_vregs[vidx_x] = chip8_vregs[vidx_y] - chip8_vregs[vidx_x];
            return true;
        }
        default:
            fprintf(stderr, "[ERROR]: Unknown last nibble `0X%X` for Opcode: 0X%X\n", l_nibble, opcode);
            return false;
        }

        fprintf(stderr, "[PANIC] Unreachable\n");
        return true;
    }

    case 0XA: {
        // 0XANNN
        printf("ANNN, LD I, addr: 0X%X\n", opcode);
        chip8_ir = opcode & 0X0FFF; // load the nnn to ir
        return true;
    }

    case 0XC: {
        // RND Vx, byte
        // extract lower byte from opcode, (&) it with random byte
        // store result in Vx
        printf("CXKK, RND Vx Byte: 0X%X\n", opcode);
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte =  (opcode & 0XFF);
        uint8_t random   = chip8_gen_random_byte();

        chip8_vregs[v_index] = random & low_byte;
        return true;
    }

    case 0XD: {
        // DXYN
        // draw Vx, Vy, nibble, frame[Vx][Vy] = nibble
        printf("Unimplemented DXYN, DRW Vx, Vy, Nibble: 0X%X\n", opcode);
        return true;
    }

    case 0XF: {
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte = (opcode & 0XFF);

        switch (low_byte) {
        case 0X1E: {
            printf("FX1E, ADD I, Vx: 0X%X\n", opcode);
            chip8_ir += chip8_vregs[v_index]; // Add V[x] to ir
            return true;
        }

        case 0X0A: {
            printf("Unimplemented: Fx0A, LD Vx, K: 0X%X\n", opcode);
            return true;
        }

        case 0X29: {
            printf("Unimplemented: Fx29, LD F, Vx: 0X%X\n", opcode);
            return true;
        }

        default:
            fprintf(stderr, "[ERROR]: Unknown low_byte `0X%X` for Opcode: 0X%X\n", low_byte, opcode);
            return false;
        }
        // UNREACHABLE
        fprintf(stderr, "[PANIC] Unreachable\n");
        return true;
    }


    default:
        fprintf(stderr, "[ERROR]: Unknown opcode: 0X%X\n", opcode);
        return false;
    }
    // UNREACHABLE
    fprintf(stderr, "[PANIC] Unreachable\n");
    return false;
}

bool chip8_read_file_into_memory(const char *chip8_file_path, uint32_t *chip8_file_size)
{
    FILE *fp = fopen(chip8_file_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "[ERROR] Could not read `%s`: `%s`\n", chip8_file_path, strerror(errno));
        return false;
    }

    int ret = fseek(fp, 0, SEEK_END);
    if (ret < 0) {
        fprintf(stderr, "[ERROR] Could not seek to end of `%s`: `%s`\n", chip8_file_path, strerror(errno));
        return false;
    }

    long size = ftell(fp);
    if (size <= 0) {
        fprintf(stderr, "[ERROR] File `%s` Empty\n", chip8_file_path);
        return false;
    }

    rewind(fp); // Rewind the file pointer to the beginning

    // Read the chip8 rom directly into the chip8 memory
    size_t bytes = fread(&chip8_memory[0x200], sizeof(uint8_t), size, fp);
    if (bytes != (size_t)size) {
        fprintf(stderr, "fread() failed: Expected %ld bytes got %zu bytes\n", size, bytes);
        return false;
    }

    *chip8_file_size = size;
    fclose(fp); // close file pointer
    return true;
}

int main(void)
{
    srand(time(NULL));
    chip8_load_fontset();
    printf("%u\n", chip8_gen_random_byte());
    uint32_t size = 0;
    if (!chip8_read_file_into_memory("RPS.ch8", &size)) return 1;

    chip8_pc = 0x200; // start of the rom
    bool quit = false;
    while (!quit) {
        printf("PC at 0X%X\n", chip8_pc);
        if (!chip8_execute_opcode()) quit = true;
    }
    return 0;
}

int main2(void)
{
    // chip8_load_fontset();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[ERROR] Failed to Initialize SDL: %s\n", SDL_GetError());
        return 1; // Exit if Error
    }

    SDL_Window *window = SDL_CreateWindow("Chip8",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          CHIP8_WINDOW_HEIGHT, CHIP8_WINDOW_WIDTH,
                                          SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        fprintf(stderr, "[ERROR] Failed to Create Window: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL) {
        fprintf(stderr, "[ERROR] Failed to Create Renderer: %s\n", SDL_GetError());
        return 1;
    }

    int ret;
    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: quit = true; break;
            }
        }
        // Update

        ret = SDL_SetRenderDrawColor(renderer, 24, 24, 24, 255); // Set Background Color
        if (ret != 0) {
            fprintf(stderr, "[ERROR] SDL_SetRenderDrawColor Failed: %s\n", SDL_GetError());
            return 1;
        }

        ret = SDL_RenderClear(renderer); // Clear Background with Set Color
        if (ret != 0) {
            fprintf(stderr, "[ERROR] SDL_SetRenderClear Failed: %s\n", SDL_GetError());
            return 1;
        }

        SDL_RenderPresent(renderer); // Present Background with Changes
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
