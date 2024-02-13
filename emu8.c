#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#define FALSE 0
#define TRUE 1
#define PAUSED 2
#define FPS 240
#define WINDOW_WIDTH 768
#define WINDOW_HEIGHT 384
#define PIXEL_WIDTH (WINDOW_WIDTH/64)+1
#define PIXEL_HEIGHT (WINDOW_HEIGHT/32)+1
#define TARGET_TIME (2000/FPS)

int game_status;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
int last_time;

typedef struct pixel{
    float x;
    float y;
    float width;
    float height;
    int value;
} pixel_t;

pixel_t display[32][64];

typedef struct registers{
    uint8_t mem[4096];
    uint16_t stack[16];
    uint8_t SP;
    uint16_t PC;
    uint16_t I;
    uint16_t delay_timer;
    uint16_t sound_timer;
    uint8_t V[16];
    uint8_t keys[16];
}registers_t;

registers_t emu;

uint16_t fetch(){
    uint16_t instruction;
    instruction = (u_int16_t)((u_int16_t)emu.mem[emu.PC] << 8 | emu.mem[emu.PC+1]);
    emu.PC+=2;
    return instruction;
}

int decode_exec(uint16_t instruction){
    int i, j, byte, bit, x, y, n;
    switch(instruction & 0xF000){
        case 0x0000:
            switch(instruction & 0x00FF){
                case 0x00E0:
                    for(i = 0; i<32;i++)
                        for(j = 0; j<64; j++)
                            display[i][j].value = 0;
                    break;
                case 0x00EE:
                    emu.PC = emu.stack[emu.SP-1];
                    emu.SP--;
                    break;
                default: break;
            }
            break;
        case 0x1000:
            emu.PC = instruction & 0x0FFF;
            break;
        case 0x2000:
            emu.stack[emu.SP] = emu.PC;
            emu.SP++;
            emu.PC = (instruction & 0x0FFF);
            break;
        case 0x3000:
            x = (instruction & 0x0F00) >> 8;
            if(emu.V[x] == (instruction & 0x00FF)) emu.PC+=2;
            break;
        case 0x4000:
            x = (instruction & 0x0F00) >> 8;
            if(emu.V[x] != (instruction & 0x00FF)) emu.PC+=2;
            break;
        case 0x5000:
            x = (instruction & 0x0F00) >> 8;
            y = (instruction & 0x00F0) >> 4;
            if(emu.V[x] == emu.V[y]) emu.PC+=2;
            break;
        case 0x6000:
            emu.V[(instruction & 0x0F00) >> 8] = instruction & 0x00FF;
            break;
        case 0x7000:
            emu.V[(instruction & 0x0F00) >> 8] += instruction & 0x00FF;
            break;
        case 0x8000: 
            x = (instruction & 0x0F00) >> 8;
            y = (instruction & 0x00F0) >> 4;
            switch(instruction & 0x000F){
                case 0x0000:
                    emu.V[x] = emu.V[y];
                    break;
                case 0x0001:
                    emu.V[x] = emu.V[x] | emu.V[y];
                    break;
                case 0x0002:
                    emu.V[x] = emu.V[x] & emu.V[y];
                    break;
                case 0x0003:
                    emu.V[x] = emu.V[x] ^ emu.V[y];
                    break;
                case 0x0004:
                    n = ((uint16_t) (emu.V[x] + emu.V[y]) > UINT8_MAX);
                    emu.V[x] += emu.V[y];
                    emu.V[15] = n;
                    break;
                case 0x0005:
                    n = emu.V[x] >= emu.V[y];
                    emu.V[x] -= emu.V[y];
                    emu.V[15] = n;
                    break;
                case 0x0006:
                    n = emu.V[x] & 1;
                    emu.V[x] = emu.V[x] >> 1;
                    emu.V[15] = n;
                    break;
                case 0x0007:
                    n = emu.V[x] <= emu.V[y];
                    emu.V[x] = emu.V[y] - emu.V[x];
                    emu.V[15] = n;
                    break;
                case 0x000E:
                    n = (emu.V[x] & 0x80) >> 7;
                    emu.V[x] = emu.V[x] << 1;
                    emu.V[15] = n;
                    break;
            }
            break;
        case 0x9000:
            x = (instruction & 0x0F00) >> 8;
            y = (instruction & 0x00F0) >> 4;
            if(emu.V[x] != emu.V[y]) emu.PC += 2;
            break;
        case 0xA000:
            emu.I = (instruction & 0x0FFF);
            break;
        case 0xB000:
            emu.PC = (instruction & 0x0FFF) + emu.V[0];
            break;
        case 0xC000: 
            n = rand() % (UINT8_MAX + 1);
            emu.V[(instruction & 0x0F00) >> 8] = n & (instruction & 0x00FF);
            break;
        case 0xD000:
            x = emu.V[(instruction & 0x0F00) >> 8] & 63;
            y = emu.V[(instruction & 0x00F0) >> 4] & 31;
            n = instruction & 0x000F;
            emu.V[15] = 0;
            for(i = 0; i<n; i++){
                byte = emu.mem[emu.I + i];
                for(j = 0; j<8; j++){
                    bit = byte & (0x80 >> j);
                    if(bit != 0 && display[y+i][x+j].value == UINT8_MAX){
                        display[y+i][x+j].value = 0;
                        emu.V[15] = 1;
                    }else if(bit != 0){
                        display[y+i][x+j].value = UINT8_MAX;
                    }
                    if(x+j == 63) break;
                }
                if(y+i == 32) break;
            }
            break;
        case 0xE000:
            x = (instruction & 0x0F00) >> 8;
            switch(instruction & 0x00FF){
                case 0x00A1:
                    if(emu.keys[emu.V[x]] == FALSE) emu.PC += 2; 
                    break;
                case 0x009E:
                    if(emu.keys[emu.V[x]] == TRUE) emu.PC += 2;
                    break;
                default: break;
            }
            break;
        case 0xF000:
            x = (instruction & 0x0F00) >> 8;
            switch(instruction & 0x00FF){
                case 0x0007: 
                    emu.V[x] = emu.delay_timer; 
                    break;
                case 0x000A: 
                    n = FALSE;
                    i = 0;
                    while(i<16 && n == FALSE){
                        if(emu.keys[i] == TRUE){
                            emu.V[x] = i;
                            n = TRUE;
                        }
                        i++;
                    }
                    if(n == FALSE) emu.PC -= 2;
                    break;
                case 0x0015: 
                    emu.delay_timer = emu.V[x]; 
                    break;
                case 0x0018: 
                    emu.sound_timer = emu.V[x]; 
                    break;
                case 0x001E: 
                    n = emu.I + emu.V[x] > UINT8_MAX; 
                    emu.I += emu.V[x];
                    emu.V[15] = n; 
                    break;
                case 0x0029:
                    emu.I = emu.V[x] * 5;
                    break;
                case 0x0033:
                    emu.mem[emu.I] = emu.V[x] / 100;
                    emu.mem[emu.I+1] = (emu.V[x] / 10) % 10;
                    emu.mem[emu.I+2] = emu.V[x] % 10;
                    break; 
                case 0x0055:
                    x = (instruction & 0x0F00) >> 8; 
                    for(i = 0; i <= x; i++)
                        emu.mem[emu.I+i] = emu.V[i];
                    break;
                case 0x0065:
                    x = (instruction & 0x0F00) >> 8; 
                    for(i = 0; i <= x; i++)
                        emu.V[i] = emu.mem[emu.I+i];
                    break;
                default: 
                    break;
            }
            break;
        default: 
            break;
    }
    return 0;
}

int initialize_window(void){
    if(SDL_Init(SDL_INIT_VIDEO) != 0){
        fprintf(stderr, "Error initializing video\n");
        return FALSE;
    }
    if(SDL_Init(SDL_INIT_EVENTS) != 0){
        fprintf(stderr, "Error initializing events\n");
        return FALSE;
    }
    if(SDL_Init(SDL_INIT_TIMER) != 0){
        fprintf(stderr, "Error initializing timer\n");
        return FALSE;
    }
    window = SDL_CreateWindow(
        NULL,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_BORDERLESS
    );
    if(!window){
        fprintf(stderr, "Error creating window\n");
        return FALSE;
    }
    if((renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)) == NULL){
        fprintf(stderr, "Error creating SDL render\n");
        return FALSE;
    }
    return TRUE;
}

void emu_init(char *program_name){
    int i;
    FILE *fd;
    uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,
        0x20, 0x60, 0x20, 0x20, 0x70,
        0xF0, 0x10, 0xF0, 0x80, 0xF0,
        0xF0, 0x10, 0xF0, 0x10, 0xF0,
        0x90, 0x90, 0xF0, 0x10, 0x10,
        0xF0, 0x80, 0xF0, 0x10, 0xF0, 
        0xF0, 0x80, 0xF0, 0x90, 0xF0, 
        0xF0, 0x10, 0x20, 0x40, 0x40, 
        0xF0, 0x90, 0xF0, 0x90, 0xF0, 
        0xF0, 0x90, 0xF0, 0x10, 0xF0, 
        0xF0, 0x90, 0xF0, 0x90, 0x90, 
        0xE0, 0x90, 0xE0, 0x90, 0xE0, 
        0xF0, 0x80, 0x80, 0x80, 0xF0, 
        0xE0, 0x90, 0x90, 0x90, 0xE0, 
        0xF0, 0x80, 0xF0, 0x80, 0xF0, 
        0xF0, 0x80, 0xF0, 0x80, 0x80
    };
    for(i = 0; i<80; i++) emu.mem[i] = font[i];
    fd = fopen(program_name, "rw");
    if(fd == NULL){
        fprintf(stderr,"File does not exist or it is not " 
                        "located in the rom's directory\n");
        exit(1);
    }
    fread(&(emu.mem[0x200]), sizeof(uint8_t), 4096-0x200, fd);
    fclose(fd);
    emu.PC = 0x200;
}

void setup(){
    int i, j;
    for(i = 0; i<32;i++){
        for(j = 0; j<64; j++){
            display[i][j].width = PIXEL_WIDTH;
            display[i][j].height = PIXEL_HEIGHT;
            display[i][j].x = j * PIXEL_WIDTH;
            display[i][j].y = i * PIXEL_HEIGHT;
            display[i][j].value = 0;
        }
    }
}

void get_input(){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_QUIT:
                game_status = FALSE;
                break;
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym){
                    case SDLK_ESCAPE: game_status = FALSE; break;
                    case SDLK_SPACE: 
                        game_status = game_status == PAUSED? 1: PAUSED; 
                        break;
                    case SDLK_1: 
                        emu.keys[0x1] = TRUE; break;
                    case SDLK_2: 
                        emu.keys[0x2] = TRUE; break;
                    case SDLK_3: 
                        emu.keys[0x3] = TRUE; break;
                    case SDLK_4: 
                        emu.keys[0xC] = TRUE; break;
                    case SDLK_q: 
                        emu.keys[0x4] = TRUE; break;
                    case SDLK_w: 
                        emu.keys[0x5] = TRUE; break;
                    case SDLK_e: 
                        emu.keys[0x6] = TRUE; break;
                    case SDLK_r: 
                        emu.keys[0xD] = TRUE; break;
                    case SDLK_a: 
                        emu.keys[0x7] = TRUE; break;
                    case SDLK_s: 
                        emu.keys[0x8] = TRUE; break;
                    case SDLK_d: 
                        emu.keys[0x9] = TRUE; break;
                    case SDLK_f: 
                        emu.keys[0xE] = TRUE; break;
                    case SDLK_z: 
                        emu.keys[0xA] = TRUE; break;
                    case SDLK_x: 
                        emu.keys[0x0] = TRUE; break;
                    case SDLK_c: 
                        emu.keys[0xB] = TRUE; break;
                    case SDLK_v: 
                        emu.keys[0xF] = TRUE; break;
                    default: break;
                }
                break;
            case SDL_KEYUP:
                switch(event.key.keysym.sym){
                    case SDLK_1: 
                        emu.keys[0x1] = FALSE; break;
                    case SDLK_2: 
                        emu.keys[0x2] = FALSE; break;
                    case SDLK_3: 
                        emu.keys[0x3] = FALSE; break;
                    case SDLK_4: 
                        emu.keys[0xC] = FALSE; break;
                    case SDLK_q: 
                        emu.keys[0x4] = FALSE; break;
                    case SDLK_w: 
                        emu.keys[0x5] = FALSE; break;
                    case SDLK_e: 
                        emu.keys[0x6] = FALSE; break;
                    case SDLK_r: 
                        emu.keys[0xD] = FALSE; break;
                    case SDLK_a: 
                        emu.keys[0x7] = FALSE; break;
                    case SDLK_s: 
                        emu.keys[0x8] = FALSE; break;
                    case SDLK_d: 
                        emu.keys[0x9] = FALSE; break;
                    case SDLK_f: 
                        emu.keys[0xE] = FALSE; break;
                    case SDLK_z: 
                        emu.keys[0xA] = FALSE; break;
                    case SDLK_x: 
                        emu.keys[0x0] = FALSE; break;
                    case SDLK_c: 
                        emu.keys[0xB] = FALSE; break;
                    case SDLK_v: 
                        emu.keys[0xF] = FALSE; break;
                    default: break;
                }
                break;
            default: break;
        }
    }
}

void render(){
    int i, j;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    for(i = 0; i<32;i++){
        for(j = 0; j<64; j++){
            SDL_Rect ball_rect = {
                (int)display[i][j].x,
                (int)display[i][j].y,
                (int)display[i][j].width,
                (int)display[i][j].height
            };
            SDL_SetRenderDrawColor(renderer, 
                                   display[i][j].value, 
                                   display[i][j].value, 
                                   display[i][j].value, 
                                   255); 
            SDL_RenderFillRect(renderer, &ball_rect);
        }
    }
    SDL_RenderPresent(renderer);
}

void update_timers(){
    if(emu.sound_timer > 0) emu.sound_timer--;
    if(emu.delay_timer > 0) emu.delay_timer--;
}

void update(){
    uint16_t instruction; 
    int start = SDL_GetPerformanceCounter();
    for(int i = 0; i < 500/60; i++){
        if((instruction = fetch()) == 0){
            fprintf(stderr, "Error in instruction fetching...\n");
            exit(1);
        }
        decode_exec(instruction);
    }
    last_time = SDL_GetPerformanceCounter();
    double delay = (double)((last_time - start) * 1000) / SDL_GetPerformanceFrequency();
    if(delay > 0 && 16.67f > delay) SDL_Delay(16.67f - delay);
    render();
    update_timers();
}

void delete_window(){
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr, "Incorrect number of arguments\n");
        return 1;
    }
    game_status = initialize_window();
    emu_init(argv[1]);
    setup();
    while(game_status){
        get_input();
        if(game_status == PAUSED) continue;
        update();
    }
    delete_window();
    return 0;   
}