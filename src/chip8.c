#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <SDL2/SDL.h>

#define CHIP8_VREG_COUNT    16       /* V registers count */
#define CHIP8_STACK_CAP     64       /* Stack capacity */
#define CHIP8_DW            64       /* Display Width */
#define CHIP8_DH            32       /* Display Height */
#define CHIP8_RAM_CAP       (1024*4) /* 4096 Addressable Memory */
#define CHIP8_PROGRAM_ENTRY 0x200    /* Program Entry Point */

#define CHIP8_WINDOW_WIDTH  640*2    /* SDL Window Width */
#define CHIP8_WINDOW_HEIGHT 320*2    /* SDL Window Height */

#define CHIP8_PIXEL_WIDTH  (CHIP8_WINDOW_WIDTH/CHIP8_DW)  /* Pixel Width */
#define CHIP8_PIXEL_HEIGHT (CHIP8_WINDOW_HEIGHT/CHIP8_DH) /* Pixel Height */

#define CHIP8_FONT_HEIGHT 5 /* FONT HEIGHT - 5 bytes*/

#define CHIP8_CPU_HZ   ((double)700.0) /* CPU Speed */
#define CHIP8_TIMER_HZ ((double)60.0)  /* CPU TIMER */

#define CHIP8_DEBUG_RENDER 0
#define CHIP8_DEBUG_OPCODE 1

#define CHIP8_SOUND_FREQUENCY 440
#define CHIP8_SOUND_SAMPLES   44100
#define CHIP8_SOUND_DURATION  1
#define CHIP8_SOUND_AMPLITUDE 1

#define CHIP8_SDL_ERROR(error, ret)                                 \
    do {                                                            \
        fprintf(stderr, "[ERROR] %s: %s\n", error, SDL_GetError()); \
        return ret;                                                 \
    }                                                               \
    while (0)

typedef struct Chip8_Stack {
    uint8_t *slots;
    uint8_t count;
    uint8_t capacity;
} Chip8_Stack;

typedef struct Chip8_Wave {
    double *samples;
    size_t count;
    size_t capacity;
} Chip8_Wave;

typedef struct Chip8_Sound {
    double      sample_rate;
    double      frequency;
    double      duration;
    double      amplitude;
    Chip8_Wave  wave;
    bool        playing;
    SDL_AudioDeviceID dev;
} Chip8_Sound;

typedef enum Chip8_Keys {
    CHIP8_ZERO = 0x0,
    CHIP8_ONE,
    CHIP8_TWO,
    CHIP8_THREE,
    CHIP8_FOUR,
    CHIP8_FIVE,
    CHIP8_SIX,
    CHIP8_SEVEN,
    CHIP8_EIGHT,
    CHIP8_NINE,
    CHIP8_A = 0xA,
    CHIP8_B = 0xB,
    CHIP8_C = 0xC,
    CHIP8_D = 0xD,
    CHIP8_E = 0XE,
    CHIP8_F = 0XF,

    // Font Count
    CHIP8_FONT_COUNT
} Chip8_Keys;

typedef struct Chip8_CPU {
    uint8_t  chip8_vregs[CHIP8_VREG_COUNT];          // Registers V0 - V15
    uint16_t chip8_ir;                               // Index register
    uint16_t chip8_pc;                               // Program Counter

    uint8_t  chip8_d_timer;                          // Delay Timer
    uint8_t  chip8_s_timer;                          // Sound Timer

    uint8_t  chip8_memory[CHIP8_RAM_CAP];            // Chip8 RAM
    uint8_t  chip8_frame_buffer[CHIP8_DW][CHIP8_DH]; // Frame Buffer
    bool     chip8_key_state[CHIP8_FONT_COUNT];      // ALL false

    Chip8_Stack  chip8_stack;                        // 16-Byte Stack
    Chip8_Sound  sound;                              // Beep Sound
} Chip8_CPU;

typedef struct Chip8_Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Chip8_Color;

#define BLACK (Chip8_Color){0,   0,     0, 255}
#define WHITE (Chip8_Color){255, 255, 255, 255}
#define RED   (Chip8_Color){255, 0,     0, 255}
#define GREEN (Chip8_Color){0,   255,   0, 255}
#define BLUE  (Chip8_Color){0,   0,   255, 255}

typedef struct Chip8_Font {
    uint8_t font[CHIP8_FONT_HEIGHT];
} Chip8_Font;

const Chip8_Font chip8_fontset[CHIP8_FONT_COUNT] = {
    [CHIP8_ZERO].font  = {0XF0, 0X90, 0X90, 0X90, 0XF0},
    [CHIP8_ONE].font   = {0X20, 0X60, 0X20, 0X20, 0X70},
    [CHIP8_TWO].font   = {0XF0, 0X10, 0XF0, 0X80, 0XF0},
    [CHIP8_THREE].font = {0XF0, 0X10, 0XF0, 0X10, 0XF0},
    [CHIP8_FOUR].font  = {0X90, 0X90, 0XF0, 0X10, 0X10},
    [CHIP8_FIVE].font  = {0XF0, 0X90, 0XF0, 0X10, 0XF0},
    [CHIP8_SIX].font   = {0XF0, 0X80, 0XF0, 0X90, 0XF0},
    [CHIP8_SEVEN].font = {0XF0, 0X10, 0X20, 0X20, 0X40},
    [CHIP8_EIGHT].font = {0XF0, 0X90, 0XF0, 0X90, 0XF0},
    [CHIP8_NINE].font  = {0XF0, 0X90, 0XF0, 0X10, 0XF0},
    [CHIP8_A].font     = {0XF0, 0X90, 0XF0, 0X90, 0X90},
    [CHIP8_B].font     = {0XE0, 0X90, 0XE0, 0X90, 0XE0},
    [CHIP8_C].font     = {0XF0, 0X80, 0X80, 0X80, 0XF0},
    [CHIP8_D].font     = {0XE0, 0X90, 0X90, 0X90, 0XE0},
    [CHIP8_E].font     = {0XF0, 0X80, 0XF0, 0X80, 0XF0},
    [CHIP8_F].font     = {0XF0, 0X80, 0XF0, 0X80, 0X80},
};

bool chip8_add_sample(Chip8_Wave *wave, double sample)
{
    if (wave->count >= wave->capacity) {
        fprintf(stderr, "[ERROR] Wave Buffer Capped: cannot add more samples\n");
        return false;
    }

    wave->samples[wave->count++] = sample;
    return true;
}

void chip8_generate_sound_wave(Chip8_CPU *cpu)
{
    int num_samples = cpu->sound.sample_rate * cpu->sound.duration;
    double period   = cpu->sound.sample_rate / cpu->sound.frequency;
    for (int i = 0; i < num_samples; ++i) {
        double y = (fmod(i, period) < period / 2) ? cpu->sound.amplitude: -cpu->sound.amplitude;
        chip8_add_sample(&cpu->sound.wave, y);
    }
}

// NOTE: Define an Audio callback Function to populate the buffer
void chip8_audio_callback(void *UserData, uint8_t *stream, int len) {
    Chip8_Sound *sound = (Chip8_Sound*)UserData; // Get Sound Object
    Chip8_Wave  *waves = &sound->wave;           // Get  Wave Values
    int sample_to_fill = len / sizeof(int16_t);

    static int sample_index = 0;
    int16_t *buffer = (int16_t*)stream;

    for (int i = 0; i < sample_to_fill; ++i) {
        if (sound->playing && (size_t)sample_index < waves->count) {
            buffer[i] = (int16_t)(waves->samples[sample_index] * 32767);
            sample_index++;
        } else {
            buffer[i] = 0;
        }
        sample_index %= waves->count;
    }
}

bool chip8_open_audio_device(Chip8_CPU *cpu)
{
    SDL_AudioSpec want, have;

    memset(&want, 0, sizeof(want));
    want.freq = cpu->sound.sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 4096;
    want.callback = chip8_audio_callback;
    want.userdata = &cpu->sound;

    cpu->sound.playing = false;

    cpu->sound.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (cpu->sound.dev == 0) {
        CHIP8_SDL_ERROR("Failed to Open Audio Device", false);
    }

    SDL_PauseAudioDevice(cpu->sound.dev, 0);
    return true;
}

uint8_t chip8_read_memory(const Chip8_CPU *cpu, const uint16_t loc)
{
    // casting to int16_t because gcc will just wrap the negative values
    if ((int16_t)loc >= 0 && loc < CHIP8_RAM_CAP) {
        return cpu->chip8_memory[loc];
    } else {
        fprintf(stderr, "[PANIC] Index(%u) Out of Bounds for (%d)sized array\n", loc, CHIP8_RAM_CAP);
        exit(EXIT_FAILURE);
    }
}

bool chip8_write_memory(Chip8_CPU *cpu, const uint16_t loc, uint8_t data)
{
    // casting to int16_t because gcc will just wrap the negative values
    if ((int16_t)loc >= 0 && loc < CHIP8_RAM_CAP) {
        cpu->chip8_memory[loc] = data;
        return true;
    } else {
        fprintf(stderr, "[PANIC] Index(%u) Out of Bounds for (%d)sized array\n", loc, CHIP8_RAM_CAP);
        return false;
    }
}

bool chip8_load_fontset(Chip8_CPU *cpu)
{
    for (uint8_t i = 0; i < CHIP8_FONT_COUNT; ++i) {
        for (uint8_t j = 0; j < CHIP8_FONT_HEIGHT; ++j) {
            uint16_t index = i * CHIP8_FONT_HEIGHT + j;
            if (!chip8_write_memory(cpu, index, chip8_fontset[i].font[j])) return false;
        }
    }

#if CHIP8_DEBUG_RENDER
    fprintf(stdout, "[INFO] Successfully Loaded the Fontset into Memory\n");
#endif
    return true;
}

// SDL Representation of 0 - F Keys
const SDL_Keycode chip8_keys[CHIP8_FONT_COUNT] = {
    [CHIP8_ZERO]  = SDLK_x,
    [CHIP8_ONE]   = SDLK_1,
    [CHIP8_TWO]   = SDLK_2,
    [CHIP8_THREE] = SDLK_3,
    [CHIP8_FOUR]  = SDLK_q,
    [CHIP8_FIVE]  = SDLK_w,
    [CHIP8_SIX]   = SDLK_e,
    [CHIP8_SEVEN] = SDLK_a,
    [CHIP8_EIGHT] = SDLK_s,
    [CHIP8_NINE]  = SDLK_d,
    [CHIP8_A]     = SDLK_z,
    [CHIP8_B]     = SDLK_c,
    [CHIP8_C]     = SDLK_4,
    [CHIP8_D]     = SDLK_r,
    [CHIP8_E]     = SDLK_f,
    [CHIP8_F]     = SDLK_v,
};

uint8_t chip8_get_frame_buffer(Chip8_CPU *cpu, uint16_t x, uint16_t y)
{
    if (((int16_t)x >= 0 && x < CHIP8_DW) &&
        ((int16_t)y >= 0 && y < CHIP8_DH))
    {
        return cpu->chip8_frame_buffer[x][y];
    } else {
        fprintf(stderr, "[PANIC] Index(%u, %u) Out of Bounds For (%d, %d) sized multi-array\n", x, y, CHIP8_DW, CHIP8_DH);
        exit(EXIT_FAILURE);
    }
}

bool chip8_set_frame_buffer(Chip8_CPU *cpu, uint16_t x, uint16_t y, uint8_t data)
{
    if (((int16_t)x >= 0 && x < CHIP8_DW) &&
        ((int16_t)y >= 0 && y < CHIP8_DH))
    {
        cpu->chip8_frame_buffer[x][y] = data;
        return true;
    } else {
        fprintf(stderr, "[PANIC] Index(%u, %u) Out of Bounds For (%d, %d) sized multi-array\n", x, y, CHIP8_DW, CHIP8_DH);
        return false;
    }
}

void chip8_clear_display(Chip8_CPU *cpu)
{
    memset(cpu->chip8_frame_buffer, 0, sizeof(cpu->chip8_frame_buffer));
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

bool chip8_stack_push(Chip8_CPU *cpu, uint16_t value)
{
    if (cpu->chip8_stack.count == cpu->chip8_stack.capacity) {
        fprintf(stderr, "[ERROR] Stack Full\n");
        return false;
    }

    uint8_t high = 0; // high
    uint8_t low  = 0; // low
    chip8_split_uint16_t(value, &high, &low); // Split the u16 to u8s
    cpu->chip8_stack.slots[cpu->chip8_stack.count++] = high; // push high
    cpu->chip8_stack.slots[cpu->chip8_stack.count++] = low; // push low

    return true;
}

uint16_t chip8_stack_pop(Chip8_CPU *cpu)
{
    if (cpu->chip8_stack.count < 2) {
        fprintf(stderr, "[ERROR] Stack Empty\n");
        exit(1);
    }

    uint8_t low  = cpu->chip8_stack.slots[--cpu->chip8_stack.count]; // Pop low
    uint8_t high = cpu->chip8_stack.slots[--cpu->chip8_stack.count]; // Pop high

    return chip8_bytes_to_uint16_t(high, low);
}

static inline uint8_t chip8_gen_random_byte()
{
    return (uint8_t)(rand() % (UINT8_MAX + 1));
}

void chip8_handle_input(Chip8_CPU *cpu, SDL_Event *event)
{
    switch (event->type) {
    case SDL_KEYDOWN: {
        for (uint8_t i = 0; i < CHIP8_FONT_COUNT; ++i) {
            if (event->key.keysym.sym == chip8_keys[i]) {
                cpu->chip8_key_state[i] = true;
                break;
            }
        }
    } break;

    case SDL_KEYUP: {
        for (uint8_t i = 0; i < CHIP8_FONT_COUNT; ++i) {
            if (event->key.keysym.sym == chip8_keys[i]) {
                cpu->chip8_key_state[i] = false;
                break;
            }
        }
    } break;

    default: {
        fprintf(stderr, "[PANIC] Unreachable SDL_Type");
        exit(EXIT_FAILURE);
    }
    }
}

bool chip8_execute_opcode(Chip8_CPU *cpu, uint16_t start, uint16_t size)
{
    if (cpu->chip8_pc >= start+size) {
        printf("Finished\n");
        return false;
    }

#if CHIP8_TRACE
    printf("PC at 0X%X\n", cpu->chip8_pc);
#endif

    uint8_t  high   = chip8_read_memory(cpu, cpu->chip8_pc);
    uint8_t  low    = chip8_read_memory(cpu, cpu->chip8_pc+1);
    uint16_t opcode = chip8_bytes_to_uint16_t(high, low);

    cpu->chip8_pc += 2;
    switch (((opcode >> 12) & 0XF)) { // switch on first nibble
    case 0X0: {
        switch ((opcode & 0XFF)) { // switch on last byte
        case 0XE0: { // 0X00E0
            chip8_clear_display(cpu);
#if CHIP8_DEBUG_OPCODE
            printf("00EE, Clear display: 0X%X\n", opcode);
#endif
            return true;
        }

        case 0XEE: {  // 0X00EE
            // Pop PC from stack, return from subroutine
            cpu->chip8_pc = chip8_stack_pop(cpu);
#if CHIP8_DEBUG_OPCODE
            printf("00E0, Return: 0X%X\n", opcode);
#endif
            return true;
        }

        default:
#if CHIP8_DEBUG_OPCODE
            fprintf(stderr, "[ERROR] Unknown Last Byte `0X%X` For Opcode 0X%X\n", (opcode & 0XFF), opcode);
#endif
            return false;
        }

        fprintf(stderr, "[PANIC] Unreachable\n");
        return false;
    }

    case 0X1: {
#if CHIP8_DEBUG_OPCODE
        printf("1NNN, JMP to opcode: 0X%X\n", opcode);
#endif
        cpu->chip8_pc = opcode & 0X0FFF; // JMP instruction 1nnn, set pc to nnn
        return true;
    }

    case 0X2: {
#if CHIP8_DEBUG_OPCODE
        printf("2NNN, CALL: 0X%X\n", opcode);
#endif
        if (!chip8_stack_push(cpu, cpu->chip8_pc)) return false; // Save  PC
        cpu->chip8_pc = opcode & 0X0FFF; // Call subroutine 2nnn, set pc to nnn
        return true;
    }

    case 0x3: {
        // SE Vx, byte
        // Skip Next instruction if Vx == byte
#if CHIP8_DEBUG_OPCODE
        printf("3XKK, SE: Vx Byte: 0X%X\n", opcode);
#endif
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte = opcode & 0XFF;

        if (cpu->chip8_vregs[v_index] == low_byte) {
            cpu->chip8_pc += 2; // skip two => 2 u8s = u16, next pc
        } else {
            ;
        }
        return true;
    }

    case 0X4: {
        // SNE Vx, byte
        // Skip Next instruction if Vx != byte
#if CHIP8_DEBUG_OPCODE
        printf("4XKK, SNE: Vx Byte: 0X%X\n", opcode);
#endif
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte = opcode & 0XFF;

        if (cpu->chip8_vregs[v_index] != low_byte) {
            cpu->chip8_pc += 2; // skip two => 2 u8s = u16, next pc
        } else {
            ;
        }
        return true;
    }

    case 0X6: {
        // 0X6XKK
        // put kk into V[X]
#if CHIP8_DEBUG_OPCODE
        printf("0X6KK, LD Vx, byte: 0X%X\n", opcode);
#endif
        uint8_t v_index = ((opcode >> 8) & 0XF);
        uint8_t low_byte = opcode & 0XFF;

        cpu->chip8_vregs[v_index] = low_byte;
        return true;
    }

    case 0X7: {
        // 0X79F8
#if CHIP8_DEBUG_OPCODE
        printf("7XKK, ADD Vx, byte\n");
#endif
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte = opcode & 0XFF;

        cpu->chip8_vregs[v_index] += low_byte; // Add Vx + byte, store it back to Vx
        return true;
    }

    case 0X8: {
        // 0X8ED7
        uint8_t vidx_x   = ((opcode >> 8) & 0XF); // 2nd nibble , x index into v
        uint8_t vidx_y   = ((opcode >> 4) & 0XF); // 3rd nibble , y index into v
        uint8_t l_nibble = (opcode & 0XF);

        switch (l_nibble) {
        case 0x0: {
#if CHIP8_DEBUG_OPCODE
            printf("8xy0, LD Vx, Vy: 0X%X\n", opcode);
#endif
            cpu->chip8_vregs[vidx_x] = cpu->chip8_vregs[vidx_y];
            return true;
        }

        case 0x1: {
#if CHIP8_DEBUG_OPCODE
            printf("8xy1, OR Vx, Vy: 0X%X\n", opcode);
#endif
            cpu->chip8_vregs[vidx_x] = cpu->chip8_vregs[vidx_x] | cpu->chip8_vregs[vidx_y];
            return true;
        }

        case 0x2: {
#if CHIP8_DEBUG_OPCODE
            printf("8xy2, AND Vx, Vy: 0X%X\n", opcode);
#endif
            cpu->chip8_vregs[vidx_x] = cpu->chip8_vregs[vidx_x] & cpu->chip8_vregs[vidx_y];
            return true;
        }

        case 0x3: {
#if CHIP8_DEBUG_OPCODE
            printf("8xy3, XOR Vx, Vy: 0X%X\n", opcode);
#endif
            cpu->chip8_vregs[vidx_x] = cpu->chip8_vregs[vidx_x] ^ cpu->chip8_vregs[vidx_y];
            return true;
        }

        case 0x4: {
#if CHIP8_DEBUG_OPCODE
            printf("8xy4, ADD Vx, Vy: 0X%X\n", opcode);
#endif
            uint16_t value = (uint16_t) cpu->chip8_vregs[vidx_x] + (uint16_t) cpu->chip8_vregs[vidx_y];
            uint8_t high;
            uint8_t low;
            chip8_split_uint16_t(value, &high, &low);
            if (value > UINT8_MAX) {
                cpu->chip8_vregs[0XF] = 1;
            } else {
                cpu->chip8_vregs[0XF] = 0;
            }
            cpu->chip8_vregs[vidx_x] = low;
            return true;
        }

        case 0x5: {
#if CHIP8_DEBUG_OPCODE
            printf("8xy5, SUB Vx, Vy: 0X%X\n", opcode);
#endif
            if (cpu->chip8_vregs[vidx_x] > cpu->chip8_vregs[vidx_y]) {
                cpu->chip8_vregs[0XF] = 1;
            } else {
                cpu->chip8_vregs[0XF] = 0;
            }
            cpu->chip8_vregs[vidx_x] -= cpu->chip8_vregs[vidx_y];
            return true;
        }

        case 0x6: {
#if CHIP8_DEBUG_OPCODE
            printf("8xy6, SHR Vx {, Vy}: 0X%X\n", opcode);
#endif
            cpu->chip8_vregs[0XF] = cpu->chip8_vregs[vidx_x] & 0x01; // set
            cpu->chip8_vregs[vidx_x] >>= 1;
            return true;
        }

        case 0x7: {
#if CHIP8_DEBUG_OPCODE
            printf("8XY7, SUBN Vx, Vy: 0X%X\n", opcode);
#endif
            if (cpu->chip8_vregs[vidx_y] > cpu->chip8_vregs[vidx_x]) {
                cpu->chip8_vregs[0XF] = 1; // Set Borrow if greater
            } else {
                cpu->chip8_vregs[0XF] = 0; // else not set
            }

            cpu->chip8_vregs[vidx_x] = cpu->chip8_vregs[vidx_y] - cpu->chip8_vregs[vidx_x];
            return true;
        }

        case 0xE: {
#if CHIP8_DEBUG_OPCODE
            printf("8xyE, SHL Vx {, Vy}: 0X%X\n", opcode);
#endif
            if (cpu->chip8_vregs[vidx_x] & 0x80) {
                cpu->chip8_vregs[0XF] = 1; // set
            } else {
                cpu->chip8_vregs[0XF] = 0; // else not set
            }
            cpu->chip8_vregs[vidx_x] <<= 1;
            return true;
        }

        default:
#if CHIP8_DEBUG_OPCODE
            fprintf(stderr, "[ERROR]: Unknown last nibble `0X%X` for Opcode: 0X%X\n", l_nibble, opcode);
#endif
            return false;
        }

        fprintf(stderr, "[PANIC] Unreachable\n");
        return false;
    }

    case 0x9: {
        // 9xy0 - SNE Vx, Vy
        uint8_t vidx_x   = ((opcode >> 8) & 0XF); // 2nd nibble , x index into v
        uint8_t vidx_y   = ((opcode >> 4) & 0XF); // 3rd nibble , y index into v
        if (cpu->chip8_vregs[vidx_x] != cpu->chip8_vregs[vidx_y]) {
            cpu->chip8_pc += 2; // Skip increment by two
        } else {
            ;
        }
        return true;
    }

    case 0XA: {
        // 0XANNN
#if CHIP8_DEBUG_OPCODE
        printf("ANNN, LD I, addr: 0X%X\n", opcode);
#endif
        cpu->chip8_ir = opcode & 0X0FFF; // load the nnn to ir
        return true;
    }

    case 0XC: {
        // RND Vx, byte
        // extract lower byte from opcode, (&) it with random byte
        // store result in Vx
#if CHIP8_DEBUG_OPCODE
        printf("CXKK, RND Vx Byte: 0X%X\n", opcode);
#endif
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte =  (opcode & 0XFF);
        uint8_t random   = chip8_gen_random_byte();

        cpu->chip8_vregs[v_index] = random & low_byte;
        return true;
    }

    case 0XD: {
        // DXYN
        // draw Vx, Vy, nibble, frame[Vx][Vy] = nibble
#if CHIP8_DEBUG_OPCODE
        printf("DXYN, DRW Vx, Vy, Nibble: 0X%X\n", opcode);
#endif
        uint8_t vidx_x  = ((opcode >> 8) & 0XF);
        uint8_t vidx_y  = ((opcode >> 4) & 0XF);
        uint8_t n_bytes = (opcode & 0XF);
        uint8_t x       = cpu->chip8_vregs[vidx_x];
        uint8_t y       = cpu->chip8_vregs[vidx_y];

        cpu->chip8_vregs[0XF] = 0; // Reset V[0XF]
        for (uint8_t i = 0; i < n_bytes; ++i) {
            uint8_t sprite_byte = chip8_read_memory(cpu, cpu->chip8_ir +i);
            for (uint8_t j = 0; j < 8; ++j) {
                if ((sprite_byte & (0x80 >> j))) {
                    uint8_t pixel_x = (x + j) % CHIP8_DW;
                    uint8_t pixel_y = (y + i) % CHIP8_DH;

                    uint8_t current = chip8_get_frame_buffer(cpu, pixel_x, pixel_y);

                    if (current) {
                        cpu->chip8_vregs[0XF] = 1; // Collision
                    }

                    // Xor the current pixel on screen
                    if (!chip8_set_frame_buffer(cpu, pixel_x, pixel_y, current ^ 1)) return false;
                }
            }
        }
        return true;
    }

    case 0XE: {
#if CHIP8_DEBUG_OPCODE
        printf("Ex9E - SKP Vx\n");
#endif
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t key = cpu->chip8_vregs[v_index];
        if (cpu->chip8_key_state[key]) {
            cpu->chip8_pc += 2;
        } else {
            ;
        }

        return true;
    }

    case 0XF: {
        uint8_t v_index  = ((opcode >> 8) & 0XF);
        uint8_t low_byte = (opcode & 0XFF);

        switch (low_byte) {
        case 0X1E: {
#if CHIP8_DEBUG_OPCODE
            printf("FX1E, ADD I, Vx: 0X%X\n", opcode);
#endif
            cpu->chip8_ir += cpu->chip8_vregs[v_index]; // Add V[x] to ir
            return true;
        }

        case 0X0A: {
#if CHIP8_DEBUG_OPCODE
            printf("Fx0A, LD Vx, K: 0X%X\n", opcode);
#endif
            // put keycode in V[x]
            bool pressed = false;
            for (uint8_t i = 0; i < CHIP8_FONT_COUNT; ++i) {
                if (cpu->chip8_key_state[i]) {
                    cpu->chip8_vregs[v_index] = i;
                    pressed = true;
                    break;
                }
            }

            if (!pressed) {
#if CHIP8_DEBUG_OPCODE
                printf("Waiting For Key Press\n");
#endif
                cpu->chip8_pc -=2; // Wait for key press
            }
            return true;
        }

        case 0x07: {
#if CHIP8_DEBUG_OPCODE
            printf("Fx07 - LD Vx, DT\n");
#endif
            cpu->chip8_vregs[v_index] = cpu->chip8_d_timer;
            return true;
        }

        case 0x15: {
#if CHIP8_DEBUG_OPCODE
            printf("Fx15 - LD DT, Vx\n");
#endif
            cpu->chip8_d_timer = cpu->chip8_vregs[v_index];
            return true;
        }
        case 0x18: {
#if CHIP8_DEBUG_OPCODE
            printf("Fx18 - LD ST, Vx\n");
#endif
            cpu->chip8_s_timer = cpu->chip8_vregs[v_index];
            return true;
        }

        case 0X29: {
#if CHIP8_DEBUG_OPCODE
            printf("Fx29, LD F, Vx: 0X%X\n", opcode);
#endif
            cpu->chip8_ir = cpu->chip8_vregs[v_index];
            return true;
        }

        case 0X33: {
            // Store BCD representation in I, I+1, I+2
#if CHIP8_DEBUG_OPCODE
            printf("Fx33 - LD B, Vx\n");
#endif
            uint8_t value = cpu->chip8_vregs[v_index];
            uint8_t hunds = (value / 100);     // Hundreds
            uint8_t tens  = (value / 10) % 10; // Tens
            uint8_t ones  = (value % 10);      // Ones

            if (!chip8_write_memory(cpu, cpu->chip8_ir, hunds))    return false;
            if (!chip8_write_memory(cpu, cpu->chip8_ir + 1, tens)) return false;
            if (!chip8_write_memory(cpu, cpu->chip8_ir + 2, ones)) return false;
            return true;
        }

        case 0x55: {
#if CHIP8_DEBUG_OPCODE
            printf("Fx55 - LD [I], Vx\n");
#endif
            for (uint8_t i = 0; i <= v_index; ++i) {
                if (!chip8_write_memory(cpu, cpu->chip8_ir + (uint16_t)i, cpu->chip8_vregs[i])) return false;
            }
            cpu->chip8_ir = cpu->chip8_ir + v_index + 1;
            return true;
        }

        case 0x65: {
#if CHIP8_DEBUG_OPCODE
            printf("Fx65 - LD Vx, [I]\n");
#endif
            for (uint8_t i = 0; i <= v_index; ++i) {
                cpu->chip8_vregs[i] = chip8_read_memory(cpu, cpu->chip8_ir + (uint16_t)i);
            }
            cpu->chip8_ir = cpu->chip8_ir + v_index + 1;
            return true;
        }

        default:
#if CHIP8_DEBUG_OPCODE
            fprintf(stderr, "[ERROR]: Unknown low_byte `0X%X` for Opcode: 0X%X\n", low_byte, opcode);
#endif
            return false;
        }
        // UNREACHABLE
        fprintf(stderr, "[PANIC] Unreachable\n");
        return false;
    }

    default:
#if CHIP8_DEBUG_OPCODE
        fprintf(stderr, "[ERROR]: Unknown opcode: 0X%X\n", opcode);
#endif
        return false;
    }
    // UNREACHABLE
    fprintf(stderr, "[PANIC] Unreachable\n");
    return false;
}

bool chip8_read_file_into_memory(Chip8_CPU *cpu, const char *chip8_file_path, size_t *chip8_file_size)
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

    size_t size = ftell(fp);
    if (size <= 0) {
        fprintf(stderr, "[ERROR] File `%s` Empty\n", chip8_file_path);
        return false;
    }

    size_t max_size = CHIP8_RAM_CAP - CHIP8_PROGRAM_ENTRY;
    if (size > max_size) {
        fprintf(stderr, "[ERROR] Cannot Fit %zu bytes: MEMORY CAPACITY: %zu\n", size, max_size);
        return false;
    }

    rewind(fp); // Rewind the file pointer to the beginning

    // Read the chip8 rom directly into the chip8 memory
    size_t bytes = fread(&cpu->chip8_memory[CHIP8_PROGRAM_ENTRY], sizeof(uint8_t), size, fp);
    if (bytes != (size_t)size) {
        fprintf(stderr, "fread() failed: Expected %ld bytes got %zu bytes\n", size, bytes);
        return false;
    }

    *chip8_file_size = size;
    fclose(fp); // close file pointer
    return true;
}

bool chip8_draw_pixel(SDL_Renderer *renderer, int x, int y, int w, int h, const Chip8_Color color)
{
    const SDL_Rect pixel = {x , y , w , h};

    int ret;
    ret = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // Set blend mode
    if (ret != 0) {
        CHIP8_SDL_ERROR("SDL_SetRenderDrawBlendMode", false);
    }

    ret = SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a); // Set Pixel Color
    if (ret != 0) {
        CHIP8_SDL_ERROR("SDL_SetRenderDrawColor", false);
    }

    ret = SDL_RenderFillRect(renderer, &pixel);
    if (ret != 0) {
        CHIP8_SDL_ERROR("SDL_RenderFillRect", false);
    }

#if CHIP8_DEBUG_RENDER
    fprintf(stdout, "[INFO] Pixel Size(%d, %d) Rendered at Position(%d, %d)\n", w , h, x, y);
#endif
    return true;
}

bool chip8_clear_background(SDL_Renderer *renderer, const Chip8_Color color)
{
    int ret;
    ret = SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a); // Set Background Color
    if (ret != 0) {
        CHIP8_SDL_ERROR("SDL_SetRenderDrawColor", false);
    }

    ret = SDL_RenderClear(renderer); // Clear Background with Set Color
    if (ret != 0) {
        CHIP8_SDL_ERROR("SDL_RenderClear", false);
    }
    return true;
}

bool chip8_render_pixels(Chip8_CPU *cpu, SDL_Renderer *renderer, const Chip8_Color color)
{
    for (int j = 0; j < CHIP8_DH; ++j) {
        for (int i = 0; i < CHIP8_DW; ++i) {
            if (chip8_get_frame_buffer(cpu, i, j)) {
                int x = i*CHIP8_PIXEL_WIDTH;
                int y = j*CHIP8_PIXEL_HEIGHT;
                if (chip8_draw_pixel(renderer, x, y, CHIP8_PIXEL_WIDTH, CHIP8_PIXEL_HEIGHT, color)) {
                    ;
                } else {
                    return false;
                }
            }
        }
    }
    return true;
}

const char *chip8_shift_args(int *argc, char ***argv)
{
    const char *result = **argv;
    (*argc)--;
    (*argv)++;
    return result;
}

bool chip8_initialize_states(Chip8_CPU *cpu, const char *chip8_rom_path, size_t *size)
{
    // Memset The Chip8 cpu structure
    memset(cpu, 0, sizeof(Chip8_CPU));
    cpu->chip8_pc      = CHIP8_PROGRAM_ENTRY;
    cpu->chip8_d_timer = CHIP8_TIMER_HZ;
    cpu->chip8_s_timer = CHIP8_TIMER_HZ;

    // Initialize Stack
    cpu->chip8_stack.capacity = CHIP8_STACK_CAP;
    cpu->chip8_stack.count    = 0;
    cpu->chip8_stack.slots    = malloc(sizeof(uint8_t)*cpu->chip8_stack.capacity);
    if (cpu->chip8_stack.slots == NULL) {
        fprintf(stderr, "[ERROR] Memory Allocation for Stack Slots Failed\n");
        return false;
    }

    // Initialize Sound Structure
    cpu->sound.sample_rate = CHIP8_SOUND_SAMPLES;
    cpu->sound.duration    = CHIP8_SOUND_DURATION;
    cpu->sound.amplitude   = CHIP8_SOUND_AMPLITUDE;
    cpu->sound.frequency   = CHIP8_SOUND_FREQUENCY;
    cpu->sound.playing     = false;
    cpu->sound.wave.count    = 0;
    cpu->sound.wave.capacity = cpu->sound.sample_rate*cpu->sound.duration;
    cpu->sound.wave.samples  = malloc(sizeof(double)*cpu->sound.wave.capacity);
    if (cpu->sound.wave.samples == NULL) {
        fprintf(stderr, "[ERROR] Memory Allocation for Samples Failed\n");
        return false;
    }

    // Clear Display
    chip8_clear_display(cpu);

    // Load Fontset into chip8 memory
    chip8_load_fontset(cpu);

    // Generate Sound Wave samples
    chip8_generate_sound_wave(cpu);

    // Load the chip8 Rom into chip8 ram
    if (!chip8_read_file_into_memory(cpu, chip8_rom_path, size)) return false;

    // Open Audio Device
    if (!chip8_open_audio_device(cpu)) return false;
    return true;
}

#define chip8_main main
int chip8_main(int argc, char **argv)
{
    srand(time(NULL));

    // Parse Command-Line Args
    const char *program_name = chip8_shift_args(&argc, &argv);
    if (argc <= 0) {
        fprintf(stderr, "[Usage] %s <input_path>\n", program_name);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        CHIP8_SDL_ERROR("Failed to Initialize SDL", 1);
    }

    const char *prefix     = "Chip8";
    const int prefix_len   = strlen(prefix);
    const char *rom_path   = chip8_shift_args(&argc, &argv);
    const int rom_path_len = strlen(rom_path);

    const int buffer_len = prefix_len + rom_path_len;
    char title[buffer_len + 1];

    snprintf(title, buffer_len, "%s - %s", prefix, rom_path);
    title[buffer_len] = '\0';

    SDL_Window *window = SDL_CreateWindow(title,
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          CHIP8_WINDOW_WIDTH, CHIP8_WINDOW_HEIGHT,
                                          SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        CHIP8_SDL_ERROR("Failed to Create Window", 1);
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL) {
        CHIP8_SDL_ERROR("Failed to Create Renderer", 1);
    }

    size_t size = 0;
    static Chip8_CPU cpu = {0};
    if(!chip8_initialize_states(&cpu, rom_path, &size)) return 1;

    double last_time = (double)SDL_GetTicks();
    double timer_accumulator = 0.0;
    double cpu_accumulator   = 0.0;

    const double cpu_step = 1000.0 / CHIP8_CPU_HZ;
    const double timer_step = 1000.0 / CHIP8_TIMER_HZ;

    bool quit = false;
    while (!quit) {
        double now = (double)SDL_GetTicks();
        double elapsed = now - last_time;
        last_time = now;

        timer_accumulator += elapsed;
        cpu_accumulator   += elapsed;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: quit = true; break;
            chip8_handle_input(&cpu, &event); break;
            }
        }
        // Update
        if (!chip8_clear_background(renderer, BLACK)) quit = true;

        // Update timers at 60Hz
        while (timer_accumulator >= timer_step) {
            timer_accumulator -= timer_step;

            if (cpu.chip8_d_timer > 0) cpu.chip8_d_timer--;
            if (cpu.chip8_s_timer > 0) {
                cpu.chip8_s_timer--;
                cpu.sound.playing = true;
            } else {
                cpu.sound.playing = false;
            }
        }

        // Update CPU at 700Hz
        while (cpu_accumulator >= cpu_step) {
            cpu_accumulator -= cpu_step;
            if (!chip8_execute_opcode(&cpu, CHIP8_PROGRAM_ENTRY, size)) quit = true;
        }

        if (!chip8_render_pixels(&cpu, renderer, GREEN))  quit = true;
        SDL_RenderPresent(renderer); // Present Frame with Changes
        SDL_Delay(1);
    }

    SDL_CloseAudioDevice(cpu.sound.dev);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
