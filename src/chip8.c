#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <SDL2/SDL.h>

typedef struct Chip8_Stack {
    uint8_t *slots;
    uint8_t count;
} Chip8_Stack;

#define CHIP8_VREG_COUNT 16     /* V registers count */
#define CHIP8_STACK_CAP  64     /* Stack capacity */
#define CHIP8_DH         64     /* Display Height */
#define CHIP8_DW         32     /* Display Width */
#define CHIP8_RAM_CAP    1024*4 /* 4096 Addressable Memory */

#define CHIP8_WINDOW_HEIGHT 800 /* SDL Window Height */
#define CHIP8_WINDOW_WIDTH  600 /* SDL Window Width */

uint8_t      chip8_vregs[CHIP8_VREG_COUNT]; // Registers V0 - V15
Chip8_Stack  chip8_stack = {0};             // 64-Byte Stack
uint8_t      chip8_sp;                      // Stack Pointer
uint16_t     chip8_ir;                      // Index register
uint16_t     chip8_pc;                      // Program Counter

uint8_t chip8_d_timer; // Delay Timer
uint8_t chip8_s_timer; // Sound Timer

static uint8_t chip8_frame_buffer[CHIP8_DH][CHIP8_DW] = {0}; // Frame Buffer
static uint8_t chip8_memory[CHIP8_RAM_CAP] = {0};            // Chip8 RAM

uint8_t chip8_read_memory(uint8_t loc)
{
    return chip8_memory[loc];
}

void chip8_write_memory(uint8_t loc, uint8_t data)
{
    chip8_memory[loc] = data;
}

bool chip8_stack_push(uint8_t value)
{
    if (chip8_stack.count == CHIP8_STACK_CAP) {
        fprintf(stderr, "[ERROR] Stack Full\n");
        return false;
    }
    chip8_sp = chip8_stack.count;
    chip8_stack.slots[chip8_stack.count++] = value;
    return true;
}

uint8_t chip8_stack_pop()
{
    if (chip8_stack.count == 0) {
        fprintf(stderr, "[ERROR] Stack Empty\n");
        exit(1);
    }
    chip8_sp = chip8_stack.count--;
    return chip8_stack.slots[chip8_sp];
}

void chip8_load_fontset()
{
    assert(0 && "chip8_load_fontset unimplemented");
}

void chip8_clear_display()
{
    assert(0 && "chip8_clear_display unimplemented");
}


// OPCODES
#define CHIP8_00E0 0X00E0 // CLS
#define CHIP8_00EE 0X00EE // RET
#define CHIP8_1NNN 0X1    // JUMP addr
#define CHIP8_2NNN 0X2    // CALL addr
#define CHIP8_3XKK 0X3    // SE Vx, byte
#define CHIP8_4XKK 0X4    // SNE Vx, byte

bool chip8_execute_opcode(uint8_t opcode)
{
    switch (opcode) {
    case CHIP8_00E0: {
        chip8_clear_display();
    } break;

    case CHIP8_00EE: {
        chip8_pc = chip8_stack_pop();
    } break;

    case CHIP8_1NNN: {
        chip8_pc = chip8_memory[chip8_pc];
    } break;

    case CHIP8_2NNN: {
        if (!chip8_stack_push(chip8_pc)) return false;
        // chip8_pc = *addr;
    } break;

    default: assert(0 && "Unreachable Opcodes");
    }
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

    rewind(fp);
    //////////////////////////////////////////////////////////////////////////////////////////////
    // uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * size);                             //
    // if (buffer == NULL) {                                                                    //
    //     fprintf(stderr, "[ERROR] Memory Allocation of %ld bytes of uint8_t Failed\n", size); //
    //     return false;                                                                        //
    // }                                                                                        //
    //////////////////////////////////////////////////////////////////////////////////////////////

    size_t bytes = fread(&chip8_memory[0x200], sizeof(uint8_t), size, fp);
    if (bytes != (size_t)size) {
        fprintf(stderr, "fread() failed: %zu\n", bytes);
        return false;
    }

    *chip8_file_size = size;
    fclose(fp);
    return true;
}

int main(void)
{
    uint32_t size = 0;
    if (!chip8_read_file_into_memory("octojam1title.ch8", &size)) return 1;
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

// TODO: Read the chip8 file directly into memory
