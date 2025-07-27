#include "address_map_arm.h"
#include <stdio.h>

/* function prototypes */
void video_text(int, int, char *);
void video_box(int, int, int, int, short);
int  resample_rgb(int, int);
int  get_data_bits(int);
void draw_paddle(int x);
void erase_paddle(int x);
void draw_blocks(void);
void draw_ball(int x, int y, short color);
void delay(int);
void init_blocks(void);
void check_ball_block_collision(void);
void reset_ball_and_paddle(void);
void update_lives_display(void);
void show_game_over_screen(void);
void show_victory_screen(void);
void clear_text_screen(void);
void show_init_screen(void);

/* constants */
#define STANDARD_X      320
#define STANDARD_Y      240
#define INTEL_BLUE      0x0071C5
#define WHITE_COLOR     0xFFFFFF
#define RED_COLOR       0xFF0000

#define PADDLE_WIDTH    40
#define PADDLE_HEIGHT   5
#define PADDLE_Y        (STANDARD_Y - 20)

#define BLOCK_WIDTH     30
#define BLOCK_HEIGHT    10
#define BLOCK_ROWS      5
#define BLOCK_COLS      8
#define BLOCK_SPACING   5

#define BALL_SIZE       4
#define BALL_START_X    (STANDARD_X / 2)
#define BALL_START_Y    (STANDARD_Y / 2)
#define BALL_COLOR      0xFFFFFF

/* global variables */
int screen_x;
int screen_y;
int res_offset;
int col_offset;

int ball_x = BALL_START_X;
int ball_y = BALL_START_Y;
int ball_dx = 2;
int ball_dy = -2;

int block_alive[BLOCK_ROWS][BLOCK_COLS];
int paddle_x;
int paddle_x_old;

int game_started = 0;
int lives = 3;

int main(void) {
    volatile int *video_resolution = (int *)(PIXEL_BUF_CTRL_BASE + 0x8);
    volatile int *key_ptr = (int *)KEY_BASE;

    screen_x = *video_resolution & 0xFFFF;
    screen_y = (*video_resolution >> 16) & 0xFFFF;

    volatile int *rgb_status = (int *)(RGB_RESAMPLER_BASE);
    int db = get_data_bits(*rgb_status & 0x3F);

    res_offset = (screen_x == 160) ? 1 : 0;
    col_offset = (db == 8) ? 1 : 0;

    short ball_color = resample_rgb(db, BALL_COLOR);
    short bg_color = resample_rgb(db, 0x000000);

    paddle_x = (screen_x - PADDLE_WIDTH) / 2;
    paddle_x_old = paddle_x;

    show_init_screen();
    while (!(*key_ptr & 0x4));

    init_blocks();
    draw_blocks();
    draw_paddle(paddle_x);
    update_lives_display();

    while (1) {
        int keys = *key_ptr;

        if (!game_started && lives > 0) {
            if (!(*key_ptr & 0x4)) {  // botão 2 pressionado
                game_started = 1;
                clear_text_screen();
                update_lives_display();
            }
            continue;
        }

        if (game_started) {
            draw_ball(ball_x, ball_y, bg_color);

            ball_x += ball_dx;
            ball_y += ball_dy;

            if (ball_x <= 0) {
                ball_dx = -ball_dx;
                ball_x = 0;
            } else if (ball_x + BALL_SIZE >= screen_x) {
                ball_dx = -ball_dx;
                ball_x = screen_x - BALL_SIZE;
            }

            if (ball_y <= 0) {
                ball_dy = -ball_dy;
                ball_y = 0;
            }

            paddle_x_old = paddle_x;

            if ((keys & 0x2)) {  // botao 0 = esquerda
                paddle_x -= 5;
                if (paddle_x < 0) paddle_x = 0;
            }
            if ((keys & 0x1)) {  // botao 1 = direita
                paddle_x += 5;
                if (paddle_x + PADDLE_WIDTH > screen_x)
                    paddle_x = screen_x - PADDLE_WIDTH;
            }

            if (paddle_x != paddle_x_old)
                erase_paddle(paddle_x_old);
            draw_paddle(paddle_x);

            if (ball_y + BALL_SIZE >= PADDLE_Y &&
                ball_x + BALL_SIZE >= paddle_x &&
                ball_x <= paddle_x + PADDLE_WIDTH) {
                ball_dy = -ball_dy;
                ball_y = PADDLE_Y - BALL_SIZE;
            }

            if (ball_y + BALL_SIZE >= screen_y) {
                lives--;
                erase_paddle(paddle_x);
                draw_ball(ball_x, ball_y, bg_color);
                update_lives_display();

                if (lives == 0) {
                    show_game_over_screen();
                    while (!(*key_ptr & 0x4));
                    lives = 3;
                    init_blocks();
                    draw_blocks();
                    clear_text_screen();
                    reset_ball_and_paddle();
                    update_lives_display();
                    game_started = 1;
                } else {
                    // Se ainda tem vidas, apenas reinicia a bola e a raquete
                    game_started = 0;
                    reset_ball_and_paddle();
                    video_text(10, 10, "Pressione botao 2 para continuar");
                }
                continue;
            }

            check_ball_block_collision();
            draw_ball(ball_x, ball_y, ball_color);
            delay(50000);
        }
    }
}

void reset_ball_and_paddle(void) {
    ball_x = BALL_START_X;
    ball_y = BALL_START_Y;
    ball_dx = 2;
    ball_dy = -2;
    paddle_x = (screen_x - PADDLE_WIDTH) / 2;
    paddle_x_old = paddle_x;
    draw_paddle(paddle_x);
    draw_ball(ball_x, ball_y, resample_rgb(get_data_bits(*(int *)(RGB_RESAMPLER_BASE) & 0x3F), BALL_COLOR));
}

void update_lives_display(void) {
    char txt[30];
    sprintf(txt, "Vidas: %d          ", lives);
    video_text(40, 0, txt);
}

void show_init_screen(void){
    int db = get_data_bits(*(int *)(RGB_RESAMPLER_BASE) & 0x3F);
    short bg_color = resample_rgb(db, 0x000000);
    clear_text_screen();
    video_box(0, 0, screen_x, screen_y, bg_color);  // Limpa toda a tela;
    video_text(16, 12, "Pressione botao 2 para iniciar!");
}

void show_game_over_screen(void) {
    int db = get_data_bits(*(int *)(RGB_RESAMPLER_BASE) & 0x3F);
    short bg_color = resample_rgb(db, 0x000000);
    clear_text_screen();
    video_box(0, 0, screen_x, screen_y, bg_color);  // Limpa toda a tela
    video_text(30, 10, "    VOCE PERDEU    ");
    video_text(16, 12, "Pressione botao 2 para reiniciar");
}

void show_victory_screen(void) {
    int db = get_data_bits(*(int *)(RGB_RESAMPLER_BASE) & 0x3F);
    short bg_color = resample_rgb(db, 0x000000);
    clear_text_screen();
    video_box(0, 0, screen_x, screen_y, bg_color);
    video_text(30, 10, "   VOCE VENCEU!   ");
    video_text(16, 12, "Pressione botao 2 para reiniciar");
}

void clear_text_screen(void) {
    int row;
    for (row = 0; row < 30; row++) {
        video_text(0, row, "                                                                               ");
    }
}

void init_blocks(void) {
    int i, j;
    for (i = 0; i < BLOCK_ROWS; i++) {
        for (j = 0; j < BLOCK_COLS; j++) {
            block_alive[i][j] = 1;
        }
    }
}

void draw_blocks(void) {
    int start_x = 10, start_y = 30;
    int db = get_data_bits(*(int *)(RGB_RESAMPLER_BASE) & 0x3F);
    short block_color = resample_rgb(db, RED_COLOR);
    short bg_color = resample_rgb(db, 0x000000);
    int row, col;

    for (row = 0; row < BLOCK_ROWS; row++) {
        for (col = 0; col < BLOCK_COLS; col++) {
            int x1 = start_x + col * (BLOCK_WIDTH + BLOCK_SPACING);
            int y1 = start_y + row * (BLOCK_HEIGHT + BLOCK_SPACING);
            int x2 = x1 + BLOCK_WIDTH;
            int y2 = y1 + BLOCK_HEIGHT;
            short color = block_alive[row][col] ? block_color : bg_color;
            video_box(x1, y1, x2, y2, color);
        }
    }
}

void check_ball_block_collision(void) {
    int start_x = 10;
    int start_y = 30;
    int row, col;

    volatile int *video_resolution = (int *)(PIXEL_BUF_CTRL_BASE + 0x8);
    volatile int *key_ptr = (int *)KEY_BASE;

    for (row = 0; row < BLOCK_ROWS; row++) {
        for (col = 0; col < BLOCK_COLS; col++) {
            if (block_alive[row][col]) {
                int x1 = start_x + col * (BLOCK_WIDTH + BLOCK_SPACING);
                int y1 = start_y + row * (BLOCK_HEIGHT + BLOCK_SPACING);
                int x2 = x1 + BLOCK_WIDTH;
                int y2 = y1 + BLOCK_HEIGHT;
                int collided = 0;

                if (ball_x + BALL_SIZE > x1 && ball_x + BALL_SIZE < x1 + 3 &&
                    ball_y + BALL_SIZE > y1 && ball_y < y2) {
                    ball_dx = -ball_dx;
                    collided = 1;
                } else if (ball_x < x2 && ball_x > x2 - 3 &&
                           ball_y + BALL_SIZE > y1 && ball_y < y2) {
                    ball_dx = -ball_dx;
                    collided = 1;
                } else if (ball_y + BALL_SIZE > y1 && ball_y + BALL_SIZE < y1 + 3 &&
                           ball_x + BALL_SIZE > x1 && ball_x < x2) {
                    ball_dy = -ball_dy;
                    collided = 1;
                } else if (ball_y < y2 && ball_y > y2 - 3 &&
                           ball_x + BALL_SIZE > x1 && ball_x < x2) {
                    ball_dy = -ball_dy;
                    collided = 1;
                }

                if (collided) {
                    int i, j, blocks_remaining = 0;
                    block_alive[row][col] = 0;
                    draw_blocks();

                    for (i = 0; i < BLOCK_ROWS; i++) {
                        for (j = 0; j < BLOCK_COLS; j++) {
                            if (block_alive[i][j]) {
                                blocks_remaining = 1;
                                break;
                            }
                        }
                        if (blocks_remaining) break;
                    }

                    if (!blocks_remaining) {
                        game_started = 0;
                        show_victory_screen();
                        while (!(*key_ptr & 0x4));
                        lives = 3;
                        init_blocks();
                        draw_blocks();
                        clear_text_screen();
                        reset_ball_and_paddle();
                        update_lives_display();
                        game_started = 1;
                    }

                    return;
                }
            }
        }
    }
}

void video_text(int x, int y, char *text_ptr) {
    int offset = (y << 7) + x;
    volatile char *character_buffer = (char *)FPGA_CHAR_BASE;
    while (*text_ptr)
        *(character_buffer + offset++) = *text_ptr++;
}

void video_box(int x1, int y1, int x2, int y2, short pixel_color) {
    int pixel_buf_ptr = *(int *)PIXEL_BUF_CTRL_BASE;
    int x_factor = 0x1 << (res_offset + col_offset);
    int y_factor = 0x1 << res_offset;

    int row, col;

    x1 = x1 / x_factor;
    x2 = x2 / x_factor;
    y1 = y1 / y_factor;
    y2 = y2 / y_factor;

    for (row = y1; row <= y2; row++) {
        for (col = x1; col <= x2; ++col) {
            int pixel_ptr = pixel_buf_ptr +
                            (row << (10 - res_offset - col_offset)) + (col << 1);
            *(short *)pixel_ptr = pixel_color;
        }
    }
}

int resample_rgb(int num_bits, int color) {
    if (num_bits == 8) {
        color = (((color >> 16) & 0x000000E0) |
                 ((color >> 11) & 0x0000001C) |
                 ((color >> 6) & 0x00000003));
        color = (color << 8) | color;
    } else if (num_bits == 16) {
        color = (((color >> 8) & 0x0000F800) |
                 ((color >> 5) & 0x000007E0) |
                 ((color >> 3) & 0x0000001F));
    }
    return color;
}

int get_data_bits(int mode) {
    switch (mode) {
        case 0x0: return 1;
        case 0x7:
        case 0x11:
        case 0x31: return 8;
        case 0x12: return 9;
        case 0x14:
        case 0x33: return 16;
        case 0x17: return 24;
        case 0x19: return 30;
        case 0x32: return 12;
        case 0x37: return 32;
        case 0x39: return 40;
        default: return 16;
    }
}

void draw_paddle(int x) {
    int db = get_data_bits(*(int *)(RGB_RESAMPLER_BASE) & 0x3F);
    short color = resample_rgb(db, WHITE_COLOR);
    video_box(x, PADDLE_Y, x + PADDLE_WIDTH, PADDLE_Y + PADDLE_HEIGHT, color);
}

void erase_paddle(int x) {
    int db = get_data_bits(*(int *)(RGB_RESAMPLER_BASE) & 0x3F);
    short bg_color = resample_rgb(db, 0x000000);
    video_box(x, PADDLE_Y, x + PADDLE_WIDTH, PADDLE_Y + PADDLE_HEIGHT, bg_color);
}

void draw_ball(int x, int y, short color) {
    video_box(x, y, x + BALL_SIZE, y + BALL_SIZE, color);
}

void delay(int n) {
    volatile int i;
    for (i = 0; i < n; i++);
}