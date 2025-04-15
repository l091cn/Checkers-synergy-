#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

// Класс Board представляет игровую доску для шашек
// Отвечает за отрисовку доски, фигур, перемещение фигур, хранение истории ходов
// и визуальную обратную связь для игрока
class Board
{
public:
    Board() = default;
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)
    {
    }

    // Функция start_draw() инициализирует SDL, создает окно и загружает все текстуры
    // Возвращает 0 при успешной инициализации, 1 в случае ошибки
    int start_draw()
    {
        // Инициализация SDL
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }

        // Определение размеров окна, если они не заданы явно
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desctop display mode");
                return 1;
            }
            W = min(dm.w, dm.h);
            W -= W / 15;
            H = W;
        }

        // Создание окна и рендерера
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }

        // Загрузка всех необходимых текстур
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }

        // Получение фактических размеров окна и инициализация начального состояния доски
        SDL_GetRendererOutputSize(ren, &W, &H);
        make_start_mtx();
        rerender();
        return 0;
    }

    // Функция redraw() сбрасывает игру в начальное состояние
    // Очищает историю ходов и восстанавливает начальную расстановку шашек
    void redraw()
    {
        game_results = -1;
        history_mtx.clear();
        history_beat_series.clear();
        make_start_mtx();
        clear_active();
        clear_highlight();
    }

    // Функция move_piece() перемещает шашку согласно переданному ходу
    // Если ход содержит взятие (turn.xb != -1), удаляет побитую шашку
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        if (turn.xb != -1)
        {
            mtx[turn.xb][turn.yb] = 0;  // Удаление побитой шашки
        }
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // Перегруженная функция move_piece() для перемещения шашки по координатам
    // Также проверяет возможность превращения в дамку
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        // Проверки на корректность хода
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }

        // Превращение в дамку при достижении последней горизонтали
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;

        mtx[i2][j2] = mtx[i][j];  // Перемещение шашки на новую позицию
        drop_piece(i, j);  // Удаление шашки с исходной позиции
        add_history(beat_series);  // Добавление хода в историю
    }

    // Функция drop_piece() удаляет шашку с указанной позиции
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;
        rerender();
    }

    // Функция turn_into_queen() превращает шашку в дамку
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2;  // Увеличение значения на 2 превращает шашку в дамку
        rerender();
    }

    // Функция get_board() возвращает текущее состояние доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Функция highlight_cells() подсвечивает клетки с возможными ходами
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;
        }
        rerender();
    }

    // Функция clear_highlight() убирает подсветку со всех клеток
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);
        }
        rerender();
    }

    // Функция set_active() помечает шашку как активную (выбранную игроком)
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();
    }

    // Функция clear_active() снимает выделение с активной шашки
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender();
    }

    // Функция is_highlighted() проверяет, подсвечена ли клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // Функция rollback() отменяет последний ход или серию ходов
    // Учитывает серию взятий как один полный ход
    void rollback()
    {
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();
            history_beat_series.pop_back();
        }
        mtx = *(history_mtx.rbegin());  // Восстановление состояния доски из истории
        clear_highlight();
        clear_active();
    }

    // Функция show_final() отображает результат игры
    // res: 0 - ничья, 1 - победа белых, 2 - победа черных
    void show_final(const int res)
    {
        game_results = res;
        rerender();
    }

    // Функция reset_window_size() обновляет размеры окна после изменения пользователем
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender();
    }

    // Функция quit() освобождает все ресурсы SDL и закрывает окно
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    ~Board()
    {
        if (win)
            quit();
    }

private:
    // Функция add_history() добавляет текущее состояние доски в историю ходов
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }

    // Функция make_start_mtx() создает начальную расстановку шашек на доске
    // Белые шашки (1) размещаются в нижней части доски, черные (2) - в верхней
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2;  // Черные шашки в верхней части
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1;  // Белые шашки в нижней части
            }
        }
        add_history();
    }

    // Функция rerender() перерисовывает всю графику игры
    // Отображает доску, шашки, подсветку, активную шашку и результат игры (если есть)
    void rerender()
    {
        // Отрисовка доски
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, board, NULL, NULL);

        // Отрисовка шашек
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j])
                    continue;
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                // Выбор текстуры в зависимости от типа шашки:
                // 1 - белая шашка, 2 - черная шашка, 3 - белая дамка, 4 - черная дамка
                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1)
                    piece_texture = w_piece;
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece;
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen;
                else
                    piece_texture = b_queen;

                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // Отрисовка подсветки возможных ходов (зеленым цветом)
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j])
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // Отрисовка активной шашки (красным цветом)
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),
                                 int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1);

        // Отрисовка кнопок управления (отмена хода и перезапуск)
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // Отрисовка результата игры (если игра завершена)
        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1)
                result_path = white_path;  // Победа белых
            else if (game_results == 2)
                result_path = black_path;  // Победа черных
            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        // Обновление экрана
        SDL_RenderPresent(ren);
        // Короткая задержка и обработка событий для Mac OS
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Функция print_exception() записывает сообщение об ошибке в лог-файл
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". " << SDL_GetError() << endl;
        fout.close();
    }

public:
    int W = 0;  // Ширина окна
    int H = 0;  // Высота окна
    // История состояний доски для отмены ходов
    vector<vector<vector<POS_T>>> history_mtx;

private:
    SDL_Window* win = nullptr;  // Окно SDL
    SDL_Renderer* ren = nullptr;  // Рендерер SDL

    // Текстуры для отображения элементов игры
    SDL_Texture* board = nullptr;  // Доска
    SDL_Texture* w_piece = nullptr;  // Белая шашка
    SDL_Texture* b_piece = nullptr;  // Черная шашка
    SDL_Texture* w_queen = nullptr;  // Белая дамка
    SDL_Texture* b_queen = nullptr;  // Черная дамка
    SDL_Texture* back = nullptr;  // Кнопка отмены хода
    SDL_Texture* replay = nullptr;  // Кнопка перезапуска игры

    // Пути к файлам текстур
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";

    // Координаты выбранной (активной) шашки
    int active_x = -1, active_y = -1;

    // Результат игры: -1 - игра продолжается, 0 - ничья, 1 - победа белых, 2 - победа черных
    int game_results = -1;

    // Матрица подсвеченных клеток (возможных ходов)
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));

    // Матрица состояния доски:
    // 0 - пусто, 1 - белая шашка, 2 - черная шашка, 3 - белая дамка, 4 - черная дамка
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));

    // История серий взятий для каждого хода (для корректной отмены ходов)
    vector<int> history_beat_series;
};