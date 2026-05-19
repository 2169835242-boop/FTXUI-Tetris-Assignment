#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <random>
#include <ctime>

#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxui;

// 【核心修复】：将原本的全局变量改名，彻底避免与 FTXUI 的 ftxui::HEIGHT 发生命名冲突
const int GAME_BOARD_WIDTH = 10;
const int GAME_BOARD_HEIGHT = 20;

// 各种俄罗斯方块的形状定义
const std::vector<std::vector<std::vector<int>>> TETRIS_SHAPES = {
    {{1, 1, 1, 1}}, // I
    {{1, 1, 1}, {0, 1, 0}}, // T
    {{1, 1, 1}, {1, 0, 0}}, // L
    {{1, 1, 1}, {0, 0, 1}}, // J
    {{1, 1}, {1, 1}}, // O
    {{1, 1, 0}, {0, 1, 1}}, // Z
    {{0, 1, 1}, {1, 1, 0}}  // S
};

class TetrisGame {
public:
    std::vector<std::vector<int>> board;
    std::vector<std::vector<int>> current_piece;
    int piece_x, piece_y;
    int score;
    bool game_over;

    TetrisGame() {
        reset();
    }

    void reset() {
        board = std::vector<std::vector<int>>(GAME_BOARD_HEIGHT, std::vector<int>(GAME_BOARD_WIDTH, 0));
        score = 0;
        game_over = false;
        spawn_piece();
    }

    void spawn_piece() {
        static std::mt19937 gen(std::time(nullptr));
        std::uniform_int_distribution<> dis(0, TETRIS_SHAPES.size() - 1);
        current_piece = TETRIS_SHAPES[dis(gen)];
        piece_y = 0;
        piece_x = GAME_BOARD_WIDTH / 2 - current_piece[0].size() / 2;

        if (check_collision(piece_x, piece_y, current_piece)) {
            game_over = true;
        }
    }

    bool check_collision(int nx, int ny, const std::vector<std::vector<int>>& piece) {
        for (int r = 0; r < piece.size(); ++r) {
            for (int c = 0; c < piece[r].size(); ++c) {
                if (piece[r][c] != 0) {
                    int board_x = nx + c;
                    int board_y = ny + r;
                    if (board_x < 0 || board_x >= GAME_BOARD_WIDTH || board_y >= GAME_BOARD_HEIGHT) {
                        return true;
                    }
                    if (board_y >= 0 && board[board_y][board_x] != 0) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void rotate() {
        if (game_over) return;
        std::vector<std::vector<int>> rotated(current_piece[0].size(), std::vector<int>(current_piece.size(), 0));
        for (int r = 0; r < current_piece.size(); ++r) {
            for (int c = 0; c < current_piece[r].size(); ++c) {
                rotated[c][current_piece.size() - 1 - r] = current_piece[r][c];
            }
        }
        if (!check_collision(piece_x, piece_y, rotated)) {
            current_piece = rotated;
        }
    }

    void move(int dx, int dy) {
        if (game_over) return;
        if (!check_collision(piece_x + dx, piece_y + dy, current_piece)) {
            piece_x += dx;
            piece_y += dy;
        } else if (dy > 0) {
            // 触底，固定方块
            lock_piece();
            clear_lines();
            spawn_piece();
        }
    }

    void lock_piece() {
        for (int r = 0; r < current_piece.size(); ++r) {
            for (int c = 0; c < current_piece[r].size(); ++c) {
                if (current_piece[r][c] != 0 && (piece_y + r) >= 0) {
                    board[piece_y + r][piece_x + c] = 1;
                }
            }
        }
    }

    void clear_lines() {
        std::vector<std::vector<int>> new_board;
        int cleared = 0;
        for (int r = 0; r < GAME_BOARD_HEIGHT; ++r) {
            bool full = true;
            for (int c = 0; c < GAME_BOARD_WIDTH; ++c) {
                if (board[r][c] == 0) { full = false; break; }
            }
            if (full) {
                cleared++;
            } else {
                new_board.push_back(board[r]);
            }
        }
        while (new_board.size() < GAME_BOARD_HEIGHT) {
            new_board.insert(new_board.begin(), std::vector<int>(GAME_BOARD_WIDTH, 0));
        }
        board = new_board;
        score += cleared * 100;
    }
};

int main() {
    // 强制开启控制台 UTF-8 支持，确保彩色和方块不乱码
    system("chcp 65001 > nul");

    auto game = std::make_shared<TetrisGame>();
    auto screen = ScreenInteractive::TerminalOutput();

    // 游戏自动下落线程
    bool running = true;
    std::thread fallback_thread([&]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(600)); // 下落速度 600ms
            if (!game->game_over && running) {
                game->move(0, 1);
                screen.PostEvent(Event::Custom); // 触发界面重绘
            }
        }
    });

    auto component = CatchEvent(Renderer([&] {
        // 创建一个用于渲染的临时网格画布
        auto display_board = game->board;

        // 把当前正在下落的方块绘入临时画布
        if (!game->game_over) {
            for (int r = 0; r < game->current_piece.size(); ++r) {
                for (int c = 0; c < game->current_piece[r].size(); ++c) {
                    if (game->current_piece[r][c] != 0) {
                        int by = game->piece_y + r;
                        int bx = game->piece_x + c;
                        if (by >= 0 && by < GAME_BOARD_HEIGHT && bx >= 0 && bx < GAME_BOARD_WIDTH) {
                            display_board[by][bx] = 2; // 用 2 表示正在下落的活动方块
                        }
                    }
                }
            }
        }

        // 开始构建 FTXUI DOM 元素
        Elements rows;
        for (int r = 0; r < GAME_BOARD_HEIGHT; ++r) {
            Elements cells;
            for (int c = 0; c < GAME_BOARD_WIDTH; ++c) {
                if (display_board[r][c] == 1) {
                    // 已固定的方块：使用双全角实体块，并涂上亮蓝色
                    cells.push_back(text("■") | color(Color::BlueLight));
                } else if (display_board[r][c] == 2) {
                    // 正在控制的活动方块：涂上更加亮眼的青色
                    cells.push_back(text("■") | color(Color::Cyan));
                } else {
                    // 空白区域：用淡淡的点缀表示网格，比空无一物更有质感
                    cells.push_back(text(" .") | color(Color::GrayDark));
                }
            }
            rows.push_back(hbox(std::move(cells)));
        }

        // 拼接成精美的游戏主机外壳 UI
        auto game_view = vbox({
            text(" 🕹️ FTXUI TETRIS 大作业 ") | bold | color(Color::Cyan) | center,
            separator(),
            text(" 得分: " + std::to_string(game->score)) | bold | color(Color::Yellow) | center,
            separator(),
            vbox(std::move(rows)) | border | center, // 游戏主舞台加边框
            separator(),
            text(game->game_over ? " ❌ 游戏结束！按 [R] 重新开始 " : " 控制: [←][→]移动  [↑]旋转  [↓]加速 ") | color(game->game_over ? Color::Red : Color::GrayLight) | center
        });

        return game_view;
    }), [&](Event event) {
        if (event == Event::ArrowLeft)  { game->move(-1, 0); return true; }
        if (event == Event::ArrowRight) { game->move(1, 0); return true; }
        if (event == Event::ArrowDown)  { game->move(0, 1); return true; }
        if (event == Event::ArrowUp)    { game->rotate(); return true; }
        if (event == Event::Character('r') || event == Event::Character('R')) {
            game->reset();
            return true;
        }
        if (event == Event::Escape) {
            running = false;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);

    running = false;
    if (fallback_thread.joinable()) {
        fallback_thread.join();
    }
    return 0;
}